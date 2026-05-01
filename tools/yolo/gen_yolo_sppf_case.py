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


def make_input(n: int):
    out = [0] * n
    state = 0x6D2B79F5
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


def maxpool5x5_s1_p2_hwc(in_buf, h: int, w: int, c: int):
    out = [0] * (h * w * c)
    for oy in range(h):
        for ox in range(w):
            out_off = (oy * w + ox) * c
            for ch in range(c):
                m = -128
                for ky in range(-2, 3):
                    iy = oy + ky
                    if iy < 0 or iy >= h:
                        continue
                    for kx in range(-2, 3):
                        ix = ox + kx
                        if ix < 0 or ix >= w:
                            continue
                        v = in_buf[(iy * w + ix) * c + ch]
                        if v > m:
                            m = v
                out[out_off + ch] = m
    return out


def concat4_hwc(a, b, c, d, channels):
    nr_pix = len(a) // channels
    out = [0] * (nr_pix * channels * 4)
    for pix in range(nr_pix):
        src_off = pix * channels
        dst_off = pix * channels * 4
        out[dst_off:dst_off + channels] = a[src_off:src_off + channels]
        out[dst_off + channels:dst_off + 2 * channels] = b[src_off:src_off + channels]
        out[dst_off + 2 * channels:dst_off + 3 * channels] = c[src_off:src_off + channels]
        out[dst_off + 3 * channels:dst_off + 4 * channels] = d[src_off:src_off + channels]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--weights-header", required=True)
    ap.add_argument("--in-h", type=int, required=True)
    ap.add_argument("--in-w", type=int, required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    text, arrays = load_header(Path(args.weights_header))
    l31 = parse_layer(text, 31)
    l32 = parse_layer(text, 32)

    in_buf = make_input(args.in_h * args.in_w * int(l31["C_in"]))
    cv1_buf, cv1_h, cv1_w = qconv_hwc(l31, arrays, in_buf, args.in_h, args.in_w)
    p1_buf = maxpool5x5_s1_p2_hwc(cv1_buf, cv1_h, cv1_w, int(l31["K_out"]))
    p2_buf = maxpool5x5_s1_p2_hwc(p1_buf, cv1_h, cv1_w, int(l31["K_out"]))
    p3_buf = maxpool5x5_s1_p2_hwc(p2_buf, cv1_h, cv1_w, int(l31["K_out"]))
    cat_buf = concat4_hwc(p3_buf, p2_buf, p1_buf, cv1_buf, int(l31["K_out"]))
    out_buf, out_h, out_w = qconv_hwc(l32, arrays, cat_buf, cv1_h, cv1_w)

    tag = "L31_32"
    guard = f"YOLOV5N_{tag}_CASE0_H"

    with open(args.out, "w") as f:
        f.write("/* AUTO-GENERATED by tools/gen_yolo_sppf_case.py — DO NOT EDIT */\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_INPUT_H {args.in_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_INPUT_W {args.in_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_INPUT_C {int(l31['C_in'])}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CV1_C {int(l31['K_out'])}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CAT_C {int(l32['C_in'])}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_H {out_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_W {out_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_C {int(l32['K_out'])}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_INPUT_HASH UINT64_C(0x{fnv1a64(in_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CV1_HASH UINT64_C(0x{fnv1a64(cv1_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P1_HASH UINT64_C(0x{fnv1a64(p1_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P2_HASH UINT64_C(0x{fnv1a64(p2_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_HASH UINT64_C(0x{fnv1a64(p3_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CAT_HASH UINT64_C(0x{fnv1a64(cat_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_HASH UINT64_C(0x{fnv1a64(out_buf):016x})\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_INPUT_ELEMS {len(in_buf)}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_OUTPUT_ELEMS {len(out_buf)}u\n\n")

        f.write(
            f"static const int8_t yolov5n_{tag.lower()}_case0_in[{len(in_buf)}] "
            "__attribute__((aligned(64))) = {\n"
        )
        for i in range(0, len(in_buf), 16):
            row = ", ".join(str(v) for v in in_buf[i:i + 16])
            f.write(f"    {row},\n")
        f.write("};\n\n")

        f.write(
            f"static const int8_t yolov5n_{tag.lower()}_case0_out[{len(out_buf)}] "
            "__attribute__((aligned(64))) = {\n"
        )
        for i in range(0, len(out_buf), 16):
            row = ", ".join(str(v) for v in out_buf[i:i + 16])
            f.write(f"    {row},\n")
        f.write("};\n\n")

        f.write(f"#endif /* {guard} */\n")


if __name__ == "__main__":
    main()
