from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

import networkx as nx

from tpa_planner.common import build_graph, load_process_jsons, parse_tpp
from tpa_planner.edge_plan import plan_edge_buffers


def load_compute_cost_hints(path: Path | None) -> dict[str, float]:
    if path is None:
        return {}
    data = json.loads(path.read_text())
    return {str(name): float(cost) for name, cost in data.items()}


def expand_context_grid(grid: dict) -> list[dict]:
    devices = grid.get("devices", ["device0"])
    local_domains_per_device = int(
        grid.get("local_domains_per_device", grid.get("local_domain_count", 1))
    )
    minions_per_local_domain = int(grid["minions_per_local_domain"])
    harts = grid.get("harts", ["h0"])
    hart_stride = int(grid.get("hart_stride", 2))

    label_format = grid.get("label_format", "m{minion}:{hart}")
    direct_domain_format = grid.get("direct_domain_format", "{device}:minion{minion}")
    local_domain_format = grid.get("local_domain_format", "{device}:local{local_domain}")
    device_domain_format = grid.get("device_domain_format", "{device}")
    edge_pool_format = grid.get("edge_pool_format")

    contexts = []
    minions_per_device = local_domains_per_device * minions_per_local_domain
    for device_index, device in enumerate(devices):
        for local_domain in range(local_domains_per_device):
            for local_minion in range(minions_per_local_domain):
                minion = device_index * minions_per_device
                minion += local_domain * minions_per_local_domain + local_minion
                for hart_position, hart in enumerate(harts):
                    hart = str(hart)
                    hart_index = context_hart_index(hart, hart_position)
                    fields = {
                        "device": device,
                        "device_index": device_index,
                        "local_domain": local_domain,
                        "local_domain_index": local_domain,
                        "local_minion": local_minion,
                        "minion": minion,
                        "hart": hart,
                        "hart_index": hart_index,
                    }
                    ctx = {
                        "minion": minion,
                        "local_minion": local_minion,
                        "hart": hart,
                        "hartid": minion * hart_stride + hart_index,
                        "cluster": local_domain,
                        "label": label_format.format(**fields),
                        "direct_domain": direct_domain_format.format(**fields),
                        "local_domain": local_domain_format.format(**fields),
                        "device_domain": device_domain_format.format(**fields),
                    }
                    if edge_pool_format is not None:
                        ctx["edge_pool"] = edge_pool_format.format(**fields)
                    contexts.append(normalize_context(ctx))
    return contexts


def context_hart_index(hart: str, fallback: int) -> int:
    if hart.startswith("h"):
        return int(hart[1:], 10)
    try:
        return int(hart, 10)
    except ValueError:
        return fallback


def expand_machine_contexts(machine: dict) -> list[dict]:
    contexts = []
    grids = machine.get("context_grid", [])
    if isinstance(grids, dict):
        grids = [grids]
    for grid in grids:
        contexts.extend(expand_context_grid(grid))
    contexts.extend(normalize_context(ctx) for ctx in machine.get("contexts", []))
    return contexts


def make_machine(
    *,
    num_minions: int,
    hart: str,
    machine_json_path: Path | None,
) -> dict:
    if machine_json_path is not None:
        machine = json.loads(machine_json_path.read_text())
        machine.setdefault("comm_cost_default", 0.1)
        machine["contexts"] = expand_machine_contexts(machine)
        if not machine["contexts"]:
            raise ValueError(f"machine description has no contexts: {machine_json_path}")
        return machine

    contexts = [
        normalize_context(
            {
                "minion": m,
                "hart": hart,
                "hartid": context_hartid({"minion": m, "hart": hart}),
                "cluster": 0,
            }
        )
        for m in range(num_minions)
    ]

    return {
        "kind": "single-cluster-default",
        "comm_cost_default": 0.1,
        "contexts": contexts,
    }


def context_hartid(ctx: dict) -> int:
    if "hartid" in ctx:
        return int(ctx["hartid"])
    hart = str(ctx.get("hart", "h0"))
    if hart.startswith("h"):
        local_hart = int(hart[1:], 10)
    else:
        local_hart = int(hart, 10)
    return int(ctx["minion"]) * 2 + local_hart


def normalize_context(ctx: dict) -> dict:
    hartid = context_hartid(ctx)
    ctx["hartid"] = hartid
    ctx["minion"] = int(ctx.get("minion", hartid // 2))
    ctx["hart"] = str(ctx.get("hart", f"h{hartid % 2}"))
    ctx.setdefault("cluster", 0)
    ctx.setdefault("label", f"m{ctx['minion']}:{ctx['hart']}")
    ctx.setdefault("direct_domain", f"minion{ctx['minion']}")
    ctx.setdefault("local_domain", f"cluster{ctx.get('cluster', 0)}")
    ctx.setdefault("device_domain", "device0")
    return ctx


def channel_kind(src_ctx: dict, dst_ctx: dict) -> str:
    if src_ctx["device_domain"] != dst_ctx["device_domain"]:
        return "external"
    if src_ctx["direct_domain"] == dst_ctx["direct_domain"]:
        return "direct"
    if src_ctx["local_domain"] == dst_ctx["local_domain"]:
        return "local"
    return "fabric"


def compute_cost(binding: dict, pdef_name: str, compute_cost_hints: dict[str, float]) -> float:
    if pdef_name in compute_cost_hints:
        return max(1.0, float(compute_cost_hints[pdef_name]))

    if "_src_" in pdef_name:
        return 1.0
    if "_fork" in pdef_name:
        return 1.0
    return 10.0


def comm_cost(src_ctx: dict, dst_ctx: dict, edge_bytes: int, machine: dict) -> float:
    _ = edge_bytes
    kind = channel_kind(src_ctx, dst_ctx)
    if kind == "direct":
        return 0.0
    if kind == "local":
        return float(machine.get("comm_cost_local", machine.get("comm_cost_default", 0.1)))
    if kind == "fabric":
        return float(machine.get("comm_cost_fabric", machine.get("comm_cost_default", 0.1)))
    return float(machine.get("comm_cost_external", machine.get("comm_cost_default", 0.1)))


def build_channel_placement_lines(conns: list[dict], placement: dict[int, dict]) -> list[str]:
    lines = []
    for conn in conns:
        src_ctx = placement[conn["src_inst"]]
        dst_ctx = placement[conn["dst_inst"]]
        lines.append(
            "chan "
            f"{conn['src_inst']} {conn['src_port']} "
            f"{conn['dst_inst']} {conn['dst_port']} "
            f"{channel_kind(src_ctx, dst_ctx)}"
        )
    return lines


def compute_upward_ranks(
    graph: nx.DiGraph,
    bindings: dict[str, dict],
    instances: dict[int, str],
    machine: dict,
    compute_cost_hints: dict[str, float],
) -> tuple[dict[int, float], dict[int, float]]:
    compute_costs: dict[int, float] = {
        inst_id: compute_cost(bindings[instances[inst_id]], instances[inst_id], compute_cost_hints)
        for inst_id in graph.nodes
    }
    ranks: dict[int, float] = {}

    def visit(inst_id: int) -> float:
        if inst_id in ranks:
            return ranks[inst_id]
        succ_ranks = []
        for _, succ, edge in graph.out_edges(inst_id, data=True):
            succ_ranks.append(float(machine.get("comm_cost_default", 0.1)) + visit(succ))
        ranks[inst_id] = compute_costs[inst_id] + (max(succ_ranks) if succ_ranks else 0)
        return ranks[inst_id]

    for inst_id in reversed(list(nx.topological_sort(graph))):
        visit(inst_id)

    return compute_costs, ranks


def find_earliest_slot(intervals: list[tuple[float, float]], ready: float, duration: float) -> float:
    start = ready
    for busy_start, busy_end in intervals:
        if start + duration <= busy_start:
            return start
        if start < busy_end:
            start = busy_end
    return start


def schedule_for_contexts(
    graph: nx.DiGraph,
    bindings: dict[str, dict],
    instances: dict[int, str],
    contexts: list[dict],
    machine: dict,
    compute_cost_hints: dict[str, float],
) -> dict:
    compute_costs, ranks = compute_upward_ranks(
        graph,
        bindings,
        instances,
        machine,
        compute_cost_hints,
    )
    remaining_preds = {inst_id: graph.in_degree(inst_id) for inst_id in graph.nodes}
    ready = {inst_id for inst_id, indeg in remaining_preds.items() if indeg == 0}
    placement: dict[int, dict] = {}
    schedule: dict[int, dict] = {}
    per_ctx_scratch = defaultdict(int)
    per_ctx_instances = defaultdict(list)
    ctx_intervals = {ctx["label"]: [] for ctx in contexts}
    ctx_load = defaultdict(int)
    ctx_task_count = defaultdict(int)
    order: list[int] = []

    while ready:
        inst_id = min(ready, key=lambda node: (-ranks[node], node))
        ready.remove(inst_id)
        order.append(inst_id)
        best = None
        binding = bindings[instances[inst_id]]
        scratch = binding["pdef"]["scratch_peak_bytes"]
        duration = float(compute_costs[inst_id])

        for ctx in contexts:
            label = ctx["label"]
            pred_ready = 0.0
            for pred, _, edge in graph.in_edges(inst_id, data=True):
                pred_finish = schedule[pred]["finish"]
                if placement[pred]["label"] != label:
                    pred_finish += comm_cost(placement[pred], ctx, edge["bytes"], machine)
                if pred_finish > pred_ready:
                    pred_ready = pred_finish

            start = find_earliest_slot(ctx_intervals[label], pred_ready, duration)
            finish = start + duration
            candidate = (
                finish,
                start,
                ctx_load[label],
                ctx_task_count[label],
                ctx["minion"],
                ctx,
            )
            if best is None or candidate < best:
                best = candidate

        assert best is not None
        _, start, _, _, _, chosen_ctx = best
        label = chosen_ctx["label"]
        finish = start + duration
        placement[inst_id] = chosen_ctx
        schedule[inst_id] = {
            "start": start,
            "finish": finish,
            "compute_cost": duration,
            "rank": ranks[inst_id],
        }
        ctx_intervals[label].append((start, finish))
        ctx_intervals[label].sort()
        per_ctx_instances[label].append(inst_id)
        per_ctx_scratch[label] = max(per_ctx_scratch[label], scratch)
        ctx_load[label] += duration
        ctx_task_count[label] += 1

        for _, succ, _ in graph.out_edges(inst_id, data=True):
            remaining_preds[succ] -= 1
            if remaining_preds[succ] == 0:
                ready.add(succ)

    makespan = max((entry["finish"] for entry in schedule.values()), default=0)
    placement_lines = [
        f"{inst_id} {placement[inst_id]['hartid']}"
        for inst_id in sorted(placement)
    ]

    return {
        "num_contexts": len(contexts),
        "num_contexts_used": len(per_ctx_instances),
        "contexts": contexts,
        "makespan_proxy": makespan,
        "placement": placement,
        "placement_lines": placement_lines,
        "schedule": schedule,
        "per_context_scratch_bytes": dict(per_ctx_scratch),
        "per_context_instances": dict(per_ctx_instances),
        "compute_costs": compute_costs,
        "ranks": ranks,
        "order": order,
    }


def summarize_fixed_memory(objects: list[dict]) -> tuple[dict[str, int], list[str]]:
    warnings: list[str] = []
    process_data_legacy_bytes = sum(obj["workspace_bytes"] for obj in objects)
    reclassified_edge_payload_bytes = sum(
        min(int(obj["workspace_bytes"]), int(obj.get("embedded_edge_payload_bytes", 0)))
        for obj in objects
    )
    process_data_bytes = process_data_legacy_bytes - reclassified_edge_payload_bytes
    immutable_bytes = sum(obj["model_blob_bytes"] + obj["other_static_bytes"] for obj in objects)

    for obj in objects:
        if len(obj["pdefs"]) > 1 and obj["workspace_bytes"] > 0:
            warnings.append(
                f"{obj['label']} defines {len(obj['pdefs'])} pdefs and {obj['workspace_bytes']} bytes "
                "of shared mutable static storage; simplified mapping treats this as fixed object cost"
            )
        if obj.get("embedded_edge_payload_bytes", 0) > obj["workspace_bytes"]:
            warnings.append(
                f"{obj['label']} reports more embedded edge payload bytes than mutable workspace bytes; "
                "clamping reclassification to workspace size"
            )

    return {
        "process_data_bytes": process_data_bytes,
        "process_data_legacy_bytes": process_data_legacy_bytes,
        "reclassified_edge_payload_bytes": reclassified_edge_payload_bytes,
        "immutable_bytes": immutable_bytes,
        "fixed_total_bytes": process_data_bytes + immutable_bytes,
    }, warnings


def evaluate_candidate(
    candidate: dict,
    objects: list[dict],
    memory_budget: int | None,
    *,
    conns: list[dict],
    instances: dict[int, str],
) -> dict:
    fixed, warnings = summarize_fixed_memory(objects)
    edge_plan = plan_edge_buffers(
        conns=conns,
        instances=instances,
        placement=candidate["placement"],
        schedule=candidate["schedule"],
    )
    scratch_total = sum(candidate["per_context_scratch_bytes"].values())
    total = fixed["fixed_total_bytes"] + scratch_total + edge_plan["total_bytes"]
    legacy_total = fixed["immutable_bytes"] + fixed["process_data_legacy_bytes"] + scratch_total
    fits = memory_budget is None or total <= memory_budget
    return {
        **candidate,
        "memory": {
            **fixed,
            "scratch_total_bytes": scratch_total,
            "edge_buffer_bytes": edge_plan["total_bytes"],
            "total_bytes": total,
            "legacy_total_bytes": legacy_total,
            "budget_bytes": memory_budget,
            "fits": fits,
        },
        "edge_plan": edge_plan,
        "warnings": [*warnings, *edge_plan["warnings"]],
    }


def build_mapped_program(
    *,
    program_path: Path,
    instances: dict[int, str],
    conns: list[dict],
    bindings: dict[str, dict],
    selected: dict,
    num_minions: int,
    hart: str,
    memory_model: str,
) -> dict:
    placement = selected["placement"]
    schedule = selected["schedule"]

    scratch_domains = []
    for ctx in selected["contexts"]:
        label = ctx["label"]
        if label not in selected["per_context_instances"]:
            continue
        scratch_domains.append(
            {
                "label": label,
                "minion": ctx["minion"],
                "hart": ctx["hart"],
                "hartid": ctx["hartid"],
                "required_scratch_bytes": selected["per_context_scratch_bytes"].get(label, 0),
                "instance_ids": sorted(selected["per_context_instances"][label]),
            }
        )

    mapped_instances = []
    for inst_id in sorted(instances):
        binding = bindings[instances[inst_id]]
        mapped_instances.append(
            {
                "inst_id": inst_id,
                "pdef_name": instances[inst_id],
                "placement": placement[inst_id],
                "scratch_peak_bytes": binding["pdef"]["scratch_peak_bytes"],
                "declared_ws_bytes": binding["pdef"]["declared_ws_bytes"],
                "schedule": schedule[inst_id],
            }
        )

    mapped_connections = []
    conn_edge_ids = selected["edge_plan"].get("connection_edge_ids", {})
    for conn in conns:
        src_ctx = placement[conn["src_inst"]]
        dst_ctx = placement[conn["dst_inst"]]
        transport_kind = channel_kind(src_ctx, dst_ctx)
        edge_id = conn_edge_ids.get(
            str(conn["conn_idx"]),
            f"edge_{conn['src_inst']}_{conn['src_port']}",
        )
        mapped_connections.append(
            {
                **conn,
                "edge_id": edge_id,
                "src_label": src_ctx["label"],
                "dst_label": dst_ctx["label"],
                "transport_kind": transport_kind,
                "is_local": src_ctx["label"] == dst_ctx["label"],
            }
        )

    return {
        "kind": "tpa_mapped_program_v0",
        "program_source": str(program_path),
        "machine": {
            "num_minions": num_minions,
            "hart": hart,
            "num_contexts_used": selected.get("num_contexts_used", 0),
            "topology_model": "domain-based",
        },
        "memory_model": memory_model,
        "scratch_domains": scratch_domains,
        "edge_memory": selected["edge_plan"],
        "instances": mapped_instances,
        "connections": mapped_connections,
        "memory_summary": selected["memory"],
    }


def write_scratch_header(path: Path, *, num_minions: int, selected: dict) -> None:
    def align_up(value: int, align: int) -> int:
        return (value + align - 1) & ~(align - 1)

    contexts = sorted(selected["contexts"], key=lambda ctx: int(ctx["hartid"]))
    if contexts:
        num_minions = max(num_minions, max(int(ctx["minion"]) for ctx in contexts) + 1)

    minion_bytes = {m: 64 for m in range(num_minions)}
    for label, scratch_bytes in selected["per_context_scratch_bytes"].items():
        ctx = next(ctx for ctx in selected["contexts"] if ctx["label"] == label)
        minion_bytes[ctx["minion"]] = max(64, int(scratch_bytes))

    minion_hartids: dict[int, int] = {}
    for ctx in contexts:
        minion = int(ctx["minion"])
        minion_hartids.setdefault(minion, int(ctx["hartid"]))

    storage_offset = 0
    initializers = []
    for minion in range(num_minions):
        bytes_ = align_up(minion_bytes[minion], 64)
        hartid = minion_hartids.get(minion, minion * 2)
        initializers.append(
            f"[{hartid}] = {{ .base = yolov5n_arena_storage + {storage_offset}u, .cap = {bytes_}u }}"
        )
        storage_offset += bytes_

    lines = [
        "#ifndef TPA_GENERATED_SCRATCH_CONFIG_H",
        "#define TPA_GENERATED_SCRATCH_CONFIG_H",
        "",
        "/* Generated by tpa_planner.map_program.py */",
        "",
    ]
    for minion in range(num_minions):
        lines.append(f"#define YV5N_ARENA_M{minion}_BYTES {align_up(minion_bytes[minion], 64)}u")
    lines.extend(
        [
            "",
            f"#define YV5N_ARENA_STORAGE_BYTES {storage_offset}u",
            "#define YV5N_ARENA_STATE_INITIALIZERS \\",
        ]
    )
    for idx, init in enumerate(initializers):
        suffix = " \\" if idx + 1 < len(initializers) else ""
        lines.append(f"    {init},{suffix}")
    lines.extend(
        [
            "",
            "#endif",
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines))


def write_edge_config_header(path: Path, *, mapped_program: dict) -> None:
    edge_memory = mapped_program["edge_memory"]
    pool_syms: dict[str, str] = {}
    lines = [
        "#ifndef TPA_GENERATED_EDGE_CONFIG_H",
        "#define TPA_GENERATED_EDGE_CONFIG_H",
        "",
        "/* Generated by tpa_planner.map_program.py */",
        "",
    ]

    def pool_sym_name(pool_name: str) -> str:
        sanitized = "".join(ch if ch.isalnum() else "_" for ch in pool_name)
        return f"__tpa_edge_pool_{sanitized}"

    for pool in edge_memory["pools"]:
        pool_name = str(pool["pool"])
        pool_sym = pool_sym_name(pool_name)
        pool_syms[pool_name] = pool_sym
        lines.append(
            f"static unsigned char {pool_sym}[{int(pool['bytes'])}] "
            "__attribute__((aligned(64)));"
        )

    if edge_memory["pools"]:
        lines.append("")

    edge_by_id = {
        str(edge["edge_id"]): edge
        for edge in edge_memory["edge_objects"]
    }
    for idx, conn in enumerate(mapped_program["connections"]):
        edge = edge_by_id[str(conn["edge_id"])]
        pool_sym = pool_syms[str(edge["pool"])]
        offset = int(edge["offset"])
        lines.extend(
            [
                f"#define TPA_EDGE_CH_{idx}_NRBUF 1u",
                f"#define TPA_EDGE_CH_{idx}_BUF0 ({pool_sym} + {offset}u)",
                f"#define TPA_EDGE_CH_{idx}_BUF1 0",
                "",
            ]
        )

    lines.extend(
        [
            "#endif",
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines))


def summarize_candidate(candidate: dict) -> dict:
    return {
        "num_contexts": candidate["num_contexts"],
        "num_contexts_used": candidate.get("num_contexts_used", 0),
        "makespan_proxy": candidate["makespan_proxy"],
        "memory": candidate["memory"],
    }


def repair_mapping_for_memory(
    graph: nx.DiGraph,
    conns: list[dict],
    bindings: dict[str, dict],
    instances: dict[int, str],
    initial_candidate: dict,
    objects: list[dict],
    memory_budget: int | None,
    machine: dict,
    compute_cost_hints: dict[str, float],
) -> tuple[dict, list[dict], list[str]]:
    history = [initial_candidate]
    warnings: list[str] = []

    if memory_budget is None or initial_candidate["memory"]["fits"]:
        return initial_candidate, history, warnings

    current = initial_candidate
    active_labels = set(current["per_context_instances"])
    active_contexts = [ctx for ctx in current["contexts"] if ctx["label"] in active_labels]

    while len(active_contexts) > 1 and not current["memory"]["fits"]:
        repairs: list[tuple[tuple, dict, str]] = []

        for removed_ctx in active_contexts:
            remaining_contexts = [
                ctx for ctx in active_contexts if ctx["label"] != removed_ctx["label"]
            ]
            candidate = schedule_for_contexts(
                graph=graph,
                bindings=bindings,
                instances=instances,
                contexts=remaining_contexts,
                machine=machine,
                compute_cost_hints=compute_cost_hints,
            )
            candidate = evaluate_candidate(
                candidate,
                objects,
                memory_budget,
                conns=conns,
                instances=instances,
            )
            candidate["repair_removed_context"] = removed_ctx["label"]

            bytes_saved = current["memory"]["total_bytes"] - candidate["memory"]["total_bytes"]
            makespan_penalty = candidate["makespan_proxy"] - current["makespan_proxy"]
            if bytes_saved <= 0:
                continue

            if candidate["memory"]["fits"]:
                key = (0, makespan_penalty, -bytes_saved, removed_ctx["label"])
            else:
                ratio = makespan_penalty / bytes_saved if makespan_penalty > 0 else 0.0
                key = (1, ratio, makespan_penalty, -bytes_saved, removed_ctx["label"])
            repairs.append((key, candidate, removed_ctx["label"]))

        if not repairs:
            warnings.append("memory repair found no context removal that reduces memory")
            break

        repairs.sort(key=lambda item: item[0])
        _, chosen, removed_label = repairs[0]
        current = chosen
        history.append(current)
        active_labels = set(current["per_context_instances"])
        active_contexts = [ctx for ctx in current["contexts"] if ctx["label"] in active_labels]
        warnings.append(
            f"memory repair removed context {removed_label}; "
            f"makespan {history[-2]['makespan_proxy']} -> {current['makespan_proxy']}, "
            f"memory {history[-2]['memory']['total_bytes']} -> {current['memory']['total_bytes']}"
        )

    return current, history, warnings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--program", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--process-jsons", nargs="+", required=True)
    parser.add_argument("--compute-costs-json")
    parser.add_argument("--machine-json")
    parser.add_argument("--mapped-program-out")
    parser.add_argument("--placement-out")
    parser.add_argument("--scratch-header-out")
    parser.add_argument("--edge-config-header-out")
    parser.add_argument("--num-minions", type=int, default=8)
    parser.add_argument("--hart", default="h0")
    parser.add_argument("--memory-budget-bytes", type=int)
    args = parser.parse_args()

    program_path = Path(args.program)
    output_path = Path(args.output)
    process_json_paths = [Path(path) for path in args.process_jsons]
    compute_cost_hints = load_compute_cost_hints(
        Path(args.compute_costs_json) if args.compute_costs_json else None
    )
    machine = make_machine(
        num_minions=args.num_minions,
        hart=args.hart,
        machine_json_path=Path(args.machine_json) if args.machine_json else None,
    )

    instances, conns = parse_tpp(program_path)
    bindings, objects = load_process_jsons(process_json_paths, required_pdefs=set(instances.values()))
    graph = build_graph(instances, conns)

    if not nx.is_directed_acyclic_graph(graph):
        raise ValueError("simplified mapper requires an acyclic program graph")

    initial_candidate = schedule_for_contexts(
        graph=graph,
        bindings=bindings,
        instances=instances,
        contexts=machine["contexts"],
        machine=machine,
        compute_cost_hints=compute_cost_hints,
    )
    initial_candidate = evaluate_candidate(
        initial_candidate,
        objects,
        args.memory_budget_bytes,
        conns=conns,
        instances=instances,
    )
    selected, repair_history, repair_warnings = repair_mapping_for_memory(
        graph=graph,
        conns=conns,
        bindings=bindings,
        instances=instances,
        initial_candidate=initial_candidate,
        objects=objects,
        memory_budget=args.memory_budget_bytes,
        machine=machine,
        compute_cost_hints=compute_cost_hints,
    )
    channel_placement_lines = build_channel_placement_lines(conns, selected["placement"])
    selected["channel_placement_lines"] = channel_placement_lines
    selected["placement_lines"] = [*selected["placement_lines"], *channel_placement_lines]

    mapped_program = build_mapped_program(
        program_path=program_path,
        instances=instances,
        conns=conns,
        bindings=bindings,
        selected=selected,
        num_minions=max(int(ctx["minion"]) for ctx in machine["contexts"]) + 1,
        hart=args.hart,
        memory_model="edge-planned-v0",
    )

    result = {
        "program": str(program_path),
        "assumptions": {
            "memory_model": "edge-planned-v0",
            "process_data_includes_outputs": False,
            "process_data_reclassifies_embedded_output_buffers": True,
            "edge_buffer_memory_explicit": True,
            "runtime_edge_ownership": True,
            "performance_model": "performance-first-heft-like",
            "repair_model": "greedy-context-collapse-under-memory-budget",
            "communication_model": "topology-domain channel classes: direct/local/fabric/external",
            "compute_cost_model": (
                "explicit pdef hints" if compute_cost_hints else "uniform fallback with tiny source/fork cost"
            ),
        },
        "initial": summarize_candidate(initial_candidate),
        "repair_trace": [summarize_candidate(candidate) for candidate in repair_history],
        "selected": {
            "num_contexts": selected["num_contexts"],
            "num_contexts_used": selected.get("num_contexts_used", 0),
            "makespan_proxy": selected["makespan_proxy"],
            "memory": selected["memory"],
            "placement_lines": selected["placement_lines"],
            "channel_placement_lines": selected["channel_placement_lines"],
            "per_context_scratch_bytes": selected["per_context_scratch_bytes"],
            "per_context_instances": selected["per_context_instances"],
        },
        "instances": [
            {
                "inst_id": inst_id,
                "pdef_name": instances[inst_id],
                "placement": selected["placement"][inst_id],
                **selected["schedule"][inst_id],
            }
            for inst_id in sorted(selected["placement"])
        ],
        "mapped_program": mapped_program,
        "warnings": [
            "initial mapping is performance-first and uses a HEFT-like list scheduling heuristic",
            "memory repair collapses contexts only when required to satisfy the memory budget",
            "mapped-program memory includes a schedule-aware edge-buffer pass with conservative lifetimes",
            "runtime channel ownership now follows the mapped edge-buffer layout for generated programs",
            *repair_warnings,
            *selected["warnings"],
        ],
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")

    if args.placement_out:
        Path(args.placement_out).write_text("\n".join(selected["placement_lines"]) + "\n")
    if args.mapped_program_out:
        mapped_program_out = Path(args.mapped_program_out)
        mapped_program_out.parent.mkdir(parents=True, exist_ok=True)
        mapped_program_out.write_text(json.dumps(mapped_program, indent=2, sort_keys=True) + "\n")
    if args.scratch_header_out:
        write_scratch_header(
            Path(args.scratch_header_out),
            num_minions=max(int(ctx["minion"]) for ctx in machine["contexts"]) + 1,
            selected=selected,
        )
    if args.edge_config_header_out:
        write_edge_config_header(
            Path(args.edge_config_header_out),
            mapped_program=mapped_program,
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
