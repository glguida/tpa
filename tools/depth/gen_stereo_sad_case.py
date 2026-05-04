#!/usr/bin/env python3
"""Generate deterministic synthetic stereo SAD reference cases.

The generator uses only Python standard-library modules. It does not download or
copy images, datasets, model weights, or third-party stereo code. All pixels are
produced from deterministic integer formulas so the generated headers can be
regenerated in a clean checkout and checked by SHA-256.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Sequence

GENERATOR_VERSION = "stereo-sad-case-v0"
INVALID_DISPARITY = 255


@dataclass(frozen=True)
class CaseSpec:
    name: str
    symbol: str
    width: int
    height: int
    dmax: int
    radius: int
    seed: int
    formula: str
    stripe_y0: int | None = None
    stripe_h: int | None = None
    description: str = ""


PRESETS: dict[str, CaseSpec] = {
    "sad-cost-5x5": CaseSpec(
        "sad-cost-5x5", "STEREO_SAD_COST_5X5", 9, 9, 5, 2, 0xC0575AD,
        "constant:3", description="Small 5x5 SAD window-cost reference case."),
    "sad-argmin-16x8": CaseSpec(
        "sad-argmin-16x8", "STEREO_SAD_ARGMIN_16X8", 16, 8, 8, 2, 0xA961501,
        "constant:3", description="Small whole-image argmin reference case for block tests."),
    "sad-demo-96x64-stripe0": CaseSpec(
        "sad-demo-96x64-stripe0", "STEREO_SAD_DEMO_96X64_STRIPE0", 96, 64, 32, 2,
        0x5AD96320, "bands:6,12,18", stripe_y0=0, stripe_h=16,
        description="First 16-row stripe for the approved 96x64/D32 demo."),
}


class GenerationError(RuntimeError):
    pass


def mix32(value: int) -> int:
    """A small deterministic 32-bit integer mixer."""

    value &= 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    value = (value * 0x846CA68B) & 0xFFFFFFFF
    value ^= value >> 16
    return value & 0xFFFFFFFF


def texture_value(x: int, y: int, seed: int) -> int:
    """Non-periodic synthetic grayscale texture used for all generated cases."""

    v = seed ^ ((x + 0x9E3779B9) * 0x85EBCA6B) ^ ((y + 0xC2B2AE35) * 0x27D4EB2F)
    return mix32(v) & 0xFF


def parse_formula(formula: str) -> Callable[[int, int, int, int], int]:
    if formula.startswith("constant:"):
        d = int(formula.split(":", 1)[1], 0)
        return lambda _x, _y, _w, _h: d

    if formula.startswith("bands:"):
        disparities = [int(part, 0) for part in formula.split(":", 1)[1].split(",")]
        if not disparities:
            raise GenerationError("bands formula requires at least one disparity")

        def band_disparity(_x: int, y: int, _w: int, h: int) -> int:
            idx = min((y * len(disparities)) // h, len(disparities) - 1)
            return disparities[idx]

        return band_disparity

    raise GenerationError(f"unsupported disparity formula: {formula}")


def band_boundaries(height: int, formula: str) -> list[int]:
    if not formula.startswith("bands:"):
        return []
    count = len(formula.split(":", 1)[1].split(","))
    return [(height * idx) // count for idx in range(1, count)]


def make_left_image(spec: CaseSpec) -> list[int]:
    return [
        texture_value(x, y, spec.seed)
        for y in range(spec.height)
        for x in range(spec.width)
    ]


def make_right_image(spec: CaseSpec, disparity_at: Callable[[int, int, int, int], int]) -> list[int]:
    """Generate the right image by sampling the left texture shifted by disparity.

    The selected formulas are row-only, so every right-image pixel at (x, y)
    corresponds to left-image texture at (x + d(y), y) when that coordinate is
    in bounds. Out-of-bounds pixels are filled with a different deterministic
    texture; expected output masks keep them invalid.
    """

    right: list[int] = []
    for y in range(spec.height):
        d = disparity_at(0, y, spec.width, spec.height)
        for x in range(spec.width):
            src_x = x + d
            if src_x < spec.width:
                right.append(texture_value(src_x, y, spec.seed))
            else:
                right.append(texture_value(x + 7919, y + 1543, spec.seed ^ 0xBAD5EED))
    return right


def pixel_valid(spec: CaseSpec, x: int, y: int) -> bool:
    r = spec.radius
    if x < spec.dmax - 1 + r:
        return False
    if x >= spec.width - r:
        return False
    if y < r or y >= spec.height - r:
        return False
    for boundary in band_boundaries(spec.height, spec.formula):
        if abs(y - boundary) <= r:
            return False
    return True


def sad_cost(left: Sequence[int], right: Sequence[int], width: int, x: int, y: int, d: int, radius: int) -> int:
    total = 0
    for dy in range(-radius, radius + 1):
        row = (y + dy) * width
        for dx in range(-radius, radius + 1):
            lx = x + dx
            rx = x + dx - d
            total += abs(left[row + lx] - right[row + rx])
    return total


def argmin_disparity_map(spec: CaseSpec, left: Sequence[int], right: Sequence[int]) -> tuple[list[int], list[int]]:
    expected: list[int] = []
    best_costs: list[int] = []
    disparity_at = parse_formula(spec.formula)

    for y in range(spec.height):
        for x in range(spec.width):
            if not pixel_valid(spec, x, y):
                expected.append(INVALID_DISPARITY)
                best_costs.append(0xFFFF)
                continue

            best_d = 0
            best_cost = 0x7FFFFFFF
            for d in range(spec.dmax):
                c = sad_cost(left, right, spec.width, x, y, d, spec.radius)
                if c < best_cost:
                    best_cost = c
                    best_d = d
            expected_d = disparity_at(x, y, spec.width, spec.height)
            if best_d != expected_d:
                raise GenerationError(
                    f"{spec.name}: ambiguous synthetic texture at ({x},{y}): "
                    f"argmin={best_d}, formula={expected_d}, cost={best_cost}"
                )
            expected.append(best_d)
            best_costs.append(best_cost)

    return expected, best_costs


def stripe_bounds(spec: CaseSpec) -> tuple[int, int]:
    if spec.stripe_y0 is None or spec.stripe_h is None:
        return 0, spec.height
    if spec.stripe_y0 < 0 or spec.stripe_h <= 0 or spec.stripe_y0 + spec.stripe_h > spec.height:
        raise GenerationError(f"invalid stripe bounds for {spec.name}")
    return spec.stripe_y0, spec.stripe_h


def image_rows(image: Sequence[int], width: int, y0: int, rows: int) -> list[int]:
    out: list[int] = []
    for y in range(y0, y0 + rows):
        out.extend(image[y * width:(y + 1) * width])
    return out


def stripe_with_halo(spec: CaseSpec, image: Sequence[int]) -> tuple[list[int], int, int]:
    y0, stripe_h = stripe_bounds(spec)
    halo_y0 = max(0, y0 - spec.radius)
    halo_y1 = min(spec.height, y0 + stripe_h + spec.radius)
    return image_rows(image, spec.width, halo_y0, halo_y1 - halo_y0), halo_y0, halo_y1 - halo_y0


def bytes_sha256(values: Iterable[int], width: int = 1) -> str:
    # width is the number of bytes used per value in little-endian form.
    h = hashlib.sha256()
    for value in values:
        h.update(int(value).to_bytes(width, "little", signed=False))
    return h.hexdigest()


def c_array(name: str, c_type: str, values: Sequence[int], per_line: int = 12) -> str:
    lines = [f"static const {c_type} {name}[] = {{"]
    for i in range(0, len(values), per_line):
        chunk = values[i:i + per_line]
        suffix = "," if i + per_line < len(values) else ""
        lines.append("    " + ", ".join(str(v) for v in chunk) + suffix)
    lines.append("};")
    return "\n".join(lines)


def header_guard(symbol: str) -> str:
    return symbol.upper() + "_H"


def make_header(spec: CaseSpec, command: str) -> tuple[str, dict[str, object]]:
    left = make_left_image(spec)
    disparity_at = parse_formula(spec.formula)
    right = make_right_image(spec, disparity_at)
    expected, best_costs = argmin_disparity_map(spec, left, right)
    y0, stripe_h = stripe_bounds(spec)

    if spec.stripe_y0 is not None:
        left_payload, halo_y0, halo_rows = stripe_with_halo(spec, left)
        right_payload, _, _ = stripe_with_halo(spec, right)
        expected_payload = image_rows(expected, spec.width, y0, stripe_h)
        best_cost_payload = image_rows(best_costs, spec.width, y0, stripe_h)
    else:
        halo_y0 = 0
        halo_rows = spec.height
        left_payload = left
        right_payload = right
        expected_payload = expected
        best_cost_payload = best_costs

    valid_count = sum(1 for v in expected_payload if v != INVALID_DISPARITY)

    probe_x = probe_y = probe_d = probe_true_cost = probe_alt_d = probe_alt_cost = None
    for yy in range(y0, y0 + stripe_h):
        for xx in range(spec.width):
            disp = expected[yy * spec.width + xx]
            if disp == INVALID_DISPARITY:
                continue
            probe_x = xx
            probe_y = yy
            probe_d = disp
            probe_true_cost = sad_cost(left, right, spec.width, xx, yy, disp, spec.radius)
            probe_alt_d = 0 if disp != 0 else 1
            probe_alt_cost = sad_cost(left, right, spec.width, xx, yy, probe_alt_d, spec.radius)
            break
        if probe_x is not None:
            break
    if probe_x is None:
        raise GenerationError(f"{spec.name}: no valid probe pixel")

    demo_stripes: list[dict[str, object]] = []
    if spec.stripe_y0 is not None and spec.stripe_h is not None:
        for sy0 in range(0, spec.height, spec.stripe_h):
            sh = min(spec.stripe_h, spec.height - sy0)
            shy0 = max(0, sy0 - spec.radius)
            shy1 = min(spec.height, sy0 + sh + spec.radius)
            left_s = image_rows(left, spec.width, shy0, shy1 - shy0)
            right_s = image_rows(right, spec.width, shy0, shy1 - shy0)
            exp_s = image_rows(expected, spec.width, sy0, sh)
            cost_s = image_rows(best_costs, spec.width, sy0, sh)
            demo_stripes.append({
                "stripe_y0": sy0,
                "stripe_h": sh,
                "halo_y0": shy0,
                "halo_rows": shy1 - shy0,
                "left_payload_len": len(left_s),
                "right_payload_len": len(right_s),
                "expected_len": len(exp_s),
                "valid_output_pixels": sum(1 for v in exp_s if v != INVALID_DISPARITY),
                "left_payload_sha256": bytes_sha256(left_s),
                "right_payload_sha256": bytes_sha256(right_s),
                "expected_payload_sha256": bytes_sha256(exp_s),
                "best_cost_payload_sha256": bytes_sha256(cost_s, width=2),
            })

    case_meta = {
        "name": spec.name,
        "description": spec.description,
        "symbol": spec.symbol,
        "width": spec.width,
        "height": spec.height,
        "dmax": spec.dmax,
        "radius": spec.radius,
        "seed": spec.seed,
        "formula": spec.formula,
        "invalid_disparity": INVALID_DISPARITY,
        "stripe_y0": spec.stripe_y0,
        "stripe_h": spec.stripe_h,
        "halo_y0": halo_y0,
        "halo_rows": halo_rows,
        "valid_output_pixels": valid_count,
        "probe_x": probe_x,
        "probe_y": probe_y,
        "probe_disparity": probe_d,
        "probe_true_cost": probe_true_cost,
        "probe_alt_disparity": probe_alt_d,
        "probe_alt_cost": probe_alt_cost,
        "demo_stripes": demo_stripes,
        "left_full_sha256": bytes_sha256(left),
        "right_full_sha256": bytes_sha256(right),
        "expected_full_sha256": bytes_sha256(expected),
        "left_payload_sha256": bytes_sha256(left_payload),
        "right_payload_sha256": bytes_sha256(right_payload),
        "expected_payload_sha256": bytes_sha256(expected_payload),
        "best_cost_payload_sha256": bytes_sha256(best_cost_payload, width=2),
        "command": command,
        "generator_version": GENERATOR_VERSION,
    }

    guard = header_guard(spec.symbol)
    prefix = spec.symbol.lower()
    lines = [
        "/*",
        " * Generated by tools/depth/gen_stereo_sad_case.py.",
        " * Do not hand-edit.",
        f" * Generator version: {GENERATOR_VERSION}",
        f" * Command: {command}",
        f" * Case: {spec.name}",
        f" * Dimensions: {spec.width}x{spec.height}, dmax={spec.dmax}, radius={spec.radius}",
        f" * Seed: 0x{spec.seed:08x}",
        f" * Disparity formula: {spec.formula}",
        f" * Invalid disparity value: {INVALID_DISPARITY}",
        " * File SHA-256 values are recorded in stereo_sad_cases_manifest.json.",
        " */",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <stdint.h>",
        "",
        f"#define {spec.symbol}_WIDTH {spec.width}u",
        f"#define {spec.symbol}_HEIGHT {spec.height}u",
        f"#define {spec.symbol}_DMAX {spec.dmax}u",
        f"#define {spec.symbol}_RADIUS {spec.radius}u",
        f"#define {spec.symbol}_SEED 0x{spec.seed:08x}u",
        f"#define {spec.symbol}_INVALID {INVALID_DISPARITY}u",
        f"#define {spec.symbol}_STRIPE_Y0 {y0}u",
        f"#define {spec.symbol}_STRIPE_H {stripe_h}u",
        f"#define {spec.symbol}_HALO_Y0 {halo_y0}u",
        f"#define {spec.symbol}_HALO_ROWS {halo_rows}u",
        f"#define {spec.symbol}_LEFT_PAYLOAD_LEN {len(left_payload)}u",
        f"#define {spec.symbol}_RIGHT_PAYLOAD_LEN {len(right_payload)}u",
        f"#define {spec.symbol}_EXPECTED_LEN {len(expected_payload)}u",
        f"#define {spec.symbol}_VALID_OUTPUT_PIXELS {valid_count}u",
        f"#define {spec.symbol}_PROBE_X {probe_x}u",
        f"#define {spec.symbol}_PROBE_Y {probe_y}u",
        f"#define {spec.symbol}_PROBE_DISPARITY {probe_d}u",
        f"#define {spec.symbol}_PROBE_TRUE_COST {probe_true_cost}u",
        f"#define {spec.symbol}_PROBE_ALT_DISPARITY {probe_alt_d}u",
        f"#define {spec.symbol}_PROBE_ALT_COST {probe_alt_cost}u",
        "",
        c_array(f"{prefix}_left", "uint8_t", left_payload),
        "",
        c_array(f"{prefix}_right", "uint8_t", right_payload),
        "",
        c_array(f"{prefix}_expected_disparity", "uint8_t", expected_payload),
        "",
        c_array(f"{prefix}_expected_best_cost", "uint16_t", best_cost_payload, per_line=8),
        "",
        f"#endif /* {guard} */",
        "",
    ]
    return "\n".join(lines), case_meta


def write_if_changed(path: Path, data: str) -> bool:
    encoded = data.encode("utf-8")
    if path.exists() and path.read_bytes() == encoded:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)
    return True


def generate(out_dir: Path, presets: Sequence[str], command: str) -> dict[str, object]:
    out_dir.mkdir(parents=True, exist_ok=True)
    cases_meta: list[dict[str, object]] = []
    files: list[dict[str, object]] = []

    for preset in presets:
        spec = PRESETS[preset]
        header, meta = make_header(spec, command)
        filename = f"{spec.name.replace('-', '_')}.h"
        path = out_dir / filename
        write_if_changed(path, header)
        data = path.read_bytes()
        files.append({
            "path": filename,
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        })
        cases_meta.append(meta)

    manifest = {
        "kind": "tpa_stereo_sad_cases_manifest_v0",
        "generator": "tools/depth/gen_stereo_sad_case.py",
        "generator_version": GENERATOR_VERSION,
        "command": command,
        "policy": "deterministic synthetic data; no external images, datasets, model weights, or third-party stereo code",
        "files": files,
        "cases": cases_meta,
    }
    manifest_text = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    manifest_path = out_dir / "stereo_sad_cases_manifest.json"
    write_if_changed(manifest_path, manifest_text)
    manifest_bytes = manifest_path.read_bytes()
    manifest["manifest_file"] = {
        "path": manifest_path.name,
        "bytes": len(manifest_bytes),
        "sha256": hashlib.sha256(manifest_bytes).hexdigest(),
    }
    return manifest


def self_check() -> None:
    command = "tools/depth/gen_stereo_sad_case.py --self-check"
    first: list[tuple[str, str, dict[str, object]]] = []
    second: list[tuple[str, str, dict[str, object]]] = []
    for preset in PRESETS:
        first.append((preset, *make_header(PRESETS[preset], command)))
        second.append((preset, *make_header(PRESETS[preset], command)))
    if first != second:
        raise GenerationError("self-check failed: generation is not deterministic")
    print(f"self-check passed for {len(PRESETS)} presets")


def normalize_presets(values: Sequence[str]) -> list[str]:
    if not values or "all" in values:
        return list(PRESETS.keys())
    unknown = [v for v in values if v not in PRESETS]
    if unknown:
        raise GenerationError(f"unknown preset(s): {', '.join(unknown)}")
    return list(values)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out-dir",
        default="tests/depth/generated",
        help="directory for generated headers and manifest (default: tests/depth/generated)",
    )
    parser.add_argument(
        "--preset",
        action="append",
        choices=["all", *PRESETS.keys()],
        help="preset to generate; may be repeated; default is all",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="run deterministic/reference self-checks without writing files",
    )
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            self_check()
            return 0

        presets = normalize_presets(args.preset or ["all"])
        command = " ".join(["tools/depth/gen_stereo_sad_case.py", *sys.argv[1:]])
        manifest = generate(Path(args.out_dir), presets, command)
        for entry in manifest["files"]:
            print(f"wrote {args.out_dir}/{entry['path']} {entry['bytes']} bytes {entry['sha256']}")
        mf = manifest["manifest_file"]
        print(f"wrote {args.out_dir}/{mf['path']} {mf['bytes']} bytes {mf['sha256']}")
    except GenerationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
