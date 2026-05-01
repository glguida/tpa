# TPA Programming Model

This document defines the core terms and artifacts used by TPA. It is meant to
be normative: use these meanings in docs, code review, and future implementation
work.

## Core terms

### Process kind

A **process kind** is a reusable behavior. It is defined by process code plus a
process manifest entry.

A process kind has:

- a name;
- a class, currently `user` or `sys`;
- a numeric process id;
- a start continuation symbol;
- a persistent state/workspace size declared in the manifest;
- a port interface;
- optional compiled metadata such as scratch peak and object-size information.

A process kind is not a placement and not a particular runtime object.

### Process instance

A **process instance** is one use of a process kind in a program graph. It has an
instance id and concrete connections to other instances. After mapping, it also
has a home runtime hart and a slot in the generated image.

Many instances may use the same process kind. Instance ids do not imply separate
code.

### Module

A **module** is a build or packaging unit: a source file, group of source files,
object file, or CMake target. A module may provide one or more process kinds.
Do not use “module” as a synonym for process kind.

### Port

A **port** is an input or output position on a process kind. Ports describe the
local interface of a reusable behavior.

Ports are not transport classes. A port can participate in a direct edge in one
mapped program and a fabric edge in another. Transport class belongs to the
mapped channel/edge.

### Channel / edge

A **channel** or **edge** is a connection between a producer instance port and a
consumer instance port. It carries a byte capacity, endpoint information, and a
mapped transport/memory realization.

The program model treats output payloads as edge data, not process-owned data,
even if a temporary implementation physically stores some bytes in a producer
object.

### Program graph

A **program graph** is the set of process instances and connections between
ports. In the current textual representation this is mostly the `.tpp` file.

### Mapped program

A **mapped program** is the program graph plus realization decisions. It may be
represented by several generated artifacts:

- mapped `.place`;
- mapped-program JSON;
- scratch configuration header;
- edge configuration header;
- planner/map reports.

### Image

An **image** is the generated low-level C/runtime representation that is
compiled into the final ELF. It materializes concrete process objects, channel
objects, port bindings, backing buffers, and boot entries.

## Continuation-style process model

TPA processes are continuation-driven. They are not parked C stacks and not
migrating threads.

A running process continuation returns a `tpa_op_t`. Common operations include:

- `tpa_send(ch, buf, len, next)`;
- `tpa_recv(ch, bufp, lenp, next)`;
- `tpa_yield(next)`;
- `tpa_block(next)`;
- `tpa_stop()`.

The runtime interprets the returned operation and either continues, blocks,
wakes, or stops the process. Process code chooses continuation boundaries.
Straight-line compute should generally remain inside one continuation or a small
local continuation chain unless a graph boundary is useful.

A process instance has:

- current continuation;
- persistent process state;
- port bindings;
- home runtime hart;
- scheduler/channel state.

Scratch is different from persistent process state. Scratch is transient compute
memory and must not be treated as an output payload or saved across TPA
boundaries unless explicitly part of the persistent state contract.

## Source artifacts

The current TPA flow uses three small text artifacts plus process source code.

### Process manifest: `.tpm`

A `.tpm` file declares process kinds and their ports.

Current grammar:

```text
pdef <name> <user|sys> <pid> <start> <ws_sz>
port <pdef_name> <port_id> <in|out|inout>
```

Example:

```text
pdef demo_stage_pdef user 202 demo_stage_recv 24
port demo_stage_pdef 0 in
port demo_stage_pdef 1 out
```

The `.tpm` belongs with the process implementation because it describes the
process kind interface and start symbol.

### Program graph: `.tpp`

A `.tpp` file instantiates process kinds and connects their ports.

Current grammar:

```text
inst <inst_id> <pdef_name>
conn <src_inst> <src_port> <dst_inst> <dst_port> <bytes>
```

Example:

```text
inst 201 demo_src_pdef
inst 202 demo_stage_pdef
inst 203 demo_sink_pdef

conn 201 0 202 0 8
conn 202 1 203 0 8
```

The `.tpp` is a program-level graph description. It does not decide where
instances run.

### Placement: `.place`

A `.place` file maps instances to runtime hart ids and may override channel
transport class.

Current grammar:

```text
<inst_id> <hartid>
chan <src_inst> <src_port> <dst_inst> <dst_port> <direct|local|fabric|external>
```

Example:

```text
201 0
202 2
203 4
```

A hand-written `.place` is useful for small examples such as
`kernels/tpa_pipe_demo.*`.

For mapped programs, the mapper may generate the `.place` along with scratch and
edge configuration artifacts.

## Hand placement vs mapper-generated placement

Hand placement is explicit and simple. It is appropriate when:

- the graph is small;
- the purpose is a smoke test or tutorial;
- the user wants deterministic placement by inspection.

Mapper-generated placement is appropriate when:

- there are many process instances;
- scratch and edge-memory pressure matter;
- machine topology and communication cost matter;
- performance-first mapping under a hard memory budget is needed.

The YOLO downstream path uses CMake targets that build process objects, extract
metadata JSON, run the planner/mapper, and then build the final generated ELF.

## Image generation path

Do not bypass the image generation path for TPA programs.

The intended CMake flow is:

```cmake
add_tpa_process(
    NAME some_proc
    MANIFEST some.tpm
    SOURCES some.c
)

add_tpa_program(
    NAME some_program
    PROCESSES some_proc
    PROGRAM some.tpp
    PLACEMENT some.place
)
```

`add_tpa_process()` compiles process implementation objects and records the
associated `.tpm` manifest.

`add_tpa_program()` collects process manifests, reads one `.tpp`, reads one
`.place` or mapper-generated placement, invokes `gen_tpa_image.cmake`, and links
the generated image with process objects and the runtime/HAL.

`gen_tpa_image.cmake` checks consistency and emits:

- process definitions;
- process instances;
- channel objects;
- channel backing buffers when needed;
- port binding arrays;
- workspace storage;
- boot entries.

## Runtime interaction from process code

Process code should normally:

1. include the public TPA compatibility/runtime header used by current demos;
2. obtain channels through `tpa_chan(port_id)`;
3. return `tpa_send`, `tpa_recv`, `tpa_yield`, or `tpa_stop` operations;
4. use process workspace/state only for persistent process-private data;
5. use scratch only as transient compute storage;
6. avoid baking placement or channel transport assumptions into the process.

The process code owns behavior. The graph owns connections. The mapper owns
placement and memory realization.

## Current supported examples

Currently integrated generated TPA programs include:

- `kernels/tpa_empty.*` -> `tpa_empty.elf`;
- `kernels/tpa_pipe_demo.*` -> `tpa_pipe_demo.elf`;
- `yolov5n/` downstream planner/map path -> `tpa_yolov5n_downstream.elf`.

Other original examples and tests, such as message tests, queue tests, and full
YOLO host/demo integration, are follow-up work. YOLO tools/models and
representative block tests are ported; original DNN demos and LTFarm are
archived under `docs/archive/` as reference material, not active generated
program targets.

## Current runtime limitation

The structured demo link harness currently proves generated process/image
metadata compile, link, load, and signal PASS in `erbium_emu`. It does not yet
represent completion of the full cooperative runtime scheduler that executes
every generated process continuation. Documentation and tests should be explicit
about that distinction.
