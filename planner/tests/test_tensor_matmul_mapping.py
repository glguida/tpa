from __future__ import annotations

import collections
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TILE_BYTES = 64 * 64 * 4


def a_feed_id(col: int) -> int:
    return 6000 + col


def b_feed_id(row: int) -> int:
    return 7000 + row


def cell_id(row: int, col: int, cols: int) -> int:
    return 8000 + row * cols + col


def check_id(row: int, col: int, cols: int) -> int:
    return 10000 + row * cols + col


def write_matmul_program(path: Path, *, rows: int, cols: int) -> None:
    lines: list[str] = []

    for col in range(cols):
        lines.append(f"inst {a_feed_id(col)} tensor_matmul_a_feed_pdef")
    for row in range(rows):
        lines.append(f"inst {b_feed_id(row)} tensor_matmul_b_feed_pdef")
    for row in range(rows):
        for col in range(cols):
            lines.append(f"inst {cell_id(row, col, cols)} tensor_matmul_cell_pdef")
            lines.append(f"inst {check_id(row, col, cols)} tensor_matmul_check_pdef")

    for col in range(cols):
        lines.append(f"conn {a_feed_id(col)} 0 {cell_id(0, col, cols)} 0 {TILE_BYTES}")
    for row in range(rows):
        lines.append(f"conn {b_feed_id(row)} 0 {cell_id(row, cols - 1, cols)} 1 {TILE_BYTES}")
    for row in range(rows):
        for col in range(cols):
            cell = cell_id(row, col, cols)
            lines.append(f"conn {cell} 4 {check_id(row, col, cols)} 0 {TILE_BYTES}")
            if row + 1 < rows:
                lines.append(f"conn {cell} 2 {cell_id(row + 1, col, cols)} 0 {TILE_BYTES}")
            if col > 0:
                lines.append(f"conn {cell} 3 {cell_id(row, col - 1, cols)} 1 {TILE_BYTES}")

    path.write_text("\n".join(lines) + "\n")


def write_process_metadata(path: Path) -> None:
    pdefs = [
        {
            "declared_ws_bytes": 65664,
            "kind": "user",
            "name": "tensor_matmul_a_feed_pdef",
            "pid": 601,
            "ports": [{"direction": "out", "id": 0}],
            "scratch_peak_bytes": 0,
            "start": "tensor_matmul_a_feed_start",
        },
        {
            "declared_ws_bytes": 65664,
            "kind": "user",
            "name": "tensor_matmul_b_feed_pdef",
            "pid": 602,
            "ports": [{"direction": "out", "id": 0}],
            "scratch_peak_bytes": 0,
            "start": "tensor_matmul_b_feed_start",
        },
        {
            "declared_ws_bytes": 16576,
            "kind": "user",
            "name": "tensor_matmul_cell_pdef",
            "pid": 603,
            "ports": [
                {"direction": "in", "id": 0},
                {"direction": "in", "id": 1},
                {"direction": "out", "id": 2},
                {"direction": "out", "id": 3},
                {"direction": "out", "id": 4},
            ],
            "scratch_peak_bytes": TILE_BYTES,
            "start": "tensor_matmul_cell_init",
        },
        {
            "declared_ws_bytes": 64,
            "kind": "user",
            "name": "tensor_matmul_check_pdef",
            "pid": 604,
            "ports": [{"direction": "in", "id": 0}],
            "scratch_peak_bytes": 0,
            "start": "tensor_matmul_check_start",
        },
    ]
    path.write_text(
        json.dumps(
            {
                "embedded_edge_payload_bytes": 0,
                "embedded_edge_payload_symbols": [],
                "label": "tensor_matmul_proc",
                "manifest": str(ROOT / "kernels" / "tpa_tensor_matmul.tpm"),
                "model_blob_bytes": 0,
                "objects": [],
                "other_static_bytes": 0,
                "pdefs": pdefs,
                "per_object_sections": {},
                "per_object_symbols": {},
                "section_sizes": {},
                "workspace_bytes": 0,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n"
    )


def write_compute_costs(path: Path) -> None:
    path.write_text(
        json.dumps(
            {
                "tensor_matmul_a_feed_pdef": 1.0,
                "tensor_matmul_b_feed_pdef": 1.0,
                "tensor_matmul_cell_pdef": 100.0,
                "tensor_matmul_check_pdef": 1.0,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n"
    )


def map_matmul(*, rows: int, cols: int, machine: str) -> dict:
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        program = tmpdir / "matmul.tpp"
        process_json = tmpdir / "process.json"
        costs_json = tmpdir / "costs.json"
        output = tmpdir / "map.json"

        write_matmul_program(program, rows=rows, cols=cols)
        write_process_metadata(process_json)
        write_compute_costs(costs_json)

        env = dict(os.environ)
        planner_src = str(ROOT / "planner" / "src")
        env["PYTHONPATH"] = planner_src + os.pathsep + env.get("PYTHONPATH", "")
        subprocess.run(
            [
                sys.executable,
                "-m",
                "tpa_planner.map_program",
                "--program",
                str(program),
                "--process-jsons",
                str(process_json),
                "--compute-costs-json",
                str(costs_json),
                "--machine-json",
                str(ROOT / "machines" / machine),
                "--output",
                str(output),
            ],
            check=True,
            env=env,
        )
        return json.loads(output.read_text())


def transport_counts(result: dict) -> collections.Counter[str]:
    return collections.Counter(
        conn["transport_kind"] for conn in result["mapped_program"]["connections"]
    )


class TensorMatmulMappingTests(unittest.TestCase):
    def test_single_minion_forces_direct_channels(self) -> None:
        result = map_matmul(rows=8, cols=8, machine="single-minion.json")
        counts = transport_counts(result)

        self.assertEqual(result["selected"]["num_contexts_used"], 1)
        self.assertEqual(set(counts), {"direct"})

    def test_erbium_spreads_without_fabric(self) -> None:
        result = map_matmul(rows=12, cols=12, machine="erbium.json")
        counts = transport_counts(result)

        self.assertGreater(result["selected"]["num_contexts_used"], 1)
        self.assertLessEqual(result["selected"]["num_contexts_used"], 8)
        self.assertGreater(counts["local"], 0)
        self.assertEqual(counts["fabric"], 0)
        self.assertEqual(counts["external"], 0)

    def test_etsoc1_large_graph_crosses_shires(self) -> None:
        result = map_matmul(rows=12, cols=12, machine="etsoc1.json")
        counts = transport_counts(result)

        self.assertGreater(result["selected"]["num_contexts_used"], 32)
        self.assertGreater(counts["fabric"], 0)
        self.assertEqual(counts["external"], 0)


if __name__ == "__main__":
    unittest.main()
