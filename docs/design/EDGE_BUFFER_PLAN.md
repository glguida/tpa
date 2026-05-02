# Edge Buffer Implementation Plan

## Purpose

This note translates the edge-buffer design into concrete implementation work.

The target is not a perfect final communication system. The target is a staged
transition from the current temporary model:

- producer-owned persistent output buffers

to the intended model:

- edge-owned storage planned offline by the mapper

## Selected Strategy

The selected strategy is:

- keep the existing mapper as the outer performance-first loop
- add a schedule-aware edge-buffer planning subpass
- compute edge memory for a fixed candidate mapped program
- feed that edge-memory result back into total memory fit

The first implementation will use a **MEG-style** or interval-based allocator:

- each edge value becomes a logical memory object
- each object has a size and live interval
- objects that can be live together exclude each other
- non-overlapping objects may reuse the same physical storage

## First-Iteration Assumptions

The first edge-buffer implementation should stay deliberately conservative.

Assumptions:

- one cluster-level edge-memory pool
- inter-minion communication cost remains a machine parameter, but does not
  drive the first allocation strategy on the current machine
- no fanout replication
- one shared edge buffer for fanout by default
- conservative lifetime:
  - birth at producer completion
  - death at last consumer completion
- offline allocation only

These assumptions are not the final system. They are the smallest step that
removes the current ownership error and gives meaningful memory numbers.

## Phase 1: Planner-Only Edge Accounting

The first phase should not change runtime semantics.

The goal is to let the planner answer:

- how much memory is needed for edge data under a candidate mapping
- how much of the current producer-owned output storage could, in principle, be
  reused

### Phase 1 Inputs

- mapped-program JSON
- process metadata
- graph edges and edge sizes
- candidate placement
- candidate schedule order or context-local execution order

### Phase 1 Outputs

- edge object table
- edge live intervals
- exclusion graph or equivalent interval set
- allocated edge pool size
- per-edge assigned offset in the chosen pool
- report comparing:
  - current process-owned output bytes
  - planned edge-memory bytes

### Required Code

Add a new planner pass, for example:

- `planner/src/tpa_planner/edge_plan.py`

and supporting utilities in:

- `planner/src/tpa_planner/common.py`

The pass should consume the mapped-program JSON emitted by `map_program.py`.

## Phase 2: Mapped-Program Edge Allocation Output

Once the planner can allocate edge memory offline, its result must become part
of the mapped program.

The mapped-program JSON should gain:

- edge-memory pools
- per-edge allocation records
- pool offsets
- lifetime summary
- fanout mode

Example fields:

- `edge_objects`
- `edge_pools`
- `edge_allocations`
- `edge_memory_bytes`

At this stage, the runtime may still ignore these records. The important point
is that the mapped program becomes the source of truth for edge ownership.

## Phase 3: Runtime Interface for Edge-Owned Outputs

After the planner can emit edge allocations, runtime and process code can begin
to consume them.

The required change is:

- producer no longer owns the final payload buffer permanently
- producer obtains storage chosen by the communication plan
- producer writes into that storage
- consumer reads from that storage

This likely requires a runtime API of the form:

- get output storage for `(instance, port)` or for a resolved edge object
- publish/commit produced value
- retain/release edge ownership according to consumer progress

The exact API can be designed later, but the ownership direction must be:

- process uses edge storage
- process does not own edge storage

## Phase 4: Remove Producer-Owned Output Accounting

Only after runtime can actually use edge-owned storage should output payload
bytes be removed from process persistent memory accounting.

That transition should be explicit:

1. planner-only accounting
2. mapped-program edge allocation
3. runtime consumes edge allocation
4. process output buffers disappear or shrink
5. process-memory accounting is updated accordingly

This avoids mixing conceptual cleanup with runtime changes too early.

## Data Model

For the first implementation, each logical edge object should contain:

- edge id
- producer instance id
- producer port id
- consumer instance ids
- payload size in bytes
- memory pool id
- birth event
- death event
- fanout mode

Optional fields for later:

- alignment
- pool affinity
- replicated copies
- versioning or multi-buffering mode

## Lifetime Computation

The first lifetime model should be conservative and simple.

Birth:

- producer completion event

Death:

- maximum completion event among all consumers

This model is intentionally pessimistic, but safe.

It should be implemented before attempting:

- partial-consumption lifetimes
- streaming consumption inside a process firing
- earlier release triggered by first or intermediate consumers

## Allocation Algorithm

The first allocator should be simple and deterministic.

Recommended first allocator:

- sort edge objects by decreasing size
- allocate with First Fit into the chosen pool
- permit reuse only across non-overlapping live intervals

Equivalent alternatives such as Best Fit are reasonable, but First Fit with a
stable ordering is enough to establish the framework.

The important point is not the allocator sophistication. The important point is
that edge objects become explicit and reusable.

## Validation

Validation should happen in stages.

### Planner Validation

- confirm every graph edge has an edge object
- confirm sizes match the program graph
- confirm lifetimes are well-formed
- confirm allocated offsets do not violate exclusions

### Accounting Validation

- compare planned edge memory against current output-buffer bytes
- check that the new total memory model is:
  - immutable
  - process data
  - scratch
  - edge memory

### Runtime Validation

When runtime support exists:

- confirm producers write into planned edge storage
- confirm consumers read the same storage
- confirm release occurs only after last use

## Immediate Coding Steps

The next concrete implementation sequence should be:

1. add edge objects to the mapped-program JSON
2. implement conservative edge lifetime analysis
3. implement a first offline edge allocator
4. emit edge-memory reports from the planner
5. plug edge-memory bytes into total memory fit
6. compare planned edge memory against current producer-owned output storage
7. only then start runtime changes

## Deferred Work

The following should remain out of scope for the first edge-buffer pass:

- fanout replication
- multi-cluster pool selection
- locality-driven homing
- timed-schedule refinement
- dynamic reference counting
- sharded edge planning

Those are valid later extensions, but they should not block the first planner
implementation.
