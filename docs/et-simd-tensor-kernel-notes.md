# ET SIMD and Tensor Kernel Notes

This note collects project-specific guidance for using ET packed-single SIMD and
Tensor instructions inside TPA process kernels. It is not a replacement for the
ET Programmer's Reference Manual in `docs/et-programmers-reference-manual.txt`.
Use the manual for instruction encodings and complete semantics; use this note
for the TPA consequences that matter when writing or reviewing kernels. For a
longer tutorial and validation roadmap, see
`docs/et-vector-tensor-hackers-guide.md`.

The current in-repository Tensor example is `kernels/tpa_tensor_matmul.c`. The
fixed attention demo under `attention/` now uses ET helpers in current source:
`attention/attention_et.h` provides TensorFMA-based 16-by-16 products for the
score and output paths plus packed-single row copy/scaling helpers for softmax
and matrix scaling. That implementation still must not be described as a
measured speedup unless the ET build, Erbium PASS marker, extension-use
disassembly, and baseline-vs-optimized trace/cycle evidence are all reviewed.

## Where ET extensions fit in the TPA model

ET SIMD/Tensor code belongs inside a process continuation's compute section. It
must not move graph or placement decisions into process code:

```text
process C continuation:
  recv edge payload -> local compute using scalar/SIMD/Tensor -> send edge payload

.tpm manifest:
  declares process kind, ports, and persistent workspace size

.tpp graph:
  declares logical connections and byte capacities

.place / mapper output:
  assigns runtime harts, channel classes, scratch domains, and edge storage
```

A process implementation may be ET-specific when it includes ET ISA headers or
inline assembly, but it should still keep these boundaries intact:

- Persistent process workspace declared in `.tpm` is not transient Tensor
  scratchpad memory.
- Tensor/SIMD scratch and aligned staging buffers are transient compute storage
  unless the process explicitly saves them as persistent state.
- Edge/channel payload bytes belong to `.tpp` connections and mapped edge
  storage, not to process-owned persistent state.
- Runtime hart placement and channel transport classes remain mapper or `.place`
  decisions. A Tensor-capable process may require an H0-capable placement, but
  that requirement should be enforced by the target/machine mapping policy, not
  by hard-coding minion or hart ids into dataflow code.

For current Erbium TPA targets this requirement matches the existing model:
`docs/et-architecture.md` documents that runtime placements use the even H0 lane
ids, and `machines/erbium.json` exposes only `h0` contexts with `hart_stride: 2`.
That is important because the ET manual states that most Tensor instructions are
only legal on hart 0 of each Minion.

## Packed-single SIMD facts to remember

Manual references:

- SIMD overview and state: Sections 3.1 and 3.2.
- Mask instructions: Chapter 4.
- Packed-single operations: Chapter 5, especially Sections 5.1, 5.4, and 5.7.

Practical consequences for TPA kernels:

- Packed-single instructions view each 256-bit FP register as eight FP32 lanes.
- Packed-single instructions execute under mask register `m0`; inactive lanes do
  not update the destination and may suppress memory accesses for masked
  load/store operations. Set `m0` deliberately, for example with the same
  `mova.m.x` pattern used by `kernels/tpa_tensor_matmul.c` when all eight lanes
  should be active.
- A 16-float row is two packed-single registers: bytes `0..31` and `32..63`.
  This makes packed-single SIMD a natural fit for row-local attention work such
  as scaling, subtracting a row maximum, elementwise exponent approximation,
  reciprocal/normalization, row copies, and row stores.
- `FEXP.PS` computes `2^x`, not `e^x`. A softmax implementation that uses it for
  `exp(x)` must multiply by `log2(e)` first or deliberately keep a separate
  approximation and validate the numerical tolerance.
- Horizontal reductions over 16 lanes, such as a row maximum or row sum, may
  still need scalar cleanup or explicit lane extraction unless the kernel uses a
  verified SIMD reduction sequence.

## Tensor instruction facts to remember

Manual references:

- Cache and scratchpad control: Chapter 8.
- Tensor overview and matrix multiply model: Section 9.1.
- Tensor CSRs and errors: Section 9.2.
- Tensor instruction categories and `TensorWait`: Sections 9.3 and 9.3.7.
- Instruction details for `TensorLoad`, `TensorLoadTranspose32`, `TensorFMA32`,
  `TensorStore`, `TensorReduce`, and `TensorBroadcast`: Section 9.4.

Practical consequences for TPA kernels:

- The Tensor extension operates on two-dimensional tiles of up to 16 rows and up
  to 64 bytes per row. A 16-by-16 FP32 matrix is exactly 16 rows by 64 bytes.
- Tensor loads bypass the L1 data cache and place data in L1 scratchpad, TenB, or
  related Tensor state. `TensorFMA32` consumes those Tensor operands and writes
  the result into the vector register file.
- Tensor row addresses must meet the instruction alignment requirement. For
  `TensorLoad`, `TensorLoadB`, and `TensorLoadTranspose32`, the effective row
  addresses are 64-byte aligned. A struct that is itself 64-byte aligned is not
  sufficient if a 4-byte header precedes the matrix data.
- Tensor operations and cache-management operations do not behave like ordinary
  in-order scalar instructions. Use `tensor_wait()` before consuming Tensor
  results with scalar/SIMD FP instructions, before reusing L1 scratchpad lines or
  Tensor resources where the manual requires ordering, and before checking
  `tensor_error` for an operation.
- Tensor setup is part of the kernel contract. `kernels/tpa_tensor_matmul.c`
  shows the current project pattern: include `<etsoc/isa/cacheops.h>` and
  `<etsoc/isa/tensors.h>`, evict/fence L1D state, put L1D into scratchpad mode,
  initialize `tensor_mask` and `tensor_coop`, set `m0`, clear `tensor_error`,
  issue `tensor_load()`/`tensor_fma()`, wait with `tensor_wait()`, and fail the
  TPA process path if `get_tensor_error()` reports an error.
- Most Tensor instructions are legal only on H0 of each Minion. On current
  Erbium TPA targets the HAL/machine model uses H0 runtime lane ids; future
  targets that expose H1 or mixed contexts must keep Tensor kernels off illegal
  harts.

## Fixed attention: current ET helper status and layout constraints

The structured attention demo has these dimensions:

```text
sequence length      = 16
embedding dimension  = 64
heads                = 4
per-head dimension   = 16
```

Each per-head pipeline therefore carries 16-by-16 FP32 matrices:

```text
QKV generator
    |
    +--> score(head i):   scores = Q * K^T * scale
           |              current code uses TensorFMA32 + packed-single scaling
           v
        softmax(head i):  row-wise softmax(scores)
           |              current code uses scalar max/exp/sum + packed-single copy/scale
           v
        output/check:     output = weights * V
                          current code uses TensorFMA32, then scalar reference validation
```

Current ET helper status in `attention/attention_et.h`:

- `attention_compute_scores_tensor()` uses `attention_et_matmul_16x16()` for
  `Q * K^T`, with `TensorLoadTranspose32` for K, then scales the score matrix
  with packed-single row scaling.
- `attention_compute_softmax_ps()` uses scalar loops for the max, exponent
  approximation, and sum; it uses packed-single helpers for row copies and final
  normalization scaling. Do not describe it as a fully vectorized softmax.
- `attention_compute_output_packet()` uses the same TensorFMA helper for
  `softmax(weights) * V`.

The checked-in attention packet layouts now reserve a 64-byte header before the
first matrix and statically assert that matrix rows are 64-byte aligned:

```text
64-byte-aligned struct base
+0:   uint32_t head
+4:   header padding
+64:  first matrix row         <-- 64-byte aligned
```

If another kernel still has a compact header before matrix data, choose one of
these approaches and update all artifacts consistently:

1. Change channel packet layouts to reserve a 64-byte header/alignment area
   before matrix data, so each 16-float row begins at a 64-byte-aligned address.
2. Copy incoming matrix payloads into explicitly aligned process workspace or
   scratch staging buffers before issuing Tensor loads.

If packet sizes change, update `ATTENTION_HEAD_INPUT_BYTES`,
`ATTENTION_SCORE_PACKET_BYTES`, `ATTENTION_SOFTMAX_PACKET_BYTES`, the `.tpp`
connection capacities, `.tpm` workspace declarations when needed, and static
asserts in the process sources. These sizes are edge payload and workspace
contracts, not incidental C struct details.

## Why TensorReduce, TensorBroadcast, and cooperative Tensor load/store are not the default first step

The current four-head attention graph already expresses independent per-head
pipelines. Each score/softmax path owns one head's Q/K/V or score/weight data and
then sends one packet to the output checker. There is no cross-hart partial sum
or shared per-row reduction in the graph.

`TensorReduce` and `TensorBroadcast` are synchronization-heavy instructions: the
manual describes matched sender/receiver or tree steps, and participating harts
must issue compatible operations. Cooperative Tensor load/store is useful when
multiple Minions intentionally load or store the same cache lines together. Those
features may be useful for a later different graph, but they add ordering and
coordination constraints that the current independent-head attention graph does
not need as its first optimization.

For the current graph, prefer local per-head TensorFMA32 and packed-single row
work first. Reconsider TensorReduce/Broadcast/cooperative operations only when a
new graph introduces a real cross-hart reduction, broadcast, or shared-memory
traffic pattern that the TPA edge structure and mapper placement make explicit.

## Validation before claims

Do not make a performance claim from source inspection alone. A credible ET
extension change needs evidence from the ET build and runtime path:

1. Build through the top-level ET superbuild with the normal TPA process/program
   path. Do not bypass `.tpm`, `.tpp`, mapper or `.place`, `add_tpa_process()`,
   `add_tpa_program()`, or `cmake/gen_tpa_image.cmake`.
2. Run the generated ELF under `erbium_emu` and require the application PASS
   marker. Host smoke-test-double builds are syntax/unit smoke only; they are
   not Erbium or ET-SoC-1 validation.
3. Compare baseline and optimized evidence. For attention, use the existing
   trace tags documented in `attention/README.md` to measure score, softmax, and
   output/check spans when emulator logs expose the cycles.
4. Inspect generated mapper artifacts when placement matters. For attention, the
   primary mapped target should still place the independent head pipelines on
   legal Erbium H0 runtime contexts unless a reviewed mapping change explains
   otherwise.
5. Inspect source or disassembly for actual extension use. Useful evidence
   includes `flw.ps`, `fsw.ps`, `fmul.ps`, `fmadd.ps`, `fmax.ps`, `fexp.ps`, or
   `frcp.ps` mnemonics for packed-single code, and Tensor CSR writes such as
   `csrw 0x83f` (`tensor_load`), `csrw 0x801` (`tensor_fma`), and `csrw 0x830`
   (`tensor_wait`) for Tensor code.
6. Keep numerical validation deterministic. `TEST_PASS` must still mean the
   logical attention output passed the reference/tolerance check; `TEST_FAIL`
   must remain the path for malformed packets, bad lengths, invalid head ids,
   Tensor errors, or numerical mismatches.

Warnings are errors for current process/device targets. If an ET dependency such
as `/opt/et`, the RISC-V binutils, or `erbium_emu` is unavailable, record that as
a validation gap; do not replace it with a host-only claim.
