#!/usr/bin/env python3
"""Inspect YOLO model artifacts and write a TPA architecture manifest.

The command deliberately avoids importing heavyweight model packages at module
import time so ``--help`` works in the lightweight planner environment. Commands
that inspect PyTorch/Ultralytics or ONNX artifacts import those optional
packages only after arguments are parsed and produce an actionable error when a
package is missing.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


INSTALL_HINT = (
    "Install the optional YOLO inspection dependencies in a throwaway "
    "environment, for example: python3 -m venv /tmp/tpa-yolo-inspect && "
    ". /tmp/tpa-yolo-inspect/bin/activate && "
    "python -m pip install 'ultralytics==8.3.0' onnx"
)


PT_SOURCE_URL = "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8n.pt"
ULTRALYTICS_LICENSE_URL = "https://github.com/ultralytics/ultralytics/blob/main/LICENSE"


def import_optional(module_name: str, purpose: str) -> Any:
    try:
        return __import__(module_name)
    except ImportError as exc:
        raise SystemExit(
            f"Missing optional dependency '{module_name}' needed for {purpose}. "
            f"{INSTALL_HINT}"
        ) from exc


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def artifact_record(path: Path, record_path: str | None) -> dict[str, Any]:
    return {
        "path": record_path or str(path),
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "checked_in": False,
    }


def jsonable(value: Any) -> Any:
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, tuple):
        return [jsonable(v) for v in value]
    if isinstance(value, list):
        return [jsonable(v) for v in value]
    if isinstance(value, dict):
        return {str(k): jsonable(v) for k, v in value.items()}
    if hasattr(value, "tolist"):
        return value.tolist()
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


def importlib_version(package: str) -> str | None:
    try:
        from importlib.metadata import version

        return version(package)
    except Exception:
        return None


def tensor_shape(value: Any, torch: Any) -> Any:
    if isinstance(value, torch.Tensor):
        return [int(dim) for dim in value.shape]
    if isinstance(value, (list, tuple)):
        return [tensor_shape(item, torch) for item in value]
    if isinstance(value, dict):
        return {str(k): tensor_shape(v, torch) for k, v in value.items()}
    return type(value).__name__


def module_class_name(module: Any) -> str:
    return f"{type(module).__module__}.{type(module).__name__}"


def parameter_count(module: Any, recurse: bool = True) -> int:
    return int(sum(param.numel() for param in module.parameters(recurse=recurse)))


def inspect_named_modules(net: Any, torch: Any) -> list[dict[str, Any]]:
    nn = torch.nn
    records: list[dict[str, Any]] = []
    interesting = {
        "Conv",
        "C2f",
        "Bottleneck",
        "SPPF",
        "Detect",
        "DFL",
        "Concat",
        "Upsample",
        "Sequential",
        "ModuleList",
    }
    for name, module in net.named_modules():
        if not name:
            continue
        tname = type(module).__name__
        if not (name.startswith("model.") and (tname in interesting or isinstance(module, (nn.Conv2d, nn.BatchNorm2d)))):
            continue
        rec: dict[str, Any] = {
            "name": name,
            "type": tname,
            "class": module_class_name(module),
            "parameter_count_recursive": parameter_count(module),
            "parameter_count_direct": parameter_count(module, recurse=False),
        }
        if isinstance(module, nn.Conv2d):
            rec.update(
                {
                    "weight_shape": [int(x) for x in module.weight.shape],
                    "bias": module.bias is not None,
                    "kernel_size": [int(x) for x in module.kernel_size],
                    "stride": [int(x) for x in module.stride],
                    "padding": [int(x) for x in module.padding],
                    "groups": int(module.groups),
                    "in_channels": int(module.in_channels),
                    "out_channels": int(module.out_channels),
                }
            )
        elif isinstance(module, nn.BatchNorm2d):
            rec.update(
                {
                    "num_features": int(module.num_features),
                    "eps": float(module.eps),
                    "momentum": None if module.momentum is None else float(module.momentum),
                }
            )
        records.append(rec)
    return records


def unwrap_singleton_tuple_shape(shape: Any) -> Any:
    if isinstance(shape, list) and len(shape) == 1:
        return shape[0]
    return shape


def infer_scale_name(stride: int) -> str:
    return {8: "P3", 16: "P4", 32: "P5"}.get(stride, f"stride_{stride}")


def product(values: list[int]) -> int:
    out = 1
    for value in values:
        out *= int(value)
    return out


def inspect_pt(path: Path, imgsz: int) -> dict[str, Any]:
    ultralytics = import_optional("ultralytics", "PyTorch YOLO artifact inspection")
    torch = import_optional("torch", "PyTorch YOLO artifact inspection")

    yolo = ultralytics.YOLO(str(path))
    net = yolo.model
    net.eval()

    pre_inputs: dict[int, Any] = {}
    outputs: dict[int, Any] = {}
    hooks = []
    for module in net.model:
        layer_index = int(module.i)

        def pre_hook(mod: Any, inp: Any, idx: int = layer_index) -> None:
            pre_inputs[idx] = tensor_shape(inp, torch)

        def post_hook(mod: Any, inp: Any, out: Any, idx: int = layer_index) -> None:
            del inp
            outputs[idx] = tensor_shape(out, torch)

        hooks.append(module.register_forward_pre_hook(pre_hook))
        hooks.append(module.register_forward_hook(post_hook))

    with torch.no_grad():
        final_output = net(torch.zeros(1, 3, imgsz, imgsz))

    for hook in hooks:
        hook.remove()

    layers: list[dict[str, Any]] = []
    detect_layer: dict[str, Any] | None = None
    for module in net.model:
        idx = int(module.i)
        rec: dict[str, Any] = {
            "index": idx,
            "module": f"model.{idx}",
            "type": type(module).__name__,
            "class": getattr(module, "type", module_class_name(module)),
            "from": jsonable(getattr(module, "f", None)),
            "parameter_count_recursive": parameter_count(module),
            "input_shapes": unwrap_singleton_tuple_shape(pre_inputs.get(idx)),
            "output_shapes": outputs.get(idx),
        }
        layers.append(rec)
        if type(module).__name__ == "Detect":
            detect_layer = {
                **rec,
                "nc": int(module.nc),
                "nl": int(module.nl),
                "reg_max": int(module.reg_max),
                "dfl_box_channels": int(4 * module.reg_max),
                "class_channels": int(module.nc),
                "raw_channels_per_scale": int(module.no),
                "stride": [int(x) for x in module.stride.tolist()],
            }

    names = getattr(yolo, "names", {}) or getattr(net, "names", {})
    names = {str(k): v for k, v in names.items()}

    return {
        "artifact": artifact_record(path, None),
        "environment": {
            "ultralytics_version": getattr(ultralytics, "__version__", importlib_version("ultralytics")),
            "torch_version": getattr(torch, "__version__", importlib_version("torch")),
        },
        "task": getattr(yolo, "task", None),
        "names_count": len(names),
        "names": names,
        "yaml": jsonable(getattr(net, "yaml", {})),
        "model_class": module_class_name(net),
        "stride": [int(x) for x in getattr(net, "stride").tolist()],
        "nc": int(getattr(net, "nc")),
        "input_probe_shape": [1, 3, imgsz, imgsz],
        "final_output_shapes": tensor_shape(final_output, torch),
        "top_level_layers": layers,
        "detect_layer": detect_layer,
        "named_modules": inspect_named_modules(net, torch),
        "parameter_count": parameter_count(net),
    }


def onnx_dims(value_info: Any) -> list[int | str | None]:
    dims: list[int | str | None] = []
    for dim in value_info.type.tensor_type.shape.dim:
        if dim.dim_value:
            dims.append(int(dim.dim_value))
        elif dim.dim_param:
            dims.append(dim.dim_param)
        else:
            dims.append(None)
    return dims


def inspect_onnx(path: Path) -> dict[str, Any]:
    onnx = import_optional("onnx", "ONNX artifact inspection")
    model = onnx.load(str(path))
    op_counts = collections.Counter(node.op_type for node in model.graph.node)
    initializer_bytes = 0
    for init in model.graph.initializer:
        try:
            initializer_bytes += onnx.numpy_helper.to_array(init).nbytes
        except Exception:
            initializer_bytes += len(init.raw_data)
    return {
        "artifact": artifact_record(path, None),
        "environment": {"onnx_version": getattr(onnx, "__version__", importlib_version("onnx"))},
        "ir_version": int(model.ir_version),
        "producer_name": model.producer_name,
        "producer_version": model.producer_version,
        "opsets": [
            {"domain": opset.domain, "version": int(opset.version)} for opset in model.opset_import
        ],
        "inputs": [{"name": item.name, "shape": onnx_dims(item)} for item in model.graph.input],
        "outputs": [{"name": item.name, "shape": onnx_dims(item)} for item in model.graph.output],
        "node_count": len(model.graph.node),
        "operator_counts": dict(sorted(op_counts.items())),
        "initializer_count": len(model.graph.initializer),
        "initializer_bytes": int(initializer_bytes),
        "nodes": [
            {
                "name": node.name,
                "op_type": node.op_type,
                "inputs": list(node.input),
                "outputs": list(node.output),
            }
            for node in model.graph.node
        ],
    }


def raw_detect_shapes(detect_output_shapes: Any) -> list[list[int]]:
    if isinstance(detect_output_shapes, list) and len(detect_output_shapes) == 2:
        raw = detect_output_shapes[1]
        if isinstance(raw, list):
            return raw
    return []


def derive_tpa_summary(pt: dict[str, Any] | None, onnx: dict[str, Any] | None, edge_dtype_bits: int) -> dict[str, Any]:
    summary: dict[str, Any] = {
        "memory_categories": {
            "immutable_model_data": "Weights, biases, batch-normalization parameters, DFL weights, and any generated quantization tables derive from the model artifact and are immutable model data.",
            "transient_scratch": "Scratch sizes are not determined by this manifest; future YOLOv8n process kernels must declare scratch_peak_bytes from their actual implementations.",
            "edge_channel_payloads": "Feature and detect tensors listed below are graph edge/channel payloads, not persistent process state. Byte counts assume signed INT8 tensors for the first structured downstream milestone unless a future quantization job chooses a different representation.",
        }
    }

    if onnx:
        summary["public_onnx_inputs"] = onnx.get("inputs", [])
        summary["public_onnx_outputs"] = onnx.get("outputs", [])
        summary["onnx_operator_families"] = onnx.get("operator_counts", {})

    if not pt or not pt.get("detect_layer"):
        return summary

    detect = pt["detect_layer"]
    detect_inputs = detect.get("input_shapes", [])
    raw_shapes = raw_detect_shapes(detect.get("output_shapes"))
    source_layers = detect.get("from", [])
    strides = detect.get("stride", [])
    features = []
    for i, shape in enumerate(detect_inputs):
        if not (isinstance(shape, list) and len(shape) == 4):
            continue
        stride = int(strides[i]) if i < len(strides) else None
        source_layer = source_layers[i] if isinstance(source_layers, list) and i < len(source_layers) else None
        payload_bytes = product(shape) * int(edge_dtype_bits) // 8
        features.append(
            {
                "scale": infer_scale_name(stride) if stride is not None else f"scale_{i}",
                "source_layer": source_layer,
                "source_module": f"model.{source_layer}" if source_layer is not None else None,
                "stride": stride,
                "shape_nchw": shape,
                "height": int(shape[2]),
                "width": int(shape[3]),
                "channels": int(shape[1]),
                "int8_edge_payload_bytes": payload_bytes,
            }
        )

    raw = []
    for i, shape in enumerate(raw_shapes):
        if not (isinstance(shape, list) and len(shape) == 4):
            continue
        stride = int(strides[i]) if i < len(strides) else None
        raw.append(
            {
                "scale": infer_scale_name(stride) if stride is not None else f"scale_{i}",
                "stride": stride,
                "shape_nchw": shape,
                "height": int(shape[2]),
                "width": int(shape[3]),
                "channels": int(shape[1]),
                "dfl_box_channels": int(detect["dfl_box_channels"]),
                "class_channels": int(detect["class_channels"]),
                "int8_edge_payload_bytes": product(shape) * int(edge_dtype_bits) // 8,
            }
        )

    summary.update(
        {
            "input_shape_nchw": pt.get("input_probe_shape"),
            "feature_pyramid_inputs_to_detect": features,
            "detect_raw_outputs_before_decode": raw,
            "detect_head": {
                "top_level_layer": detect.get("module"),
                "from_layers": detect.get("from"),
                "nc": detect.get("nc"),
                "strides": strides,
                "reg_max": detect.get("reg_max"),
                "dfl_box_channels": detect.get("dfl_box_channels"),
                "class_channels": detect.get("class_channels"),
                "raw_channels_per_scale": detect.get("raw_channels_per_scale"),
                "public_output_channels_after_decode": 4 + int(detect.get("class_channels", 0)),
            },
            "current_yolov5n_downstream_comparison": {
                "yolov5n_source_edges": [
                    {"scale": "P5", "shape_hwc": [20, 20, 128], "int8_payload_bytes": 51200},
                    {"scale": "P4", "shape_hwc": [40, 40, 128], "int8_payload_bytes": 204800},
                    {"scale": "P3", "shape_hwc": [80, 80, 64], "int8_payload_bytes": 409600},
                ],
                "yolov8n_detect_input_edges": [
                    {
                        "scale": f["scale"],
                        "shape_hwc": [f["height"], f["width"], f["channels"]],
                        "int8_payload_bytes": f["int8_edge_payload_bytes"],
                    }
                    for f in features
                ],
                "block_reuse_notes": [
                    "Conv+BatchNorm+SiLU layers remain the basic CBS-style primitive used by current YOLOv5n tooling.",
                    "YOLOv8n uses C2f top-level blocks where the current YOLOv5n graph and block tests use C3/Bottleneck-style blocks; representative C2f block tests are needed before kernels are reused.",
                    "SPPF, nearest-neighbor upsample, and concat remain present, but exact channel counts differ from the YOLOv5n downstream graph and must be regenerated from this artifact.",
                    "The YOLOv8n Detect head is anchor-free and DFL-based with reg_max=16, 64 box-distribution channels plus 80 class channels per scale before decode; it should not reuse YOLOv5n detect assumptions without a new detect/DFL implementation and tests.",
                ],
            },
        }
    )
    return summary


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Inspect YOLO .pt/.onnx artifacts and emit a JSON architecture manifest."
    )
    parser.add_argument("--pt", type=Path, help="Ultralytics/PyTorch .pt artifact to inspect")
    parser.add_argument("--onnx", type=Path, help="ONNX artifact to inspect")
    parser.add_argument("--pt-record-path", help="Stable path to record for the .pt artifact in JSON")
    parser.add_argument("--onnx-record-path", help="Stable path to record for the ONNX artifact in JSON")
    parser.add_argument("--imgsz", type=int, default=640, help="Square NCHW probe image size for .pt inspection")
    parser.add_argument("--edge-dtype-bits", type=int, default=8, help="Assumed edge tensor element width for byte summaries")
    parser.add_argument("--source-url", default=PT_SOURCE_URL, help="Canonical source URL for the primary artifact")
    parser.add_argument("--license-name", default="AGPL-3.0", help="Upstream license name to record")
    parser.add_argument("--license-url", default=ULTRALYTICS_LICENSE_URL, help="Upstream license URL to record")
    parser.add_argument("--export-command", help="Command used to export the ONNX artifact, if applicable")
    parser.add_argument("--output", type=Path, required=True, help="Output manifest JSON path")
    args = parser.parse_args()
    if not args.pt and not args.onnx:
        parser.error("provide at least one of --pt or --onnx")
    return args


def main() -> int:
    args = parse_args()
    manifest: dict[str, Any] = {
        "kind": "tpa_yolov8n_artifact_manifest_v0",
        "model": "yolov8n",
        "status": "external-artifacts-not-checked-in",
        "provenance": {
            "source_url": args.source_url,
            "upstream_project": "Ultralytics YOLO",
            "upstream_license": args.license_name,
            "upstream_license_url": args.license_url,
            "license_note": "Artifacts are recorded as external until project owners explicitly approve vendoring AGPL-3.0 model files in this repository.",
        },
        "export": {
            "onnx_command": args.export_command,
            "static_shape_policy": f"batch=1, channels=3, height={args.imgsz}, width={args.imgsz}",
            "dynamic": False,
            "nms": False,
        },
    }

    pt_info = None
    onnx_info = None
    if args.pt:
        if not args.pt.exists():
            raise SystemExit(f"PT artifact not found: {args.pt}")
        pt_info = inspect_pt(args.pt, args.imgsz)
        pt_info["artifact"]["path"] = args.pt_record_path or str(args.pt)
        manifest["pytorch"] = pt_info
    if args.onnx:
        if not args.onnx.exists():
            raise SystemExit(f"ONNX artifact not found: {args.onnx}")
        onnx_info = inspect_onnx(args.onnx)
        onnx_info["artifact"]["path"] = args.onnx_record_path or str(args.onnx)
        manifest["onnx"] = onnx_info

    manifest["tpa_summary"] = derive_tpa_summary(pt_info, onnx_info, args.edge_dtype_bits)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=False) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
