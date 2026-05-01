# Memory and Edge Buffers

TPA mapping only makes sense when memory categories are kept separate. This
document defines the memory taxonomy used by the current planner/mapper and
explains how edge-buffer planning works today.

## Memory taxonomy

### Persistent process state

Persistent process state is per-instance state that must survive between TPA
continuations. It includes the workspace declared in `.tpm` and any mutable
object storage that the process implementation exposes through compiled object
metadata.

In manifests, the declared workspace size is the last field of a `pdef` line:

```text
pdef demo_stage_pdef user 202 demo_stage_recv 24
```

Here the declared persistent workspace for that process kind is 24 bytes.

Do not count channel payloads as persistent process state at the program-model
level. If old or transitional code embeds output buffers in process objects, the
metadata extractor may reclassify known symbols as edge payload bytes.

### Scratch

Scratch is transient compute workspace. It is needed while a process runs, but
it is not persistent process state and should not be used to communicate between
processes.

The extractor records `scratch_peak_bytes` from `.tpa.proc.meta` records when
process objects provide them. The mapper takes the maximum scratch peak for all
instances assigned to a context and reports per-context scratch requirements.

Generated scratch configuration is currently represented by arena macros and
initializers. In the YOLO path, `tpa-map-program --scratch-header-out` writes a
header containing macros such as:

```c
#define YV5N_ARENA_M0_BYTES ...
#define YV5N_ARENA_STORAGE_BYTES ...
#define YV5N_ARENA_STATE_INITIALIZERS ...
```

The current writer aligns arena sizes to 64 bytes and indexes initializers by
runtime hart id.

### Immutable model data

Immutable model data is static read-only or model/blob storage, such as weights.
The current extractor classifies `.mram_data` as model blob bytes and also
accounts for other relevant read-only/TPA metadata sections.

Immutable model data contributes to total memory pressure but is not scratch and
is not edge/channel storage.

### Edge/channel data

Edge/channel data is payload storage for connections between process instances.
A `.tpp` `conn` declares the byte capacity for a connection:

```text
conn 202 1 203 0 8
```

The mapper treats this as edge memory. Edge storage is associated with a
producer port/root edge and with a memory pool determined from mapped placement.

A channel is the runtime object used by process code. An edge is the graph-level
connection that needs payload storage. In generated mapped programs, channel
configuration can point at planned edge storage.

## Scratch domains

A scratch domain is the mapper's grouping for scratch arena requirements. In the
current implementation, the mapped program records a scratch domain for each
used context:

- label;
- minion;
- hart;
- runtime hart id;
- required scratch bytes;
- sorted instance ids assigned to that context.

The generated scratch header then converts these requirements into minion arena
macros and state initializers. The current YOLO-oriented header format is not a
generic permanent ABI; it is the currently integrated representation.

## Edge-buffer ownership

The current edge planner builds one logical edge object per root producer port.
For each connection, it assigns a connection index to an edge id such as:

```text
edge_<producer_inst>_<producer_port>
```

Fanout is represented as shared edge storage when all consumers share the same
root producer/port. Forwarder-like process definitions may alias their outgoing
edges back to the incoming root edge when the forwarder has exactly one input.
If a forwarder has an unsupported shape, the planner emits warnings and treats
outputs as distinct storage.

The producer's mapped context chooses the edge pool. If a machine context has an
`edge_pool`, that value is used; otherwise the local domain or cluster fallback
is used.

## Edge-buffer lifetime

The current model uses conservative schedule-derived lifetimes:

- birth: producer schedule start;
- death: last consumer schedule finish;
- fanout kind: shared;
- alignment: 64 bytes.

The allocator groups edge objects by pool and places them with a
first-fit-largest-first-causal strategy. Two edge objects may reuse storage when
causal reachability proves their lifetimes cannot overlap. Otherwise their
storage ranges must not overlap.

This is intentionally conservative. It is schedule-aware, but it is not a full
runtime proof system for all possible dynamic behavior.

## Edge config header

`tpa-map-program --edge-config-header-out` writes a generated header with:

- static pool arrays, one per used edge pool;
- `TPA_EDGE_CH_<idx>_NRBUF`;
- `TPA_EDGE_CH_<idx>_BUF0`;
- `TPA_EDGE_CH_<idx>_BUF1`.

`BUF0` points into the selected pool array at the planned offset. This header is
passed to image generation for mapped programs that use explicit edge storage.

## Memory pressure and mapping

`tpa-map-program` evaluates total memory as:

```text
fixed process/model bytes
+ scratch total bytes
+ edge buffer bytes
```

The report also retains a legacy total that includes older process-data
classification. The selected memory model is currently `edge-planned-v0`.

If no memory budget is provided, mapping remains performance-first. If
`--memory-budget-bytes` is provided and the initial mapping exceeds the budget,
the mapper runs greedy memory repair:

1. Consider removing one active context at a time.
2. Re-schedule on the remaining active contexts.
3. Keep repairs that reduce total memory.
4. Prefer mappings that fit the budget.
5. Otherwise prefer lower makespan penalty per byte saved.
6. Stop when the selected mapping fits or no useful context removal remains.

This design makes memory a hard constraint without turning the mapper into a
low-memory-first scheduler.

## Generated mapped-program memory summary

The mapped-program JSON includes:

- `scratch_domains`;
- `edge_memory` with edge objects, aliases, pools, total bytes, and warnings;
- mapped instances with declared workspace and scratch peak;
- mapped connections with edge ids and transport kind;
- `memory_summary` with process data, immutable bytes, scratch total, edge
  buffer bytes, total bytes, optional budget, and fit result.

Review this JSON when a mapping changes. It is usually more informative than the
plain generated `.place` file.

## Practical rules

For program authors and reviewers:

- Keep persistent process state, scratch, immutable model data, and edge/channel
  data separate in design and docs.
- Do not hide output tensors in process-owned mutable globals unless there is an
  explicit transition plan to reclassify them as edge payloads.
- Use mapper output for large graphs where scratch domains, edge pools, and
  memory pressure matter.
- Review generated mapped-program JSON, not only final ELF success.
- Treat host smoke-test-double builds as syntax/unit smoke only, not memory or
  platform validation.

## Current limitations

The current edge-buffer path is real but still evolving:

- edge buffers are not yet first-class in every old runtime/test path;
- lifetime analysis is conservative and schedule-derived;
- fanout is shared and forwarder aliasing is heuristic;
- compute costs and memory metadata need broader coverage;
- original message/channel, queue, and negative test assets are ported as
  structured ELF targets, but full behavioral validation still depends on the
  cooperative scheduler follow-up;
- YOLO block CTest wiring is still missing/follow-up;
- old YOLO regeneration/model tools are not ported;
- the full cooperative runtime scheduler remains separate follow-up work.

See `docs/MISSING_ORIGINAL_ARTIFACTS.md` for the broader missing-artifact
inventory.
