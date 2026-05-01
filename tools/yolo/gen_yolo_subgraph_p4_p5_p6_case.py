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


def upsample2x_nearest_hwc(src_buf, in_h, in_w, c):
    out_h = in_h * 2
    out_w = in_w * 2
    out = [0] * (out_h * out_w * c)
    for iy in range(in_h):
        for ix in range(in_w):
            src_off = (iy * in_w + ix) * c
            pix = src_buf[src_off:src_off + c]
            for oy in range(2):
                for ox in range(2):
                    dy = iy * 2 + oy
                    dx = ix * 2 + ox
                    dst_off = (dy * out_w + dx) * c
                    out[dst_off:dst_off + c] = pix
    return out, out_h, out_w


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
    ap.add_argument("--p4-weights", required=True)
    ap.add_argument("--p5-weights", required=True)
    ap.add_argument("--p6-weights", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    p4_text, p4_arrays = load_header(Path(args.p4_weights))
    p5_text, p5_arrays = load_header(Path(args.p5_weights))
    p6_text, p6_arrays = load_header(Path(args.p6_weights))

    l34 = parse_layer(p4_text, 34)
    l35 = parse_layer(p4_text, 35)
    l36 = parse_layer(p4_text, 36)
    l37 = parse_layer(p4_text, 37)
    l38 = parse_layer(p4_text, 38)
    l39 = parse_layer(p4_text, 39)

    l40 = parse_layer(p5_text, 40)
    l41 = parse_layer(p5_text, 41)
    l42 = parse_layer(p5_text, 42)
    l43 = parse_layer(p5_text, 43)
    l44 = parse_layer(p5_text, 44)

    l57 = parse_layer(p6_text, 57)
    l58 = parse_layer(p6_text, 58)
    l69 = parse_layer(p6_text, 69)
    l63 = parse_layer(p6_text, 63)
    l64 = parse_layer(p6_text, 64)
    l72 = parse_layer(p6_text, 72)

    p5_in_h = 20
    p5_in_w = 20
    p5_in_c = 128
    p4_skip_h = 40
    p4_skip_w = 40
    p4_skip_c = 128
    p3_skip_h = 80
    p3_skip_w = 80
    p3_skip_c = 64

    p5_in = make_input(p5_in_h * p5_in_w * p5_in_c, 0x6D2B79F5)
    p4_skip = make_input(p4_skip_h * p4_skip_w * p4_skip_c, 0x31415926)
    p3_skip = make_input(p3_skip_h * p3_skip_w * p3_skip_c, 0xC001D00D)

    p4_up, p4_up_h, p4_up_w = upsample2x_nearest_hwc(p5_in, p5_in_h, p5_in_w, p5_in_c)
    if p4_up_h != p4_skip_h or p4_up_w != p4_skip_w:
        raise SystemExit("p4 upsample shape mismatch")
    p4_cat0 = concat_hwc(p4_up, p5_in_c, p4_skip, p4_skip_c)
    p4_a0, p4_a0_h, p4_a0_w = qconv_hwc(l34, p4_arrays, p4_cat0, p4_skip_h, p4_skip_w)
    p4_mid, p4_mid_h, p4_mid_w = qconv_hwc(l35, p4_arrays, p4_a0, p4_a0_h, p4_a0_w)
    p4_body, p4_body_h, p4_body_w = qconv_hwc(l36, p4_arrays, p4_mid, p4_mid_h, p4_mid_w)
    p4_res_lut = make_res_lut(parse_f32(l35["act_in_scale"]),
                              parse_f32(l36["act_out_scale"]))
    p4_a = add_residual(p4_a0, p4_body, p4_res_lut)
    p4_b, p4_b_h, p4_b_w = qconv_hwc(l37, p4_arrays, p4_cat0, p4_skip_h, p4_skip_w)
    p4_cat1 = concat_hwc(p4_a, int(l36["K_out"]), p4_b, int(l37["K_out"]))
    p4_pre, p4_pre_h, p4_pre_w = qconv_hwc(l38, p4_arrays, p4_cat1, p4_a0_h, p4_a0_w)
    p4_out, p4_out_h, p4_out_w = qconv_hwc(l39, p4_arrays, p4_pre, p4_pre_h, p4_pre_w)

    p5_up, p5_up_h, p5_up_w = upsample2x_nearest_hwc(p4_out, p4_out_h, p4_out_w,
                                                     int(l39["K_out"]))
    if p5_up_h != p3_skip_h or p5_up_w != p3_skip_w:
        raise SystemExit("p5 upsample shape mismatch")
    p5_cat0 = concat_hwc(p5_up, int(l39["K_out"]), p3_skip, p3_skip_c)
    p5_a0, p5_a0_h, p5_a0_w = qconv_hwc(l40, p5_arrays, p5_cat0, p3_skip_h, p3_skip_w)
    p5_mid, p5_mid_h, p5_mid_w = qconv_hwc(l41, p5_arrays, p5_a0, p5_a0_h, p5_a0_w)
    p5_body, p5_body_h, p5_body_w = qconv_hwc(l42, p5_arrays, p5_mid, p5_mid_h, p5_mid_w)
    p5_res_lut = make_res_lut(parse_f32(l41["act_in_scale"]),
                              parse_f32(l42["act_out_scale"]))
    p5_a = add_residual(p5_a0, p5_body, p5_res_lut)
    p5_b, p5_b_h, p5_b_w = qconv_hwc(l43, p5_arrays, p5_cat0, p3_skip_h, p3_skip_w)
    p5_cat1 = concat_hwc(p5_a, int(l42["K_out"]), p5_b, int(l43["K_out"]))
    p5_out, p5_out_h, p5_out_w = qconv_hwc(l44, p5_arrays, p5_cat1, p5_a0_h, p5_a0_w)

    det_box1, h1, w1 = qconv_hwc(l57, p6_arrays, p5_out, p5_out_h, p5_out_w)
    det_box2, h2, w2 = qconv_hwc(l58, p6_arrays, det_box1, h1, w1)
    det_box, hb, wb = qconv_hwc(l69, p6_arrays, det_box2, h2, w2)
    det_cls1, hc1, wc1 = qconv_hwc(l63, p6_arrays, p5_out, p5_out_h, p5_out_w)
    det_cls2, hc2, wc2 = qconv_hwc(l64, p6_arrays, det_cls1, hc1, wc1)
    det_cls, ho, wo = qconv_hwc(l72, p6_arrays, det_cls2, hc2, wc2)
    if (hb, wb) != (ho, wo):
        raise SystemExit("detect output spatial mismatch")

    tag = "P4_P5_P6"
    guard = f"YOLOV5N_{tag}_CASE0_H"

    with open(args.out, "w") as f:
        f.write("/* AUTO-GENERATED by tools/gen_yolo_subgraph_p4_p5_p6_case.py — DO NOT EDIT */\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")

        f.write(f"#define YOLOV5N_{tag}_CASE0_P5_IN_H {p5_in_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P5_IN_W {p5_in_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P5_IN_C {p5_in_c}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P4_SKIP_H {p4_skip_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P4_SKIP_W {p4_skip_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P4_SKIP_C {p4_skip_c}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_SKIP_H {p3_skip_h}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_SKIP_W {p3_skip_w}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_SKIP_C {p3_skip_c}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_MID_P4_ELEMS {len(p4_out)}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_P3_ELEMS {len(p5_out)}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_BOX_ELEMS {len(det_box)}u\n")
        f.write(f"#define YOLOV5N_{tag}_CASE0_CLS_ELEMS {len(det_cls)}u\n")

        hashes = {
            "P5_IN": p5_in,
            "P4_SKIP": p4_skip,
            "P3_SKIP": p3_skip,
            "P4_UP": p4_up,
            "P4_CAT0": p4_cat0,
            "P4_A0": p4_a0,
            "P4_MID": p4_mid,
            "P4_BODY": p4_body,
            "P4_A": p4_a,
            "P4_B": p4_b,
            "P4_CAT1": p4_cat1,
            "P4_PRE": p4_pre,
            "P4_OUT": p4_out,
            "P5_UP": p5_up,
            "P5_CAT0": p5_cat0,
            "P5_A0": p5_a0,
            "P5_MID": p5_mid,
            "P5_BODY": p5_body,
            "P5_A": p5_a,
            "P5_B": p5_b,
            "P5_CAT1": p5_cat1,
            "P5_OUT": p5_out,
            "DET_BOX1": det_box1,
            "DET_BOX2": det_box2,
            "DET_BOX": det_box,
            "DET_CLS1": det_cls1,
            "DET_CLS2": det_cls2,
            "DET_CLS": det_cls,
        }
        for name, buf in hashes.items():
            f.write(
                f"#define YOLOV5N_{tag}_CASE0_{name}_HASH UINT64_C(0x{fnv1a64(buf):016x})\n"
            )
        f.write("\n")

        emit_i8_array(f, "yolov5n_p4_p5_p6_case0_p5_in", p5_in)
        emit_i8_array(f, "yolov5n_p4_p5_p6_case0_p4_skip", p4_skip)
        emit_i8_array(f, "yolov5n_p4_p5_p6_case0_p3_skip", p3_skip)
        emit_i8_array(f, "yolov5n_p4_p5_p6_p4_res_lut", p4_res_lut)
        emit_i8_array(f, "yolov5n_p4_p5_p6_p5_res_lut", p5_res_lut)
        emit_i8_array(f, "yolov5n_p4_p5_p6_case0_box", det_box)
        emit_i8_array(f, "yolov5n_p4_p5_p6_case0_cls", det_cls)
        f.write(f"#endif /* {guard} */\n")


if __name__ == "__main__":
    main()
