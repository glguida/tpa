#!/usr/bin/env python3
"""Generate deterministic non-model-derived YOLOv8n block-test fixtures."""

import argparse
import math
from pathlib import Path


C2F_H = 2
C2F_W = 3
C2F_IN_C = 6
C2F_HIDDEN_C = 4
C2F_BLOCKS = 2
C2F_CAT_C = (2 + C2F_BLOCKS) * C2F_HIDDEN_C
C2F_OUT_C = 5

DETECT_REG_MAX = 16
DETECT_DFL_CHANNELS = 4 * DETECT_REG_MAX
DETECT_CLASS_CHANNELS = 80
DETECT_RAW_CHANNELS = DETECT_DFL_CHANNELS + DETECT_CLASS_CHANNELS
DETECT_PUBLIC_CHANNELS = 4 + DETECT_CLASS_CHANNELS
DETECT_SCALES = [
    ("P3", 8, 2, 2),
    ("P4", 16, 1, 2),
    ("P5", 32, 1, 1),
]


def clamp_i8(v: int) -> int:
    return max(-128, min(127, int(v)))


def trunc_div(n: int, d: int) -> int:
    if n >= 0:
        return n // d
    return -((-n) // d)


def fnv1a64_step(h: int, byte: int) -> int:
    h ^= byte & 0xFF
    return (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF


def hash_i8(values: list[int]) -> int:
    h = 0xCBF29CE484222325
    for value in values:
        h = fnv1a64_step(h, value)
    return h


def hash_i32(values: list[int]) -> int:
    h = 0xCBF29CE484222325
    for value in values:
        v = value & 0xFFFFFFFF
        for shift in (0, 8, 16, 24):
            h = fnv1a64_step(h, (v >> shift) & 0xFF)
    return h


def make_c2f_input() -> list[int]:
    nr_pix = C2F_H * C2F_W
    return [((pix * 17 + c * 29 + 11) % 127) - 63 for pix in range(nr_pix) for c in range(C2F_IN_C)]


def c2f_linear(src: list[int], pix: int, in_c: int, out_c: int, seed: int) -> list[int]:
    out = []
    for oc in range(out_c):
        acc = seed * 19 + (pix + 1) * (oc + 3) - 37
        for ic in range(in_c):
            w = ((seed + 3 * (oc + 1) + 5 * (ic + 1)) % 9) - 4
            acc += src[pix * in_c + ic] * w
        out.append(clamp_i8(trunc_div(acc, 8)))
    return out


def c2f_bottleneck(src: list[int], seed: int) -> list[int]:
    nr_pix = C2F_H * C2F_W
    conv0 = []
    conv1 = []
    out = []
    for pix in range(nr_pix):
        conv0.extend(c2f_linear(src, pix, C2F_HIDDEN_C, C2F_HIDDEN_C, seed))
    for pix in range(nr_pix):
        conv1.extend(c2f_linear(conv0, pix, C2F_HIDDEN_C, C2F_HIDDEN_C, seed + 11))
    for i, value in enumerate(conv1):
        out.append(clamp_i8(value + src[i]))
    return out


def c2f_fixture() -> dict[str, list[int] | int]:
    nr_pix = C2F_H * C2F_W
    inp = make_c2f_input()
    cv1 = []
    for pix in range(nr_pix):
        cv1.extend(c2f_linear(inp, pix, C2F_IN_C, 2 * C2F_HIDDEN_C, 3))
    chunk0 = []
    chunk1 = []
    for pix in range(nr_pix):
        base = pix * 2 * C2F_HIDDEN_C
        chunk0.extend(cv1[base:base + C2F_HIDDEN_C])
        chunk1.extend(cv1[base + C2F_HIDDEN_C:base + 2 * C2F_HIDDEN_C])
    bottleneck0 = c2f_bottleneck(chunk1, 17)
    bottleneck1 = c2f_bottleneck(bottleneck0, 29)
    cat = []
    for pix in range(nr_pix):
        for part in (chunk0, chunk1, bottleneck0, bottleneck1):
            cat.extend(part[pix * C2F_HIDDEN_C:(pix + 1) * C2F_HIDDEN_C])
    out = []
    for pix in range(nr_pix):
        out.extend(c2f_linear(cat, pix, C2F_CAT_C, C2F_OUT_C, 43))
    return {
        "input": inp,
        "cv1": cv1,
        "chunk0": chunk0,
        "chunk1": chunk1,
        "bottleneck0": bottleneck0,
        "bottleneck1": bottleneck1,
        "cat": cat,
        "output": out,
    }


def exp_q12(logit: int) -> int:
    return max(1, int(round(math.exp(logit / 4.0) * 4096.0)))


def sigmoid_q15(logit: int) -> int:
    return int(round((1.0 / (1.0 + math.exp(-(logit / 4.0)))) * 32767.0))


def dfl_logits(point: int, coord: int, bin_idx: int) -> int:
    return ((point * 7 + coord * 11 + bin_idx * 5) % 9) - 4


def class_logit(point: int, cls: int) -> int:
    return ((point * 13 + cls * 3 + 5) % 17) - 8


def dfl_project_q8(raw: list[int], point: int, coord: int, exp_lut: list[int]) -> int:
    base = point * DETECT_RAW_CHANNELS + coord * DETECT_REG_MAX
    total = 0
    weighted = 0
    for bin_idx in range(DETECT_REG_MAX):
        weight = exp_lut[raw[base + bin_idx] + 8]
        total += weight
        weighted += bin_idx * weight
    return (weighted * 256 + total // 2) // total


def detect_points() -> list[tuple[int, int, int, int]]:
    points = []
    for scale_idx, (_name, stride, height, width) in enumerate(DETECT_SCALES):
        for y in range(height):
            for x in range(width):
                points.append((scale_idx, stride, y, x))
    return points


def detect_fixture() -> dict[str, list[int] | int]:
    points = detect_points()
    exp_lut = [exp_q12(i - 8) for i in range(17)]
    sigmoid_lut = [sigmoid_q15(i - 8) for i in range(17)]
    raw = []
    for point, _point_info in enumerate(points):
        for coord in range(4):
            for bin_idx in range(DETECT_REG_MAX):
                raw.append(dfl_logits(point, coord, bin_idx))
        for cls in range(DETECT_CLASS_CHANNELS):
            raw.append(class_logit(point, cls))
    box_q8 = []
    cls_q15 = []
    for point, (_scale_idx, stride, y, x) in enumerate(points):
        left = dfl_project_q8(raw, point, 0, exp_lut)
        top = dfl_project_q8(raw, point, 1, exp_lut)
        right = dfl_project_q8(raw, point, 2, exp_lut)
        bottom = dfl_project_q8(raw, point, 3, exp_lut)
        cx_grid_q8 = x * 256 + 128
        cy_grid_q8 = y * 256 + 128
        x1 = (cx_grid_q8 - left) * stride
        y1 = (cy_grid_q8 - top) * stride
        x2 = (cx_grid_q8 + right) * stride
        y2 = (cy_grid_q8 + bottom) * stride
        box_q8.extend([(x1 + x2) // 2, (y1 + y2) // 2, x2 - x1, y2 - y1])
        cls_base = point * DETECT_RAW_CHANNELS + DETECT_DFL_CHANNELS
        for cls in range(DETECT_CLASS_CHANNELS):
            cls_q15.append(sigmoid_lut[raw[cls_base + cls] + 8])
    public = []
    for channel in range(DETECT_PUBLIC_CHANNELS):
        for point in range(len(points)):
            if channel < 4:
                public.append(box_q8[point * 4 + channel])
            else:
                cls = channel - 4
                public.append(cls_q15[point * DETECT_CLASS_CHANNELS + cls])
    return {
        "exp_lut_q12": exp_lut,
        "sigmoid_lut_q15": sigmoid_lut,
        "raw": raw,
        "box_q8": box_q8,
        "cls_q15": cls_q15,
        "public": public,
        "points": [item for point in points for item in point],
    }


def emit_i8_array(name: str, values: list[int]) -> str:
    rows = [f"static const int8_t {name}[{len(values)}] __attribute__((aligned(64))) = {{"]
    for i in range(0, len(values), 16):
        rows.append("    " + ", ".join(str(v) for v in values[i:i + 16]) + ",")
    rows.append("};")
    return "\n".join(rows)


def emit_i32_array(name: str, values: list[int]) -> str:
    rows = [f"static const int32_t {name}[{len(values)}] __attribute__((aligned(64))) = {{"]
    for i in range(0, len(values), 8):
        rows.append("    " + ", ".join(str(v) for v in values[i:i + 8]) + ",")
    rows.append("};")
    return "\n".join(rows)


def emit_u16_array(name: str, values: list[int]) -> str:
    rows = [f"static const uint16_t {name}[{len(values)}] __attribute__((aligned(64))) = {{"]
    for i in range(0, len(values), 8):
        rows.append("    " + ", ".join(str(v) + "u" for v in values[i:i + 8]) + ",")
    rows.append("};")
    return "\n".join(rows)


def emit_u32_array(name: str, values: list[int]) -> str:
    rows = [f"static const uint32_t {name}[{len(values)}] __attribute__((aligned(64))) = {{"]
    for i in range(0, len(values), 8):
        rows.append("    " + ", ".join(str(v) + "u" for v in values[i:i + 8]) + ",")
    rows.append("};")
    return "\n".join(rows)


def write_c2f_header(out_dir: Path) -> None:
    fixture = c2f_fixture()
    path = out_dir / "yolov8n_c2f_block_case0.h"
    text = [
        "/* AUTO-GENERATED by tools/yolo/gen_yolov8n_synthetic_cases.py. */",
        "/* Deterministic synthetic fixture; no YOLOv8n model weights or activations. */",
        "#ifndef YOLOV8N_C2F_BLOCK_CASE0_H",
        "#define YOLOV8N_C2F_BLOCK_CASE0_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define YOLOV8N_C2F_CASE0_H {C2F_H}u",
        f"#define YOLOV8N_C2F_CASE0_W {C2F_W}u",
        f"#define YOLOV8N_C2F_CASE0_IN_C {C2F_IN_C}u",
        f"#define YOLOV8N_C2F_CASE0_HIDDEN_C {C2F_HIDDEN_C}u",
        f"#define YOLOV8N_C2F_CASE0_BLOCKS {C2F_BLOCKS}u",
        f"#define YOLOV8N_C2F_CASE0_CAT_C {C2F_CAT_C}u",
        f"#define YOLOV8N_C2F_CASE0_OUT_C {C2F_OUT_C}u",
        f"#define YOLOV8N_C2F_CASE0_NR_PIX {C2F_H * C2F_W}u",
        f"#define YOLOV8N_C2F_CASE0_INPUT_ELEMS {len(fixture['input'])}u",
        f"#define YOLOV8N_C2F_CASE0_CV1_ELEMS {len(fixture['cv1'])}u",
        f"#define YOLOV8N_C2F_CASE0_CHUNK_ELEMS {len(fixture['chunk0'])}u",
        f"#define YOLOV8N_C2F_CASE0_CAT_ELEMS {len(fixture['cat'])}u",
        f"#define YOLOV8N_C2F_CASE0_OUTPUT_ELEMS {len(fixture['output'])}u",
        f"#define YOLOV8N_C2F_CASE0_INPUT_HASH UINT64_C(0x{hash_i8(fixture['input']):016x})",
        f"#define YOLOV8N_C2F_CASE0_CHUNK0_HASH UINT64_C(0x{hash_i8(fixture['chunk0']):016x})",
        f"#define YOLOV8N_C2F_CASE0_CHUNK1_HASH UINT64_C(0x{hash_i8(fixture['chunk1']):016x})",
        f"#define YOLOV8N_C2F_CASE0_B0_HASH UINT64_C(0x{hash_i8(fixture['bottleneck0']):016x})",
        f"#define YOLOV8N_C2F_CASE0_B1_HASH UINT64_C(0x{hash_i8(fixture['bottleneck1']):016x})",
        f"#define YOLOV8N_C2F_CASE0_CAT_HASH UINT64_C(0x{hash_i8(fixture['cat']):016x})",
        f"#define YOLOV8N_C2F_CASE0_OUTPUT_HASH UINT64_C(0x{hash_i8(fixture['output']):016x})",
        "",
        emit_i8_array("yolov8n_c2f_case0_in", fixture["input"]),
        "",
        emit_i8_array("yolov8n_c2f_case0_out", fixture["output"]),
        "",
        "#endif /* YOLOV8N_C2F_BLOCK_CASE0_H */",
        "",
    ]
    path.write_text("\n".join(text))


def write_detect_header(out_dir: Path) -> None:
    fixture = detect_fixture()
    path = out_dir / "yolov8n_detect_dfl_case0.h"
    points = detect_points()
    text = [
        "/* AUTO-GENERATED by tools/yolo/gen_yolov8n_synthetic_cases.py. */",
        "/* Deterministic synthetic fixture; no YOLOv8n model weights or activations. */",
        "/* Public output is int32 channel-major [84][NR_POINTS]: box q8 then class q15. */",
        "#ifndef YOLOV8N_DETECT_DFL_CASE0_H",
        "#define YOLOV8N_DETECT_DFL_CASE0_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define YOLOV8N_DETECT_CASE0_REG_MAX {DETECT_REG_MAX}u",
        f"#define YOLOV8N_DETECT_CASE0_DFL_CHANNELS {DETECT_DFL_CHANNELS}u",
        f"#define YOLOV8N_DETECT_CASE0_CLASS_CHANNELS {DETECT_CLASS_CHANNELS}u",
        f"#define YOLOV8N_DETECT_CASE0_RAW_CHANNELS {DETECT_RAW_CHANNELS}u",
        f"#define YOLOV8N_DETECT_CASE0_PUBLIC_CHANNELS {DETECT_PUBLIC_CHANNELS}u",
        f"#define YOLOV8N_DETECT_CASE0_NR_SCALES {len(DETECT_SCALES)}u",
        f"#define YOLOV8N_DETECT_CASE0_NR_POINTS {len(points)}u",
        f"#define YOLOV8N_DETECT_CASE0_RAW_ELEMS {len(fixture['raw'])}u",
        f"#define YOLOV8N_DETECT_CASE0_BOX_ELEMS {len(fixture['box_q8'])}u",
        f"#define YOLOV8N_DETECT_CASE0_CLS_ELEMS {len(fixture['cls_q15'])}u",
        f"#define YOLOV8N_DETECT_CASE0_PUBLIC_ELEMS {len(fixture['public'])}u",
        f"#define YOLOV8N_DETECT_CASE0_RAW_HASH UINT64_C(0x{hash_i8(fixture['raw']):016x})",
        f"#define YOLOV8N_DETECT_CASE0_BOX_HASH UINT64_C(0x{hash_i32(fixture['box_q8']):016x})",
        f"#define YOLOV8N_DETECT_CASE0_CLS_HASH UINT64_C(0x{hash_i32(fixture['cls_q15']):016x})",
        f"#define YOLOV8N_DETECT_CASE0_PUBLIC_HASH UINT64_C(0x{hash_i32(fixture['public']):016x})",
        "",
        emit_i8_array("yolov8n_detect_case0_raw", fixture["raw"]),
        "",
        emit_i32_array("yolov8n_detect_case0_box_q8", fixture["box_q8"]),
        "",
        emit_u16_array("yolov8n_detect_case0_cls_q15", fixture["cls_q15"]),
        "",
        emit_i32_array("yolov8n_detect_case0_public", fixture["public"]),
        "",
        emit_u32_array("yolov8n_detect_case0_points", fixture["points"]),
        "",
        emit_u32_array("yolov8n_detect_case0_exp_lut_q12", fixture["exp_lut_q12"]),
        "",
        emit_u16_array("yolov8n_detect_case0_sigmoid_lut_q15", fixture["sigmoid_lut_q15"]),
        "",
        "#endif /* YOLOV8N_DETECT_DFL_CASE0_H */",
        "",
    ]
    path.write_text("\n".join(text))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate small synthetic YOLOv8n C2f and Detect/DFL block-test fixtures."
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("tests/yolo/generated"),
        help="Directory for generated fixture headers",
    )
    parser.add_argument(
        "--case",
        choices=("all", "c2f", "detect"),
        default="all",
        help="Fixture case to generate",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    if args.case in ("all", "c2f"):
        write_c2f_header(args.out_dir)
    if args.case in ("all", "detect"):
        write_detect_header(args.out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
