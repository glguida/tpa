from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

import networkx as nx

from tpa_planner.common import build_graph, load_process_jsons, parse_place, parse_tpp


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--program", required=True)
    parser.add_argument("--placement", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--process-jsons", nargs="+", required=True)
    args = parser.parse_args()

    program_path = Path(args.program)
    placement_path = Path(args.placement)
    output_path = Path(args.output)
    process_json_paths = [Path(path) for path in args.process_jsons]

    pdef_to_object, objects = load_process_jsons(process_json_paths)
    instances, conns = parse_tpp(program_path)
    placements = parse_place(placement_path)
    graph = build_graph(instances, conns)

    topo = list(nx.topological_sort(graph))
    topo_index = {inst_id: idx for idx, inst_id in enumerate(topo)}
    generations = [list(gen) for gen in nx.topological_generations(graph)]

    instance_rows: list[dict] = []
    object_instance_count = defaultdict(int)
    per_hart = defaultdict(lambda: {"instances": [], "scratch_peak_bytes": 0, "declared_ws_bytes": 0})
    warnings: list[str] = []

    for inst_id in topo:
        pdef_name = instances[inst_id]
        binding = pdef_to_object[pdef_name]
        obj = binding["object"]
        pdef = binding["pdef"]
        placement = placements.get(inst_id)
        object_instance_count[obj["label"]] += 1

        row = {
            "inst_id": inst_id,
            "pdef_name": pdef_name,
            "object_label": obj["label"],
            "placement": placement,
            "declared_ws_bytes": pdef["declared_ws_bytes"],
            "scratch_peak_bytes": pdef["scratch_peak_bytes"],
            "topological_index": topo_index[inst_id],
            "incoming_edges": [],
            "outgoing_edges": [],
        }

        for src, _, edge in graph.in_edges(inst_id, data=True):
            row["incoming_edges"].append(
                {
                    "from": src,
                    "src_port": edge["src_port"],
                    "dst_port": edge["dst_port"],
                    "bytes": edge["bytes"],
                    "live_from": topo_index[src],
                    "live_until": topo_index[inst_id],
                }
            )
        for _, dst, edge in graph.out_edges(inst_id, data=True):
            row["outgoing_edges"].append(
                {
                    "to": dst,
                    "src_port": edge["src_port"],
                    "dst_port": edge["dst_port"],
                    "bytes": edge["bytes"],
                    "live_from": topo_index[inst_id],
                    "live_until": topo_index[dst],
                }
            )

        instance_rows.append(row)

        if placement is not None:
            hart = per_hart[placement["label"]]
            hart["instances"].append(inst_id)
            hart["scratch_peak_bytes"] = max(hart["scratch_peak_bytes"], pdef["scratch_peak_bytes"])
            hart["declared_ws_bytes"] += pdef["declared_ws_bytes"]

    for obj in objects:
        count = object_instance_count[obj["label"]]
        if count > 1 and obj["workspace_bytes"] > 0:
            warnings.append(
                f"{obj['label']} has {count} graph instances but {obj['workspace_bytes']} bytes "
                "of shared mutable static storage"
            )

    result = {
        "program": str(program_path),
        "placement": str(placement_path),
        "topological_order": topo,
        "topological_generations": generations,
        "process_objects": [
            {
                "label": obj["label"],
                "workspace_bytes": obj["workspace_bytes"],
                "model_blob_bytes": obj["model_blob_bytes"],
                "other_static_bytes": obj["other_static_bytes"],
                "pdefs": [pdef["name"] for pdef in obj["pdefs"]],
                "instance_count": object_instance_count[obj["label"]],
            }
            for obj in objects
        ],
        "instances": instance_rows,
        "per_hart": per_hart,
        "warnings": warnings,
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
