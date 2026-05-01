from __future__ import annotations

import json
from pathlib import Path

import networkx as nx


def parse_tpp(path: Path) -> tuple[dict[int, str], list[dict]]:
    instances: dict[int, str] = {}
    conns: list[dict] = []

    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split()
        if fields[0] == "inst":
            _, inst_id, pdef_name = fields
            instances[int(inst_id, 10)] = pdef_name
        elif fields[0] == "conn":
            _, src_inst, src_port, dst_inst, dst_port, bytes_ = fields
            conns.append(
                {
                    "conn_idx": len(conns),
                    "src_inst": int(src_inst, 10),
                    "src_port": int(src_port, 10),
                    "dst_inst": int(dst_inst, 10),
                    "dst_port": int(dst_port, 10),
                    "bytes": int(bytes_, 10),
                }
            )
        else:
            raise ValueError(f"unknown tag in {path}: {raw_line}")

    return instances, conns


def parse_place(path: Path) -> dict[int, dict]:
    placements: dict[int, dict] = {}
    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split()
        if fields[0] == "chan":
            continue
        inst_id, hartid = fields
        placements[int(inst_id, 10)] = {
            "hartid": int(hartid, 10),
            "label": f"h{hartid}",
        }
    return placements


def load_process_jsons(
    paths: list[Path],
    required_pdefs: set[str] | None = None,
) -> tuple[dict[str, dict], list[dict]]:
    pdef_to_object: dict[str, dict] = {}
    objects: list[dict] = []

    for path in paths:
        obj = json.loads(path.read_text())
        obj["json_path"] = str(path)
        relevant_pdefs = [
            pdef
            for pdef in obj["pdefs"]
            if required_pdefs is None or pdef["name"] in required_pdefs
        ]
        if not relevant_pdefs:
            continue
        obj = {
            **obj,
            "pdefs": relevant_pdefs,
        }
        objects.append(obj)
        for pdef in obj["pdefs"]:
            if pdef["name"] in pdef_to_object:
                raise ValueError(f"duplicate pdef metadata for {pdef['name']}")
            pdef_to_object[pdef["name"]] = {
                "object": obj,
                "pdef": pdef,
            }

    return pdef_to_object, objects


def build_graph(instances: dict[int, str], conns: list[dict]) -> nx.DiGraph:
    graph = nx.DiGraph()
    for inst_id, pdef_name in instances.items():
        graph.add_node(inst_id, pdef_name=pdef_name)
    for conn in conns:
        graph.add_edge(
            conn["src_inst"],
            conn["dst_inst"],
            src_port=conn["src_port"],
            dst_port=conn["dst_port"],
            bytes=conn["bytes"],
        )
    return graph
