# TPA Process Model

## Purpose

This note defines the core program-model terms used by TPA.

Its job is to remove ambiguity around words such as:

- process
- process kind
- process instance
- module
- program
- mapped program

This note is normative for terminology. It should be preferred over ad hoc
usage in code comments or debugging notes.

## Scope

This note is about the **program model** and the **build model**.

It is not a memory-planning note and not a topology note. Those are covered by:

- [MEMORY.md](../design/MEMORY.md)
- [MAPPING.md](../design/MAPPING.md)
- [TOPOLOGY.md](../design/TOPOLOGY.md)

## Core Definitions

### Process Kind

A **process kind** is the reusable definition of a behavior.

A process kind defines:

- the start continuation symbol
- the port interface
- the process class (`user` or `sys`)
- the persistent process-state contract
- the scratch contract
- any compiled metadata associated with that behavior

A process kind is the thing declared in a `.tpm` manifest with a `pdef`.

A process kind is **not**:

- a particular runtime instance
- a placement
- an object file

### Process Instance

A **process instance** is one use of a process kind in a program graph.

A process instance adds:

- an instance identifier
- concrete incoming and outgoing edges
- a concrete mapped home in the final image

Instances are declared in `.tpp`.

Different programs may instantiate the same process kind many times. Different
instance ids do not imply different process code or different process
definitions.

### Port

A **port** is a named input or output position on a process kind.

A port belongs to the process interface. A connection in the program graph
connects one output port to one input port.

The port contract is part of the process kind definition, not of any single
instance.

### Program

A **program** is a graph of process instances and connections.

In the current textual flow:

- `.tpp` declares process instances
- `.tpp` declares connections between ports

The logical program does not by itself say where processes run or how scratch
and edge memory are realized physically.

### Mapped Program

A **mapped program** is the logical program together with machine-specific
realization decisions.

A mapped program contains, at minimum:

- the logical process instances
- the logical connections
- placement
- derived scratch capacities
- edge-storage decisions
- any scheduling metadata required by the backend

In the current implementation, the mapped program is represented by a set of
artifacts rather than one single file:

- original `.tpp`
- mapper-generated `.place`
- mapper-generated scratch configuration
- mapper-generated edge configuration
- mapped-program JSON

### Image

An **image** is the generated low-level C/runtime representation of a mapped
program.

The image contains:

- concrete process objects
- concrete channel objects
- boot data
- bindings between instances and start symbols
- mapped memory objects and backing storage declarations

The image is generated from program-level inputs. It is then compiled and
linked with the runtime and process implementation objects to produce the final
ELF.

## Module

The word **module** is overloaded in many systems. In TPA it should be used in
exactly one sense:

- a **module** is a reusable implementation or packaging unit that may provide
  one or more process kinds

A module may correspond to:

- one C source file
- a group of C source files
- one object file
- one CMake process target

The important rule is:

- a module is a **build/packaging unit**
- a process kind is a **program-model unit**

Therefore:

- one module may provide multiple process kinds
- one process kind may be instantiated multiple times
- one object file is not automatically “one process”

This distinction matters because the runtime reasons about process kinds and
instances, while the build system may group them differently.

## Process Code

**Process code** is the implementation of a process kind.

It defines:

- the continuation functions
- the send/receive order
- the kernel calls
- scratch usage inside a firing

Process code does not define:

- where the instance runs
- which edge buffer offsets are assigned
- how much shared scratch backing exists per machine context

Those are mapper/backend responsibilities.

## Current Source Artifacts

The current TPA flow uses these source artifacts:

1. process implementation C
2. process manifest `.tpm`
3. program graph `.tpp`
4. placement `.place` or mapper-generated replacement

The current backend generates the final image from those inputs.

This means the current system is:

- manual in process/library definition
- generated in mapped-program/image realization

## Process Contract

A process kind contract should be understood as four separate parts:

1. **interface**
   - ports
   - direction

2. **persistent process state**
   - mutable state that survives across firings

3. **scratch**
   - transient per-firing compute memory

4. **behavior**
   - continuation sequence
   - send/recv protocol
   - kernel execution

These parts must not be conflated.

In particular:

- scratch is not process-persistent state
- output payload ownership is not process-persistent state
- placement is not part of the process contract

## Runtime Semantics

A TPA process is continuation-driven rather than thread-stack-driven.

At runtime, a process instance consists of:

- current continuation
- process workspace/state
- port bindings
- mapped home

The continuation returns a `tpa_op_t`, and the runtime interprets that
operation.

This means:

- a process instance is a runtime object
- a process kind is a reusable definition
- a module is only the carrier that provides the implementation

## Process Registry

A future process registry is compatible with this model.

If introduced, the registry should own:

- process-kind identity
- port definitions
- start symbol binding
- metadata association
- build object association

In that design:

- `.tpp` would refer to registry process kinds
- `.tpm` could become generated compatibility material or disappear

This does not change the definitions above. It only changes where the process
kind catalog is stored.

## Mapper Responsibilities

The mapper consumes process kinds and a program graph. It does not invent
behavior.

The mapper is responsible for:

- assigning process instances to execution contexts
- deriving scratch capacities
- planning edge storage
- producing machine-specific realization artifacts

The mapper is not responsible for:

- changing the process interface arbitrarily
- repairing incorrect send/receive protocols inside process code
- inventing new kernel behavior

## Invariants

The following invariants should hold throughout the system:

- a process kind is reusable and independent of instance id
- a process instance is local to one program
- a module is not a process
- a mapped program is a program plus realization decisions
- image generation consumes program-level artifacts and emits glue, not kernel
  semantics

These rules should be enforced in documentation, naming, and code review.
