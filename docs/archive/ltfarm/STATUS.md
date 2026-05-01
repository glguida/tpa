# Archived LTFarm Experiment

This directory preserves the original `ltfarm/` experiment sources and plan as
reference material.

## Status

Archived/reference only. LTFarm is not wired into the structured ET build.

## Preserved material

- `PLAN.md` — design goals, hard rules, milestones, and worker contract.
- `ltfarm_litecoin_core_vectors.h` — official Litecoin Core vector mirror used
  as the intended golden reference.
- TPA graph/process sources and manifests:
  - `ltfarm_job_source.c/.tpm`
  - `ltfarm_scrypt_core.c/.tpm/.tpp/.place`
  - `ltfarm_result_sink.c/.tpm`
- `ltfarm_worker_core.h`
- `ltfarm_scrypt_core_host.cpp`
- original `CMakeLists.txt`

## Why not build it now?

The original plan explicitly requires host-decided correctness against Litecoin
Core vectors and a `sw-sysemu` library harness that reads/writes device symbols.
The current structured repo has the ET host launcher and generated ELF paths,
but not the LTFarm-specific host oracle/harness integration. Porting the device
sources without that oracle would violate the original `PLAN.md` hard rules.

## Future port steps

1. Recreate the host `sw-sysemu` library harness in the structured `tpa-host/`
   or a dedicated `ltfarm/` host subproject.
2. Use the preserved Litecoin Core vectors as the only correctness oracle for
   the initial scalar worker.
3. Integrate `job_source -> scrypt_worker -> result_sink` through
   `add_tpa_process()` / `add_tpa_program()`.
4. Add host-read/write symbol plumbing for input header and output hash.
5. Only after scalar vector correctness is established, port the PI/PS
   vectorization milestones described in `PLAN.md`.
