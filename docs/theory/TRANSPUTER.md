# Transputer Runtime and CSP

## Scope

This note is about the **classic INMOS transputer runtime model** and how it realizes the
**occam / CSP line of concurrency** in implementation terms.

It focuses on:

- process representation
- scheduling
- channel communication
- internal vs external channels
- `PAR`, `ALT`, and timers
- what is genuinely CSP-like, and what is engineering beyond CSP

It deliberately does **not** try to cover:

- later T9000-specific mechanisms
- general operating-system design
- arbitrary language runtimes on top of the transputer

## Core finding

The transputer is not a normal processor with a software concurrency library bolted on.

It is a processor whose **hardware and microcode were designed around occam's execution
model**:

- processes
- synchronous point-to-point channels
- `PAR`
- `ALT`
- timer waits
- two priority levels

So the runtime is largely **architectural**, not just software.

## 1. Process model

A transputer process is centered on a **workspace in memory**.

The live execution state is deliberately small:

- `Wptr`: workspace pointer
- `Iptr`: instruction pointer
- a small evaluation stack in registers

The process workspace holds:

- local variables
- temporary values
- control words used by scheduling and communication

The key implementation idea is that a process is cheap to deschedule because very little
state has to move.

The process descriptor is essentially:

- **workspace pointer**
- **priority bit**

This is enough for the scheduler to identify and requeue a process.

## 2. Scheduler

The scheduler is **microcoded**.

That matters: the classic transputer does not need a software kernel just to multiplex
many concurrent processes.

Main properties:

- there are **two active queues**
  - high priority
  - low priority
- a process runs until it:
  - waits for communication
  - waits for a timer
  - terminates
- blocked processes consume **no processor time**

Priority behavior:

- high-priority processes run before low-priority ones
- high-priority processes can preempt low-priority execution
- low-priority execution is timesliced
- high-priority execution is effectively run-until-wait-or-terminate

This is not an afterthought. It is part of the transputer execution model.

## 3. `PAR` implementation

`PAR` is not implemented as a heavyweight thread package.

The processor provides direct support for process creation and completion:

- `STARTP` starts a process
- `ENDP` participates in finishing a parallel construct

The usual implementation is:

1. initialize a **join counter** in the parent workspace
2. start the child processes
3. each component decrements the counter when it finishes
4. the last finisher allows continuation past the `PAR`

So `PAR` compiles into workspace manipulation plus a small amount of scheduler support,
not a general-purpose thread runtime.

## 4. Channel semantics

This is the central point.

Transputer communication is:

- **point-to-point**
- **synchronous**
- **unbuffered**

This is very close to the occam / CSP communication model.

The first important consequence is:

- a channel does **not** need a process queue
- a channel does **not** need a message queue
- a channel does **not** need a mailbox buffer

It is a rendezvous object.

## 5. Internal channel implementation

For two processes on the **same transputer**, a channel is implemented by a
**single word in memory**.

That word is either:

- `empty`
- or the identity / descriptor of the waiting process

The communication sequence is simple.

If the first process arrives first:

1. it stores its identity in the channel word
2. it stores transfer state in its own process/workspace state
3. it blocks

When the second process arrives:

1. it finds the waiting process through the channel word
2. the message bytes are copied
3. the waiting process is made runnable
4. the channel word is reset to `empty`

Important point:

- the channel is the **rendezvous word**
- the message is **not** modeled as a permanent channel buffer

This is the core reason the transputer channel feels so "pure".

## 6. External channel implementation

For processes on **different transputers**, the programming model is kept the same.

The same communication instructions are used; the hardware decides from the channel
address whether the channel is:

- internal, handled through memory
- external, handled through a link

This is one of the most important architectural ideas in the transputer:

- **same channel semantics**
- **different transport mechanism**

For an external channel:

- the processor hands the transfer to a link interface
- the process is descheduled while transfer proceeds
- when the transfer completes, the process is rescheduled

The link interface holds the transfer state:

- process/workspace reference
- message pointer
- byte count

So a process does not spin waiting for link completion.

## 7. Data movement

Communication transfers a **message** from sender to receiver.

Operationally, the transputer message instructions use:

- a pointer to the message
- the channel address
- a byte count

This is important for understanding the runtime:

- channels are **control / rendezvous objects**
- message storage is **process-side memory**

That is much closer to "rendezvous with data transfer" than to "mailbox with stored payload".

## 8. `ALT` and timers

The transputer also implements the occam `ALT` model directly.

It provides instructions to:

- enable channel guards
- enable timer guards
- wait for one of them
- disable and select the winner

So `ALT` is not compiled into an ad hoc polling loop. It has direct runtime support.

Timers are also first-class:

- a process may wait until a specified time
- if the time is in the future, the process is descheduled
- when the time arrives, the process becomes runnable again

This matters because occam uses communication and timed waiting as basic control
mechanisms, not as peripheral library features.

## 9. How this reflects CSP

The transputer reflects the **operational** side of occam/CSP very strongly.

What carries over cleanly:

- processes are the basic units of structure
- channels are point-to-point
- communication is synchronous
- communication is unbuffered
- buffering, if wanted, should be modeled by an extra process
- `ALT` is primitive
- placement should not change logical behavior

That last point is crucial.

The same occam program can be run:

- on one transputer, with channels implemented in memory
- or across many transputers, with channels implemented by links

The runtime mechanism changes, but the process/channel semantics are intended to stay
the same.

## 10. Where it goes beyond Hoare's CSP

The transputer is not "the failures model in silicon".

It reflects the **occam implementation line** of CSP, not the whole mathematical theory
of Hoare's later denotational models.

Engineering additions include:

- two hardware priority levels
- microcoded scheduling
- timeslicing of low-priority processes
- timer support
- physical communication links
- explicit byte-counted message transfer

So the transputer should be understood as:

- a very faithful **operational realization** of synchronous process/channel concurrency

not as:

- a direct hardware encoding of every part of formal CSP semantics

## 11. Why the runtime works

The runtime is efficient because it keeps the semantic core small:

- process workspace in memory
- tiny live CPU state
- process descriptor
- channel rendezvous word
- scheduler queues

That is why the machine can support large numbers of blocked or runnable processes
without a heavy software kernel.

The strongest implementation lesson is this:

- a local channel can be just **one word of memory**

because synchronous unbuffered communication does not require a buffered channel object.

## Short summary

### Pros

- Very clean match to synchronous process/channel concurrency.
- Local channels are extremely small and cheap.
- Blocked processes consume no CPU.
- The same abstract channel model works for local memory and external links.
- `PAR`, `ALT`, and timers are runtime primitives rather than library conventions.

### Cons

- The model is built around synchronous rendezvous, which is restrictive if buffering is
  wanted everywhere.
- High and low priority are engineering choices, not part of pure Hoare CSP.
- Efficient implementation depends on a runtime/hardware design built around the model.
- It is closer to occam's operational semantics than to the full abstract theory of CSP.

## Sources

Primary and near-primary sources used for this note:

- INMOS, *Transputer Architecture Reference Manual*.
  - https://www.transputer.net/fbooks/tarch/tarch.html
- David May and Roger Shepherd, *The transputer implementation of occam* (INMOS TN21), 1988.
  - https://www.transputer.net/tn/21/tn21.html
- *Inside the Transputer*.
  - https://www.transputer.net/iset/isbn-063201689-2/inside.pdf
- INMOS, *Using transputers as embedded controllers* (TN57).
  - https://www.transputer.net/tn/57/tn57.html
- INMOS, *Performance Maximisation* (TN17), for practical priority/communication guidance.
  - https://www.transputer.net/tn/17/tn17.html
