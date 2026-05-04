#!/usr/bin/env python3
"""Generate external YOLOv8n quantized weight headers.

The help/config paths use only the standard library. Real generation verifies
external artifact checksums, delegates PTQ to the heavyweight exporter, and
keeps model-derived outputs in an explicit external directory.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shlex
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPO_ROOT / "models" / "yolov8n_artifact_manifest.json"
DEFAULT_SELECTION = Path(__file__).resolve().with_name("yolov8n_external_layer_selection.json")
DEFAULT_PTQ_TOOL = Path(__file__).resolve().with_name("ptq_yolov5.py")
DEFAULT_OUT_DIR = Path("/tmp/tpa-yolov8n/generated-weights")
DEFAULT_STEM = "yolov8n_external_detect_c2f"

MODULE_REF_KEYS = {"module", "top_level_module", "top_level_layer", "source_module", "conv", "batch_norm"}
CONV_RECORD_FIELDS = (
    "weight_shape",
    "bias",
    "kernel_size",
    "stride",
    "padding",
    "groups",
    "in_channels",
    "out_channels",
)


class ConfigError(RuntimeError):
    """Raised when checked-in manifest/config data is inconsistent."""


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text())
    except FileNotFoundError as exc:
        raise ConfigError(f"JSON file not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ConfigError(f"Invalid JSON in {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise ConfigError(f"Expected top-level JSON object in {path}")
    return data


def is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def named_module_index(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    try:
        records = manifest["pytorch"]["named_modules"]
    except KeyError as exc:
        raise ConfigError("Manifest is missing pytorch.named_modules") from exc
    if not isinstance(records, list):
        raise ConfigError("Manifest pytorch.named_modules must be a list")
    out: dict[str, dict[str, Any]] = {}
    for rec in records:
        if not isinstance(rec, dict) or not isinstance(rec.get("name"), str):
            raise ConfigError("Every named module record must contain a string name")
        out[rec["name"]] = rec
    return out


def iter_module_refs(value: Any) -> Iterable[tuple[str, str]]:
    if isinstance(value, dict):
        for key, item in value.items():
            if key in MODULE_REF_KEYS and isinstance(item, str) and item.startswith("model."):
                yield key, item
            else:
                yield from iter_module_refs(item)
    elif isinstance(value, list):
        for item in value:
            yield from iter_module_refs(item)


def iter_records(value: Any) -> Iterable[dict[str, Any]]:
    if isinstance(value, dict):
        yield value
        for item in value.values():
            yield from iter_records(item)
    elif isinstance(value, list):
        for item in value:
            yield from iter_records(item)


def compare_optional(record: dict[str, Any], manifest_record: dict[str, Any], field: str, label: str) -> None:
    if field in record and record[field] != manifest_record.get(field):
        raise ConfigError(
            f"Selection {label} field {field!r}={record[field]!r} does not match "
            f"manifest value {manifest_record.get(field)!r}"
        )


def validate_selection_against_manifest(manifest: dict[str, Any], selection: dict[str, Any]) -> list[int]:
    if manifest.get("kind") != "tpa_yolov8n_artifact_manifest_v0":
        raise ConfigError(f"Unexpected manifest kind: {manifest.get('kind')!r}")
    if manifest.get("model") != "yolov8n":
        raise ConfigError(f"Unexpected manifest model: {manifest.get('model')!r}")
    if selection.get("kind") != "tpa_yolov8n_external_layer_selection_v0":
        raise ConfigError(f"Unexpected selection kind: {selection.get('kind')!r}")
    if selection.get("model") != manifest.get("model"):
        raise ConfigError("Selection model does not match manifest model")

    source = selection.get("source_manifest", {})
    if source.get("kind") != manifest.get("kind"):
        raise ConfigError("Selection source_manifest.kind does not match manifest kind")
    expected_pt_sha = manifest["pytorch"]["artifact"]["sha256"]
    expected_onnx_sha = manifest["onnx"]["artifact"]["sha256"]
    if source.get("pt_sha256") != expected_pt_sha:
        raise ConfigError("Selection PT checksum does not match manifest PT checksum")
    if source.get("onnx_sha256") != expected_onnx_sha:
        raise ConfigError("Selection ONNX checksum does not match manifest ONNX checksum")

    summary = manifest.get("tpa_summary", {})
    if selection.get("detect_input_edges") != summary.get("feature_pyramid_inputs_to_detect"):
        raise ConfigError("Selection detect_input_edges do not match manifest tpa_summary")
    if selection.get("detect_head") != summary.get("detect_head"):
        raise ConfigError("Selection detect_head does not match manifest tpa_summary.detect_head")

    layers = selection.get("layers")
    if not isinstance(layers, dict) or not layers:
        raise ConfigError("Selection must contain a non-empty layers object")
    try:
        ids = sorted(int(key) for key in layers)
    except ValueError as exc:
        raise ConfigError("Selection layer ids must be integers encoded as strings") from exc
    full_n_layers = selection.get("full_n_layers")
    if not isinstance(full_n_layers, int) or full_n_layers <= 0:
        raise ConfigError("Selection full_n_layers must be a positive integer")
    if ids != list(range(full_n_layers)):
        raise ConfigError(
            "Selection layers must be contiguous from 0 to full_n_layers - 1 "
            f"for this exported table; got ids {ids!r} with full_n_layers={full_n_layers}"
        )

    named = named_module_index(manifest)
    for layer_id in ids:
        name = layers[str(layer_id)]
        if not isinstance(name, str) or not name.startswith("model."):
            raise ConfigError(f"Selection layer {layer_id} has invalid module name {name!r}")
        if name not in named:
            raise ConfigError(f"Selection layer {layer_id} module {name!r} is not in manifest named_modules")

    for _key, module in iter_module_refs(selection):
        if module not in named:
            raise ConfigError(f"Selection references module {module!r}, but the manifest does not contain it")

    for rec in iter_records(selection):
        module = rec.get("module")
        if not isinstance(module, str) or "weight_shape" not in rec:
            continue
        if module not in named:
            raise ConfigError(f"Selection record references missing module {module!r}")
        module_rec = named[module]
        if module_rec.get("type") == "Conv":
            conv_name = module + ".conv"
            bn_name = module + ".bn"
            if conv_name not in named or bn_name not in named:
                raise ConfigError(f"Conv wrapper {module!r} is missing conv/bn children in manifest")
            conv_rec = named[conv_name]
            bn_rec = named[bn_name]
            if rec.get("conv") != conv_name:
                raise ConfigError(f"Selection record for {module!r} must name conv child {conv_name!r}")
            if rec.get("batch_norm") != bn_name:
                raise ConfigError(f"Selection record for {module!r} must name batch_norm child {bn_name!r}")
            for field in CONV_RECORD_FIELDS:
                compare_optional(rec, conv_rec, field, module)
            if rec.get("batch_norm_features") != bn_rec.get("num_features"):
                raise ConfigError(f"Selection batch_norm_features for {module!r} do not match manifest")
        elif module_rec.get("type") == "Conv2d":
            for field in CONV_RECORD_FIELDS:
                compare_optional(rec, module_rec, field, module)
        else:
            raise ConfigError(f"Selection weighted module {module!r} has unsupported type {module_rec.get('type')!r}")

    return ids


def artifact_record(manifest: dict[str, Any], key: str) -> dict[str, Any]:
    try:
        rec = manifest[key]["artifact"]
    except KeyError as exc:
        raise ConfigError(f"Manifest is missing {key}.artifact") from exc
    required = {"path", "size_bytes", "sha256", "checked_in"}
    missing = sorted(required - set(rec))
    if missing:
        raise ConfigError(f"Manifest {key}.artifact is missing fields: {', '.join(missing)}")
    return rec


def resolve_external_artifacts(args: argparse.Namespace, manifest: dict[str, Any], parser: argparse.ArgumentParser) -> tuple[Path, Path]:
    pt_rec = artifact_record(manifest, "pytorch")
    onnx_rec = artifact_record(manifest, "onnx")

    if args.external_root is not None:
        if args.pt is not None or args.onnx is not None:
            parser.error("use either --external-root or the --pt/--onnx pair, not both")
        root = args.external_root.expanduser().resolve()
        return root / pt_rec["path"], root / onnx_rec["path"]

    if (args.pt is None) != (args.onnx is None):
        parser.error("--pt and --onnx must be provided together")
    if args.pt is None or args.onnx is None:
        parser.error("provide --external-root or provide both --pt and --onnx")
    return args.pt.expanduser().resolve(), args.onnx.expanduser().resolve()


def verify_artifact(label: str, path: Path, expected: dict[str, Any]) -> dict[str, Any]:
    if not path.exists():
        raise SystemExit(f"{label} artifact not found: {path}")
    size = path.stat().st_size
    if size != int(expected["size_bytes"]):
        raise SystemExit(
            f"{label} artifact size mismatch for {path}: got {size}, expected {expected['size_bytes']}"
        )
    digest = sha256_file(path)
    if digest != expected["sha256"]:
        raise SystemExit(
            f"{label} artifact SHA-256 mismatch for {path}: got {digest}, expected {expected['sha256']}"
        )
    return {
        "label": label,
        "path": str(path),
        "manifest_record_path": expected["path"],
        "size_bytes": size,
        "sha256": digest,
        "checked_in": bool(expected.get("checked_in", False)),
    }


def parse_layer_ids(spec: str | None, valid_ids: set[int]) -> list[int] | None:
    if spec is None:
        return None
    ids: list[int] = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            layer_id = int(part, 10)
        except ValueError as exc:
            raise SystemExit(f"Invalid --layer-ids entry {part!r}: expected integer") from exc
        if layer_id not in valid_ids:
            raise SystemExit(f"Layer id {layer_id} is not present in the selection config")
        ids.append(layer_id)
    if not ids:
        raise SystemExit("--layer-ids did not select any layers")
    return ids


def build_ptq_command(
    args: argparse.Namespace,
    pt_path: Path,
    selection_path: Path,
    out_dir: Path,
    header_path: Path,
    selected_ids: list[int] | None,
) -> list[str]:
    cmd = [
        str(args.python_exe),
        str(args.ptq_tool),
        "--model",
        str(pt_path),
        "--stem",
        args.stem,
        "--n-calib",
        str(args.n_calib),
        "--out-dir",
        str(out_dir),
        "--layer-map",
        str(selection_path),
        "--inline-header",
        str(header_path),
    ]
    if args.calib_dir is not None:
        cmd.extend(["--calib-dir", str(args.calib_dir.expanduser().resolve())])
    if selected_ids is not None:
        cmd.extend(["--layers", ",".join(str(x) for x in selected_ids)])
    return cmd


def output_records(out_dir: Path, stem: str, manifest_out: Path) -> list[dict[str, Any]]:
    records = []
    for path in sorted(out_dir.iterdir()):
        if not path.is_file() or not path.name.startswith(stem) or path.resolve() == manifest_out.resolve():
            continue
        records.append(
            {
                "path": str(path),
                "path_relative_to_out_dir": path.name,
                "size_bytes": path.stat().st_size,
                "sha256": sha256_file(path),
            }
        )
    return records


def write_generated_manifest(
    args: argparse.Namespace,
    manifest_path: Path,
    selection_path: Path,
    out_dir: Path,
    manifest_out: Path,
    command: list[str],
    artifact_records: list[dict[str, Any]],
    selected_ids: list[int] | None,
    selection: dict[str, Any],
) -> None:
    outputs = output_records(out_dir, args.stem, manifest_out)
    if not outputs:
        raise SystemExit(f"No generated outputs beginning with {args.stem!r} found in {out_dir}")

    layers = selection["layers"]
    selected = selected_ids if selected_ids is not None else sorted(int(k) for k in layers)
    generated = {
        "kind": "tpa_yolov8n_external_weights_manifest_v0",
        "model": "yolov8n",
        "generated_at_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "generator": {
            "wrapper": str(Path(__file__).resolve()),
            "ptq_tool": str(args.ptq_tool),
            "python_exe": str(args.python_exe),
        },
        "policy": {
            "source_model_binaries": "external-not-checked-in",
            "generated_outputs": "model-derived; keep external/untracked unless project owners approve vendoring",
            "immutable_model_data": "Generated weights, biases, fused BN parameters, and quantization tables are immutable model data for future kernels, not process scratch or edge/channel payloads.",
            "edge_channel_payloads": selection.get("detect_input_edges", []),
        },
        "inputs": {
            "artifact_manifest": {
                "path": str(manifest_path),
                "sha256": sha256_file(manifest_path),
            },
            "layer_selection": {
                "path": str(selection_path),
                "sha256": sha256_file(selection_path),
                "selected_layer_ids": selected,
                "selected_layers": {str(i): layers[str(i)] for i in selected},
            },
            "external_artifacts": artifact_records,
        },
        "calibration": {
            "n_calib": args.n_calib,
            "calib_dir": str(args.calib_dir.expanduser().resolve()) if args.calib_dir else None,
            "synthetic_random": args.calib_dir is None,
            "note": "Synthetic random calibration is reproducible smoke plumbing only; production quantization should use representative images via --calib-dir.",
        },
        "output_dir": str(out_dir),
        "outputs": outputs,
        "command": command,
        "command_shell": " ".join(shlex.quote(part) for part in command),
    }
    manifest_out.write_text(json.dumps(generated, indent=2) + "\n")
    print(f"Wrote generated-output manifest: {manifest_out}")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Verify external YOLOv8n artifacts and generate quantized C headers "
            "into an external/untracked output directory."
        )
    )
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST, help="YOLOv8n artifact manifest JSON")
    parser.add_argument("--selection", type=Path, default=DEFAULT_SELECTION, help="YOLOv8n layer-selection JSON")
    parser.add_argument("--external-root", type=Path, help="Root containing manifest-recorded paths such as external/yolov8n.pt")
    parser.add_argument("--pt", type=Path, help="Explicit external yolov8n.pt path")
    parser.add_argument("--onnx", type=Path, help="Explicit external yolov8n.onnx path")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR, help="External/untracked generated-output directory")
    parser.add_argument("--manifest-out", type=Path, help="Generated-output checksum/provenance manifest path")
    parser.add_argument("--stem", default=DEFAULT_STEM, help="Output stem passed to the PTQ exporter")
    parser.add_argument("--python-exe", default=sys.executable, help="Python interpreter for the heavyweight PTQ exporter")
    parser.add_argument("--ptq-tool", type=Path, default=DEFAULT_PTQ_TOOL, help="PTQ exporter script to run after verification")
    parser.add_argument("--calib-dir", type=Path, help="Directory of calibration images; omit only for synthetic smoke generation")
    parser.add_argument("--n-calib", type=int, default=32, help="Number of calibration images or synthetic samples")
    parser.add_argument("--layer-ids", help="Optional comma-separated subset of selection export layer ids")
    parser.add_argument(
        "--allow-repo-out-dir",
        action="store_true",
        help="Allow generated outputs inside this repository. Avoid this unless a project owner approved vendoring.",
    )
    parser.add_argument(
        "--validate-config-only",
        action="store_true",
        help="Validate the checked-in manifest/selection config without requiring external artifacts or heavyweight packages.",
    )
    parser.add_argument(
        "--print-ptq-command-only",
        action="store_true",
        help="Verify artifacts and print the PTQ command without running it.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    manifest_path = args.manifest.expanduser().resolve()
    selection_path = args.selection.expanduser().resolve()
    args.ptq_tool = args.ptq_tool.expanduser().resolve()

    manifest = load_json(manifest_path)
    selection = load_json(selection_path)
    layer_ids = validate_selection_against_manifest(manifest, selection)
    selected_ids = parse_layer_ids(args.layer_ids, set(layer_ids))

    if args.validate_config_only:
        selected_count = len(selected_ids) if selected_ids is not None else len(layer_ids)
        print(
            f"Validated {selection_path} against {manifest_path}: "
            f"{selected_count} selected quantized modules, "
            f"{len(selection.get('detect_input_edges', []))} detect input edges."
        )
        return 0

    pt_path, onnx_path = resolve_external_artifacts(args, manifest, parser)
    pt_verified = verify_artifact("PT", pt_path, artifact_record(manifest, "pytorch"))
    onnx_verified = verify_artifact("ONNX", onnx_path, artifact_record(manifest, "onnx"))

    out_dir = args.out_dir.expanduser().resolve()
    repo_root = REPO_ROOT.resolve()
    if is_within(out_dir, repo_root) and not args.allow_repo_out_dir:
        raise SystemExit(
            f"Refusing to write YOLOv8n model-derived outputs inside repository {repo_root}: {out_dir}. "
            "Use an external --out-dir such as /tmp/tpa-yolov8n/generated-weights, or pass "
            "--allow-repo-out-dir only with explicit project-owner approval."
        )
    out_dir.mkdir(parents=True, exist_ok=True)

    header_path = out_dir / f"{args.stem}_weights.h"
    manifest_out = (
        args.manifest_out.expanduser().resolve()
        if args.manifest_out is not None
        else out_dir / f"{args.stem}_generated_manifest.json"
    )
    if is_within(manifest_out, repo_root) and not args.allow_repo_out_dir:
        raise SystemExit(
            f"Refusing to write the YOLOv8n generated-output manifest inside repository {repo_root}: "
            f"{manifest_out}. Keep generated provenance with the external outputs."
        )
    manifest_out.parent.mkdir(parents=True, exist_ok=True)

    command = build_ptq_command(args, pt_path, selection_path, out_dir, header_path, selected_ids)
    if args.print_ptq_command_only:
        print(" ".join(shlex.quote(part) for part in command))
        return 0

    print("Verified external YOLOv8n artifacts against manifest checksums.")
    print(f"Writing generated YOLOv8n headers under: {out_dir}")
    subprocess.run(command, check=True, cwd=REPO_ROOT)
    write_generated_manifest(
        args=args,
        manifest_path=manifest_path,
        selection_path=selection_path,
        out_dir=out_dir,
        manifest_out=manifest_out,
        command=command,
        artifact_records=[pt_verified, onnx_verified],
        selected_ids=selected_ids,
        selection=selection,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ConfigError as exc:
        raise SystemExit(f"Configuration error: {exc}") from exc
