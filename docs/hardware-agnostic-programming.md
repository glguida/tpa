# Hardware-Agnostic TPA Programming

TPA programs should describe dataflow first and hardware realization second. The
same process code and `.tpp` graph should be understandable without knowing which
Erbium minion, ET-SoC-1 shire, memory pool, or channel transport class will be
chosen later.

## Fundamental rule: write dataflow, not placement

Program authors write hardware-independent computation and communication:

```text
process C + .tpm manifests + .tpp graph
        = reusable process kinds and logical dataflow
```

Keep hardware out of this layer:

- Process code must not name harts, minions, shires, or fabric routes.
- `.tpp` graphs must not encode runtime hart ids or channel transport classes.
- Ports describe a process kind's local interface, not whether an edge is
  `direct`, `local`, `fabric`, or `external`.
- Edge/channel payload sizes belong to `.tpp` connections; edge storage and
  transport realization belong to mapping/image generation.

Placement is still required for a device ELF. The point is that placement is a
separate mapping artifact, not part of process behavior or graph dataflow.

## What programmers write

### Continuation C process code

Process code implements reusable process kinds. A continuation returns TPA
runtime operations such as `tpa_send()`, `tpa_recv()`, `tpa_yield()`, and
`tpa_stop()`. It should use `tpa_chan(port_id)` and `tpa_ws()` rather than
inspecting hardware topology.

### `.tpm` process manifests

A `.tpm` manifest declares process kind names, ids, start continuations,
workspace sizes, and ports:

```text
pdef demo_stage_pdef user 202 demo_stage_recv 24
port demo_stage_pdef 0 in
port demo_stage_pdef 1 out
```

This describes the reusable behavior's interface. It does not place any
instance on hardware.

### `.tpp` program graphs

A `.tpp` graph instantiates process kinds and connects ports:

```text
inst 201 demo_src_pdef
inst 202 demo_stage_pdef
conn 201 0 202 0 8
```

The graph states logical dataflow and channel byte capacity. It does not decide
runtime hart ids, minion ids, shire ids, or channel transport classes.

## What mapping decides

Mapping and image generation supply the hardware realization:

- runtime hart ids for process instances;
- channel transport classes (`direct`, `local`, `fabric`, or `external`);
- edge-buffer placement and backing storage when needed;
- scratch-memory artifacts for mapped programs;
- generated image C containing process objects, channels, port bindings,
  workspace storage, and boot entries.

A hand or generated `.place` file maps instance ids to runtime harts and may
include channel class overrides:

```text
201 0
202 2
chan 201 0 202 0 local
```

Those lines are mapping decisions. They are intentionally outside process C and
outside `.tpp` graph dataflow.

## Placement policy

Use mapper-generated placement for non-trivial, production, or
topology-sensitive programs. The mapper is the scalable path when:

- the graph has many instances;
- communication cost and memory domains matter;
- scratch or edge-buffer memory pressure matters;
- you need generated reports, mapped-program JSON, scratch headers, or edge
  configuration headers.

Hand `.place` files remain valid when they are explicitly small/debug/example
artifacts:

- tiny worked examples such as `kernels/tpa_pipe_demo.*`;
- deterministic smoke tests where the intended harts are part of the example;
- mapper debugging or report inspection with `tpa-plan-program`.

Do not convert this policy into an absolute. The current repository supports
both small hand placements and mapper-generated placements. The important rule
is to keep either kind of placement out of process code and `.tpp` graphs.

## Current structured workflow

The current CMake API is:

```cmake
add_tpa_process(
    NAME some_proc
    MANIFEST path/to/some.tpm
    SOURCES path/to/some.c
)

add_tpa_program(
    NAME some_program
    PROCESSES some_proc
    PROGRAM path/to/some.tpp
    PLACEMENT path/to/some.place
)
```

`add_tpa_program()` invokes `cmake/gen_tpa_image.cmake`; do not bypass it for a
real graph program. The primary build is still the top-level ET superbuild:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target some_program.elf
```

For mapper-backed programs, pass the mapper-generated `.place` and any generated
edge configuration header into `add_tpa_program()` using the current helper
arguments documented in `cmake/tpa-kernel.cmake` and existing YOLO CMake paths.

## Mapper quick start

Use `docs/mapper-planner.md` as the detailed mapper reference. The current CLI
names are:

```sh
tpa-plan-program \
  --program path/to/program.tpp \
  --placement path/to/program.place \
  --process-jsons path/to/process.json \
  --output /tmp/plan.json

tpa-map-program \
  --program path/to/program.tpp \
  --process-jsons path/to/process.json \
  --machine-json machines/erbium.json \
  --output /tmp/tpa_map.json \
  --mapped-program-out /tmp/tpa_mapped_program.json \
  --placement-out /tmp/tpa_mapped.place \
  --scratch-header-out /tmp/tpa_scratch_config.h \
  --edge-config-header-out /tmp/tpa_edge_config.h
```

`--output` is the mapper/report JSON output. Use `--placement-out` for the
generated `.place` file.

## Example roles in this repository

- `kernels/tpa_pipe_demo.*` uses a hand `.place` because it is a tiny,
  deterministic worked example.
- `kernels/tpa_tensor_matmul.*` demonstrates generated graph and placement
  artifacts without the full YOLO mapper flow.
- `yolov5n/` demonstrates the mapper-integrated CMake path with process
  metadata extraction, planner/map reports, mapped placement, scratch config,
  edge config, and the final device ELF.

## Common misconceptions

- **Misconception:** hardware-agnostic means no `.place` files exist.
  **Correction:** placement exists, but it is a separate mapping artifact.

- **Misconception:** hand placement is always forbidden.
  **Correction:** hand `.place` files are valid for small examples, smoke tests,
  and mapper/debug experiments; mapper output is preferred for scalable or
  topology-sensitive programs.

- **Misconception:** a port is a transport class.
  **Correction:** ports are local process-kind interfaces. Transport class is a
  mapped edge property.

- **Misconception:** `.tpp` should name harts or minions.
  **Correction:** `.tpp` names process instances and connections only. Runtime
  hart ids belong to `.place` or mapper output.

- **Misconception:** host smoke-test-double success proves device mapping.
  **Correction:** host smoke tests are syntax/unit smoke only; Erbium or
  ET-SoC-1 validation requires the ET platform path.
