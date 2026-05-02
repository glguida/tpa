# Edge Buffer Planning for TPA Programs

## Scope

This note describes how inter-process data should be represented and managed in
TPA programs.

The focus is the memory that carries values between processes:

- ownership
- lifetime
- physical placement
- reuse
- interaction with mapping

This note is not about kernel scratch and not about process-private persistent
state. It is specifically about **edge buffers**, meaning the storage used for
values that flow across the program graph.

## Problem Statement

When one process produces data that another process later consumes, the program
needs physical storage for that value. That storage has a lifetime, a size, a
location, and often multiple consumers.

If this storage is treated as “just a producer output buffer,” several problems
appear immediately:

- ownership is unclear
- the same bytes may be counted as both process state and communication data
- buffer lifetime is disconnected from last use
- memory reuse becomes difficult
- mapping decisions cannot reason about communication storage directly

The correct model is therefore:

- the produced value belongs to the **edge**
- the process only computes or consumes it

This note uses the term **edge buffer** for the storage that carries such a
value.

## Edge Buffers as First-Class Objects

An edge buffer represents a value produced on one port and consumed on one or
more downstream ports.

At the program level, it should be modeled independently from both:

- process-private state
- transient scratch

An edge buffer must carry at least:

- producing process instance
- producing port
- consuming process instance or instances
- byte size
- lifetime
- physical memory home

This is true even if an implementation chooses to place the storage inside a
producer object.

Physical storage location does not determine conceptual ownership.

## Ownership Model

The ownership rule should be:

- the process computes a value
- the edge owns the storage that carries that value
- the consumer reads from edge-owned storage
- the storage is released after last consumer use

This is the most important conceptual point in the design.

If an implementation chooses to realize an edge buffer through a producer-local
static array, that is only a storage strategy. It must still be accounted as
**edge memory**, not process memory.

Otherwise the program model becomes inconsistent and memory accounting becomes
incorrect.

## Single-Consumer Edges

The simplest case is one producer and one consumer.

For an edge:

- `P -> Q`

the natural realization is:

- one edge buffer
- producer writes into it
- consumer reads from it
- buffer is released after `Q` has consumed it

This should be treated as the default case.

The key point is that the buffer lifetime is determined by:

- producer completion
- consumer use

and not by the lifetime of the process object that happened to allocate it.

## Fanout Edges

If one produced value has multiple consumers, then logically there is still
only one produced value, but there may be multiple physical realizations.

Two main implementation choices exist.

### Shared Fanout Buffer

One physical edge buffer is shared by all consumers.

Properties:

- no duplication of payload bytes
- simple producer contract
- longer lifetime
- possible fabric/external access by some consumers

This favors memory efficiency.

### Replicated Fanout Buffers

The produced value is copied into more than one physical edge buffer.

Properties:

- more payload storage
- shorter or more localized access paths
- possible reduction in fabric/external traffic
- possible shorter lifetime per replica

This favors locality and sometimes performance.

The mapper therefore needs to decide, per fanout:

- keep one shared edge buffer
- or replicate into multiple edge buffers

This is a real optimization choice, not an implementation detail.

## Lifetime Model

Each edge buffer has a live interval.

The live interval starts when:

- the producer has made the value available

The live interval ends when:

- the last consumer has finished using the value

This definition is simple but crucial.

It means edge storage should not be reclaimed:

- when the producer finishes
- when the first consumer reads
- when the producing process yields

It should be reclaimed only after **last use**.

This is the basis for all reuse analysis.

## Physical Placement

Edge buffers require a physical memory home.

Choosing that home is not the same as choosing where the producer runs.

The memory home should be chosen using:

- producer placement
- consumer placement
- fanout structure
- traffic cost
- memory pressure in each domain

Typical choices include:

- near the producer
- near the consumer
- shared in a common memory domain
- replicated across multiple domains

The important point is that **edge-buffer placement is a planning problem of
its own**.

It is not automatically solved by process placement.

## Relationship to the Mapper

Edge-buffer planning is a distinct subproblem, but it should still be part of
one mapping pipeline.

The reason is simple:

- process placement influences edge-buffer home and lifetime
- edge-buffer pressure influences whether the chosen placement is feasible

So edge-buffer planning should not be a disconnected post-processing step.

The right architecture is:

1. propose a performance-oriented process mapping
2. plan edge buffers and scratch under that mapping
3. check fit and traffic
4. feed infeasibility information back into mapping

This is one mapper with multiple subpasses, not multiple unrelated tools.

## Chosen Planning Strategy

The most appropriate planning strategy for TPA is a **schedule-aware edge-memory
planner** that runs after a candidate mapping has been proposed.

The key point is that edge-memory pressure is not independent from mapping:

- placement changes producer-consumer relationships
- overlap changes edge lifetimes
- fanout and replication choices change both memory and traffic

So edge-buffer planning should be performed for a **fixed candidate mapped
program**, not as a one-time graph-only computation.

For TPA, the right first implementation is:

1. build a performance-oriented candidate mapping
2. derive edge-memory objects from that candidate
3. compute conservative edge lifetimes
4. allocate edge storage to minimize peak communication memory
5. feed the resulting edge-memory cost back into total memory fit

This is close to the Memory Exclusion Graph style used in dataflow toolchains:

- derive memory objects
- derive exclusions from possible simultaneous liveness
- allocate physical storage offline

The first TPA implementation should stay deliberately simple:

- one cluster-level edge-memory pool
- no fanout replication
- shared buffer for fanout by default
- conservative lifetime from producer completion to last consumer completion
- offline reuse allocator only

That is enough to answer the first important question:

- how much memory is saved once outputs stop being counted as permanent
  process-owned storage and become reclaimable edge objects

## Why Edge Buffers Matter for Fit

A program may fail to fit even if process state and scratch are acceptable,
simply because too many edge payloads are live at the same time.

This happens when:

- several branches overlap
- fanout values stay live for a long time
- producer-consumer locality is poor
- replication creates too many simultaneous copies

So edge buffers are not a minor accounting detail. They are often one of the
main contributors to peak memory.

## Required Analyses

Edge-buffer planning requires at least the following analyses.

### 1. Edge Size Analysis

For each edge:

- determine byte size

This comes from the program graph and the process output contract.

### 2. Consumer Analysis

For each edge:

- determine the set of consumers
- determine whether it is single-consumer or fanout

This is required to compute both lifetime and replication options.

### 3. Lifetime Analysis

For each edge:

- determine when the producer makes the value available
- determine the last use among all consumers
- compute the live interval

This is the central analysis for reuse.

### 4. Placement Analysis

For each edge:

- determine candidate memory homes
- estimate access cost
- evaluate shared versus replicated placement

This ties edge-buffer planning back to mapping.

### 5. Buffer Reuse Analysis

Two edge buffers may share physical storage if:

- their live intervals do not overlap
- they are compatible in memory home
- alignment and size constraints allow packing

This is the main path to reducing communication memory.

## Buffer Reuse Problem

Once live intervals are known, edge-buffer planning becomes a packing problem.

The planner must:

- assign physical storage to logical edges
- reuse storage across non-overlapping lifetimes
- respect memory-home constraints
- possibly choose between shared and replicated fanout placement

This can be viewed as:

- interval packing
- coloring
- memory-object allocation with exclusions

That is why edge-buffer planning belongs naturally inside the mapper.

In practice, two equivalent views are useful:

- **interval view**
  - each edge object has a live interval
  - non-overlapping intervals may share storage
- **exclusion-graph view**
  - each edge object is a weighted vertex
  - overlapping or otherwise incompatible objects exclude each other

The exclusion-graph view is particularly useful when additional constraints are
introduced later:

- multiple memory pools
- fanout replication
- alignment classes
- storage-home restrictions
- semantic exclusions between related inputs and outputs

## Interaction with Performance

Edge-buffer decisions are not purely about saving memory.

They also affect performance.

For example:

- a shared fanout buffer may reduce memory but increase fabric/external access
- a replicated fanout buffer may increase memory but reduce traffic
- keeping an edge near a producer may shorten producer write cost
- keeping an edge near consumers may shorten read cost

So edge-buffer planning is one of the places where the mapper must trade:

- memory
- locality
- traffic
- parallelism

## Programming Consequences

A clean process model should not force the process to own output payload storage
permanently.

Instead, the process contract should be:

- describe produced values by port
- write produced values into storage provided according to the edge plan
- commit the value to the communication graph

This allows the same process implementation to be reused under different buffer
plans:

- shared edge buffer
- replicated fanout buffer
- localized edge buffer
- packed or reused edge storage

That is much cleaner than hard-wiring the communication storage into the process
object.

## Recommended Design Principle

Edge buffers should be treated as a first-class memory class and planned
explicitly.

That means:

- do not collapse edge storage into process ownership at the program-model level
- compute edge lifetimes explicitly
- choose buffer homes explicitly
- reuse storage when live ranges do not overlap
- allow fanout replication only when it pays for itself

## Literature Guidance

Several existing systems point in the same direction.

The most directly relevant reference is the Preesm work on memory allocation
for dataflow graphs. It derives a **Memory Exclusion Graph (MEG)** from the
application and shows that memory allocation after an **untimed schedule** gives
a strong trade-off between reduced memory and preserved scheduling flexibility.
That is very close to the required TPA architecture: choose a candidate mapping,
derive edge-memory objects and exclusions, allocate, then feed the result back
into mapping if the program still does not fit.

SDF3 and related SDF literature reinforce the same idea from the static
dataflow side: once rates and execution structure are known, communication
storage can be analyzed offline as part of the graph.

Modern compiler systems reach similar conclusions:

- TVM USMP treats inter-op and intra-op memory planning as one static planning
  problem over known objects and lifetimes
- IREE Stream models resources explicitly with lifetime classes and leaves the
  final allocation decision to a resource-aware planning stage

Relevant references:

- Preesm memory tutorial:
  `https://preesm.github.io/tutos/memory/`
- Pre- and Post-Scheduling Memory Allocation:
  `https://preesm.github.io/assets/tutos/memory/desnos_pre_and_post.pdf`
- SDF3 analysis manual:
  `https://sstuijk.estue.nl/tools/sdf3/manuals/moc/sdf/analyze.html`
- TVM Unified Static Memory Planning:
  `https://discuss.tvm.apache.org/t/rfc-unified-static-memory-planning/10099`
- IREE Stream dialect:
  `https://iree.dev/reference/mlir-dialects/Stream/`

## Conclusion

The central idea is simple:

- an output value is not a process buffer
- it is an edge value with a lifetime

Once that distinction is maintained, the mapper can:

- plan communication storage explicitly
- reuse it after last use
- trade locality against memory
- integrate communication memory into global fit decisions

That is the correct foundation for edge-buffer planning in TPA programs.
