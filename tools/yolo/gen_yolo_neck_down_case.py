#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


def parse_array_values(ctype: str, body: str):
    vals = []
    for part in body.replace("\n", " ").split(","):
        part = part.strip()
        if not part:
            continue
        if ctype == "float":
            vals.append(float(part.rstrip("f")))
        else:
            vals.append(int(part, 10))
    return vals


def load_header(path: Path):
    text = path.read_text()
    arrays = {}
    for m in re.finditer(
        r"const\s+(int8_t|int32_t|float|uint8_t)\s+(\w+)\[\d+\][^{]*=\s*\{(.*?)\};",
        text,
        re.S,
    ):
        ctype, name, body = m.groups()
        arrays[name] = parse_array_values(ctype, body)
    return text, arrays


def parse_layer(text: str, layer_idx: int):
    m = re.search(rf"\[{layer_idx}\]\s*=\s*\{{(.*?)\n\s*\}},", text, re.S)
    if not m:
        raise SystemExit(f"layer {layer_idx} not found in header")
    return dict(re.findall(r"\.(\w+)\s*=\s*([^,]+),", m.group(1)))


def make_input(n: int, seed: int):
    out = [0] * n
    state = seed & 0xFFFFFFFF
    for i in range(n):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        v = (state >> 24) & 0xFF
        out[i] = v - 256 if v >= 128 else v
    return out


def round_away(x: float):
    return int(x + 0.5) if x >= 0.0 else int(x - 0.5)


def clamp_i8(x: int):
    if x < -128:
        return -128
    if x > 127:
        return 127
    return x


def parse_f32(s: str):
    return float(s.rstrip("f"))


def fnv1a64(buf):
    h = 0xCBF29CE484222325
    for v in buf:
        h ^= (v & 0xFF)
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def qconv_hwc(layer, arrays, in_buf, in_h: int, in_w: int):
    c_in = int(layer["C_in"])
    k_h = int(layer["kH"])
    k_w = int(layer["kW"])
    s_h = int(layer["stride_h"])
    s_w = int(layer["stride_w"])
    p_h = int(layer["pad_h"])
    p_w = int(layer["pad_w"])
    k_out = int(layer["K_out"])
    w_stride = int(layer["w_stride"])
    out_h = (in_h + (2 * p_h) - k_h) // s_h + 1
    out_w = (in_w + (2 * p_w) - k_w) // s_w + 1

    out_buf = [0] * (out_h * out_w * k_out)
    weights = arrays[layer["w"]]
    bias = arrays[layer["b"]]
    scales = arrays[layer["s"]]
    lut = arrays[layer["lut"]] if layer["lut"] != "((void*)0)" else None

    for oy in range(out_h):
        for ox in range(out_w):
            out_off = (oy * out_w + ox) * k_out
            for oc in range(k_out):
                acc = int(bias[oc])
                w_off = oc * w_stride
                k = 0
                for ky in range(k_h):
                    iy = oy * s_h + ky - p_h
                    for kx in range(k_w):
                        ix = ox * s_w + kx - p_w
                        if iy < 0 or iy >= in_h or ix < 0 or ix >= in_w:
                            k += c_in
                            continue
                        in_off = (iy * in_w + ix) * c_in
                        for ic in range(c_in):
                            acc += int(in_buf[in_off + ic]) * int(weights[w_off + k])
                            k += 1
                q = clamp_i8(round_away(acc * scales[oc]))
                if lut is not None:
                    q = int(lut[q & 0xFF])
                    q = q - 256 if q >= 128 else q
                out_buf[out_off + oc] = q

    return out_buf, out_h, out_w


def make_res_lut(s_in: float, s_out: float):
    scale = s_in / s_out
    lut = [0] * 256
    for i in range(256):
        v = i - 256 if i >= 128 else i
        lut[i] = clamp_i8(round_away(v * scale))
    return lut


def add_residual(skip_buf, body_buf, res_lut):
    out = [0] * len(body_buf)
    for i, y in enumerate(body_buf):
        out[i] = clamp_i8(int(y) + int(res_lut[skip_buf[i] & 0xFF]))
    return out


def concat_hwc(a_buf, a_c, b_buf, b_c):
    nr_pix = len(a_buf) // a_c
    out = [0] * (nr_pix * (a_c + b_c))
    for pix in range(nr_pix):
        ao = pix * a_c
        bo = pix * b_c
        oo = pix * (a_c + b_c)
        out[oo:oo + a_c] = a_buf[ao:ao + a_c]
        out[oo + a_c:oo + a_c + b_c] = b_buf[bo:bo + b_c]
    return out


def emit_i8_array(f, name, data):
    f.write(
        f"static const int8_t {name}[{len(data)}] __attribute__((aligned(64))) = {{\n"
    )
    for i in range(0, len(data), 16):
        row = ", ".join(str(v) for v in data[i:i + 16])
        f.write(f"    {row},\n")
    f.write("};\n\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--weights-header", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    text, arrays = load_header(Path(args.weights_header))
    l45 = parse_layer(text, 45)
    l46 = parse_layer(text, 46)
    l47 = parse_layer(text, 47)
    l48 = parse_layer(text, 48)
    l49 = parse_layer(text, 49)
    l50 = parse_layer(text, 50)

    p3_h = 80
    p3_w = 80
    p3_c = 64
    skip_h = 40
    skip_w = 40
    skip_c = 64

    p3_in = make_input(p3_h * p3_w * p3_c, 0x6D2B79F5)
    skip_in = make_input(skip_h * skip_w * skip_c, 0x31415926)

    down_buf, down_h, down_w = qconv_hwc(l45, arrays, p3_in, p3_h, p3_w)
    if down_h != skip_h or down_w != skip_w:
        raise SystemExit("downsample shape mismatch")
    cat0_buf = concat_hwc(down_buf, int(l45["K_out"]), skip_in, skip_c)

    a0_buf, a0_h, a0_w = qconv_hwc(l46, arrays, cat0_buf, skip_h, skip_w)
    mid_buf, mid_h, mid_w = qconv_hwc(l47, arrays, a0_buf, a0_h, a0_w)
    body_buf, body_h, body_w = qconv_hwc(l48, arrays, mid_buf, mid_h, mid_w)
    if body_h != a0_h or body_w != a0_w:
        raise SystemExit("neck_down bottleneck body changed spatial shape")
    res_lut = make_res_lut(parse_f32(l47["act_in_scale"]), parse_f32(l48["act_out_scale"]))
    a_buf = add_residual(a0_buf, body_buf, res_lut)
    b_buf, b_h, b_w = qconv_hwc(l49, arrays, cat0_buf, skip_h, skip_w)
    if b_h != a0_h or b_w != a0_w:
        raise SystemExit("neck_down branch mismatch")
    cat1_buf = concat_hwc(a_buf, int(l48["K_out"]), b_buf, int(l49["K_out"]))
    out_buf, out_h, out_w = qconv_hwc(l50, arrays, cat1_buf, a0_h, a0_w)

    tag = "L45_50"
    guard = f"YOLOV5N_{tag}_CASE0_H"

    with open(args.out, "w") as f:
        f.write("/* AUTO-GENERATED by tools/gen_yolo_neck_down_case.py — DO NOT EDIT */\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_H {p3_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_W {p3_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_C {p3_c}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_SKIP_H {skip_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_SKIP_W {skip_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_SKIP_C {skip_c}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_H {out_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_W {out_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_C {int(l50['K_out'])}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_HASH UINT64_C(0x{fnv1a64(p3_in):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_SKIP_HASH UINT64_C(0x{fnv1a64(skip_in):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_DOWN_HASH UINT64_C(0x{fnv1a64(down_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CAT0_HASH UINT64_C(0x{fnv1a64(cat0_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_A0_HASH UINT64_C(0x{fnv1a64(a0_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_MID_HASH UINT64_C(0x{fnv1a64(mid_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_BODY_HASH UINT64_C(0x{fnv1a64(body_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_A_HASH UINT64_C(0x{fnv1a64(a_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_B_HASH UINT64_C(0x{fnv1a64(b_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CAT1_HASH UINT64_C(0x{fnv1a64(cat1_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_HASH UINT64_C(0x{fnv1a64(out_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_ELEMS {len(p3_in)}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_SKIP_ELEMS {len(skip_in)}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_ELEMS {len(out_buf)}u\n\n")

        emit_i8_array(f, "yolov5n_l45_50_case0_p3", p3_in)
        emit_i8_array(f, "yolov5n_l45_50_case0_skip", skip_in)
        emit_i8_array(f, "yolov5n_l45_50_res_lut", res_lut)
        emit_i8_array(f, "yolov5n_l45_50_case0_out", out_buf)
        f.write(f"#endif /* {guard} */\n")


if __name__ == "__main__":
    main()
