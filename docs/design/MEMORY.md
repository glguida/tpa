# Memory Model and Analysis for TPA Programs

## Scope

This note describes the memory model that should be used to reason about a **TPA
program**.

The goal is to separate:

- memory that belongs to the model
- memory that belongs to a process
- memory that belongs to transient computation
- memory that belongs to communication

This note is intentionally written at the **program and mapping** level. It is
not a kernel-implementation note.

## Problem Statement

For a non-trivial TPA program, total memory use is not determined by weights
alone. It is the combined result of:

- large immutable model blobs
- per-process persistent state
- transient compute scratch
- inter-process communication data

A correct implementation therefore needs more than “make the code fit.” It
needs a memory model that can be analyzed offline and a runtime contract that
can be validated.

## Memory Classes

For a TPA **program**, memory should be divided into four classes.

### 1. Global Immutable Memory

This is the model payload and other program-wide read-only data.

Examples:

- weights
- biases
- scales
- packed model tables
- shared constant LUTs
- read-only metadata tables

Properties:

- lifetime is the whole program
- mapping does not change it
- it is usually the largest static memory class

This class is the natural home for the actual model.

### 2. Scratch

Scratch is transient memory used inside one process firing during computation.

Examples:

- temporary feature maps
- patch buffers
- concat staging
- packing buffers
- accumulation scratch

Properties:

- not persistent
- not sendable
- not valid across a TPA boundary
- reusable across different process firings
- strongly affected by mapping

Scratch is the main memory class that can be reduced by intelligent mapping.
If two heavy processes do not need scratch at the same time on the same mapped
execution context, they should be able to reuse the same scratch backing.

### 3. Process Data

This is memory that belongs to the process implementation itself and is not
transient scratch.

Examples:

- persistent mutable state
- small receive-side state
- process-private flags
- process-private pointers
- small immutable per-process tables

Properties:

- belongs to the process kind or instance
- mostly insensitive to mapping
- replicated if the process is instantiated or sharded

This note avoids the term `ws` except where the existing ABI already uses it.
The useful concept is **persistent process state**, not “workspace.”

### 4. Channel / Edge Data

This is memory required because one process communicates with another.

Examples:

- buffers for channel payloads
- edge storage between producer and consumer
- fabric/external transfer backing where transport requires it

Properties:

- lifetime starts when the producer makes data available
- lifetime ends after the last consumer has used the data
- should be reclaimable and reusable after last use
- depends on graph structure and schedule

Conceptually this class is distinct from process data. A process produces or
consumes channel data, but the data belongs to the **program graph**, not to the
process implementation.

Channel data includes the payload storage that carries outputs from producers to
consumers. It must not be counted a second time as process data.

## Implementation Note

One implementation choice is to realize local channel data through a buffer
physically allocated inside the producer object rather than a distinct
channel-owned buffer.

Under that implementation:

- the receiver observes the sender’s buffer pointer
- local edge storage is physically stored in producer-owned memory

This is an implementation shortcut, not the desired final memory model and not
the correct program-level ownership model.

The desired final model is:

- process outputs are produced into edge-owned storage
- edge storage remains live until last consumer use
- the planner decides when that storage can be reused

If an implementation chooses to place channel payload storage inside a producer
object, those bytes must still be accounted as **channel / edge data**, not as
process data.

## ET-SoC-1 Memory-Home Policy

On ET-SoC-1, the shire L2SCP is a scarce shire-local resource. It should not be
the default home for all process memory.

The default policy should be:

- persistent process state lives in normal cached memory unless explicitly
  mapped elsewhere
- immutable model data and large activation tensors live in cached global memory
  or DDR-backed memory
- shire L2SCP is used for explicit local staging, channel state, bounded edge
  payload windows, and small same-shire edge payloads
- L1SCP is used for tensor-instruction working sets

Persistent process state is not a good default occupant for L2SCP. A shire can
run many process instances, and persistent state would become a permanent tax on
the same limited memory needed for streaming windows and communication.

Scratch should be split into distinct backing classes instead of treated as one
undifferentiated byte count:

- L1SCP tensor working lines
- shire L2SCP staging windows or double buffers
- cached scratch for larger temporary data that does not need deterministic
  shire-local residence

Likewise, edge memory should distinguish logical tensor size from resident
window size. A logical edge may represent a large tensor, while the resident
working set is only a bounded streaming window.

For ports and channels:

- direct edges should use direct handoff where possible
- local edges should use local channel state and, when useful, local payload
  windows
- fabric edges should use a concrete source or destination memory home and cross
  the device fabric
- very large edges should normally be backed by cached global memory and stream
  through bounded local windows

On ET-SoC-1, the local memory home is the shire L2SCP/shared shire-cache
partition and the same-device fabric is the NoC. Those are machine mappings of
the generic classes, not generic TPA names.

The planner should therefore reason about at least:

- process state bytes
- logical edge bytes
- resident edge-window bytes
- L1SCP window bytes
- shire L2SCP window bytes
- cached scratch bytes
- immutable model bytes

## Process Contract

A process should expose three things explicitly.

### Persistent State Size

This is the process-owned persistent mutable state.

It includes:

- receive-side state that survives across continuations
- persistent mutable flags and pointers
- process-private state that is neither scratch nor channel payload

In ELF terms, this is the mutable static footprint of the process object,
after removing any bytes used purely as channel payload backing:

- `.data`
- `.sdata`
- `.bss`
- `.sbss`

### Scratch Peak

This is the maximum transient compute memory needed by one firing of the
process.

It should describe:

- temporary memory used only during computation
- not outputs
- not persistent state
- not immutable model data

### Port / Output Contract

The process definition must say what data it produces and consumes.

At minimum this includes:

- ports
- channel byte sizes from the program graph

For a more complete model it should also expose:

- output payload classes and sizes
- whether a process is tileable
- whether a process needs halo or neighborhood data

## Program Contract

The program graph should describe:

- process instances
- connections
- connection byte sizes
- placement

From the memory-planning point of view, the graph is the source of truth for:

- channel sizes
- dependency order
- fork / fanout structure
- consumer multiplicity
- live ranges of inter-process data

## Operations Required of Each Component

### Process Implementation

A process implementation must:

- declare its persistent state honestly
- declare its scratch requirement honestly
- treat scratch as transient only
- not keep scratch pointers across `recv/send/yield/block`
- publish outputs according to the channel contract
- distinguish process state from channel payload storage in its metadata

### Program Builder / Mapper

The mapper must:

- know the process graph
- know the mapping
- know per-process persistent state and scratch
- know channel payload sizes independently of process state
- compute scratch reuse opportunities
- compute edge/channel live ranges
- allocate edge storage and scratch backing

### Runtime

The runtime must:

- provide the scratch abstraction
- enforce scratch limits
- manage channel transport
- eventually manage edge-storage lifetime according to planner output

## Implemented Mechanisms

An implementation of this model requires at least the following mechanisms.

### Per-Process Metadata

Each process emits a metadata record containing:

- process id
- declared scratch peak

This is compiled into a dedicated ELF section and later extracted by tooling.

### Per-Process Object Inspection

Each process object is post-processed to extract:

- persistent mutable size
- model blob size
- other static size

This provides a real object-level memory inventory instead of relying only on
source declarations.

That object-level inventory is not yet sufficient for full program memory
accounting, because mutable bytes used as channel payload backing must be
reclassified from process data into channel / edge data.

### Program-Level Planning Report

A planner should reconstruct:

- the instance graph
- topological order
- placement
- per-mapped-context scratch peaks
- warnings about shared mutable state across multiple instances

This is a reporting path, not a full optimizer by itself.

### Runtime Scratch Validation

A scratch allocator should validate:

- actual allocation does not exceed the declared process scratch peak
- actual allocation does not exceed the backing storage capacity

This is a useful first-line validator.

## Additional Required Capabilities

The following capabilities are required for a full memory planner:

- first-class channel / edge storage planning
- offline channel buffer reuse
- scratch planning driven automatically by the mapper
- a strong offline checker for scratch declarations

## Required Analyses

The following analyses are needed to make memory planning correct and efficient.

### 1. Process Object Analysis

For each process object:

- measure mutable persistent bytes
- measure immutable bytes
- extract declared scratch peak
- identify mutable bytes used only as channel payload backing
- reclassify those bytes as channel / edge data

This analysis is local to the process object and can be done after compilation.

### 2. Scratch Validation

There are two useful levels.

Runtime validation:

- trap if actual scratch use exceeds the declared peak

Offline validation:

- derive or recompute scratch peak from structured scratch metadata
- compare that value with the declared peak

The runtime check is necessary but not sufficient. A stronger offline check is
preferred.

### 3. Edge Lifetime Analysis

For every channel / edge:

- identify the producing process
- identify the consuming process or processes
- determine the allocation point
- determine the last-use point
- compute the live range

This is the basis for channel buffer reuse.

### 4. Buffer Reuse / Coloring

Once edge live ranges are known, the planner must reuse edge storage across
non-overlapping live ranges.

This is the central optimization for inter-process data.

### 5. Scratch Placement Analysis

Scratch is not global. It should be shared only by processes that can reuse it
under the chosen mapping.

The planner must therefore compute, for each mapped execution context:

- the maximum scratch requirement of processes that can execute there
- or, for a richer model, the peak over overlapping execution intervals

This is where mapping reduces scratch cost.

### 6. Global Memory Accounting

The planner must be able to answer, for the whole program:

- total immutable memory
- total process data
- total required scratch backing
- total required channel / edge storage
- peak memory by memory home or island

Without this, there is no reliable way to determine whether a full mapped model
fits.

## Recommended Direction

The recommended target model is:

- global immutable memory is explicitly separated
- process data is explicitly separated
- scratch is a first-class transient resource
- channel / edge data is a first-class communication resource
- the planner computes scratch and channel buffer reuse offline
- the runtime enforces the resulting contracts

The key design principle is that **mapping should be able to optimize scratch
and channel storage**, while immutable model data and process state are
largely fixed.

## Practical Consequence for Full Models

For large models such as YOLO, fitting the program is not mainly a matter of
shrinking code. It is a matter of:

- avoiding duplicated persistent process data
- minimizing scratch through mapping
- making channel data reclaimable after last use
- separating model blobs, process state, and channel data so each class can be
  reasoned about correctly

That is the memory problem the tooling must solve.
