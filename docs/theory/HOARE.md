# Hoare's CSP

## Scope

This note sticks to the **Hoare CSP line itself**:

- Hoare's 1978 paper
- Hoare's 1981 model note
- Hoare's 1983 notes
- the 1984 semantic paper by Brookes, Hoare, and Roscoe
- Hoare's 1985 book

It deliberately excludes:

- CCS
- occam
- CSPM / FDR
- transputer engineering
- later timed / mobile / probabilistic variants

## Core findings

### 1. CSP begins from interaction, not shared state

Hoare's starting move was to treat **input/output as primitive programming operations** and **parallel composition of communicating sequential processes** as a basic structuring method. The point was to make interaction first-class rather than bolt it onto a sequential core.

### 2. Communication is synchronous

In Hoare's mature presentation, communication is a special case of interaction:

- one process outputs
- another process inputs
- the two happen **together**

This is rendezvous, not mailbox semantics.

If buffering is wanted, Hoare's answer is not "make the channel buffered". It is: **insert a buffer process**.

### 3. Events are the semantic atoms

The semantic unit is the **event**.

Important consequences:

- events are treated as instantaneous
- duration is not primitive
- if an activity has duration, model it by separate start/end events
- what matters first is the **order of observable events**

### 4. A process is defined by observable behaviour

The 1978 paper is the proposal. The 1981-1984 work turns it into a mathematical theory.

The mature semantic vocabulary is:

- **traces**: possible finite sequences of events
- **refusals**: finite sets of events a process may refuse after a trace
- **failures**: trace/refusal pairs

This is the key shift: a process is not defined by machine internals, but by what can be observed of its interaction with an environment.

### 5. Alphabets and composition are central

Each process has an **alphabet** of events it may engage in.

Parallel composition is defined through these alphabets:

- shared events synchronize
- non-shared events proceed independently

This is one of the core structural ideas in Hoare's CSP, not a later add-on.

### 6. Concealment is fundamental

Hoare's later theory treats **concealment / hiding** as essential.

Why it matters:

- systems are built from interacting subprocesses
- their internal communications should often become invisible from outside
- after hiding those internal events, the whole still denotes a process

This is a major part of why CSP composes cleanly.

### 7. Recursion is basic

Long-running or infinite behaviour is expressed by **recursive process definitions**.

This is not a scheduler story. It is part of the process algebra / semantics itself.

### 8. Nondeterminism is part of the theory, but not the whole point

Hoare's later presentation is careful here:

- concurrency does **not** by itself force nondeterminism
- nondeterminism also arises from abstraction, concealment, and explicit choice

So "parallel" and "nondeterministic" are related in CSP, but not identical.

## The line of development

### 1978: language proposal

The CACM paper introduces CSP as a concurrent programming notation:

- communication and parallel composition are primitive
- the work is strongly motivated by programming method
- guarded-command style choice is part of the presentation

This paper is historically decisive, but it is not yet the final semantic form most people now mean by "CSP".

### 1981: traces model

Hoare's *A Model for Communicating Sequential Processes* explicitly says it is supporting the earlier proposal by giving a simplified mathematical model using **traces** of process/environment interaction.

This is the first clear semantic consolidation.

### 1984: failures semantics

The JACM paper with Brookes and Roscoe gives the mature mathematical model:

- processes interact with environments
- traces record interaction histories
- refusals record what can be declined
- failures combine the two
- recursion and the main operators are given denotational meaning

This is where CSP becomes a full semantic theory rather than only a language proposal.

### 1985: unified presentation

Hoare's book is the cleanest single source for the mature picture:

- events
- traces
- deterministic and nondeterministic processes
- communication
- parallel composition
- concealment
- recursion
- laws and proof methods

## What seems fundamental in Hoare's CSP

- A process is a pattern of interaction with an environment.
- Communication is a synchronized event, not queued delivery.
- Buffering is modeled by processes, not made primitive.
- Internal communication can be hidden, and the result is still a process.
- Composition and abstraction are first-class.
- The theory is observational and algebraic.

## What is not fundamental in Hoare's CSP

- threads
- stacks
- CPUs
- register transfer
- shared-memory interference as the primitive communication model
- any specific hardware topology

Those may matter in an implementation, but they are not the semantic foundation of Hoare's CSP.

## Practical reading

If the goal is to understand **Hoare's CSP itself**, the clean reading order is:

1. 1978 paper for the original programming idea
2. 1981 model note for the traces move
3. 1984 JACM paper for failures semantics
4. 1985 book for the full language-and-theory presentation

## Sources

Primary and near-primary sources used for this note:

- C. A. R. Hoare, *Communicating Sequential Processes*, CACM 21(8), 1978.
  - Oxford copy: https://www.cs.ox.ac.uk/files/6164/H76%20-%20Communicating.pdf
  - DOI: https://doi.org/10.1145/359576.359585
- C. A. R. Hoare, *A Model for Communicating Sequential Processes*, 1981.
  - Oxford publication page: https://www.cs.ox.ac.uk/publications/publication3766-abstract.html
- C. A. R. Hoare, *Notes on Communicating Sequential Processes*, 1983.
  - Oxford publication page: https://www.cs.ox.ac.uk/publications/publication3783-abstract.html
- S. D. Brookes, C. A. R. Hoare, A. W. Roscoe, *A Theory of Communicating Sequential Processes*, JACM 31(3), 1984.
  - Oxford-hosted PDF: https://www.cs.ox.ac.uk/people/bill.roscoe/publications/4.pdf
- C. A. R. Hoare, *Communicating Sequential Processes*, Prentice Hall, 1985.
  - Oxford electronic edition: https://www.cs.ox.ac.uk/ucs/hoarebook.pdf
