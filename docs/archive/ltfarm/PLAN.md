# LTFarm Plan

## Goal

Build a serious `ltfarm/` subtree for Erbium that exercises TPA on a Litecoin-style workload with:

- official Litecoin Core test vectors as the first correctness gate;
- a TPA worker that is checked under `sw-sysemu` as a library;
- a path to packed PI/PS vectorization on the actual Erbium ISA.

The non-goal is a toy “miner” that only reaches `PASS`. The result must be checked against an upstream Litecoin implementation.

## Hard Rules

1. **No hand-written host golden path.**
   - Initial correctness comes from official Litecoin Core vectors.
   - Ad hoc host implementations are not accepted as correctness references.

2. **Host decides correctness.**
   - Device code may still signal `PASS`/`FAIL` for basic contract failures.
   - Real success is host comparison against the Litecoin Core result.

3. **Keep the worker coarse.**
   - The real TPA graph should move jobs in and hits out.
   - Internal ROM state and intermediate `BlockMix` state stay inside the worker.

4. **Vectorization targets Erbium PI/PS directly.**
   - Gather/scatter and swizzle come from the PRM, not assumptions.
   - Tensor/MMA is not the primary engine for this workload.

## Why This Workload Fits TPA

Litecoin PoW is a poor match for the existing tensor demos, but a good fit for TPA as a coarse worker farm:

- workers are long-running and stateful;
- workers are embarrassingly parallel across nonce ranges;
- the hot path is integer-heavy and memory-structured;
- the graph can stay small even when the compute is large.

On Erbium, the relevant ISA support is there:

- packed-single gather/scatter:
  - `FGB/FGH/FGW.PS`
  - `FSCB/FSCH/FSCW.PS`
- packed-single swizzle:
  - `FSWIZZ.PS`
- packed-integer arithmetic / logic / shifts:
  - add/sub
  - and/or/xor
  - logical and arithmetic shifts

So a real vectorized worker is credible. The constraint is not ISA feasibility. The constraint is getting the worker contract and oracle right first.

## Golden Reference

Official known vectors are mirrored from Litecoin Core test data in:

- `ltfarm_litecoin_core_vectors.h`

Source:

- repo: `https://github.com/litecoin-project/litecoin`
- commit: `022b7ea5b33d88b61d46338ce9a96384345931db`
- file: `src/test/scrypt_tests.cpp`

These vectors are the default accepted golden path for `ltfarm`.
If broader coverage is needed later, add an upstream-derived host oracle deliberately; do not improvise one.

## Target End State

The eventual graph should look like:

```text
job_source
-> nonce_batch_source
-> scrypt_worker[N]
-> result_sink
```

Where:

- `job_source` publishes the fixed 76-byte header prefix, target, and range metadata.
- `nonce_batch_source` emits nonce batches sized to the vector width we actually choose.
- `scrypt_worker` owns:
  - header assembly;
  - PBKDF2-HMAC-SHA256;
  - ROMix scratch;
  - final 32-byte hash;
  - target compare;
  - optional hit compaction.
- `result_sink` records hits for host readback.

The important point is that `scrypt_worker` is the main unit of work. We do not want a graph of tiny crypto micro-processes passing 128 KiB state through channels.

## Immediate Direction

The checked path is:

1. real TPA graph:
   - `job_source`
   - `scrypt_worker`
   - `result_sink`
2. `sw-sysemu` library harness that:
   - writes the source backing object for one 80-byte block header;
   - runs the generated ELF;
   - reads the sink backing object containing the 32-byte hash result;
   - compares against the official vector result.

## Milestones

### M0: Reference Integration

Deliver:

- copied official test vectors;

Acceptance:

- no `ltfarm` correctness path depends on hand-written host crypto.

### M1: Single-Nonce Scalar Graph

Deliver:

- one real TPA graph with:
  - source process publishing one 80-byte header on a channel;
  - worker process receiving that header and computing full Litecoin `scrypt_1024_1_1_256`;
  - sink process receiving the 32-byte hash and publishing a host-readable result object;
- host-writable source backing symbol;
- host-readable sink backing symbol;
- `sw-sysemu` library harness using the official vectors.

Acceptance:

- host runs official vectors through the generated ELF;
- device hash matches the official expected hashes exactly;
- the host decides pass/fail.

### M2: Vector-Oriented Worker Layout

Deliver:

- worker state layout reorganized for packed lanes;
- explicit lane-major or word-major policy for nonce batching;
- stable device/host contract for batched inputs and outputs.

Acceptance:

- scalar and vector-oriented layouts produce identical results for the same nonce batch;
- the worker contract is documented and fixed.

### M3: Vectorized Hot Path

Deliver:

- PI/PS implementation of the hot worker path;
- at minimum:
  - packed add/xor/shift sequences;
  - packed gather/scatter staging for ROM access;
  - swizzle usage where needed.

Acceptance:

- bit-exact match against M1;
- measurable cycle improvement on Erbium emulation.

### M4: Farm Graph

Deliver:

- `job_source`
- `nonce_batch_source`
- `scrypt_worker`
- `result_sink`
- mapped farm graph

Acceptance:

- host can submit a nonce range and read back hits;
- correctness still derives from official vectors plus any later upstream-derived oracle we explicitly add.

## Worker Contract

### Input

The first real worker contract should be deliberately small:

- `header[80]`
  - full serialized Litecoin block header
- optionally later:
  - `nonce_base`
  - `lane_count`
  - `target`

For M1, simplest is one full 80-byte header per run.

### Output

For M1:

- `hash[32]`
- `status`

For later farm mode:

- hit count
- hit nonces
- hit hashes or compact share records

### Memory Ownership

The worker owns:

- ROM scratchpad for one or more nonces;
- transient PBKDF2 and `BlockMix` state;
- any vector staging buffers.

The host only owns:

- input block header
- final hash / hit records

## Vectorization Plan

The worker should be written with the end vector shape in mind from the start.

### Likely Lane Strategy

Use a batch width that matches the natural packed width on Erbium:

- start with 8 lanes as the default hypothesis
- keep this explicit in the worker contract

### ROM Access Strategy

The ROMix lookup is not a reason to give up on vectors. On Erbium it should be approached as:

- gather/scatter-backed staging from MRAM;
- packed word operations after staging;
- swizzle/repack where lane order needs adjustment.

### ARX Strategy

Salsa-style operations need:

- add
- xor
- rotate synthesized from:
  - shift-left
  - shift-right
  - or

There is no assumption of a dedicated packed rotate opcode.

## Test Strategy

### Reference Vectors

Use the official Litecoin Core scrypt vectors first.

These are the baseline “must pass” checks before any broader mining-style tests.

### Harness Shape

The host harness must:

1. load the generated ELF via `sw-sysemu` library API;
2. resolve input/output symbols;
3. write the block header bytes;
4. run to completion;
5. read the resulting hash;
6. compare to the official expected hash;
7. print the first mismatch clearly if any.

### Failure Policy

Any mismatch is a hard failure.

The worker should not silently keep running after internal contract breaks. Device-side contract bugs should fail early; numerical mismatches should fail in the host comparison.

## Repository Shape

The subtree should evolve toward:

```text
ltfarm/
  PLAN.md
  CMakeLists.txt
  ltfarm_worker_core.h
  ltfarm_litecoin_core_vectors.h
  ltfarm_scrypt_core.c
  ltfarm_scrypt_core.tpm
  ltfarm_scrypt_core.tpp
  ltfarm_scrypt_core.place
  ltfarm_scrypt_core_host.cpp
  ltfarm_job_source.c
  ltfarm_result_sink.c
  ltfarm_farm.tpp
```

## What Is Explicitly Not Accepted

- a hand-written host “golden” implementation;
- correctness justified only by `PASS`;
- splitting ROM state across fine-grained process edges;
- treating tensor/MMA as the default engine for this workload.

## Immediate Next Step

Replace the current `salsa`-only checked path with:

1. a real `ltfarm_scrypt_core` device worker;
2. a new host harness that checks official expected hashes;
3. official Litecoin Core test vectors as the first acceptance suite.
