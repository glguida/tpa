# Creating TPA Programs

This guide shows the practical path from a computation to a built and validated
TPA device program. For terminology, read `docs/programming-model.md` first. For
hardware/topology background, read `docs/et-architecture.md`; for the dataflow
vs. mapping split, read `docs/hardware-agnostic-programming.md`.

## The end-to-end pipeline

A TPA program is authored as a hardware-independent graph of continuation-style
processes and then built through the ET superbuild. The normal flow is:

1. Decompose the computation into process kinds and ports without choosing
   harts, minions, shires, or transport classes.
2. Write C process code that returns TPA runtime operations.
3. Declare process kinds and ports in one or more `.tpm` manifests.
4. Instantiate process kinds and connect ports in a `.tpp` program graph.
5. Supply placement as a separate artifact, either with a small/debug hand
   `.place` file or mapper-generated output.
6. Integrate the sources with CMake using `add_tpa_process()` and
   `add_tpa_program()`.
7. Configure and build through the top-level ET superbuild.
8. Run the generated ELF with `erbium_emu` or load it with `tpa_launcher`.
9. Validate application output, emulator PASS/FAIL markers, generated artifacts,
   and relevant tests.

Do not bypass this flow for real TPA programs. A handcrafted standalone ELF may
be useful for isolated platform experiments, but it is not the TPA
process/graph/image path.

## Step 1: decompose the computation

Start from dataflow boundaries, not from hardware details. Process code and
`.tpp` graphs should describe logical communication and reusable behavior; they
should not name runtime hart ids, minions, shires, or channel transport classes.

Good process-kind boundaries are usually:

- natural fan-in/fan-out points;
- named tensor or buffer lifetime boundaries;
- independently placeable stages;
- repeated kernels that can share one process kind;
- boundaries where communication and scratch requirements are clear.

Avoid splitting straight-line compute just to make the graph look parallel. A
process kind should be a useful reusable behavior with a clear port interface.

For each process kind, decide:

- input ports;
- output ports;
- persistent state/workspace size;
- scratch requirements, if planner metadata will need them;
- start continuation symbol;
- whether the process is `user` or `sys`.

Keep the concepts separate:

- **process state** is persistent per-instance data;
- **scratch** is transient compute workspace;
- **edge/channel data** belongs to graph connections and may be mapped into
  channel storage.

## Step 2: write continuation-style C process code

Process code uses TPA operations rather than blocking C calls. A continuation
returns a `tpa_op_t` that tells the runtime what to do next.

Common operations are:

- `tpa_send(channel, data, length, next_continuation)`;
- `tpa_recv(channel, data_pointer_out, length_out, next_continuation)`;
- `tpa_yield(next_continuation)`;
- `tpa_stop()`.

Current demo process code includes the compatibility header:

```c
#include "tpa/tpa.h"
```

A process obtains a channel from its port id with `tpa_chan(port_id)`. It obtains
persistent process workspace with `tpa_ws()` when the process manifest declares
a non-zero workspace size.

The process code should not hard-code placement, minion ids, shire ids, or
channel transport classes. Those belong to `.place` or mapper output.

## Step 3: declare process kinds in `.tpm`

A `.tpm` manifest declares process kinds and their ports:

```text
pdef <name> <user|sys> <pid> <start> <ws_sz>
port <pdef_name> <port_id> <in|out|inout>
```

Example from `kernels/tpa_pipe_demo.tpm`:

```text
pdef demo_src_pdef user 201 demo_src_start 0
port demo_src_pdef 0 out
pdef demo_stage_pdef user 202 demo_stage_recv 24
port demo_stage_pdef 0 in
port demo_stage_pdef 1 out
pdef demo_sink_pdef user 203 demo_sink_start 0
port demo_sink_pdef 0 in
pdef demo_chk_pdef user 204 demo_chk 0
```

Here `demo_stage_pdef` has one input port, one output port, and 24 bytes of
persistent workspace.

## Step 4: write the `.tpp` program graph

A `.tpp` file instantiates process kinds and connects ports:

```text
inst <inst_id> <pdef_name>
conn <src_inst> <src_port> <dst_inst> <dst_port> <bytes>
```

Example from `kernels/tpa_pipe_demo.tpp`:

```text
inst 201 demo_src_pdef
inst 202 demo_stage_pdef
inst 203 demo_stage_pdef
inst 204 demo_sink_pdef
inst 205 demo_chk_pdef
conn 201 0 202 0 8
conn 202 1 203 0 8
conn 203 1 204 0 8
```

This graph uses one source, two instances of the same stage process kind, one
sink, and one checker. Each edge has an 8-byte capacity. The graph does not say
where the instances run or whether a connection is direct, local, fabric, or
external; those are placement/mapping decisions.

## Step 5: supply placement as a separate artifact

### Hand `.place`

For small examples, deterministic smoke tests, tutorials, or mapper/debug
experiments, a placement file may be written by hand:

```text
<inst_id> <runtime_hart_id>
```

Example from `kernels/tpa_pipe_demo.place`:

```text
201 0
202 2
203 4
204 6
205 8
```

A placement file can also include explicit channel kind overrides:

```text
chan <src_inst> <src_port> <dst_inst> <dst_port> <direct|local|fabric|external>
```

Hand placement is best when the graph is small, deterministic, and intended as a
worked example or smoke target. It is still a mapping artifact: do not move these
hart ids or channel class choices into process code or `.tpp` dataflow graphs.

### Mapper-generated output

Use mapper-generated placement for larger, production, or topology-sensitive
graphs where manual placement is error-prone or where scratch/edge memory and
topology costs matter. The YOLO downstream path is the current integrated
example. Its CMake flow extracts process metadata, runs the planner/mapper
against `machines/erbium.json` or `machines/etsoc1.json`, and emits:

- mapped `.place`;
- mapped-program JSON;
- planner/map reports;
- scratch config header;
- edge config header.

See `docs/mapper-planner.md` for the current mapper guide, and use
`planner/README.md` plus `yolov5n/CMakeLists.txt` as implementation references.

## Step 6: integrate with CMake

Use `add_tpa_process()` for process object targets and `add_tpa_program()` for
the final generated ELF target.

Example from `kernels/CMakeLists.txt`:

```cmake
add_tpa_process(
    NAME tpa_pipe_demo_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.c"
)

add_tpa_program(
    NAME tpa_pipe_demo
    PROCESSES
        tpa_pipe_demo_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.place"
)
```

`add_tpa_process()` compiles the process sources as an object library, links the
selected TPA core/HAL dependencies, enables C11, and records the `.tpm` manifest
on the target.

`add_tpa_program()` collects process objects/manifests, calls
`cmake/gen_tpa_image.cmake`, and links the generated image, process objects,
platform startup code, runtime harness, and selected HAL/core into
`<name>.elf`.

For mapper-backed programs, pass mapper-generated placement and generated edge
configuration to `add_tpa_program()` when needed. The current YOLO downstream
CMake path demonstrates this pattern.

## Worked example: `tpa_pipe_demo`

`kernels/tpa_pipe_demo.*` is the small current example for authoring a generated
TPA program.

Files:

- `kernels/tpa_pipe_demo.c` — continuation-style process implementations.
- `kernels/tpa_pipe_demo.tpm` — process kind and port declarations.
- `kernels/tpa_pipe_demo.tpp` — program graph and edge capacities.
- `kernels/tpa_pipe_demo.place` — hand placement onto runtime hart ids.
- `kernels/CMakeLists.txt` — `add_tpa_process()` and `add_tpa_program()` calls.

Behavior:

1. `demo_src_start` sends the constant `7` on port 0.
2. A first `demo_stage_pdef` instance receives 8 bytes, computes `x * 2 + 1`,
   and sends the result on its output port.
3. A second `demo_stage_pdef` instance repeats the transform.
4. `demo_sink_start` receives the final value and records it.
5. `demo_chk` yields until the sink records a value, then emits PASS if the
   value is `31`.

The important authoring lessons are:

- one process kind can have multiple instances;
- ports are declared on the process kind, while connections live in `.tpp`;
- placement is separate from the graph;
- the CMake target is `tpa_pipe_demo.elf`, generated from the TPA artifacts.

## Packed-single micro-example: `tpa_packed_single_row`

`kernels/tpa_packed_single_row.*` is the smallest current ET packed-single SIMD
process/graph example. It uses a source -> compute -> checker graph with two
64-byte row edges. The compute process obtains channel-backed row storage,
loads a 16-float row as two packed-single registers, applies `fmul.ps` and
`fadd.ps` under all-lane mask `m0`, stores the row, and the checker validates
every lane against the scalar formula `input * 2 + 1`.

Files:

- `kernels/tpa_packed_single_row.c` — source, packed-single compute, and checker
  continuations;
- `kernels/tpa_packed_single_row.tpm` — process kinds and ports;
- `kernels/tpa_packed_single_row.tpp` — three-instance row dataflow graph;
- `kernels/tpa_packed_single_row.place` — hand Erbium H0 placement with fabric
  channel overrides so the 64-byte row payloads live in aligned edge storage;
- `kernels/CMakeLists.txt` — `tpa_packed_single_row.elf` target and Erbium CTest
  registration when `erbium_emu` is available.

Use it when you need a reviewable row-local packed-single example without Tensor
scratchpad setup or YOLO model artifacts. It is correctness/tooling evidence,
not a performance benchmark.

## Intermediate example: `tpa_tensor_matmul`

`kernels/tpa_tensor_matmul.*` is the intermediate original demo now ported to
the structured repo. It sits between the small hand-written pipe example and the
larger YOLO downstream path:

- `kernels/tpa_tensor_matmul.c` defines tensor-feed, cell, and check process
  continuations;
- `kernels/tpa_tensor_matmul.tpm` declares the process kinds and ports;
- `kernels/gen_tpa_tensor_matmul.cmake` generates the matrix program graph and
  placement;
- `kernels/CMakeLists.txt` wires those generated `.tpp/.place` artifacts into
  `add_tpa_program()`;
- the forwarded ET target is `tpa_tensor_matmul.elf`.

Use it when you need an example with generated graph artifacts but do not need
YOLO's full mapper-integrated process inventory. It is also the current
in-repository example of ET Tensor scratchpad setup and `tensor_load()` /
`tensor_fma()` use inside continuation-style process code; see
`docs/et-simd-tensor-kernel-notes.md` before applying those instructions to
attention or other fixed-size kernels.

## Full no-weights example: `tpa_stereo_sad`

`depth/stereo_sad.*` is the current full depth-vision demo. It demonstrates a
larger hand-placed TPA graph without external data or model artifacts:

```text
stereo_source
  -> sad_worker stripe 0 -> stereo_checker port 0
  -> sad_worker stripe 1 -> stereo_checker port 1
  -> sad_worker stripe 2 -> stereo_checker port 2
  -> sad_worker stripe 3 -> stereo_checker port 3
```

Files:

- `depth/stereo_sad.c` — source, worker, checker, and deterministic synthetic
  stereo data policy.
- `depth/stereo_sad_common.h` — fixed 96x64, 5x5 SAD, max-disparity-32 packet
  layout and shared validation helpers.
- `depth/stereo_sad.tpm` — process kinds for the source, reusable worker, and
  checker.
- `depth/stereo_sad.tpp` — hardware-independent source/four-worker/checker graph
  and edge capacities.
- `depth/stereo_sad.place` — reviewed Erbium hand placement for the first demo
  target.
- `depth/CMakeLists.txt` — `add_tpa_process()` / `add_tpa_program()` integration
  and the Erbium CTest registration.

The source process regenerates deterministic synthetic grayscale stereo data
using the same `bands:6,12,18` policy as the depth block-test generator. It does
not load external images, datasets, pretrained weights, or third-party stereo
code. `depth/stereo_sad.place` is the current hand placement; mapper-generated
placement, mapped-program JSON, and map reports remain follow-up work.
The forwarded ET target is `tpa_stereo_sad.elf`, and the Erbium CTest is
`tpa_stereo_sad_erbium` when `erbium_emu` is available.

## Build with the ET superbuild

Configure from the repository root. Use the top-level CMake entry point, not an
ad-hoc subdirectory build.

Erbium:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_pipe_demo.elf
```

The generated ELF is under the ET device sub-build, currently:

```text
build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf
```

If the program uses YOLO-style mapper targets, install/select a Python
environment with the planner package before configuring or pass `-DPYTHON`:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf \
  -max_cycles 100000000
```

ET-SoC-1 core build:

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/opt/et -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

For ET-SoC-1 YOLO mapping, the current full-card machine description requires:

```sh
-DTPA_ETSOC1_NR_SHIRES=32
```

Do not enable or document ET-SoC-1 YOLO as a default one-shire target.

## Run and validate

### Run with `erbium_emu`

For Erbium ELFs:

```sh
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  -max_cycles 10000
```

Check for the expected PASS/FAIL trace behavior, not just that the ELF loads.
Generated graph tests should define an unambiguous application-level success
condition and report it with `Signal end test with PASS` or
`Signal end test with FAIL`. Prefer the registered CTests or
`cmake/run_erbium_test_fast.cmake`, which accepts a PASS marker, rejects an
explicit FAIL or missing-PASS run, and only uses the raw emulator process return
code when no PASS marker was observed. A direct `erbium_emu` run may still exit
non-zero after printing a PASS marker if the emulator later reports
waiting/sleeping harts; that raw return code alone should not overturn the PASS
marker, but a missing PASS marker or explicit FAIL remains a validation failure.

### Run with `tpa_launcher`

Build the host tools through the ET host subproject:

```sh
cmake --build build-et-erbium --target tpa_host_tools
```

Then load a generated ELF with the launcher:

```sh
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher \
  --kernel build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  --mode sysemu \
  --timeout 300
```

`tpa_launcher` also supports `--mode pcie` for silicon and `--mode fake` for
host runtime API smoke checks. Use `--help` for the full option list.

### Host smoke tests are not platform validation

For local syntax/unit smoke on machines without ET platform support:

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This mode is intentionally named `TPA_HOST_SMOKE_TEST_DOUBLE`. It does not prove
that Erbium or ET-SoC-1 device integration works.

## Validation checklist for a new program

Use the relevant groups below before asking for review.

### Always

```sh
git diff --check
```

If process code or CMake changed, build with warnings as errors through the
normal targets. Current process targets use `-Wall -Wextra -Werror`.

### Planner/mapper changes

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests
```

For mapper-backed CMake targets, configure with `-DPYTHON=$(command -v python)`
from that environment.

### Erbium program validation

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_pipe_demo.elf
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  -max_cycles 10000
```

Replace `tpa_pipe_demo.elf` and the ELF path with your new program target/path.

### Host launcher path

```sh
cmake --build build-et-erbium --target tpa_host_tools
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher --help
```

Then run your generated ELF in the appropriate launcher mode if the job requires
launcher coverage.

### ET-SoC-1 core validation

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/opt/et -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

Only add ET-SoC-1 program targets when their placement/machine assumptions match
the configured `TPA_ETSOC1_NR_SHIRES`.

### Host smoke-test double

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

Report this as smoke coverage only.

## Authoring traps and anti-patterns

Avoid these common mistakes:

- Bypassing `.tpm`, `.tpp`, `.place`, and `gen_tpa_image.cmake` for a graph
  program.
- Encoding placement or channel transport in process code or `.tpp` graphs.
- Inventing an alternate CMake path instead of the top-level ET superbuild.
- Treating host smoke tests as Erbium or ET-SoC-1 validation.
- Confusing process kind ids with process instance ids.
- Treating ports as transport classes; transport belongs to mapped edges.
- Counting edge/channel payloads as persistent process state.
- Hiding required ET toolchain or HAL behavior behind host fallbacks.
- Claiming original tests/demos/tools are integrated before they are ported.

Current missing or partial areas include full YOLO host/demo integration and
broader scheduler coverage. Tensor matmul and YOLO downstream device-runtime now
have Erbium PASS-marker validation. Representative message/channel and queue
test assets report PASS under Erbium, and the negative expected-failure ELF
reports the intended FAIL marker; do not generalize those representative checks
into exhaustive scheduler validation. YOLO tools/models and representative block
tests are ported; mention remaining items only as follow-up until implementation
jobs port or validate them. Original DNN demos and LTFarm are preserved under
`docs/archive/` as reference material with future port steps, not active program
examples.
