#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys
from pathlib import Path


DET_RE = re.compile(
    r"^\s*(\d+):\s+cls=(\d+)\s+score=([0-9eE+\-.]+)\s+box=\[([^\]]+)\]\s+stride=(\d+)\s*$"
)


def parse_ppm_p6(path: Path):
    data = path.read_bytes()
    n = len(data)
    i = 0

    def skip_ws_and_comments(idx: int) -> int:
        while idx < n:
            b = data[idx]
            if b in b" \t\r\n":
                idx += 1
                continue
            if b == ord("#"):
                while idx < n and data[idx] != ord("\n"):
                    idx += 1
                continue
            break
        return idx

    def read_token(idx: int):
        idx = skip_ws_and_comments(idx)
        start = idx
        while idx < n and data[idx] not in b" \t\r\n#":
            idx += 1
        if start == idx:
            raise ValueError("malformed PPM header")
        return data[start:idx].decode("ascii"), idx

    magic, i = read_token(i)
    if magic != "P6":
        raise ValueError("only P6 PPM is supported")
    width_s, i = read_token(i)
    height_s, i = read_token(i)
    maxval_s, i = read_token(i)
    i = skip_ws_and_comments(i)

    width = int(width_s)
    height = int(height_s)
    maxval = int(maxval_s)
    if maxval != 255:
        raise ValueError("only 8-bit PPM is supported")

    raster = data[i:]
    expected = width * height * 3
    if len(raster) != expected:
        raise ValueError(
            f"PPM raster size mismatch: got {len(raster)}, expected {expected}"
        )

    return width, height, bytearray(raster)


def write_ppm_p6(path: Path, width: int, height: int, raster: bytearray):
    header = f"P6\n{width} {height}\n255\n".encode("ascii")
    path.write_bytes(header + bytes(raster))


def clamp(v: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, v))


def draw_box(raster: bytearray, width: int, height: int, x1: float, y1: float, x2: float, y2: float,
             color=(160, 160, 160), thickness: int = 2):
    ix1 = clamp(int(round(x1)), 0, width - 1)
    iy1 = clamp(int(round(y1)), 0, height - 1)
    ix2 = clamp(int(round(x2)), 0, width - 1)
    iy2 = clamp(int(round(y2)), 0, height - 1)
    if ix2 < ix1:
        ix1, ix2 = ix2, ix1
    if iy2 < iy1:
        iy1, iy2 = iy2, iy1

    def set_px(x: int, y: int):
        off = (y * width + x) * 3
        raster[off + 0] = color[0]
        raster[off + 1] = color[1]
        raster[off + 2] = color[2]

    for t in range(thickness):
        yt = iy1 + t
        yb = iy2 - t
        xl = ix1 + t
        xr = ix2 - t
        if yt > yb or xl > xr:
            break
        for x in range(xl, xr + 1):
            set_px(x, yt)
            set_px(x, yb)
        for y in range(yt, yb + 1):
            set_px(xl, y)
            set_px(xr, y)


def parse_detections(text: str):
    dets = []
    for line in text.splitlines():
        m = DET_RE.match(line)
        if not m:
            continue
        _, cls_s, score_s, box_s, stride_s = m.groups()
        parts = [p.strip() for p in box_s.split(",")]
        if len(parts) != 4:
            continue
        x1, y1, x2, y2 = [float(p) for p in parts]
        dets.append(
            {
                "cls": int(cls_s),
                "score": float(score_s),
                "x1": x1,
                "y1": y1,
                "x2": x2,
                "y2": y2,
                "stride": int(stride_s),
            }
        )
    return dets


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True, help="Input P6 PPM image used for yolo_demo")
    ap.add_argument("--output", required=True, help="Output P6 PPM with boxes drawn")
    ap.add_argument(
        "--demo-bin",
        default="/home/glguida/think/etsw/tpa/build-cmake/et-device-release/yolov5n/yolo_demo",
        help="Path to yolo_demo binary",
    )
    ap.add_argument("--conf", default="0.25")
    ap.add_argument("--iou", default="0.45")
    ap.add_argument("--topk", default="20")
    ap.add_argument("--thickness", type=int, default=2)
    args = ap.parse_args()

    image = Path(args.image)
    output = Path(args.output)
    demo_bin = Path(args.demo_bin)

    width, height, raster = parse_ppm_p6(image)
    if width != 640 or height != 640:
        raise SystemExit(f"expected 640x640 PPM, got {width}x{height}")

    cmd = [
        str(demo_bin),
        "--image",
        str(image),
        "--conf",
        str(args.conf),
        "--iou",
        str(args.iou),
        "--topk",
        str(args.topk),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    combined = proc.stdout
    if proc.stderr:
        combined += ("\n" if combined else "") + proc.stderr

    sys.stdout.write(combined)
    if combined and not combined.endswith("\n"):
        sys.stdout.write("\n")

    dets = parse_detections(combined)
    if proc.returncode != 0 and not dets:
        raise SystemExit(proc.returncode)

    for det in dets:
        draw_box(
            raster,
            width,
            height,
            det["x1"],
            det["y1"],
            det["x2"],
            det["y2"],
            thickness=args.thickness,
        )

    write_ppm_p6(output, width, height, raster)
    print(f"wrote {output} with {len(dets)} boxes")


if __name__ == "__main__":
    main()
