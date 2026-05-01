from __future__ import annotations

from collections import defaultdict

import networkx as nx


EDGE_ALIGNMENT_BYTES = 64


def _is_forwarder_pdef(pdef_name: str) -> bool:
    return "fork" in pdef_name.lower()


def _align_up(value: int, alignment: int) -> int:
    if alignment <= 1:
        return value
    return ((value + alignment - 1) // alignment) * alignment


def _storage_overlap(a_off: int, a_size: int, b_off: int, b_size: int) -> bool:
    return not (a_off + a_size <= b_off or b_off + b_size <= a_off)


def build_edge_objects(
    *,
    conns: list[dict],
    instances: dict[int, str],
    placement: dict[int, dict],
    schedule: dict[int, dict],
) -> tuple[list[dict], dict[int, str], list[dict], list[str]]:
    warnings: list[str] = []
    grouped: dict[tuple[int, int], list[dict]] = defaultdict(list)
    conn_edge_ids: dict[int, str] = {}
    alias_rows: list[dict] = []

    incoming_by_inst: dict[int, list[dict]] = defaultdict(list)
    for conn in conns:
        incoming_by_inst[int(conn["dst_inst"])].append(conn)

    source_alias_root: dict[tuple[int, int], tuple[int, int]] = {}

    def resolve_root(source_key: tuple[int, int]) -> tuple[int, int]:
        cached = source_alias_root.get(source_key)
        if cached is not None:
            return cached

        inst_id, _ = source_key
        pdef_name = instances[inst_id]
        incoming = incoming_by_inst.get(inst_id, [])

        if not _is_forwarder_pdef(pdef_name):
            source_alias_root[source_key] = source_key
            return source_key

        if len(incoming) != 1:
            warnings.append(
                f"forwarder {inst_id} ({pdef_name}) has {len(incoming)} inputs; "
                "treating its outputs as distinct edge storage"
            )
            source_alias_root[source_key] = source_key
            return source_key

        parent = incoming[0]
        root = resolve_root((int(parent["src_inst"]), int(parent["src_port"])))
        source_alias_root[source_key] = root
        if root != source_key:
            alias_rows.append(
                {
                    "source_inst": inst_id,
                    "source_port": int(source_key[1]),
                    "root_inst": int(root[0]),
                    "root_port": int(root[1]),
                }
            )
        return root

    for conn in conns:
        source_key = (int(conn["src_inst"]), int(conn["src_port"]))
        root_key = resolve_root(source_key)
        grouped[root_key].append(conn)
        conn_edge_ids[int(conn["conn_idx"])] = f"edge_{root_key[0]}_{root_key[1]}"

    edge_objects: list[dict] = []
    for src_inst, src_port in sorted(grouped):
        members = grouped[(src_inst, src_port)]
        sizes = {conn["bytes"] for conn in members}
        if len(sizes) != 1:
            warnings.append(
                f"source port {src_inst}:{src_port} fans out with differing payload sizes; "
                f"using max size {max(sizes)} for shared edge allocation"
            )
        payload_bytes = max(sizes)
        src_ctx = placement[src_inst]
        pool_label = str(
            src_ctx.get(
                "edge_pool",
                src_ctx.get("local_domain", f"cluster{src_ctx.get('cluster', 0)}"),
            )
        )
        birth = float(schedule[src_inst]["start"])
        death = max(float(schedule[conn["dst_inst"]]["finish"]) for conn in members)

        consumers = [
            {
                "dst_inst": conn["dst_inst"],
                "dst_port": conn["dst_port"],
                "dst_pdef_name": instances[conn["dst_inst"]],
                "bytes": conn["bytes"],
            }
            for conn in sorted(members, key=lambda item: (item["dst_inst"], item["dst_port"]))
        ]

        edge_objects.append(
            {
                "edge_id": f"edge_{src_inst}_{src_port}",
                "producer": {
                    "inst_id": src_inst,
                    "pdef_name": instances[src_inst],
                    "port": src_port,
                },
                "producer_inst_id": src_inst,
                "consumer_inst_ids": [conn["dst_inst"] for conn in members],
                "consumers": consumers,
                "bytes": payload_bytes,
                "alignment_bytes": EDGE_ALIGNMENT_BYTES,
                "birth": birth,
                "death": death,
                "fanout_kind": "shared",
                "pool": pool_label,
            }
        )

    return edge_objects, conn_edge_ids, alias_rows, warnings


def _build_reachability(conns: list[dict], instances: dict[int, str]) -> dict[int, set[int]]:
    graph = nx.DiGraph()
    for inst_id in instances:
        graph.add_node(inst_id)
    for conn in conns:
        graph.add_edge(conn["src_inst"], conn["dst_inst"])
    return {
        inst_id: set(nx.descendants(graph, inst_id))
        for inst_id in graph.nodes
    }


def _edges_are_causally_disjoint(a: dict, b: dict, reachability: dict[int, set[int]]) -> bool:
    a_after_b = all(
        a["producer_inst_id"] in reachability.get(consumer, set())
        for consumer in b["consumer_inst_ids"]
    )
    if a_after_b:
        return True

    b_after_a = all(
        b["producer_inst_id"] in reachability.get(consumer, set())
        for consumer in a["consumer_inst_ids"]
    )
    return b_after_a


def allocate_edge_objects(
    edge_objects: list[dict],
    *,
    reachability: dict[int, set[int]],
) -> tuple[list[dict], list[dict], list[str]]:
    warnings: list[str] = []
    by_pool: dict[str, list[dict]] = defaultdict(list)
    for obj in edge_objects:
        by_pool[obj["pool"]].append(obj)

    allocated_objects: list[dict] = []
    pool_rows: list[dict] = []

    for pool_label in sorted(by_pool):
        pool_objects = sorted(
            by_pool[pool_label],
            key=lambda obj: (-int(obj["bytes"]), obj["birth"], obj["edge_id"]),
        )
        placed: list[dict] = []

        for obj in pool_objects:
            offset = 0
            while True:
                offset = _align_up(offset, int(obj["alignment_bytes"]))
                conflict = None
                for other in sorted(placed, key=lambda item: item["offset"]):
                    if not _storage_overlap(
                        offset,
                        int(obj["bytes"]),
                        other["offset"],
                        int(other["bytes"]),
                    ):
                        continue
                    if _edges_are_causally_disjoint(obj, other, reachability):
                        continue
                    conflict = other
                    break
                if conflict is None:
                    break
                offset = conflict["offset"] + int(conflict["bytes"])

            allocated = {
                **obj,
                "offset": offset,
                "end_offset": offset + int(obj["bytes"]),
            }
            placed.append(allocated)
            allocated_objects.append(allocated)

        pool_bytes = max((obj["end_offset"] for obj in placed), default=0)
        pool_rows.append(
            {
                "pool": pool_label,
                "bytes": pool_bytes,
                "alignment_bytes": EDGE_ALIGNMENT_BYTES,
                "edge_count": len(placed),
            }
        )

    return allocated_objects, pool_rows, warnings


def plan_edge_buffers(
    *,
    conns: list[dict],
    instances: dict[int, str],
    placement: dict[int, dict],
    schedule: dict[int, dict],
) -> dict:
    edge_objects, conn_edge_ids, alias_rows, build_warnings = build_edge_objects(
        conns=conns,
        instances=instances,
        placement=placement,
        schedule=schedule,
    )
    reachability = _build_reachability(conns, instances)
    allocated_objects, pools, alloc_warnings = allocate_edge_objects(
        edge_objects,
        reachability=reachability,
    )
    total_bytes = sum(pool["bytes"] for pool in pools)

    return {
        "kind": "edge_plan_v0",
        "model": {
            "pooling": "single-shared-buffer-per-root-edge",
            "fanout_replication": False,
            "lifetime": "producer-start-to-last-consumer-finish",
            "allocator": "first-fit-largest-first-causal",
            "alignment_bytes": EDGE_ALIGNMENT_BYTES,
            "forwarder_aliasing": True,
        },
        "edge_objects": allocated_objects,
        "connection_edge_ids": {str(k): v for k, v in sorted(conn_edge_ids.items())},
        "aliases": alias_rows,
        "pools": pools,
        "total_bytes": total_bytes,
        "warnings": [*build_warnings, *alloc_warnings],
    }
