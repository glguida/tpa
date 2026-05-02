# TPA Runtime on ET

## Scope

This note captures the ET-specific design points for implementing a transputer-like
runtime on:

- **Erbium**
- **ET-SoC-1**

The runtime layer is called **TPA**:

- **TPA = transputer array**

The goal is to preserve the **virtual CSP/occam channel/process model** while mapping
it onto the actual ET machine structure.

## 1. Two layers

The design is easiest to understand as two layers.

### Virtual layer

This is the CSP / occam level:

- processes
- channels
- rendezvous semantics
- composition independent of placement

At this level, a channel is an end-to-end communication object between processes.

### Physical layer

This is the ET machine:

- minions
- harts
- MRAM
- shire L2 scratchpads
- NoC

The programmer should see the virtual channel. The TPA runtime maps it onto ET memory
and transport.

## 2. ET machine structure

### Erbium

- 8 minions
- each minion has `H0 + H1`
- all minions share one tightly-coupled `MRAM`

Erbium behaves like a small single-island machine.

### ET-SoC-1

- 32 shires
- each shire is `4 x 8` minions
- each shire has a shared `4 MiB` `L2` scratchpad
- shires are connected by a NoC in an `X-Y` grid

So the natural physical "memory islands" are:

- Erbium-wide `MRAM`
- one shire-wide `L2`

The NoC is the physical inter-island link.

## 3. TPA node

The transputer-like node on ET is:

- **one minion**

Not `H0` alone. The node is:

- `H0 + H1`

This is the right analogue of the classic transputer's two priority levels.

## 4. Process model

The runtime is **continuation-style**.

A process is not a thread and not a parked C stack. It is a memory-resident object with:

- workspace
- current continuation
- state
- home hart

Important points:

- a process is **pinned to one runtime hart**
- placement is explicit and static
- yield granularity is chosen by the process author, not by the runtime

## 5. Hart mapping

TPA placement uses runtime hart ids.

On ET, the arch implementation interprets those hart ids in terms of the
underlying topology:

- which harts share a minion
- which harts share an island / neighborhood
- how wakeup and cache-maintenance operations reach that hart

## 6. Preemption model

This is ET-specific behavior, not a generic TPA scheduler rule.

When `H1` becomes runnable:

- `H1` pauses/preempts `H0`
- `H1` executes
- `H1` runs until it blocks or finishes
- `H0` then resumes

The TPA runtime treats both `H0` and `H1` as runtime harts. The ET arch layer is
where the preemption/control mechanism belongs.

`H1` should have its own small runtime stack. It should not reuse `H0`'s stack.

## 7. Channel model

The **channel is fundamental**.

A channel is a virtual end-to-end communication object between two processes.

A port is not a transport. A port is a process-interface endpoint. The mapper
decides how a connection between two ports is physically realized for a
particular placement.

For ET, that channel should be implemented as:

- a **single memory-resident rendezvous object**
- with one **canonical home**

The important property is not sender-owned vs receiver-owned. The important property is:

- there is **one channel object**
- it lives in **one place**
- both processes rendezvous through that object

So:

- no duplicated channel state
- no route-visible channel abstraction
- one channel per process-process communication path

## 8. Channel placement and locality

Channel placement is a memory-home decision for the edge object plus a
transport-path decision between the producer and consumer.

The current implementation names the transport classes:

- `direct`
- `local`
- `fabric`
- `external`

- **direct**: direct handoff within one execution context or compute tile
- **local**: channel state and small/bounded payload windows live in a local
  shared-memory domain
- **fabric**: same-device transport to a non-local memory home
- **external**: different-device transport, for example PCIe peer-to-peer

Intermediate topology levels are machine-description details. They matter for
costs, shared resources, tensor cooperation, and memory-home selection, but
they should not be forced into the channel-kind ABI unless they change the
runtime mechanism or chosen memory home.

For same-device non-local communication:

- the same channel model is kept
- the channel is still homed in a concrete memory home
- the other side reaches it through the device fabric

For multi-card communication:

- the process-facing send/recv contract should remain the same
- the edge should be classified as external/network rather than overloading
  same-device `fabric`
- the backend may realize the edge through PCIe peer-to-peer or another
  host/device network path

So the channel abstraction stays the same. Only the memory home and physical
access path change.

## 9. Uniform communication mechanism

This is the core ET design point.

Unlike the classic transputer, which had:

- internal channels in memory
- external channels on serial links

ET can use one more uniform mechanism:

- **memory-backed channels everywhere**

Inside Erbium:

- communication happens through `MRAM`

Inside one shire:

- communication happens through `L2`

Across shires:

- communication still happens through channel/data objects in memory
- the `NoC` is the transport used to access non-local `L2`

This is an ET mapping of the generic `fabric` class. The generic TPA name
should not be `shire` or `NoC`.

Across devices:

- communication should still be exposed as a channel/edge in the TPA program
- the transport is no longer the on-chip NoC
- the edge belongs to an external/network locality class

So the mechanism is still:

- memory rendezvous
- memory transfer

This preserves the transputer idea while fitting ET much better than a fake serial-link
model would.

## 10. Scheduler consequences

Since processes are pinned to runtime harts:

- each hart has one scheduler loop
- each hart has one ready queue
- the runtime consumes the mapped channel kind and does not decode topology
  itself

When a channel or event makes a process runnable:

- the process is requeued on its **home hart**
- non-local wakeup targets that home hart through the arch interface

So the virtual process/channel model is preserved, while ET-specific wakeup and
neighborhood decisions stay behind the arch interface.

## 11. Design summary

The design that emerged is:

- processes are continuation-style and pinned to runtime harts
- channel-kind decisions are made by topology-aware mapping/image generation,
  with the arch implementation providing the fallback classification expression
- ET-specific priority/preemption behavior stays in the arch layer
- channels are the fundamental communication object
- each channel is one memory-resident rendezvous object with one canonical home
- inside Erbium and inside a shire, channels are implemented directly in shared memory
- across shires, the same memory channel model is kept, with the `NoC` as transport

This gives ET a transputer-like runtime without forcing ET to imitate the original
serial-link implementation literally.
