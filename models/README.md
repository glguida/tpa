# Model Artifacts

This directory contains source model artifacts or model-artifact manifests used
by the YOLO reproduction path. The authoritative YOLOv8n policy and claim
checklist is in
`docs/yolo-demo.md#yolov8n-calibration-data-and-generated-artifact-policy`.

## Checked-in YOLOv5n artifacts

The current structured YOLO implementation uses the checked-in YOLOv5nu source
artifacts:

| Path | SHA-256 |
|---|---|
| `models/yolov5nu.onnx` | `a25f225d4d29135249c756addc1ca388a00cf4a302790aedc739d3e771ebc915` |
| `models/yolov5nu.pt` | `9e9c1be448b0e1b8598975a9abcab9a0fd0e21182eb422cda7f044a0442d0937` |

## External YOLOv8n artifact manifest

`models/yolov8n_artifact_manifest.json` records the targeted YOLOv8n artifact
for future work without vendoring the model files into git. The upstream
Ultralytics artifact is AGPL-3.0; keep the `.pt` and exported `.onnx` external
until project owners explicitly approve committing those binaries.

Recorded artifacts:

| Artifact | Source or export | Size | SHA-256 |
|---|---|---:|---|
| `external/yolov8n.pt` | `https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8n.pt` | 6,549,796 bytes | `f59b3d833e2ff32e194b5bb8e08d211dc7c5bdf144b90d2c8412c47ccfc83b36` |
| `external/yolov8n.onnx` | exported from the `.pt` with Ultralytics 8.3.0, static `640x640`, opset 12, no NMS | 12,823,444 bytes | `3192f31d65cf4d6a3f00c06a0ba1ae9b78f9c3a14f50c60ea3b086fbcec4f034` |

Recreate the external files in a scratch directory, then inspect them:

```sh
mkdir -p /tmp/tpa-yolov8n/external
python3 - <<'PY'
from pathlib import Path
import hashlib
import urllib.request
url = 'https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8n.pt'
out = Path('/tmp/tpa-yolov8n/external/yolov8n.pt')
urllib.request.urlretrieve(url, out)
print(out, out.stat().st_size, hashlib.sha256(out.read_bytes()).hexdigest())
PY

python3 -m venv --system-site-packages /tmp/tpa-yolov8n/venv
. /tmp/tpa-yolov8n/venv/bin/activate
python -m pip install 'ultralytics==8.3.0' onnx
python -c "from ultralytics import YOLO; YOLO('/tmp/tpa-yolov8n/external/yolov8n.pt').export(format='onnx', imgsz=640, opset=12, dynamic=False, simplify=False, nms=False, batch=1, device='cpu')"
python tools/yolo/inspect_yolo_artifact.py \
  --pt /tmp/tpa-yolov8n/external/yolov8n.pt \
  --onnx /tmp/tpa-yolov8n/external/yolov8n.onnx \
  --pt-record-path external/yolov8n.pt \
  --onnx-record-path external/yolov8n.onnx \
  --imgsz 640 \
  --output /tmp/tpa-yolov8n/yolov8n_artifact_manifest.json
```

The manifest is architecture input only. The repository's YOLOv8n device
milestones consume separately generated external headers, but this manifest does
not itself add calibration data, generated weight headers, production accuracy
evidence, or a full YOLOv8n program graph. Kernel jobs must keep immutable model
data, process scratch, and edge/channel payload storage distinct.

## External YOLOv8n generated headers

Use `tools/yolo/regen_yolov8n_external_weights.py` to generate YOLOv8n INT8
headers from those external artifacts without vendoring model-derived outputs:

```sh
python3 tools/yolo/regen_yolov8n_external_weights.py --validate-config-only
python3 tools/yolo/regen_yolov8n_external_weights.py \
  --external-root /tmp/tpa-yolov8n \
  --out-dir /tmp/tpa-yolov8n/generated-weights \
  --calib-dir /path/to/representative/calibration/images
```

The wrapper verifies the `.pt` and ONNX checksums from this manifest before
calling the heavyweight PTQ exporter. It writes generated headers and
`<stem>_generated_manifest.json` under the external output directory. Those
outputs are model-derived immutable weight/quantization data for future kernels,
not process scratch and not graph edge/channel payloads. Do not commit them, the
external model binaries, calibration data, or model-derived activations unless
project owners explicitly approve vendoring. Pass generated paths to builds with
explicit CMake cache variables or equivalent inputs; do not hardcode AgentWS
workspace or scratch paths in committed source.
