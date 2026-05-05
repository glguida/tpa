# Hacker's Guide to ET Vector and Tensor Programming for TPA

This guide is a practical bridge between the TPA process/graph model and the
ET packed-single SIMD and Tensor facilities. It is for engineers writing or
reviewing TPA process kernels that use ET-specific instruction extensions.

It is not a replacement for the ET Programmer's Reference Manual (PRM) in
`docs/et-programmers-reference-manual.txt`. Use the PRM for instruction
encodings, CSR bit fields, and complete architectural semantics. This guide
records the TPA-specific consequences, the current in-repository examples, and
the validation discipline required before making claims about ET extension use.

## 1. Scope and non-goals

### Scope

This guide covers:

- where ET packed-single SIMD and Tensor code belongs in a TPA program;
- how to keep `.tpm`, `.tpp`, placement, scratch, edge payloads, immutable model
  data, and process workspace separate;
- the packed-single facts that matter for row-local FP32 kernels;
- the Tensor facts that matter for FP32 16-by-16 tiles;
- alignment and payload-layout contracts for Tensor-ready edge packets;
- the current examples in `kernels/tpa_tensor_matmul.c` and `attention/`;
- how to validate extension use without confusing scalar/reference coverage,
  host smoke tests, source inspection, or mapper placement with optimized
  Erbium performance evidence.

### Non-goals

This guide does not:

- define new ET ISA semantics;
- replace the ET PRM;
- introduce alternate CMake, image-generation, or launch paths;
- claim speedups from ET extensions without reviewed build/run/disassembly/trace
  evidence;
- claim that current YOLOv8n milestones are optimized vector/tensor kernels;
- claim full YOLOv8n graph, accuracy, production quantization, host-demo, or
  ET-SoC-1 YOLOv8n validation.

Current YOLOv8n milestones are external-header deterministic plumbing checks:
sampled and dense C2f/Detect/DFL/neck-tail graph milestones with synthetic-
calibration artifacts and documented external-artifact policy. They are useful
future targets for ET vector/tensor work, but they are not current optimized
ET vector/tensor evidence.

## 2. Mental model: TPA graph vs. ET kernel code

TPA separates logical dataflow from hardware realization. ET packed-single SIMD
and Tensor instructions are implementation details of a process continuation's
compute section; they do not belong in `.tpp` graph semantics.

```text
Process C continuation
  recv edge payload
  -> local compute using scalar C, packed-single SIMD, Tensor, or a mix
  -> send edge payload

.tpm manifest
  process kind name, start continuation, persistent workspace size, ports

.tpp program graph
  process instances and logical connections with byte capacities

.place or mapper output
  runtime hart placement, channel class, scratch domains, edge storage
```

A process implementation may be ET-specific when it includes headers such as
`<etsoc/isa/cacheops.h>` or `<etsoc/isa/tensors.h>`, or when it uses inline
assembly for `flw.ps`, `fsw.ps`, or `fmul.ps`. That does not change the TPA
artifact boundaries:

- **Process workspace** is persistent per-instance state declared by `.tpm` and
  accessed with `tpa_ws()`.
- **Scratch** is transient compute memory used while a continuation runs. Tensor
  scratchpad setup, aligned staging buffers, and temporary vector/Tensor state
  are not edge payloads and are not persistent state unless a process explicitly
  stores them in its workspace contract.
- **Immutable model data** is read-only weight/blob data such as generated YOLO
  weights. It is not scratch and is not an edge buffer.
- **Edge/channel data** is the payload capacity declared by `.tpp` `conn` lines
  and realized by image generation or mapper edge-buffer output.
- **Placement** is a mapping artifact. Tensor legality may require H0-capable
  contexts, but process code should not encode minion ids, shire ids, runtime
  hart ids, or channel transport classes.

Current Erbium TPA placement follows this separation. `machines/erbium.json`
exposes only `h0` contexts with `hart_stride: 2`, and `docs/et-architecture.md`
documents that current Erbium runtime placements use the even H0 lane ids. That
matters because the ET PRM's Tensor overview says most Tensor instructions are
only legal on hart 0 of each Minion.

## 3. Source map

Read these sources before changing ET vector/tensor kernels or documentation:

| Source | Use |
| --- | --- |
| `docs/et-programmers-reference-manual.txt` | Primary local ET ISA source. Cite section names/chapters for instruction behavior. |
| `docs/et-simd-tensor-kernel-notes.md` | Short project-specific checklist for packed-single/Tensor kernel work. |
| `docs/et-architecture.md` | TPA-visible Erbium and ET-SoC-1 topology, H0 placement notes, host-vs-device validation distinction. |
| `docs/programming-model.md` | Normative TPA terms: process kind, instance, port, edge, mapped program, image, scratch. |
| `docs/creating-programs.md` | Required `.c + .tpm + .tpp + .place` or mapper-generated build path. |
| `docs/mapper-planner.md` | Mapper inputs/outputs, machine JSON contexts, generated placement, scratch, and edge artifacts. |
| `docs/memory-and-edge-buffers.md` | Memory taxonomy: process workspace, scratch, immutable model data, edge payloads. |
| `kernels/tpa_packed_single_row.c` | Minimal packed-single row micro-example: source/compute/check graph, aligned 16-float edge rows, `flw.ps`/`fmul.ps`/`fadd.ps`/`fsw.ps`, and deterministic scalar validation. |
| `kernels/tpa_tensor_matmul.c` | Current Tensor matmul example: scratchpad setup, `tensor_load`, `tensor_fma`, `tensor_wait`, error checks, packed-single RF load/store helpers. |
| `attention/attention_common.h` | Attention dimensions, 64-byte header layout, packet sizes, trace tags, scalar reference helpers. |
| `attention/attention_et.h` | Current attention ET helpers: TensorFMA-based 16-by-16 products and packed-single row copy/scaling helpers. |
| `attention/README.md` | Attention build/run commands, trace tags, mapper/baseline placement status, performance-claim warning. |
| `docs/yolo-demo.md` | Current YOLOv5n/YOLOv8n scope, external-artifact policy, and claim gates. |
| `docs/archive/` | Reference-only historical DNN, LTFarm, and generated YOLO material; not active build inputs. |

Useful ET PRM anchors:

- SIMD overview/state: Sections 3.1 and 3.2.
- Mask instructions: Chapter 4, especially `MOV.M.X` and `MOVA.M.X` semantics.
- Packed-single instructions: Chapter 5, especially Sections 5.1, 5.4, 5.7,
  and the instruction definitions in Section 5.8.
- Cache/scratchpad control: Chapter 8, especially `mcache_control` in Section
  8.3.1 and `ucache_control` in Section 8.3.2.
- Tensor overview and matrix multiply: Section 9.1.
- Tensor CSRs and errors: Section 9.2.
- Tensor instruction categories and `TensorWait`: Sections 9.3 and 9.3.7.
- Tensor instruction descriptions: Section 9.4.

## 4. Packed-single SIMD basics

The ET PRM describes packed-single SIMD as 256-bit floating-point registers
viewed as eight FP32 elements. All packed-single (`.ps`) instructions execute
under mask register `m0`.

Practical consequences:

- A packed-single register holds **eight FP32 lanes**.
- A 16-float row is **two packed-single registers**: bytes `0..31` and bytes
  `32..63`.
- Set `m0` deliberately before `.ps` operations. Use all ones (`0xff`) only when
  all eight lanes are valid.
- If a lane's `m0` bit is clear, computational instructions leave the
  corresponding destination lane unchanged. Masked load/store operations may
  suppress memory accesses for inactive lanes.
- Scalar RV64F operations use only the low 32 bits of the widened FP register
  and zero the upper bits. Keep scalar and packed-single register lifetimes
  explicit in inline assembly.

A 16-float row copy is the simplest row-local pattern:

```asm
mov.m.x m0, mask, 0      # or use the project-wide all-mask setup pattern
flw.ps f0, 0(src)        # lanes 0..7
flw.ps f1, 32(src)       # lanes 8..15
fsw.ps f0, 0(dst)
fsw.ps f1, 32(dst)
```

`attention/attention_et.h` uses this pattern in
`attention_et_copy_row_ps()` and uses `fmul.ps` in
`attention_et_mul_row_scalar_ps()` to scale a 16-float row. The current
attention softmax helper still computes the row maximum, exponent approximation,
and row sum with scalar loops; it uses packed-single helpers for row copies and
final normalization scaling. Do not describe it as a fully vectorized softmax.

Packed-single is a natural fit for row-local work:

- row copies and stores;
- scale, bias, subtract, or multiply over 16-float rows;
- elementwise activation approximations when the approximation is numerically
  validated;
- row normalization after a scalar or vector reduction has produced a factor.

Caveats:

- `FEXP.PS` computes `2^x`, not `e^x` (ET PRM Section 5.7). A softmax that uses
  it for `exp(x)` must multiply by `log2(e)` first or document and validate a
  separate approximation.
- Horizontal reductions over 16 lanes need a verified SIMD reduction sequence or
  scalar cleanup. Do not assume that a row maximum or row sum is vectorized just
  because row loads/stores are packed-single.
- Division and some transcendental operations may trap to firmware for emulation
  on ET-SoC-1 according to the PRM's SIMD overview. Treat them as correctness
  tools first; measure before assuming they are the fastest path.

## 5. Tensor basics

The ET Tensor extension accelerates small matrix products. The PRM's Tensor
section describes matrix multiply over A, B, and C with dimensions bounded by
M <= 16, N <= 16, and row widths up to 64 bytes for the supported element sizes.
For FP32, a 16-by-16 matrix is exactly 16 rows by 64 bytes per row.

### H0 legality

The PRM's Tensor overview states that most Tensor instructions are available
only on hart 0 of each Minion. Hart 1 may execute only the documented exceptions
such as `TensorLoadL2Scp`, `TensorWait`, and `tensor_coop` CSR accesses. Current
Erbium and ET-SoC-1 machine JSONs expose `h0` contexts for mapper-visible TPA
work. If a future platform exposes H1 or mixed contexts, Tensor process kinds
must be kept off illegal harts by placement/machine policy.

### Scratchpad/cache setup

Tensor loads place data into L1 scratchpad, TenB, or related Tensor state. They
are not ordinary scalar loads. Current project examples use this setup pattern:

1. Include `<etsoc/isa/cacheops.h>` and `<etsoc/isa/tensors.h>`.
2. Enter an exclusive/cache-control section with `excl_mode(1)`.
3. Evict L1D state (`et_cache_evict_l1d_to_l2()`).
4. Execute a memory fence.
5. Put L1D into split/scratchpad mode with `mcache_control(...)`.
6. Initialize `tensor_mask` and `tensor_coop`.
7. Set `m0` for packed-single helpers when needed.
8. Clear `tensor_error` in software.
9. Leave the exclusive section.

See `tm_enable_tensor_scratchpad()` in `kernels/tpa_tensor_matmul.c` and
`attention_et_enable_tensor_scratchpad()` in `attention/attention_et.h`.

The PRM's cache-control chapter describes `mcache_control` modes. In scratchpad
mode, part of L1 is converted to Tensor scratchpad usable by hart 0. Changing
these modes invalidates/zeros affected sets as documented by the PRM, so setup
is a kernel contract, not a harmless local variable assignment.

### Row alignment

`TensorLoad`, `TensorLoadB`, and `TensorLoadTranspose32` use effective row
addresses with low six address bits omitted; practical row addresses are
64-byte aligned. A 64-byte-aligned struct is not enough if a small header places
the first matrix at offset 4.

For Tensor-ready FP32 16-by-16 payloads, ensure:

- the first row base is 64-byte aligned;
- every row stride is 64 bytes;
- packet headers reserve a whole 64-byte area before matrix data, or process
  code copies into aligned scratch before issuing Tensor loads;
- static asserts check offsets and packet sizes.

### `tensor_wait` and `tensor_error`

Tensor and cache-management effects do not follow normal scalar program order.
The PRM's `TensorWait` section says software must wait for producer/consumer,
write/write, and shared-resource dependencies. Current examples use:

- `tensor_wait(TENSOR_LOAD_WAIT_0)` and `tensor_wait(TENSOR_LOAD_WAIT_1)` before
  consuming Tensor-loaded scratchpad state;
- `tensor_wait(TENSOR_FMA_WAIT)` before consuming TensorFMA results from the
  vector register file;
- `get_tensor_error()` only after the relevant waits;
- `tensor_error` clearing before the operation sequence because hardware does
  not clear it automatically.

A TPA process should report malformed packets, illegal lengths, invalid heads,
Tensor errors, and numerical mismatches through the process's normal FAIL path.
Do not continue silently after a Tensor error.

### Cooperative Tensor operations are not the first default

The PRM provides cooperative Tensor load/store and Tensor reduction/broadcast
facilities. They require compatible operations and synchronization across
participating harts. The current attention graph already expresses independent
per-head pipelines, so the first local optimization path is per-head TensorFMA
and row-local packed-single work. Revisit cooperative operations only when a
new graph has an explicit cross-hart reduction, broadcast, or shared-memory
traffic pattern and the placement/mapping artifacts make that coordination
reviewable.

## 6. Alignment and payload layout

Tensor alignment is a graph contract when edge payloads are sent between
processes. If a packet layout changes to satisfy Tensor, the corresponding
`.tpp` connection capacity, `.tpm` workspace size, static asserts, and any
mapper metadata must change with it.

The current attention packets reserve a 64-byte header:

```c
#define ATTENTION_PACKET_HEADER_BYTES 64u

struct attention_head_input {
    uint32_t head;
    uint8_t header_pad[ATTENTION_PACKET_HEADER_PAD_BYTES];
    float q[16][16];
    float k[16][16];
    float v[16][16];
} __attribute__((aligned(64)));
```

`attention/attention_common.h` statically checks that:

- `q`, `score`, and `weight` begin at offset 64;
- `k`, `v`, and subsequent matrices are cacheline aligned;
- each 16-float row is one `TPA_CACHELINE_BYTES` row;
- packet sizes match the `.tpp` capacities.

The resulting edge capacities in `attention/attention.tpp` are:

- `3136` bytes for `attention_head_input` packets;
- `2112` bytes for score packets;
- `2112` bytes for softmax packets.

When adding a Tensor-ready layout to another kernel, choose one of two policies:

1. **Make the edge payload Tensor-ready.** Reserve aligned headers/padding in the
   packet contract and update all capacities and static asserts. This is best
   when multiple processes can benefit from the layout and the extra bytes are
   acceptable edge memory.
2. **Copy into aligned scratch.** Leave the external packet compact and copy the
   matrix into process-local aligned scratch before Tensor loads. This is best
   when the layout is a private optimization, when changing the edge ABI is too
   costly, or when only one process needs the alignment.

Do not hide edge payload bytes in process-owned mutable globals to avoid a
`.tpp` capacity change. That makes memory accounting and mapper review harder.

## 7. Canonical examples

### `kernels/tpa_packed_single_row.c`

`kernels/tpa_packed_single_row.c` is the minimal structured packed-single row
micro-example. It keeps the normal TPA process/graph/image boundaries while
using ET `.ps` operations inside one compute continuation:

```text
packed_single_row_source -> packed_single_row_compute -> packed_single_row_check
```

It demonstrates:

- a 16-float row represented as one 64-byte edge payload;
- explicit 64-byte static assertions for the row packet;
- channel-backed edge storage obtained with `tpa_send_buf()` and fabric channel
  overrides in the hand `.place` file so source and output rows are aligned
  edge/channel payloads rather than hidden process-owned globals;
- an all-lane `m0` mask;
- `flw.ps` loads for the low and high halves of the row;
- `fmul.ps` followed by `fadd.ps` for the deterministic formula
  `output = input * 2 + 1`;
- `fsw.ps` stores for the low and high halves of the row;
- scalar checker validation of all 16 lanes and normal TEST_PASS/TEST_FAIL
  marker behavior.

What it proves:

- Packed-single row operations can be used inside a continuation-style TPA
  process built through `.c + .tpm + .tpp + .place` and `add_tpa_program()`.
- The ET objdump currently used for validation decodes this target's
  packed-single instructions as `flw.ps`, `fmul.ps`, `fadd.ps`, and `fsw.ps`
  mnemonics.
- The target is a correctness/tooling micro-example, not a performance
  benchmark.

What it does not prove:

- It does not prove a speedup over scalar code.
- It does not validate horizontal reductions, transcendental approximations,
  Tensor operations, attention softmax, YOLOv8n kernels, ET-SoC-1 behavior, or
  silicon/PCIe behavior.

### `kernels/tpa_tensor_matmul.c`

`kernels/tpa_tensor_matmul.c` is the current in-repository Tensor matmul example.
It demonstrates:

- process-continuation integration through `tensor_matmul_*` continuations;
- generated `.tpp` and `.place` artifacts from
  `kernels/gen_tpa_tensor_matmul.cmake`;
- `.tpm` workspace contracts for feed, cell, and checker process kinds;
- aligned workspace arrays for tiles and accumulators;
- Tensor scratchpad setup with cache operations and `mcache_control`;
- explicit `tensor_mask`, `tensor_coop`, `m0`, and `tensor_error` setup;
- `tensor_load()` and `tensor_fma()` inside `tm_tensor_matmul_acc()`;
- `tensor_wait()` before consuming loads or FMA results;
- packed-single helpers that move a 16-by-16 FP32 block between memory and the
  FP register file with `flw.ps`/`fsw.ps`;
- PASS/FAIL marker behavior through the normal TPA test path.

What it proves:

- ET Tensor instructions can be used inside continuation-style TPA process code.
- The structured build can generate graph artifacts and build
  `tpa_tensor_matmul.elf` through the ET superbuild.
- Current status docs record Erbium PASS-marker validation for this target.

What it does not prove:

- It is not a generic Tensor abstraction layer.
- It does not validate every Tensor instruction.
- It does not by itself prove attention, YOLO, or ET-SoC-1 performance.

### `attention/`

`attention/` is the current fixed-size structured fast-attention demo:

```text
QKV generator
  -> score(head 0) -> softmax(head 0) -> output/check
  -> score(head 1) -> softmax(head 1) -> output/check
  -> score(head 2) -> softmax(head 2) -> output/check
  -> score(head 3) -> softmax(head 3) -> output/check
```

The dimensions are sequence length 16, embedding dimension 64, four heads, and
head dimension 16. Each per-head Q, K, V, score, and weight matrix is 16-by-16
FP32, which is the natural FP32 Tensor tile shape.

Current ET helper status:

- `attention/attention_et.h` defines the shared Tensor scratchpad setup helper.
- `attention_compute_scores_tensor()` computes `Q * K^T` with
  `attention_et_matmul_16x16()` and uses a `TensorLoadTranspose32` transform for
  K. It then scales the score matrix with packed-single row scaling.
- `attention_compute_softmax_ps()` uses scalar loops for max, exponent
  approximation, and sum, plus packed-single row copy and row-scaling helpers.
- `attention_compute_output_packet()` computes `softmax * V` with the same
  TensorFMA helper.
- `attention/README.md` documents trace tags for score, softmax, output product,
  and validation spans.

What it proves:

- The current attention code uses ET TensorFMA for score and output products.
- It uses packed-single helpers for row copies and row scaling.
- The packet layout reserves a 64-byte header and statically verifies Tensor-ready
  row alignment.
- Current repository status documents Erbium PASS-marker validation for
  `tpa_fast_attention.elf` and `tpa_fast_attention_serial.elf`.

What it does not prove:

- It does not prove a measured speedup over the serial baseline.
- It does not prove that softmax is fully vectorized.
- It does not prove that mapper placement alone creates performance.
- It does not prove ET-SoC-1 attention behavior.

### YOLO references

`yolov5n/` has the current downstream planner/map/device path and Erbium
PASS-marker validation. `yolov8n/` has opt-in external-header milestones for
Detect/DFL, C2f, dense C2f summaries, combined graph composition, and the first
P4-to-P5 neck-tail Conv+Concat boundary. These paths are scalar/deterministic
plumbing and graph-validation milestones unless a future reviewed job adds ET
vector/tensor kernels and the evidence gates in `docs/yolo-demo.md` are met.

Use YOLO code as a source of realistic edge sizes, immutable model data policy,
and graph/mapping constraints. Do not use it as current evidence for optimized
packed-single or Tensor YOLO kernels.

### Archived/reference-only material

`docs/archive/` preserves original DNN demos, LTFarm, and historical generated
YOLO analysis. These documents can inform future experiments, but archived
sources are not active build targets and should not be cited as current
validation.

## 8. Validation methodology

Validation must prove the claim being made. A source-level observation that a
file contains inline assembly is not enough to claim an optimized Erbium path.
A host smoke test is not ET platform validation. Mapper placement is not a
performance measurement.

### Reviewed Erbium evidence packet: tensor matmul and attention

Reviewed evidence from AgentWS jobs `et-vector-tensor-attention-evidence` and
`et-vector-tensor-attention-evidence-review` records the current Erbium ET
superbuild behavior for `tpa_tensor_matmul.elf`, `tpa_fast_attention.elf`, and
`tpa_fast_attention_serial.elf`. The evidence packet is under:

```text
/home/glguida/think/new-tpa-structured/agentws/jobs/et-vector-tensor-attention-evidence/workspace/
```

Primary files:

- `evidence-summary.md` — narrative summary of build, runtime, disassembly,
  trace, mapper, and unsupported-claim findings.
- `evidence-artifact-sha256.txt` — checksum list; review verified it with
  `sha256sum -c`.
- `attention-trace-summary.md` and `attention-mapper-summary.md` — compact trace
  and mapper summaries used by the tables below.

The evidence build used the top-level ET superbuild and current structured TPA
process/program/mapper path:

```sh
cmake -S . -B /tmp/et-vector-tensor-evidence-build \
  -DET_ROOT=/opt/et \
  -DTPA_PLATFORM=erbium \
  -DPYTHON=$(command -v python3)
cmake --build /tmp/et-vector-tensor-evidence-build --target tpa_tensor_matmul.elf
cmake --build /tmp/et-vector-tensor-evidence-build --target tpa_fast_attention_map_mapped_program
cmake --build /tmp/et-vector-tensor-evidence-build --target tpa_fast_attention.elf
cmake --build /tmp/et-vector-tensor-evidence-build --target tpa_fast_attention_serial.elf
```

All three direct `erbium_emu` runs reported application PASS markers and then
returned raw process exit code `1` because the emulator reported sleeping harts
after PASS. Treat this as application PASS-marker evidence plus a post-PASS raw
emulator condition, matching the repository's PASS-marker wrapper caveat. Do not
report the raw exit code as a clean process success, and do not use it by itself
to overturn the observed PASS marker.

| Target | PASS marker cycle | Raw emulator exit | Evidence files |
| --- | ---: | ---: | --- |
| `tpa_tensor_matmul.elf` | `1,182,859` | `1` | `run-tpa_tensor_matmul.log`, `run-tpa_tensor_matmul.exitcode` |
| `tpa_fast_attention.elf` | `4,579,357` | `1` | `run-tpa_fast_attention.log`, `run-tpa_fast_attention.exitcode` |
| `tpa_fast_attention_serial.elf` | `4,474,439` | `1` | `run-tpa_fast_attention_serial.log`, `run-tpa_fast_attention_serial.exitcode` |

Tensor instruction evidence is disassembly-backed for the expected paths. The
RISC-V objdump used by the experiment was
`/opt/et/bin/riscv64-unknown-elf-objdump`; snippets and full objdumps are in the
evidence workspace.

| ELF | Evidence-backed Tensor CSR writes |
| --- | --- |
| `tpa_tensor_matmul.elf` | `csrw 0x83f` (`TensorLoad`), `csrw 0x801` (`TensorFMA`), and `csrw 0x830` (`TensorWait`) appear in the matmul compute path, with setup CSR writes for `tensor_mask`, `tensor_coop`, and `tensor_error`. |
| `tpa_fast_attention.elf` | The score path and output path both show Tensor setup plus `csrw 0x83f`, `csrw 0x801`, and `csrw 0x830` in the expected regions. |

Packed-single evidence is source-backed and build-backed, not mnemonic-decoded by
that objdump. `kernels/tpa_tensor_matmul.c` and `attention/attention_et.h`
contain inline assembly with `flw.ps`, `fsw.ps`, and `fmul.ps`; the ET build and
Erbium PASS runs prove those sources compiled and executed in these targets. The
available objdump printed the corresponding encodings as custom `.insn` words,
not as decoded `flw.ps`/`fsw.ps`/`fmul.ps` mnemonics. Therefore the honest claim
is: packed-single inline assembly is present in source and compiled successfully,
while this evidence packet does not provide decoded packed-single objdump
mnemonics.

Attention DEBUG trace extraction also worked. The trace command used
`erbium_emu -l` with `-lt 0 -lt 2 -lt 4 -lt 6 -lt 8`, filtered `validation0`
writes and PASS/FAIL markers, and parsed the filtered log with
`tools/trace/parse_attention_trace.py`. These are trace checkpoints for the
current target, not speedup claims.

| Aggregate metric | Cycles |
| --- | ---: |
| `program_begin→pass_tag` | `4,570,356` |
| `program_begin→PASS marker` | `4,570,360` |
| `output_begin→all_received` | `207,361` |
| `all_received→output_end` | `4,364,029` |
| `output_begin→output_end` | `4,571,390` |
| product sum | `513` |
| scalar validation sum | `4,363,296` |
| residual output overhead | `220` |

| Head | Score | Softmax | Product | Scalar validation |
| ---: | ---: | ---: | ---: | ---: |
| 0 | `560` | `9,984` | `165` | `1,090,848` |
| 1 | `560` | `9,980` | `116` | `1,090,784` |
| 2 | `560` | `9,980` | `116` | `1,090,784` |
| 3 | `560` | `9,986` | `116` | `1,090,880` |

Interpretation limits:

- The trace spans are from one current Erbium emulator run and one current
  mapping; they are not stable production performance numbers.
- No controlled baseline-vs-optimized comparison was measured, so this packet
  does **not** support a speedup claim.
- The output process performs scalar reference validation after production output
  work, and the trace shows scalar validation dominates the output-to-PASS span.
- The output process can start and wait before the QKV generator emits
  `program_begin`; do not read every aggregate span as strict serial phase
  ordering.

Attention mapper evidence shows Tensor-using work on H0 contexts and separates
memory categories in the mapped-program summary:

| Mapper value | Evidence |
| --- | --- |
| Placement | QKV plus head 0 score/softmax on `m0:h0`; heads 1, 2, and 3 score/softmax on `m1:h0`, `m2:h0`, and `m3:h0`; output/check on `m4:h0`. Hart ids are `0`, `2`, `4`, `6`, and `8`. |
| Edge buffer bytes | `20,992` |
| Immutable bytes | `248` |
| Process data bytes | `128` |
| Scratch total bytes | `10,240` |
| Total bytes | `31,608`, `fits=true` |

This packet supports statements about the current Erbium build/run path, Tensor
CSR use, source-backed packed-single inline assembly, trace extraction, and
H0-only attention placement. It does not support claims about decoded
packed-single objdump mnemonics, baseline-vs-optimized speedup, ET-SoC-1,
silicon/PCIe, host demo behavior, production performance, or cross-machine
stability. No host smoke-test-double evidence was used.

### Packed-single row micro-example evidence

The `tpa_packed_single_row.elf` target is the micro-example for decoded
packed-single row instruction evidence. The validation used the top-level ET
superbuild and the normal structured process/program path:

```sh
cmake -S . -B build-et-erbium \
  -DET_ROOT=/opt/et \
  -DTPA_PLATFORM=erbium \
  -DPYTHON=$(command -v python3)
cmake --build build-et-erbium --target tpa_packed_single_row.elf
ctest --test-dir build-et-erbium/tpa-device-prefix/src/tpa-device-build \
  -R '^tpa_packed_single_row$' --output-on-failure
```

The registered CTest passed by observing the application PASS marker from
`cmake/run_erbium_test_fast.cmake`. A direct emulator run with
`-minions 0x7 -max_cycles 1000000` reported PASS at cycle `10,888` and then
returned raw process exit code `1` because the emulator reported sleeping harts
after PASS. Treat that as application PASS-marker evidence plus the normal
post-PASS direct-emulator caveat, not as a clean raw process exit.

Unlike the earlier tensor/attention evidence packet, the available ET objdump
decoded this target's packed-single instructions directly. The compute section
contains the expected row-local sequence:

```text
flw.ps   ft0,0(a1)
flw.ps   ft1,32(a1)
flw.ps   ft2,0(a0)
flw.ps   ft3,0(a6)
fmul.ps  ft0,ft0,ft2
fmul.ps  ft1,ft1,ft2
fadd.ps  ft0,ft0,ft3
fadd.ps  ft1,ft1,ft3
fsw.ps   ft0,0(a5)
fsw.ps   ft1,32(a5)
```

This supports a narrow claim: the packed-single row micro-example builds,
executes, validates its deterministic scalar oracle, and has decoded `.ps`
mnemonic disassembly under the current ET Erbium toolchain. It does not support
performance, YOLOv8n, ET-SoC-1, silicon/PCIe, or host-demo claims.

### Claim-to-evidence checklist

| Claim | Minimum evidence |
| --- | --- |
| Documentation describes existing source accurately | Source inspection and grep showing no contradictory docs. |
| A process uses packed-single instructions | Source or decoded disassembly showing `.ps` mnemonics such as `flw.ps`, `fsw.ps`, `fmul.ps`, `fmadd.ps`, `fmax.ps`, `fexp.ps`, or `frcp.ps`; if objdump emits custom `.insn` words instead, state that decoding gap and keep the claim source-backed/build-backed. |
| A process uses Tensor instructions | Source or disassembly showing Tensor CSR writes such as `tensor_load`/CSR `0x83f`, `tensor_fma`/CSR `0x801`, and `tensor_wait`/CSR `0x830`. |
| A generated ELF works on Erbium | Top-level ET superbuild plus `erbium_emu` or registered CTest showing the application PASS marker and no explicit FAIL. |
| A Tensor process is legally placed | Placement or mapped-program JSON showing H0-capable contexts; for current Erbium, even runtime hart ids from `machines/erbium.json`. |
| A performance speedup exists | Baseline and optimized builds, same input, PASS markers, extension-use evidence, trace/cycle data or counters, and a documented comparison method. |
| YOLOv8n accuracy or production quantization | The policy gates in `docs/yolo-demo.md`, including approved artifacts, representative calibration/evaluation data, checksums, commands, and metrics. |

### Build path

Use the top-level ET superbuild and the TPA process/program path:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_tensor_matmul.elf
cmake --build build-et-erbium --target tpa_fast_attention_map_mapped_program
cmake --build build-et-erbium --target tpa_fast_attention.elf
cmake --build build-et-erbium --target tpa_fast_attention_serial.elf
```

Do not bypass `.tpm`, `.tpp`, `.place` or mapper output, `add_tpa_process()`,
`add_tpa_program()`, or `cmake/gen_tpa_image.cmake` for graph programs.

### Runtime PASS/FAIL markers

Generated graph tests signal application status with emulator markers such as:

```text
Signal end test with PASS
Signal end test with FAIL
```

Prefer registered CTests or `cmake/run_erbium_test_fast.cmake` where available.
The wrapper rejects explicit FAIL or missing PASS runs and accepts an
application PASS marker before considering raw emulator return-code quirks from
sleeping/waiting harts. A missing PASS marker, explicit FAIL marker, or non-zero
raw emulator result without a PASS marker is still a validation failure.

### Disassembly checks

Use disassembly to confirm the optimized instructions survived compilation.
Record the exact ELF and objdump command in review evidence. Useful search
terms include:

```text
flw.ps fsw.ps fmul.ps fmadd.ps fmax.ps fexp.ps frcp.ps
tensor_load tensor_fma tensor_wait
csrw 0x83f csrw 0x801 csrw 0x830
```

The local toolchain path depends on the ET platform installation. Some ET
objdump versions may not decode packed-single mnemonics and may print custom
`.insn` words instead. In that case, do not claim mnemonic-decoded packed-single
disassembly; record the source-backed inline assembly, successful ET build/run,
and objdump decoding gap. Do not replace missing ET binutils with a host-only
claim; log the validation gap.

### Attention trace tags

`attention/README.md` documents stable `arch_trace()` tags. Use them to compare
score, softmax, output-product, and validation spans. For attention performance
claims, capture both the mapped target and the serial baseline under comparable
conditions, preserve the filtered logs or parsed timing table, and state exactly
which spans are being compared.

### Mapper placement review

When placement matters, inspect the generated mapped-program JSON, not only the
ELF result. Check:

- runtime hart ids and labels;
- H0 legality for Tensor processes;
- edge capacities and edge pools;
- scratch domains and scratch peaks;
- warnings about memory or metadata.

Current `machines/erbium.json` exposes H0 contexts only; a future machine JSON
with H1 contexts must add a clear policy for Tensor process kinds.

### Documentation and local checks

For docs-only changes, run:

```sh
git diff --check
```

Then search changed and adjacent docs for contradictions. For this guide, at
least search `docs/et-simd-tensor-kernel-notes.md`, `attention/README.md`,
`docs/yolo-demo.md`, and `README.md` for stale claims about attention being only
hypothetical, YOLOv8n vector/tensor status, or unsupported performance claims.

## 9. Experiment ladder

Use small experiments to retire one risk at a time.

1. **Tensor matmul evidence refresh.** Build `tpa_tensor_matmul.elf`, run under
   Erbium, confirm the PASS marker, disassemble for Tensor CSR writes, and
   verify packed-single RF load/store helpers through decoded disassembly or an
   explicit source-backed/objdump-decoding-gap note.
2. **Packed-single micro-example.** Use `tpa_packed_single_row.elf` as the
   baseline row-local packed-single evidence target: it scales/biases a
   16-float row with `.ps` helpers, validates output deterministically, and
   currently disassembles to decoded `.ps` mnemonics under the Erbium toolchain.
3. **Tensor alignment success/failure tests.** Prove that a 64-byte-header
   payload succeeds and that an intentionally misaligned staging experiment fails
   or reports `tensor_error` as expected. Keep expected-failure behavior explicit.
4. **Attention trace/disassembly pass.** For `tpa_fast_attention.elf` and
   `tpa_fast_attention_serial.elf`, preserve PASS logs, extension-use
   disassembly, mapper placement, and trace spans for score/softmax/output.
5. **Attention softmax vectorization.** Replace one scalar substep at a time:
   row subtract/scale, exponent approximation, reciprocal, then row reduction.
   Keep tolerance and PASS behavior deterministic at each step.
6. **YOLOv8n prototype preconditions.** Before changing YOLOv8n kernels, choose a
   small candidate (for example a 1x1 Conv or a C2f inner product), document its
   edge/model/scratch contract, keep generated model-derived artifacts external,
   and define scalar-vs-optimized evidence before writing ET-specific code.

## 10. YOLOv8n optimization roadmap

YOLOv8n is a future consumer of this guide, not current vector/tensor evidence.
A realistic roadmap is:

1. Preserve the existing external-artifact policy in `docs/yolo-demo.md`.
2. Pick one small kernel boundary whose inputs and outputs are already validated
   by deterministic hashes.
3. Classify every byte:
   - generated weights, fused BN parameters, DFL weights, and quantization tables
     are immutable model data;
   - input/output feature maps, summaries, and activations between process
     instances are edge/channel payloads;
   - convolution accumulators, C2f temporaries, DFL temporaries, Tensor staging,
     and packed-single row buffers are scratch;
   - process workspace stores only continuation state.
4. Choose packed-single or Tensor based on the actual data shape, not on the name
   of the layer.
5. Keep scalar reference coverage active and compare optimized output against the
   existing deterministic oracle.
6. Require Erbium PASS, extension-use disassembly, and trace/cycle evidence
   before claiming speedup.
7. Do not claim production quantization, model accuracy, full graph validation,
   host demo support, or ET-SoC-1 validation without the evidence gates listed in
   `docs/yolo-demo.md`.

Likely first candidates are dense local operations with stable layouts, such as
1x1 Conv-like inner products, small row/feature transforms, or selected C2f
substeps. DFL softmax/decode may benefit from packed-single row work, but it
also has layout and numerical tolerance risks. Large feature-map convolutions
need a mapper/scratch/edge-memory review before instruction selection.

## 11. Open questions and literature map

The planning environment that requested this guide did not have web browsing.
Future workers with explicit internet or library access should fetch and cite
primary sources rather than infer from secondary summaries.

Primary sources to fetch or confirm:

- the current official ET Programmer's Reference Manual version and errata;
- ET compiler/binutils documentation for packed-single and Tensor mnemonics;
- ET C header documentation for `<etsoc/isa/cacheops.h>` and
  `<etsoc/isa/tensors.h>` wrappers;
- emulator trace/logging documentation for `arch_trace()` and Tensor events;
- performance-counter documentation and examples for TensorFMA, mask, and
  packed-single events;
- any official cooperative Tensor load/store, TensorReduce, and TensorBroadcast
  programming examples;
- ET-SoC-1 platform notes that affect L1/L2 scratchpad setup, H0/H1 exposure,
  and user-mode cache-control legality.

Open technical questions:

- Which Tensor and cache-control wrapper sequences are the recommended stable
  ABI for current ET platform headers?
- What is the smallest reliable performance-counter setup for generated TPA
  ELFs under `erbium_emu` and silicon?
- How should the mapper express Tensor-required contexts if future machine JSONs
  expose both H0 and H1?
- What is the right reusable abstraction level for Tensor scratchpad setup
  without hiding required waits, errors, and alignment checks?
- Which packed-single softmax approximation is acceptable for attention and
  YOLO DFL tolerances?
- What ET-SoC-1-specific validation is required beyond current one-shire
  `tpa_core` before claiming vector/tensor behavior there?

Until those questions are answered by reviewed evidence, keep documentation
explicit: state what is implemented, what was validated, what was only inspected,
and what remains a follow-up experiment.
