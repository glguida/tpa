# Mapping and Optimization for TPA Programs

## Scope

This note describes how mapping should be understood and optimized for a TPA
program.

The focus is not the kernel implementation and not the memory taxonomy by
itself. The focus is the mapper’s job:

- read the process graph and process contracts
- assign processes to execution contexts
- shape concurrency
- control memory pressure
- derive reusable scratch requirements
- choose when to preserve or sacrifice parallelism
- emit the mapped program that runtime will execute

Mapping is therefore treated as a joint:

- scheduling problem
- memory-allocation problem
- locality problem

## Problem Statement

For a fixed process graph, there is no single notion of “the best mapping.”

A mapping that exposes the most useful overlap is often the one with the best
performance. That same mapping is also often the one with the highest peak
memory. A more serialized mapping may fit more easily, but at the cost of lost
overlap and lower machine utilization.

The mapper therefore has to trade among:

- latency
- throughput
- memory fit
- locality
- traffic
- replication cost

The right formulation is:

- maximize performance
- subject to a hard memory budget

Memory is the feasibility constraint. It is not, by itself, the optimization
goal.

## Mapping Inputs

To optimize mapping, the mapper needs more than the graph topology.

At minimum it needs:

- process graph
- process metadata
- channel sizes
- target machine topology
- target memory budget

### Process Metadata

For each process kind:

- persistent process state
- scratch peak
- immutable process-local data
- execution cost estimate
- port information

For richer optimization, it should also know:

- sharding capability
- halo requirements
- tile granularity
- affinity constraints

### Graph Metadata

For each connection:

- source instance
- destination instance
- payload size
- fanout
- whether the data is point-to-point or forked

### Machine Metadata

For the target machine:

- execution contexts
- scratch-sharing domains
- total memory budget
- communication cost between contexts or memory homes
- overlap/concurrency capability

Scratch capacity is not an independent input. It is derived by the mapper from
the processes assigned to each reusable scratch domain.

## Internal Structure of the Mapper

The mapper should be implemented as one optimization pipeline with distinct
subpasses, not as unrelated tools.

The important split is:

1. **outer mapping loop**
   - propose placement and overlap
   - optimize for performance first
2. **inner memory-planning passes**
   - derive scratch capacities
   - derive edge-memory objects and lifetimes
   - allocate reusable communication storage
3. **feedback**
   - if total memory does not fit, report the pressure back to the outer mapper
   - reduce overlap only where that buys enough memory

This is especially important for edge memory. Edge-buffer usage is not an
independent constant. It depends on the candidate mapping.

## Mapper Outputs

The mapper should not stop at producing a placement table.

Its output should be a mapped program description containing:

- process instances
- connections
- placement
- any schedule metadata needed by runtime
- per-context or per-domain scratch capacity
- later, edge-buffer placement and reuse decisions

The runtime should consume this mapped program directly.

In particular, scratch sizing is a mapper result:

- each process kind declares its scratch peak
- the mapper groups processes into reusable scratch domains
- the mapper derives the required scratch capacity for each domain
- the generated program carries those capacities
- the runtime selects the right backing for the running context

This means scratch sizing belongs to mapping, not to hand-maintained build
configuration.

## What Mapping Controls

The mapper does not just choose where a process runs. It also determines:

- which processes may overlap in time
- which processes can reuse scratch
- how large each reusable scratch domain must be
- which edge buffers overlap in lifetime
- how much live channel data exists at once
- whether replication is worth the memory and communication cost

This is why mapping cannot be reduced to static placement alone.

## The Core Tension

The main optimization tension is:

- **more parallelism**
- versus
- **lower peak memory**

### Why Parallelism Increases Memory Pressure

A more parallel mapping tends to increase peak memory in several ways.

First, more processes run at the same time, so more scratch must exist at the
same time.

Second, more branches are active concurrently, so more channel payloads remain
live at the same time.

Third, replication and sharding usually add structural memory cost:

- more process instances
- more edge buffers
- more halo traffic
- more metadata
- more duplicated state

So parallelism does not only improve latency. It also creates more simultaneous
live data.

### Why Serialization Reduces Memory

A more serialized mapping often reduces the amount of memory that is live at one
time.

If two heavy processes are mapped so that they do not overlap, they can reuse
the same scratch backing. If one branch completes before another begins, their
edge buffers can reuse the same storage. If a producer-consumer chain stays
tight, channel lifetimes can be short.

This means a program that does not fit under a highly parallel mapping may fit
under a more serialized one, even when the kernels and graph are unchanged.

The cost is reduced overlap:

- longer latency
- lower throughput
- less machine utilization

The key point is that the mapper is often choosing between:

- keeping heavy independent stages apart so they can run concurrently
- putting those same stages in the same reusable memory domain so they reuse
  scratch and edge storage

That is the real tradeoff. Separation preserves performance. Co-location
preserves memory.

## What “Fit” Actually Means

A fit failure is rarely “one process is too large.”

More often, the problem is that too much memory is live at once. Peak live
memory can come from:

- concurrent scratch users
- overlapping channel payloads
- replicated process state
- replicated edge state
- locality decisions that lengthen edge lifetimes

So the right response to a fit failure is not simply “reduce memory.” It is:

- reduce the amount of simultaneously live memory

That usually means changing schedule and overlap, not only changing data sizes.

## Mapping Responsibilities

A mapper must make deliberate choices about:

- which processes should be colocated to reuse scratch
- which branches should run concurrently
- which branches should be serialized to reduce live edge data
- where producer-consumer locality shortens edge lifetimes
- when replication is worth the extra memory and traffic
- when sharding should be introduced

These are not independent choices. They interact.

For example:

- colocating two processes may improve scratch reuse
- but it may also serialize them and increase critical-path latency

Likewise:

- spreading processes across many contexts may reduce local contention
- but it may also increase edge lifetime and communication cost

## Optimization Objective

The most useful objective is:

- find the highest-performance legal mapping whose total memory footprint fits
  the target memory budget

That means:

- expose useful overlap first
- preserve independent parallel branches where possible
- separate heavy stages when doing so improves latency or throughput
- then test the resulting mapping against the memory budget

If that aggressive mapping fits, it should be kept. Unused memory is wasted
opportunity.

If it does not fit, the mapper should degrade performance in the smallest
possible way:

- serialize some heavy independent stages
- colocate stages so they reuse scratch
- reduce edge overlap
- reduce replication or sharding if necessary

So the optimization is not “minimize memory, then speed it up.” It is:

- maximize performance
- with memory as a hard constraint
- and degrade overlap only when required for feasibility

## Key Analyses for Mapping

The mapper needs several analyses to optimize well.

### 1. Topological and Critical-Path Analysis

The mapper needs:

- legal dependency order
- ASAP / ALAP timing
- critical path
- branch structure

This shows where parallelism exists and where serialization is unavoidable.

### 2. Scratch Co-Residence Analysis

Scratch is reduced when processes that do not need to overlap share the same
execution context.

The mapper therefore needs to compute:

- which processes can reuse scratch
- the peak scratch requirement per mapped context

### 3. Edge Lifetime Analysis

For every channel / edge, the mapper needs:

- producer completion point
- last consumer use
- live interval

Without this, edge storage cannot be reused safely.

### 4. Buffer Reuse / Coloring

Once edge lifetimes are known, the mapper must pack edge buffers so that
non-overlapping edges reuse the same storage.

This is the main optimization for communication memory.

The first implementation should use a schedule-aware edge-memory planner after a
candidate mapping has been proposed. The most appropriate style for TPA is a
Memory Exclusion Graph or interval-based allocator:

- identify edge memory objects
- identify exclusions or overlapping lifetimes
- allocate reusable storage offline

This planner should run after placement and coarse ordering are known, but
before the mapping is accepted as feasible.

### 5. Locality and Traffic Analysis

The mapper must estimate:

- direct/local/fabric/external communication classes
- cost of fanout across memory homes
- whether moving a process shortens or lengthens edge lifetimes

Low traffic is not the only goal, but high traffic can destroy both performance
and memory reuse.

## A Practical Mapping Strategy

The most practical strategy is staged, but performance-led.

### Stage 1: Build the Highest-Performance Coarse Mapping

Start with the coarse graph:

- one instance per major process
- no unnecessary replication
- preserve all useful coarse-grain overlap

Optimize for:

- exposed parallelism
- locality
- critical-path reduction

The natural first-pass algorithm family is performance-oriented DAG scheduling:

- HEFT-like list scheduling
- PEFT-like look-ahead refinements later if needed

The first pass should therefore:

- rank work by criticality
- place work to minimize finish time
- preserve useful overlap

This pass should not aggressively colocate work merely to reuse scratch. That
is a repair decision, not the starting point.

Then evaluate its memory footprint.

### Stage 2: Reduce Overlap Only If Required

If the high-performance coarse mapping does not fit:

- serialize independent heavy stages where the memory savings are largest
- colocate scratch-heavy stages to reuse backing storage
- reduce overlapping channel lifetimes

The goal is to give up as little performance as necessary to make the mapping
feasible.

For the first implementation, a practical repair strategy is:

- start from the performance-first mapping
- evaluate memory fit
- if it does not fit, collapse execution contexts greedily
- at each step, choose the collapse that gives the best memory reduction for the
  smallest performance penalty

This keeps the optimization ordered correctly:

- maximize performance first
- retreat only when memory requires it

For edge memory, the preferred evaluation point is after an **untimed**
candidate schedule or coarse ordering has been decided. This gives the planner:

- enough structure to derive conservative lifetimes
- enough flexibility to still feed pressure information back into mapping

That is the right trade-off for TPA as it stands.

### Stage 3: Replicate or Shard When Beneficial

After the coarse mapping is understood, the mapper may introduce:

- process replication
- spatial sharding
- halo-aware partitioning

These are performance tools, not defaults. They should be applied only when the
extra memory and communication cost is justified by the gain in performance.

## When Mapping Alone Is Not Enough

Sometimes no legal mapping fits the graph as written.

That usually means one of the following is required:

- a better scratch model
- explicit channel buffer reuse
- process refactoring to shorten live ranges
- sharding or tiling
- graph restructuring or fusion

Mapping is powerful, but it is not magic. It can only exploit the degrees of
freedom present in the process and communication model.

## Recommended Design Principle

The mapper should be designed as a **performance-first optimizer under a hard
memory budget**.

That means:

- treat performance as the primary objective
- treat fit as a hard feasibility constraint
- sacrifice overlap only when the mapping is otherwise infeasible

This keeps the optimizer honest in both directions:

- it does not accept slow mappings when faster ones fit
- it does not chase overlap that cannot be afforded

## Conclusion

The key idea is simple:

- mapping determines not only where work runs, but how much memory is live at
  one time

Therefore, mapping must be treated as an optimization over:

- concurrency
- memory overlap
- locality
- replication

and not merely as a placement table.

It is better understood as a program-generation step:

- process compilation defines process contracts
- mapping combines those contracts with the graph and machine model
- the mapper emits the mapped program, including derived memory capacities
- runtime executes that generated program

Relevant references:

- Preesm memory tutorial:
  `https://preesm.github.io/tutos/memory/`
- Pre- and Post-Scheduling Memory Allocation:
  `https://preesm.github.io/assets/tutos/memory/desnos_pre_and_post.pdf`
- SDF3 analysis manual:
  `https://sstuijk.estue.nl/tools/sdf3/manuals/moc/sdf/analyze.html`
- TVM Unified Static Memory Planning:
  `https://discuss.tvm.apache.org/t/rfc-unified-static-memory-planning/10099`
