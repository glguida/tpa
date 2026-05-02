# TPA Topology Model

## Purpose

This note defines the hardware-topology and machine-description model used by
TPA mapping and runtime generation.

Its goals are:

- define the topology entities the mapper is allowed to reason about
- separate execution resources from communication resources
- define what a machine description must contain
- make cost-model assumptions explicit

This note is normative for machine modeling.

## Scope

This note is about:

- hardware topology
- schedulable execution contexts
- shared resources
- communication paths
- machine-description fields

It is not a process-model note and not a memory-class note. Those are covered
by:

- [PROCESS_MODEL.md](../theory/PROCESS_MODEL.md)
- [MEMORY.md](MEMORY.md)

## Topology Entities

### Chip

A **chip** is the whole execution target for one mapped program image.

The chip contains:

- one or more clusters
- one or more memory pools or homes
- one or more communication fabrics

### Cluster

A **cluster** is a group of compute and memory resources with relatively cheap
internal communication compared to communication outside the cluster.

A cluster is useful for:

- locality modeling
- edge-buffer pool selection
- communication-cost modeling

### Minion

A **minion** is the primary coarse compute tile used by the current TPA mapper.

A minion may contain:

- one or more harts
- one shared accelerator or VPU
- local memory or local caches

For the current Erbium-style target, the minion is the main schedulable compute
resource for accelerator work.

### Hart

A **hart** is a hardware thread or control thread inside a minion.

A hart is not automatically a schedulable compute context for the mapper.

The machine description must say whether harts inside a minion are:

- independent compute resources
- priority lanes over shared compute resources
- reserved for different classes of work

For the current TPA target, the important rule is:

- `h0` and `h1` must not be treated as two independent accelerator contexts for
  the VPU-backed YOLO kernels, because they share the same compute engine

This means a machine model must distinguish:

- **control identity**
- **compute independence**

They are not the same thing.

### Compute Engine

A **compute engine** is the resource that actually executes a kernel family.

Examples include:

- scalar core
- vector core
- tensor engine / VPU

The mapper should schedule over independent compute engines, not over every
thread label that happens to exist in the machine.

### Scratch Domain

A **scratch domain** is a resource-sharing domain for transient compute memory.

Processes mapped into the same scratch domain may reuse the same physical
scratch backing, subject to overlap constraints.

A scratch domain is a mapping concept derived from topology plus scheduling.

It is not automatically identical to:

- a process
- an edge
- a channel

### Memory Home

A **memory home** is a concrete place where data may live.

Examples:

- cluster-local edge pool
- minion-local backing store
- global immutable store

Edge-buffer planning and communication placement need explicit memory homes.

### Transport Path

A **transport path** is the communication path between producer and consumer
contexts or between memory homes.

The machine model must describe enough about the path to estimate:

- time cost
- locality effect
- whether data must be copied or may be handed off

Transport paths must distinguish same-device and off-device communication. A
same-device path may use a direct handoff, a local memory home, or an internal
device fabric. An off-device path may use PCIe peer-to-peer or another external
fabric. These are not the same locality class.

## Execution Contexts

An **execution context** is a mapper-visible scheduling slot.

The machine description must define which topology entities are legal execution
contexts.

Examples:

- one context per minion
- one context per independent VPU
- one context per scalar core

An execution context should exist only when the mapper is allowed to place
independent work there.

The machine description must not expose fake parallelism. If two labels share
the same compute engine, they must not be modeled as independent throughput
resources for that workload class.

## Communication Model

Communication cost has at least two distinct roles:

1. **time cost**
   - startup latency
   - bandwidth cost
   - possible contention

2. **memory cost**
   - edge-buffer home
   - replication
   - lifetime effects from placement

These two roles must not be collapsed into one scalar by accident.

## Cost Terms

A serious machine model should distinguish at least:

- `startup_cost`
- `bytes_cost`
- `direct_cost`
- `local_cost`
- `fabric_cost`
- `external_cost`
- optional contention or serialization effects

For early heuristics, a simplified scalar may be acceptable. But that should be
understood as a temporary approximation, not as the topology definition itself.

## Current Minimal Machine Hook

The current mapper already supports a minimal machine description hook.

The checked-in machine descriptions are:

- [single-minion.json](../../machines/single-minion.json)
- [erbium.json](../../machines/erbium.json)
- [etsoc1.json](../../machines/etsoc1.json)

They currently encode:

- execution contexts
- direct, local, device, and edge-pool domains
- local, fabric, and external communication costs

The mapper normalizes each context into generic communication domains:

- `direct_domain`
- `local_domain`
- `device_domain`

If a machine file does not provide them, the defaults are:

- one direct domain per minion
- one local domain per `cluster`
- one device domain for the whole machine

A machine file may provide explicit `contexts`, a compact `context_grid`, or
both. `context_grid` expands repeated hardware shapes without putting
architecture-specific topology loops into the mapper.

Channel classes are derived from those domains:

- different device domain -> `external`
- same direct domain -> `direct`
- same local domain -> `local`
- same device domain -> `fabric`

This is useful as a bootstrap model, but it is not yet a serious topology
specification.

It does **not** fully model:

- shared compute engines inside a minion
- scratch domains as first-class objects
- memory-home choices
- startup and bandwidth as separate communication terms
- contention
- per-resource capacities other than the implicit context count

## Required Machine-Description Content

A machine description should ultimately define:

1. **topology**
   - clusters
   - minions
   - harts
   - compute engines

2. **execution contexts**
   - which contexts are schedulable
   - which process classes may run there

3. **shared-resource domains**
   - scratch-sharing domains
   - shared accelerator domains
   - optionally cache/local-memory domains

4. **memory homes**
   - immutable storage
   - scratch pools
   - edge-buffer pools

5. **communication paths**
   - local handoff
   - local-memory-domain transfer
   - same-device fabric transfer
   - external fabric transfer

6. **cost model**
   - startup
   - per-byte slope
   - optional contention terms

7. **capacity model**
   - number of contexts
   - memory sizes
   - optional bandwidth/cumulative limits

## Recommended JSON Shape

The exact schema may evolve, but a serious description should look more like:

```json
{
  "kind": "erbium-like",
  "clusters": [
    {
      "id": 0,
      "minions": [0, 1, 2, 3, 4, 5, 6, 7],
      "edge_pools": ["cluster0_edge"],
      "scratch_pools": ["cluster0_scratch"]
    }
  ],
  "minions": [
    {
      "id": 0,
      "cluster": 0,
      "direct_domain": "minion0",
      "local_domain": "cluster0",
      "device_domain": "card0",
      "harts": ["h0", "h1"],
      "compute_engines": ["vpu0"],
      "schedulable_contexts": ["m0"],
      "scratch_domain": "m0_scratch"
    }
  ],
  "communication": {
    "direct": { "startup": 0.0, "bytes": 0.0 },
    "local": { "startup": 0.1, "bytes": 0.0 },
    "fabric": { "startup": 5.0, "bytes": 0.01 },
    "external": { "startup": 50.0, "bytes": 0.1 }
  }
}
```

The important point is not the exact field names. The important point is that
the schema should describe:

- topology
- schedulable contexts
- sharing domains
- memory homes
- communication cost structure

## Mapper Contract

The mapper consumes the machine description and uses it to decide:

- legal placements
- overlap possibilities
- scratch reuse
- edge-buffer homes
- communication-cost estimates

The mapper must not infer critical topology facts from ad hoc naming.

For example:

- it must not assume that every hart is an independent compute engine
- it must not assume that every minion has private scratch unless stated

Those properties belong in the machine description.

## Invariants

The following invariants should hold:

- a topology label is not automatically a schedulable context
- shared compute engines must be modeled explicitly
- communication time cost and memory cost are distinct concepts
- scratch domains and memory homes must be explicit
- machine descriptions must be stable inputs to the mapper, not implicit
  folklore

## Immediate Documentation Rule

Until a richer schema exists, any machine JSON used by TPA should be treated as:

- a minimal executable approximation
- not a full architectural specification

The architectural specification belongs in this note, and the JSON should be
considered one concrete instance of it.
