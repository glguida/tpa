#!/usr/bin/env python3
"""
PTQ toolchain: YOLOv5* → Erbium INT8 weight export.

For each Conv (CBS) and raw Conv2d layer:
  - Fuse BatchNorm into weights/bias
  - Per-channel symmetric INT8 weight quantization
  - Calibrate per-tensor activation input/output scales
  - Compute TensorQuant combined_scale (act_in * w_scale[oc] / act_out)
  - Compute INT32 bias (b_fused / (act_in * w_scale[oc]))
  - Build SiLU LUT (INT8→INT8 via act_out scale) if layer has SiLU

Outputs:
  tests/yolo/generated/<stem>_weights.c   data arrays in .mram_data section
  tests/yolo/generated/<stem>_weights.h   extern declarations + layer metadata table
"""

import sys, os, argparse, math, textwrap, json
import numpy as np
import torch
import torch.nn as nn
from pathlib import Path

# ---------------------------------------------------------------------------
# BN fusion
# ---------------------------------------------------------------------------

def fuse_conv_bn(conv, bn):
    """Return (W_fused [K,C,kH,kW], b_fused [K]) float32 numpy."""
    W = conv.weight.detach().float().cpu().numpy()
    b = conv.bias.detach().float().cpu().numpy() if conv.bias is not None \
        else np.zeros(W.shape[0], dtype=np.float32)

    gamma = bn.weight.detach().float().cpu().numpy()
    beta  = bn.bias.detach().float().cpu().numpy()
    mean  = bn.running_mean.detach().float().cpu().numpy()
    var   = bn.running_var.detach().float().cpu().numpy()

    scale = gamma / np.sqrt(var + bn.eps)
    W_f = (W * scale[:, None, None, None]).astype(np.float32)
    b_f = (beta + (b - mean) * scale).astype(np.float32)
    return W_f, b_f


def extract_raw_conv(conv):
    """Return (W [K,C,kH,kW], b [K]) for raw Conv2d (no BN)."""
    W = conv.weight.detach().float().cpu().numpy()
    b = conv.bias.detach().float().cpu().numpy() if conv.bias is not None \
        else np.zeros(W.shape[0], dtype=np.float32)
    return W.astype(np.float32), b.astype(np.float32)


# ---------------------------------------------------------------------------
# Weight quantization
# ---------------------------------------------------------------------------

def w_stride_for(K_inner):
    return max(64, math.ceil(K_inner / 64) * 64)


def quantize_weights(W_fused):
    """
    W_fused: [K_out, C_in, kH, kW]
    Returns w_int8 [K_out, w_stride] (int8, zero-padded),
            w_scale [K_out] (float32, per-channel).

    Per-output-channel rows are emitted in [kH][kW][C_in] order to match the
    dnn-library style scalar convolution worker.
    """
    K_out = W_fused.shape[0]
    K_inner = W_fused.shape[1] * W_fused.shape[2] * W_fused.shape[3]
    W_flat = np.transpose(W_fused, (0, 2, 3, 1)).reshape(K_out, K_inner)

    w_scale = np.maximum(np.abs(W_flat).max(axis=1) / 127.0, 1e-8).astype(np.float32)
    w_int8_flat = np.clip(np.round(W_flat / w_scale[:, None]), -128, 127).astype(np.int8)

    stride = w_stride_for(K_inner)
    padded = np.zeros((K_out, stride), dtype=np.int8)
    padded[:, :K_inner] = w_int8_flat
    return padded, w_scale


# ---------------------------------------------------------------------------
# Activation range collection
# ---------------------------------------------------------------------------

class Percentile99Collector:
    def __init__(self):
        self._samples = []

    def update(self, x: torch.Tensor):
        v = x.detach().abs().reshape(-1).float().cpu().numpy()
        # keep top-1000 per batch to limit memory
        if len(v) > 1000:
            v = np.partition(v, -1000)[-1000:]
        self._samples.append(v)

    def scale(self):
        if not self._samples:
            return 1.0 / 127.0
        all_v = np.concatenate(self._samples)
        # 99.9th percentile to clip outliers
        p = float(np.percentile(all_v, 99.9))
        return max(p, 1e-8) / 127.0


def collect_activation_ranges(model, calib_tensors):
    """
    Run calibration and collect per-layer (act_in_scale, act_out_scale).
    Returns dict: module_path → (in_scale_float, out_scale_float).
    """
    m = model.model
    collectors = {}   # name → (in_col, out_col)
    hooks = []

    def register(name, mod):
        ic = Percentile99Collector()
        oc = Percentile99Collector()
        collectors[name] = (ic, oc)

        def pre_hook(module, inp):
            ic.update(inp[0])

        def post_hook(module, inp, out):
            oc.update(out)

        hooks.append(mod.register_forward_pre_hook(pre_hook))
        hooks.append(mod.register_forward_hook(post_hook))

    # CBS Conv wrappers
    for name, mod in m.named_modules():
        if type(mod).__name__ == 'Conv' and hasattr(mod, 'conv') and hasattr(mod, 'bn'):
            register(name, mod)

    # Raw Conv2d (detect head — no BN parent)
    for name, mod in m.named_modules():
        if isinstance(mod, nn.Conv2d):
            parts = name.split('.')
            parent = m
            for p in parts[:-1]:
                parent = getattr(parent, p)
            if not hasattr(parent, 'bn'):
                register(name, mod)

    m.eval()
    with torch.no_grad():
        for t in calib_tensors:
            m(t)

    for h in hooks:
        h.remove()

    return {name: (ic.scale(), oc.scale()) for name, (ic, oc) in collectors.items()}


# ---------------------------------------------------------------------------
# SiLU LUT
# ---------------------------------------------------------------------------

def build_silu_lut(act_scale):
    """
    Build 256-entry INT8→INT8 SiLU LUT.
    Index i (uint8): treat as int8 v = i if i<128 else i-256.
    real = v * act_scale; out_int8 = clamp(round(silu(real)/act_scale), -128, 127).
    """
    lut = np.zeros(256, dtype=np.uint8)
    for i in range(256):
        v = i if i < 128 else i - 256         # reinterpret as int8
        real = v * act_scale
        silu_val = real * (1.0 / (1.0 + math.exp(-real)))
        out = int(round(silu_val / act_scale))
        out = max(-128, min(127, out))
        lut[i] = out & 0xFF                    # store as uint8 (two's complement)
    return lut


# ---------------------------------------------------------------------------
# Padding helpers
# ---------------------------------------------------------------------------

def pad_to_64b_int32(arr):
    """Pad [K] int32 array to multiple of 16 (= 64 bytes)."""
    K = len(arr)
    K_pad = math.ceil(K / 16) * 16
    out = np.zeros(K_pad, dtype=np.int32)
    out[:K] = arr
    return out


def pad_to_64b_float32(arr):
    """Pad [K] float32 array to multiple of 16 (= 64 bytes)."""
    K = len(arr)
    K_pad = math.ceil(K / 16) * 16
    out = np.zeros(K_pad, dtype=np.float32)
    out[:K] = arr
    return out


# ---------------------------------------------------------------------------
# C emission helpers
# ---------------------------------------------------------------------------

def short_prefix_for_stem(stem):
    if stem.startswith('yolov5'):
        return 'yv5' + stem[len('yolov5'):]
    return stem.replace('-', '_')


def stem_upper(stem):
    return stem.replace('-', '_').upper()


def c_varname(module_path, prefix):
    """'model.2.cv1' → '<prefix>_model_2_cv1'"""
    return prefix + '_' + module_path.replace('.', '_')


def emit_int8_array(f, varname, data, comment=''):
    """Emit const int8_t varname[] = {...};"""
    flat = data.reshape(-1)
    vals = ', '.join(str(int(x)) for x in flat)
    n = len(flat)
    if comment:
        f.write(f'/* {comment} */\n')
    f.write(f'const int8_t {varname}_w[{n}]'
            f' __attribute__((section(".mram_data"))) = {{\n')
    # 16 values per line
    lines = [flat[i:i+16] for i in range(0, n, 16)]
    for row in lines:
        f.write('    ' + ', '.join(str(int(x)) for x in row) + ',\n')
    f.write('};\n\n')


def emit_int32_array(f, varname, data, suffix='_b', comment=''):
    flat = data.reshape(-1)
    n = len(flat)
    if comment:
        f.write(f'/* {comment} */\n')
    f.write(f'const int32_t {varname}{suffix}[{n}]'
            f' __attribute__((section(".mram_data"))) = {{\n')
    lines = [flat[i:i+8] for i in range(0, n, 8)]
    for row in lines:
        f.write('    ' + ', '.join(str(int(x)) for x in row) + ',\n')
    f.write('};\n\n')


def emit_float32_array(f, varname, data, suffix='_s', comment=''):
    flat = data.reshape(-1)
    n = len(flat)
    if comment:
        f.write(f'/* {comment} */\n')
    f.write(f'const float {varname}{suffix}[{n}]'
            f' __attribute__((section(".mram_data"))) = {{\n')
    lines = [flat[i:i+8] for i in range(0, n, 8)]
    for row in lines:
        vals = ', '.join(f'{float(x):.8e}f' for x in row)
        f.write(f'    {vals},\n')
    f.write('};\n\n')


def emit_uint8_array(f, varname, data, suffix='_lut', comment=''):
    flat = data.reshape(-1)
    n = len(flat)
    if comment:
        f.write(f'/* {comment} */\n')
    f.write(f'const uint8_t {varname}{suffix}[{n}]'
            f' __attribute__((section(".mram_data"))) = {{\n')
    lines = [flat[i:i+16] for i in range(0, n, 16)]
    for row in lines:
        f.write('    ' + ', '.join(str(int(x)) for x in row) + ',\n')
    f.write('};\n\n')


# ---------------------------------------------------------------------------
# Layer descriptor
# ---------------------------------------------------------------------------

class LayerDesc:
    def __init__(self, name, conv, W_f, b_f, has_silu,
                 act_in_scale, act_out_scale, prefix):
        self.name        = name
        self.varname     = c_varname(name, prefix)
        self.conv        = conv
        self.W_f         = W_f              # float32 [K,C,kH,kW]
        self.b_f         = b_f              # float32 [K]
        self.has_silu    = has_silu

        K_out, C_in, kH, kW = W_f.shape
        K_inner = C_in * kH * kW

        self.K_out    = K_out
        self.C_in     = C_in
        self.kH       = kH
        self.kW       = kW
        self.stride_h = conv.stride[0]
        self.stride_w = conv.stride[1]
        self.pad_h    = conv.padding[0] if isinstance(conv.padding, tuple) else conv.padding
        self.pad_w    = conv.padding[1] if isinstance(conv.padding, tuple) else conv.padding
        self.K_inner  = K_inner
        self.w_stride = w_stride_for(K_inner)

        self.act_in_scale  = act_in_scale
        self.act_out_scale = act_out_scale

        # Quantize weights
        self.w_int8, self.w_scale = quantize_weights(W_f)

        # INT32 bias: b_fused / (act_in * w_scale[oc])
        denom = act_in_scale * self.w_scale           # [K_out]
        bias_f = np.round(b_f / denom).astype(np.int32)
        self.bias_int32 = pad_to_64b_int32(bias_f)

        # Combined TensorQuant scale: act_in * w_scale[oc] / act_out
        cs = (act_in_scale * self.w_scale / act_out_scale).astype(np.float32)
        self.combined_scale = pad_to_64b_float32(cs)

        # SiLU LUT (uses act_out_scale — the scale of the TensorQuant output)
        self.silu_lut = build_silu_lut(act_out_scale) if has_silu else None


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args():
    p = argparse.ArgumentParser(description='YOLOv5 PTQ export')
    p.add_argument('--model', default='models/yolov5nu.pt',
                   help='Path to YOLOv5 .pt file')
    p.add_argument('--stem', default='yolov5s',
                   help='Output stem, e.g. yolov5s or yolov5n')
    p.add_argument('--calib-dir', default=None,
                   help='Directory of calibration JPEG/PNG images (640×640 recommended)')
    p.add_argument('--n-calib', type=int, default=32,
                   help='Number of calibration images (default 32)')
    p.add_argument('--out-dir', default='tests/yolo/generated',
                   help='Output directory for generated C files')
    p.add_argument('--layers', default=None,
                   help='Comma-separated export subset, e.g. 27,28,40')
    p.add_argument('--layer-map', default=None,
                   help='JSON file mapping explicit logical layer ids to stable module names')
    p.add_argument('--inline-header', default=None,
                   help='Emit a self-contained static header instead of .c/.h')
    return p.parse_args()


def parse_layer_subset(spec):
    if not spec:
        return None
    vals = []
    for part in spec.split(','):
        part = part.strip()
        if part:
            vals.append(int(part, 10))
    return vals


def load_layer_map(path):
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if isinstance(data, dict) and 'layers' in data:
        layers = data['layers']
        full_n_layers = data.get('full_n_layers')
    elif isinstance(data, dict):
        layers = data
        full_n_layers = None
    else:
        sys.exit(f'Invalid layer map in {path}: expected object')

    norm = {}
    for k, v in layers.items():
        try:
            idx = int(k)
        except ValueError as e:
            sys.exit(f'Invalid layer id {k!r} in {path}: {e}')
        if not isinstance(v, str):
            sys.exit(f'Invalid layer name for id {k!r} in {path}: expected string')
        norm[idx] = v

    return {
        'path': str(Path(path).resolve()),
        'full_n_layers': int(full_n_layers) if full_n_layers is not None else None,
        'layers': norm,
    }


def build_provenance(args, ultralytics_version, full_n_layers, layers, layer_map):
    calib_desc = (str(Path(args.calib_dir).resolve())
                  if args.calib_dir else '<synthetic-random>')
    lines = [
        f'Source model: {Path(args.model).resolve()}',
        f'Calibration: {calib_desc}',
        f'Calibration images: {args.n_calib}',
        f'Ultralytics: {ultralytics_version}',
        f'Full layer count: {full_n_layers}',
        f'Layer map: {layer_map["path"] if layer_map else "<runtime-indices>"}',
        'Selected layers:',
    ]
    for ld in layers:
        export_idx = getattr(ld, 'export_index', ld.index)
        lines.append(f'  {export_idx} <- {ld.name} (runtime {ld.index})')
    return lines


def emit_banner(f, provenance_lines):
    f.write(textwrap.dedent("""\
        /*-------------------------------------------------------------------------
         * Copyright (c) 2025 Ainekko, Co.
         * SPDX-License-Identifier: Apache-2.0
         *
         * AUTO-GENERATED by tools/ptq_yolov5.py — DO NOT EDIT
    """))
    for line in provenance_lines:
        f.write(f' * {line}\n')
    f.write(" *-------------------------------------------------------------------------*/\n\n")


def emit_inline_header(path, stem, prefix, full_n_layers, layers, provenance_lines):
    guard = stem_upper(Path(path).stem + '_H')
    with open(path, 'w') as f:
        emit_banner(f, provenance_lines)
        f.write(textwrap.dedent(f"""\
            #ifndef {guard}
            #define {guard}

            #include <stdint.h>

            typedef struct {{
                const int8_t   *w;
                const int32_t  *b;
                const float    *s;
                const uint8_t  *lut;
                uint32_t        K_out;
                uint32_t        C_in;
                uint32_t        kH, kW;
                uint32_t        stride_h, stride_w;
                uint32_t        pad_h, pad_w;
                uint32_t        K_inner;
                uint32_t        w_stride;
                uint32_t        K_out_pad;
                float           act_in_scale;
                float           act_out_scale;
            }} {prefix}_layer_t;

            #define {stem_upper(stem)}_N_LAYERS {full_n_layers}

        """))

        for ld in layers:
            vn = ld.varname
            K_out = ld.K_out
            K_pad = len(ld.bias_int32)

            comment_w = (f'{ld.name}: weight [{K_out}x{ld.w_stride}] '
                         f'(K_inner={ld.K_inner}, row-order=[kH][kW][C])')
            emit_int8_array(f, vn, ld.w_int8, comment=comment_w)

            comment_b = (f'{ld.name}: bias_int32 [{K_pad}] '
                         f'(act_in={ld.act_in_scale:.4e})')
            emit_int32_array(f, vn, ld.bias_int32, suffix='_b', comment=comment_b)

            comment_s = (f'{ld.name}: combined_scale [{K_pad}] '
                         f'(act_out={ld.act_out_scale:.4e})')
            emit_float32_array(f, vn, ld.combined_scale, suffix='_s', comment=comment_s)

            if ld.silu_lut is not None:
                emit_uint8_array(f, vn, ld.silu_lut, suffix='_lut',
                                 comment=f'{ld.name}: SiLU LUT [256]')

        f.write(f'static const {prefix}_layer_t {prefix}_layers[{stem_upper(stem)}_N_LAYERS]'
                f' __attribute__((section(".mram_data"))) = {{\n')
        for ld in layers:
            vn = ld.varname
            lut_p = f'{vn}_lut' if ld.silu_lut is not None else '((void*)0)'
            K_pad = len(ld.bias_int32)
            export_idx = getattr(ld, 'export_index', ld.index)
            f.write(f'    [{export_idx}] = {{ /* {ld.name} */\n')
            f.write(f'        .w          = {vn}_w,\n')
            f.write(f'        .b          = {vn}_b,\n')
            f.write(f'        .s          = {vn}_s,\n')
            f.write(f'        .lut        = {lut_p},\n')
            f.write(f'        .K_out      = {ld.K_out},\n')
            f.write(f'        .C_in       = {ld.C_in},\n')
            f.write(f'        .kH         = {ld.kH},\n')
            f.write(f'        .kW         = {ld.kW},\n')
            f.write(f'        .stride_h   = {ld.stride_h},\n')
            f.write(f'        .stride_w   = {ld.stride_w},\n')
            f.write(f'        .pad_h      = {ld.pad_h},\n')
            f.write(f'        .pad_w      = {ld.pad_w},\n')
            f.write(f'        .K_inner    = {ld.K_inner},\n')
            f.write(f'        .w_stride   = {ld.w_stride},\n')
            f.write(f'        .K_out_pad  = {K_pad},\n')
            f.write(f'        .act_in_scale  = {ld.act_in_scale:.8e}f,\n')
            f.write(f'        .act_out_scale = {ld.act_out_scale:.8e}f,\n')
            f.write(f'    }},\n')
        f.write('};\n\n')
        f.write(f'#endif /* {guard} */\n')


def load_calib_images(calib_dir, n, device):
    """Load images from directory, resize to 640×640, normalize to [0,1]."""
    try:
        import cv2
    except ImportError:
        sys.exit('cv2 not available; install opencv-python-headless')

    paths = []
    for ext in ('*.jpg', '*.jpeg', '*.png', '*.bmp'):
        paths += list(Path(calib_dir).glob(ext))
        paths += list(Path(calib_dir).glob(ext.upper()))
    if not paths:
        sys.exit(f'No images found in {calib_dir}')
    paths = sorted(paths)[:n]
    tensors = []
    for p in paths:
        img = cv2.imread(str(p))
        img = cv2.resize(img, (640, 640))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        t = torch.from_numpy(img).float().permute(2, 0, 1) / 255.0
        tensors.append(t.unsqueeze(0).to(device))
    print(f'Loaded {len(tensors)} calibration images from {calib_dir}')
    return tensors


def make_synthetic_calib(n, device):
    print(f'WARNING: using {n} synthetic random images for calibration.')
    print('         For production, pass --calib-dir with real images.')
    # Use a fixed seed for reproducibility
    rng = np.random.default_rng(42)
    tensors = []
    for _ in range(n):
        img = rng.uniform(0, 1, (1, 3, 640, 640)).astype(np.float32)
        tensors.append(torch.from_numpy(img).to(device))
    return tensors


def main():
    args = parse_args()
    stem = args.stem
    prefix = short_prefix_for_stem(stem)
    stem_uc = stem_upper(stem)
    layer_subset = parse_layer_subset(args.layers)
    layer_map = load_layer_map(args.layer_map) if args.layer_map else None

    print(f'Loading model from {args.model} ...')
    import ultralytics
    from ultralytics import YOLO
    model = YOLO(args.model)
    m = model.model.eval()
    device = next(m.parameters()).device

    # --- Prepare calibration tensors ---
    if args.calib_dir:
        calib = load_calib_images(args.calib_dir, args.n_calib, device)
    else:
        calib = make_synthetic_calib(args.n_calib, device)

    # --- Collect activation ranges ---
    print('Collecting activation ranges ...')
    ranges = collect_activation_ranges(model, calib)
    print(f'  Collected ranges for {len(ranges)} layers.')

    # --- Build layer descriptors ---
    layers = []

    # Walk CBS Conv wrappers in execution order (pre_hook captures exec order)
    exec_order = []
    tmp_hooks = []
    for name, mod in m.named_modules():
        if type(mod).__name__ == 'Conv' and hasattr(mod, 'conv') and hasattr(mod, 'bn'):
            def _h(n):
                def hook(mod, inp, out):
                    exec_order.append(n)
                return hook
            tmp_hooks.append(mod.register_forward_hook(_h(name)))
    # Also raw Conv2d
    raw_convs_ordered = []
    for name, mod in m.named_modules():
        if isinstance(mod, nn.Conv2d):
            parts = name.split('.')
            parent = m
            for p in parts[:-1]:
                parent = getattr(parent, p)
            if not hasattr(parent, 'bn'):
                raw_convs_ordered.append(name)

    with torch.no_grad():
        m(torch.zeros(1, 3, 64, 64, device=device))
    for h in tmp_hooks:
        h.remove()

    # CBS layers in exec order
    seen = set()
    for name in exec_order:
        if name in seen:
            continue
        seen.add(name)
        mod = m
        for p in name.split('.'):
            mod = getattr(mod, p)
        # mod is the CBS Conv wrapper
        conv = mod.conv
        bn   = mod.bn
        has_silu = hasattr(mod, 'act') and not isinstance(mod.act, nn.Identity)

        W_f, b_f = fuse_conv_bn(conv, bn)
        ain, aout = ranges.get(name, (1.0/127.0, 1.0/127.0))
        layers.append(LayerDesc(name, conv, W_f, b_f, has_silu, ain, aout, prefix))

    # Raw Conv2d layers
    for name in raw_convs_ordered:
        if name in seen:
            continue
        seen.add(name)
        mod = m
        for p in name.split('.'):
            mod = getattr(mod, p)
        W_f, b_f = extract_raw_conv(mod)
        ain, aout = ranges.get(name, (1.0/127.0, 1.0/127.0))
        layers.append(LayerDesc(name, mod, W_f, b_f, False, ain, aout, prefix))

    for idx, ld in enumerate(layers):
        ld.index = idx
        ld.export_index = idx

    full_n_layers = len(layers)
    if layer_map is not None:
        by_name = {ld.name: ld for ld in layers}
        selected_ids = (sorted(layer_map['layers'])
                        if layer_subset is None else layer_subset)
        selected_layers = []
        for export_idx in selected_ids:
            if export_idx not in layer_map['layers']:
                sys.exit(f'Layer id {export_idx} not found in layer map {layer_map["path"]}')
            name = layer_map['layers'][export_idx]
            if name not in by_name:
                sys.exit(f'Layer name {name!r} from {layer_map["path"]} not found in current model')
            ld = by_name[name]
            ld.export_index = export_idx
            selected_layers.append(ld)
        layers = selected_layers
        if layer_map['full_n_layers'] is not None:
            full_n_layers = layer_map['full_n_layers']
    elif layer_subset is not None:
        want = set(layer_subset)
        layers = [ld for ld in layers if ld.index in want]

    print(f'Total layers to export: {len(layers)}')
    provenance_lines = build_provenance(
        args, ultralytics.__version__, full_n_layers, layers, layer_map)

    # --- Emit C files ---
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.inline_header:
        out_path = Path(args.inline_header)
        print(f'Writing {out_path} ...')
        emit_inline_header(out_path, stem, prefix, full_n_layers, layers,
                           provenance_lines)
        return

    c_path = out_dir / f'{stem}_weights.c'
    h_path = out_dir / f'{stem}_weights.h'

    print(f'Writing {c_path} ...')
    with open(c_path, 'w') as f:
        emit_banner(f, provenance_lines)
        f.write('#include <stdint.h>\n')
        f.write(f'#include "{stem}_weights.h"\n\n')

        for ld in layers:
            vn = ld.varname
            K_out  = ld.K_out
            C_in   = ld.C_in
            stride = ld.w_stride
            K_pad  = len(ld.bias_int32)

            comment_w = (f'{ld.name}: weight [{K_out}x{stride}] '
                         f'(K_inner={ld.K_inner}, row-order=[kH][kW][C])')
            emit_int8_array(f, vn, ld.w_int8, comment=comment_w)

            comment_b = (f'{ld.name}: bias_int32 [{K_pad}] '
                         f'(act_in={ld.act_in_scale:.4e})')
            emit_int32_array(f, vn, ld.bias_int32, suffix='_b', comment=comment_b)

            comment_s = (f'{ld.name}: combined_scale [{K_pad}] '
                         f'(act_out={ld.act_out_scale:.4e})')
            emit_float32_array(f, vn, ld.combined_scale, suffix='_s', comment=comment_s)

            if ld.silu_lut is not None:
                emit_uint8_array(f, vn, ld.silu_lut, suffix='_lut',
                                 comment=f'{ld.name}: SiLU LUT [256]')

    # Emit layer table in the .c file (in .mram_data, not .rodata)
    with open(c_path, 'a') as f:
        f.write('/* Layer metadata table — in MRAM (.mram_data), not .rodata */\n')
        f.write(f'const {prefix}_layer_t {prefix}_layers[{stem_uc}_N_LAYERS]'
                f' __attribute__((section(".mram_data"))) = {{\n')
        for ld in layers:
            vn    = ld.varname
            lut_p = f'{vn}_lut' if ld.silu_lut is not None else '((void*)0)'
            K_pad = len(ld.bias_int32)
            export_idx = getattr(ld, 'export_index', ld.index)
            f.write(f'    [{export_idx}] = {{ /* {ld.name} */\n')
            f.write(f'        .w          = {vn}_w,\n')
            f.write(f'        .b          = {vn}_b,\n')
            f.write(f'        .s          = {vn}_s,\n')
            f.write(f'        .lut        = {lut_p},\n')
            f.write(f'        .K_out      = {ld.K_out},\n')
            f.write(f'        .C_in       = {ld.C_in},\n')
            f.write(f'        .kH         = {ld.kH},\n')
            f.write(f'        .kW         = {ld.kW},\n')
            f.write(f'        .stride_h   = {ld.stride_h},\n')
            f.write(f'        .stride_w   = {ld.stride_w},\n')
            f.write(f'        .pad_h      = {ld.pad_h},\n')
            f.write(f'        .pad_w      = {ld.pad_w},\n')
            f.write(f'        .K_inner    = {ld.K_inner},\n')
            f.write(f'        .w_stride   = {ld.w_stride},\n')
            f.write(f'        .K_out_pad  = {K_pad},\n')
            f.write(f'        .act_in_scale  = {ld.act_in_scale:.8e}f,\n')
            f.write(f'        .act_out_scale = {ld.act_out_scale:.8e}f,\n')
            f.write(f'    }},\n')
        f.write('};\n')

    print(f'Writing {h_path} ...')
    with open(h_path, 'w') as f:
        emit_banner(f, provenance_lines)
        f.write(f'#ifndef {stem_uc}_WEIGHTS_H\n')
        f.write(f'#define {stem_uc}_WEIGHTS_H\n\n')
        f.write(textwrap.dedent("""\

            #include <stdint.h>

            /* Layer metadata — one entry per quantized Conv layer.
             * The layer table is defined in the generated weights .c file
             * in .mram_data to keep it out of the 4KB .rodata/.bootrom. */
            typedef struct {
                const int8_t   *w;       /* weight [K_out * w_stride], row-order=[kH][kW][C_in] */
                const int32_t  *b;       /* bias_int32 [K_out_pad] */
                const float    *s;       /* combined_scale [K_out_pad] */
                const uint8_t  *lut;     /* SiLU LUT [256], NULL if no SiLU */
                uint32_t        K_out;
                uint32_t        C_in;
                uint32_t        kH, kW;
                uint32_t        stride_h, stride_w;
                uint32_t        pad_h, pad_w;
                uint32_t        K_inner; /* C_in*kH*kW */
                uint32_t        w_stride;/* ceil(K_inner/64)*64 */
                uint32_t        K_out_pad;
                float           act_in_scale;
                float           act_out_scale;
            } """))
        f.write(f'{prefix}_layer_t;\n\n')
        f.write(f'#define {stem_uc}_N_LAYERS {full_n_layers}\n\n')
        f.write(f'extern const {prefix}_layer_t {prefix}_layers[{stem_uc}_N_LAYERS];\n\n')

        # Extern declarations for individual arrays
        for ld in layers:
            vn = ld.varname
            K_n  = ld.K_out * ld.w_stride
            K_pad = len(ld.bias_int32)
            f.write(f'extern const int8_t   {vn}_w[{K_n}];\n')
            f.write(f'extern const int32_t  {vn}_b[{K_pad}];\n')
            f.write(f'extern const float    {vn}_s[{K_pad}];\n')
            if ld.silu_lut is not None:
                f.write(f'extern const uint8_t  {vn}_lut[256];\n')
            f.write('\n')

        f.write(f'#endif /* {stem_uc}_WEIGHTS_H */\n')

    # Summary
    total_bytes = sum(
        ld.K_out * ld.w_stride +           # weights INT8
        len(ld.bias_int32) * 4 +           # bias INT32
        len(ld.combined_scale) * 4 +       # scale FP32
        (256 if ld.silu_lut is not None else 0)
        for ld in layers
    )
    print(f'Done. Total weight data: {total_bytes/1024:.1f} KB')
    print(f'  ({c_path})')
    print(f'  ({h_path})')

    # Print per-layer summary
    print('\nLayer summary:')
    print(f'{"name":45s}  {"K":>5} {"C":>5} {"kH":>3} {"s":>2} {"p":>2}  '
          f'{"wstride":>7}  {"ain":>10}  {"aout":>10}  {"silu":>5}')
    for ld in layers:
        print(f'{ld.name:45s}  {ld.K_out:5d} {ld.C_in:5d} {ld.kH:3d} '
              f'{ld.stride_h:2d} {ld.pad_h:2d}  '
              f'{ld.w_stride:7d}  '
              f'{ld.act_in_scale:10.4e}  {ld.act_out_scale:10.4e}  '
              f'{"yes" if ld.silu_lut is not None else "no":>5}')


if __name__ == '__main__':
    main()
