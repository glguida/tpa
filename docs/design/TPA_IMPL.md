# TPA Practical Plan

## Scope

This file is the practical checklist for implementing the new TPA runtime from
scratch.

It is not the architectural note. That is in [`TPA.md`](../theory/TPA.md).

## Ground rules

- minimal C
- short names
- no camel case
- no bloated type names
- runtime-facing hardware helpers use `arch_*`
- platform-private helpers use platform-specific names
- runtime helpers use `tpa_*`

## Semantic target

We want a **transputer-like runtime** on ET.

That means:

- continuation-style processes only
- channels are fundamental
- synchronous rendezvous
- blocked processes consume no CPU
- process semantics stay separate from physical placement

## ET mapping we agreed on

- one minion = one TPA node
- a node is `H0 + H1`
- `H0` = low priority
- `H1` = high priority
- `H1` preempts / pauses `H0`

Priority is fixed per process:

- low priority = compute / long work
- high priority = RT / control / service / debug

## Physical mapping we agreed on

Virtual layer:

- processes
- channels
- rendezvous semantics

Physical layer:

- Erbium `MRAM`
- shire `L2`
- NoC between shires

So:

- channels are memory-backed objects
- inside Erbium: channel state/data in `MRAM`
- inside a shire: channel state/data in `L2`
- across shires: same memory-backed channel model, reached through the `NoC`

We are **not** modeling fake serial links on ET.

## What we keep from the current code

Only the ET substrate:

- hart bring-up
- `fcc0` wakeups
- cache flush / evict helpers
- `H1 -> H0` disable / enable path

## What we do not keep

We are **not** building TPA as a library on top of the current `xpu` runtime.

We are **not** reusing as semantics:

- `wq_t`
- SPSC work queues
- `chan_send` / `chan_recv` in `xpu/csp.h`
- `TensorSend` / `TensorRecv`

Those belong to the old active-endpoint runtime.

## Process model

A process is a memory object, not a parked C stack.

Minimum process state:

- workspace pointer
- current continuation
- home hart
- state
- scheduler link

Initial process states:

- `dead`
- `ready`
- `run`
- later: `wait_send`
- later: `wait_recv`
- later: `wait_alt`

What belongs to a process:

- workspace
- continuation state
- local ports

What does **not** belong to a process:

- the full channel graph of the program

A process exposes endpoints.
A program connects those endpoints.

First concrete manifest shapes:

- `struct tpa_port`
- `struct tpa_pdef`
- `struct tpa_inst`
- `struct tpa_end`
- `struct tpa_conn`
- `struct tpa_prog`

## Image model

We do **not** want a hand-written `tpa_dispatch()` to instantiate every
process manually.

The kernel image should define processes statically, and the runtime should boot
from that image.

There are three distinct layers:

### 1. Process declaration

Each process object file declares one process kind.

That object file contains:

- continuation functions
- one declaration entry in a special ELF section

For the first cut, we can also keep that declaration in a separate text
manifest file instead of extracting it from the object itself.

The declaration entry should identify:

- process id
- start continuation
- workspace size
- local port interface

The process id does not need to be a real UUID. A simple generated integer id is
enough.

So the process declaration is a manifest for:

- the code entry point
- how much workspace the mapper must allocate
- whether the process is user or system, from the declaration section
- what ports the process exposes

It is **not** one fully placed runtime process yet.
The same process declaration can be instantiated many times in one program.

### 2. Program manifest

The program manifest says how many instances exist and how they are connected.

That is where the channel graph lives.

So the program manifest should describe:

- process instances
- which process kind each instance uses
- the connections between instance ports

This is the right place for channels, because a channel is not owned by one
process.
A channel is a connection between two process endpoints.

So the clean split is:

- process manifest = local interface
- program manifest = global wiring

### 3. Mapping

Placement is **not** the linker's job.

A separate mapper tool, specific to the hardware architecture, decides:

- which minion runs a process
- whether it belongs on `H0` or `H1`

The mapper emits generated C / object code that:

- allocates workspace storage for each placed process
- allocates / instantiates the channels described by the program manifest
- instantiates the live `tpa_proc` objects for the chosen minion / hart
- emits the per-hart boot sections

First cut:

- one small process manifest per process kind
- one small program manifest describing instances and connections
- one small placement file describing where instances go

Example process-manifest line:

- `pdef <name> <user|sys> <pid> <start> <ws_sz>`

Example program-manifest lines:

- `inst <inst_id> <pdef_name>`
- `conn <src_inst> <src_port> <dst_inst> <dst_port>`

Example placement line:

- `<inst_id> <hartid>`
- `chan <src_inst> <src_port> <dst_inst> <dst_port> <direct|local|fabric|external>`

The `chan` names are channel/edge transport classes, not port classes. A port
is part of a process interface; the mapped edge between two ports gets the
transport class.

This is only the first cut.

The intended direction is:

- process manifest defines `id`, `start`, `ws_sz`, and ports
- program manifest defines instances and channel connections
- mapper reads both
- placement input only says where each instance goes

Build API:

- `add_tpa_process(...)`
- `add_tpa_program(...)`

### 4. Boot image

The mapper output should be translated into per-hart boot sections that the
runtime can read directly.

So the boot section contains pointers to already-initialized live process
objects. That keeps runtime startup minimal.

So the runtime does not do global process matching by itself at boot. It just
reads the local boot list for the current hart.

## User vs system processes

We agreed that `H1` processes are not just "high priority" in the generic sense.

They are **system processes**:

- control
- debug
- service
- RT work

So the declaration stage should distinguish them structurally.

The best way is separate declaration sections:

- `.tpa.proc.user`
- `.tpa.proc.sys`

This is better than just a type field because:

- the distinction is structural
- the mapper can validate more strongly
- the runtime model stays cleaner

The mapper then decides where they go:

- user processes map to `H0`
- system processes map to `H1`

## Ports and channels

We should model:

- **port** = process-local endpoint
- **channel** = connection between two ports

So a process declaration should say things like:

- this process has 4 ports
- the ports are `up`, `down`, `east`, `west`

The program manifest then says:

- instance `pe_3_4.up` is connected to `pe_2_4.down`
- instance `pe_3_4.east` is connected to `pe_3_5.west`

That makes channels a program property, not a process property.

## Repeated process kinds

The same process code + manifest should be reusable many times in one program.

That is important for things like virtual systolic arrays.

Example:

- one process kind: matrix-multiply cell
- ports:
  - `up`
  - `down`
  - `east`
  - `west`
- behavior:
  - receive from `up`
  - receive from `east`
  - multiply and accumulate into local state
  - send to `down`
  - send to `west`

Then a whole array is not many different process binaries.
It is:

- one process kind
- many instances
- one program manifest wiring them together as a grid

In code, that means:

- one `struct tpa_pdef` for the cell kind
- many `struct tpa_inst` using that same `tpa_pdef`
- many `struct tpa_conn` wiring those instances together

## Boot table

The final image contains a mapper-generated boot table in `.tpa.boot`.

At runtime:

- each hart has a runtime `hartid`
- the runtime scans `.tpa.boot`
- entries whose `hartid` matches the current hart are registered locally
- each matching entry gives one process to enqueue initially

So:

- declaration sections say what exists
- mapper decides placement
- boot table entries say what each hart starts

## Runtime boot model

The runtime should boot from the mapper-generated boot table, not from raw
process declarations and not from platform-specific section names.

So `tpa_main()` should:

1. initialize global runtime state
2. read the runtime hart id
3. scan `.tpa.boot` for matching entries
4. enqueue those already-instantiated process objects
5. enter the scheduler loop

This is better than a central manual dispatcher.

## Scheduler model

One scheduler loop implementation, parameterized by runtime `hartid`.

Each runtime hart has:

- one ready queue
- one non-local-ready bitset

Wakeup rule:

- enqueue local runnable processes on their home hart
- mark non-local-ready processes by home-hart slot
- ring the home hart through the arch wake interface when it is armed

## Channel model

Channel is fundamental.

A channel is:

- one memory-resident rendezvous object
- one canonical home
- shared by the two communicating processes

We are not using buffered mailbox semantics.

We are not using current-hart endpoints.

The channel endpoint is the **process**.

## Implementation order

### 1. Clean ET substrate

- one `et_*` header for hardware helpers
- one `tpa_main()`
- one `tpa_loop(hartid)`

### 2. Image declarations and mapper path

- process declaration section for user processes
- process declaration section for system processes
- mapper-generated boot entries per hart
- runtime boot from those sections

### 3. Minimal runnable-process runtime

- `tpa_proc`
- `tpa_op`
- `tpa_yield`
- `tpa_stop`
- one ready queue per hart
- `tpa_run()`
- `tpa_runq_pop()`
- `tpa_step()`
- `tpa_ws()`

### 4. Minimal smoke tests

- processes run on their mapped harts
- same-hart wakeup works
- cross-hart wakeup works

### 5. Real blocking semantics

- `send`
- `recv`
- process blocks when rendezvous cannot complete
- process wakes when peer arrives

### 6. Then `alt`

- `alt`
- winner tracking
- stale enrollment cleanup

### 7. Then timers if needed

- timer wait
- wakeup into the proper queue

## First tests we need

- process executes on its mapped hart
- same-hart wake/reschedule works
- cross-hart wake/reschedule works
- process can yield and be rescheduled
- sender blocks until receiver arrives
- receiver blocks until sender arrives
- cross-hart rendezvous works

## Non-goals for the first cut

- no stackful processes
- no dynamic process priority changes
- no buffered channels
- no `TensorSend` / `TensorRecv`
- no attempt to preserve the old `xpu/csp.h` API
- no pretending the old work queue runtime already is TPA

## Immediate next step

Before writing more code, keep one clean checkpoint:

- substrate
- declaration + mapper + boot-section shape
- process shape
- ready queue shape
- first three smoke tests

Only after that do channel blocking semantics.
