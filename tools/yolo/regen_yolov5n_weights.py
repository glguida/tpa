#!/usr/bin/env python3

import argparse
import subprocess
import sys
from pathlib import Path


BUNDLES = {
    "l0_6": [0, 1, 2, 3, 4, 5, 6],
    "l7_14": [7, 8, 9, 10, 11, 12, 13, 14],
    "l15_24": [15, 16, 17, 18, 19, 20, 21, 22, 23, 24],
    "l25_33": [25, 26, 27, 28, 29, 30, 31, 32, 33],
    "l34_39": [34, 35, 36, 37, 38, 39],
    "l40_44": [40, 41, 42, 43, 44],
    "l45_50": [45, 46, 47, 48, 49, 50],
    "l51_56": [51, 52, 53, 54, 55, 56],
    "l57_58_69_63_64_72": [57, 58, 69, 63, 64, 72],
    "l59_60_70_65_66_73": [59, 60, 70, 65, 66, 73],
    "l61_62_71_67_68_74": [61, 62, 71, 67, 68, 74],
}


def parse_args():
    ap = argparse.ArgumentParser(description="Regenerate YOLOv5n quantized weight headers")
    ap.add_argument("--python-exe", default=sys.executable,
                    help="Python interpreter to run the PTQ tools")
    ap.add_argument("--model", default="models/yolov5nu.pt",
                    help="Path to the YOLOv5n .pt model")
    ap.add_argument("--calib-dir", default=None,
                    help="Directory of real calibration images")
    ap.add_argument("--n-calib", type=int, default=32,
                    help="Number of calibration images")
    ap.add_argument("--out-dir", default="tests/yolo/generated",
                    help="Output directory for generated headers")
    ap.add_argument("--bundles", default="all",
                    help="Comma-separated bundle names, or 'all'")
    return ap.parse_args()


def main():
    args = parse_args()
    root = Path(__file__).resolve().parent.parent
    tools = root / "tools"
    out_dir = (root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    layer_map = tools / "yolov5n_legacy_layer_map.json"
    ptq = tools / "ptq_yolov5.py"
    tensor = tools / "gen_yolo_tensor_weights.py"

    if args.bundles == "all":
        selected = list(BUNDLES.items())
    else:
        names = [name.strip() for name in args.bundles.split(",") if name.strip()]
        bad = [name for name in names if name not in BUNDLES]
        if bad:
            raise SystemExit(f"Unknown bundles: {', '.join(bad)}")
        selected = [(name, BUNDLES[name]) for name in names]

    for name, layers in selected:
        stem = f"yolov5n_{name}"
        header = out_dir / f"{stem}_weights.h"
        layer_csv = ",".join(str(x) for x in layers)
        cmd = [
            args.python_exe,
            str(ptq),
            "--model", str((root / args.model).resolve()),
            "--stem", stem,
            "--n-calib", str(args.n_calib),
            "--layer-map", str(layer_map),
            "--layers", layer_csv,
            "--inline-header", str(header),
        ]
        if args.calib_dir:
            cmd.extend(["--calib-dir", str(Path(args.calib_dir).resolve())])
        subprocess.run(cmd, check=True, cwd=root)

        for layer_idx in layers:
            subprocess.run(
                [
                    args.python_exe,
                    str(tensor),
                    "--weights-header", str(header),
                    "--layer-index", str(layer_idx),
                    "--out", str(out_dir / f"yolov5n_l{layer_idx}_tensor_weights.h"),
                ],
                check=True,
                cwd=root,
            )


if __name__ == "__main__":
    main()
