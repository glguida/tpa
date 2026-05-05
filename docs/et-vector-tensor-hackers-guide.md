# Hacker's Guide to ET Vector and Tensor Programming

This is a field manual for writing, inspecting, debugging, and validating ET
packed-single SIMD and Tensor code. It is not an ISA replacement. The primary
ISA source remains `docs/et-programmers-reference-manual.txt`; this guide turns
PRM facts and current repository evidence into programming consequences.

The guide is intentionally low-level. It explains registers, CSRs, masks,
cache/scratchpad setup, Tensor instruction families, ordering waits, error bits,
layout constraints, disassembly expectations, and TPA process integration. It is
not organized around any model or product roadmap.

## 1. Source map and trust rules

Read the ET PRM first when an instruction encoding or architectural rule matters.
Use this guide to remember what those rules imply for code review.

| Source | Use |
| --- | --- |
| `docs/et-programmers-reference-manual.txt` | Primary local reference for SIMD state, mask instructions, packed-single instructions, cache-control CSRs, Tensor CSRs, Tensor instruction encodings, wait rules, and PMU event names. |
| `docs/et-simd-tensor-kernel-notes.md` | Short project checklist. This guide is the deeper manual. |
| `kernels/tpa_packed_single_row.c` | Minimal structured packed-single row micro-example: aligned 16-float edge rows, `flw.ps`/`fmul.ps`/`fadd.ps`/`fsw.ps`, scalar checking, and decoded objdump evidence. |
| `kernels/tpa_tensor_alignment.c` | Small Tensor alignment/error evidence target: 64-byte-header packet contract, aligned 16-by-16 FP32 TensorLoad/TensorFMA success path, and controlled `tensor_error[4]` expected-error subtest with L1 scratchpad disabled. |
| `kernels/tpa_tensor_matmul.c` | Current in-repository Tensor matrix multiply process: scratchpad setup, `tensor_load`, `tensor_fma`, waits, error handling, and packed-single register-file load/store helpers. |
| `attention/attention_et.h` | Current reusable ET helper patterns for 16-by-16 FP32 TensorFMA, packed-single row copy/scale, and the opt-in softmax subtract-max packed-single experiment. |
| `attention/attention_common.h` | Packet layout, 64-byte headers, static asserts, trace tags, and scalar reference helpers for a fixed 16-by-16 workload. |
| `attention/attention_score.c`, `attention/attention_softmax.c`, `attention/attention_output.c` | Continuation integration around Tensor score/output products, packed-single row work, and scalar validation. |
| `attention/README.md` | Build/run commands, mapper placement notes, trace-tag extraction, and no-speedup policy. |
| `docs/et-architecture.md` | H0 placement, Erbium and ET-SoC-1 topology, host/device validation split. |
| `docs/programming-model.md` | Process, instance, channel/edge, scratch, image, and mapped-program terms. |
| `docs/creating-programs.md` | Required `.c + .tpm + .tpp + .place` or mapper-generated build flow. |
| `docs/memory-and-edge-buffers.md` | Persistent state, scratch, immutable data, and edge/channel memory taxonomy. |
| `machines/erbium.json` | Current mapper-visible Erbium contexts: eight `h0` minion contexts with runtime hart stride 2. |
| `~/aifoundry/et-platform/et-common-libs/include/etsoc/isa/tensors.h` | Current local wrapper definitions for Tensor CSRs, wait constants, error constants, and `flw_ps`/`fsw_ps` macros. |
| `~/aifoundry/et-platform/et-common-libs/include/etsoc/isa/cacheops.h` | Current local wrapper definitions for cacheops, `mcache_control`, `ucache_control`, and `excl_mode`. |
| `~/aifoundry/et-platform/sw-sysemu/insns/tensors.cpp` | Read-only simulator evidence for Tensor state machines, error updates, TenB pairing, waits, and cooperative handling. |
| `~/aifoundry/et-platform/sw-sysemu/cache.h` | Read-only simulator constants for L1D line size, scratchpad entries, and cache-index mapping. |
| AgentWS evidence packet `et-vector-tensor-attention-evidence` | Reviewed build/run/disassembly/trace evidence for tensor matmul and attention. |

Trust hierarchy:

1. PRM architectural text controls instruction semantics.
2. ET platform headers control the wrapper API actually compiled by current
   source.
3. Current repository source controls what the project actually builds.
4. Evidence packets control what was validated in a specific environment.
5. This guide is explanatory; if it conflicts with a primary source, fix the
   guide.

## 2. ET execution context model for vector and Tensor code

An ET Minion has two harts. Current TPA Erbium generated programs place runtime
work on the even H0 lane ids. `machines/erbium.json` exposes only contexts named
`m0:h0` through `m7:h0`, with `hart_stride: 2`; the runtime hart ids are
therefore `0, 2, 4, ...`.

This matters because the PRM says most Tensor instructions are available only on
hart 0 of each Minion. Hart 1 may execute only documented exceptions such as
`TensorLoadL2Scp`, `TensorWait`, and `tensor_coop` CSR accesses. If a future
machine description exposes H1 contexts, Tensor process kinds must be kept off
those contexts by placement/machine policy. Do not encode a minion id or hart id
inside process dataflow code.

Keep these layers separate:

```text
TPA graph layer:
  process kinds, ports, instances, edge byte capacities

Mapping layer:
  runtime hart ids, direct/local/fabric/external channel classes,
  scratch domains, edge-buffer pools

Kernel layer:
  scalar C, packed-single SIMD, Tensor CSRs, cache/scratchpad setup,
  waits, error checks, local aligned staging
```

A process continuation can be ET-specific internally. It may include
`<etsoc/isa/cacheops.h>`, `<etsoc/isa/tensors.h>`, or inline `.ps` assembly.
That does not move placement, graph edges, or channel transport into source
semantics.

## 3. Packed-single SIMD machine state

PRM sections 3 and 5 define ET packed-single SIMD.

Core facts:

- The floating-point register file is widened to 256 bits: `f0` through `f31`
  are FLEN=256 registers.
- A packed-single register is viewed as eight 32-bit FP lanes: `e0` occupies
  bits `31:0`, `e1` bits `63:32`, ..., `e7` bits `255:224`.
- Packed-integer instructions use the same widened FP registers as eight
  int32/uint32 values.
- Eight mask registers, `m0` through `m7`, are added. Each mask register has
  eight bits.
- All packed-single (`.ps`) and packed-integer (`.pi`) instructions execute
  under `m0`.
- Scalar RV64F instructions operate on the low 32 bits of an FP register and set
  upper bits `32:FLEN-1` to zero. Treat scalar and packed lifetimes as separate.
- The sticky InputDenorm flag appears in `fcsr[31]` / `fflags[31]` when an input
  denormal is flushed to zero. Output denormals are also flushed to zero and set
  underflow/inexact, but not InputDenorm.
- On ET-SoC-1, division, square root, reciprocal-square-root, and sine variants
  listed by the PRM trap to M-mode emulation. Correctness may still be available,
  but latency assumptions must be measured.
- Gather/scatter instructions use `GSC_PROGRESS` CSR `0x840` to record partial
  progress on trap/interruption and clear it on successful completion.

Practical register rule: a 16-float FP32 row is exactly two packed-single
registers, 64 bytes total:

```text
row bytes  0..31  -> one .ps register, lanes e0..e7
row bytes 32..63  -> second .ps register, lanes e0..e7
```

## 4. Mask registers and `m0`

PRM chapter 4 defines mask instructions. `m0` is special because it controls all
packed-single and packed-integer operations. The other mask registers are useful
for comparisons, saved masks, and mask algebra.

Important operations:

| Instruction | Practical use |
| --- | --- |
| `mov.m.x md, rs1, imm8` | Write one mask register from `rs1[7:0] | imm8`. Common all-lanes setup is `mov.m.x m0, mask, 0` with `mask = 0xff`. |
| `mova.m.x rs1` | Split one 64-bit integer into eight mask registers: low byte to `m0`, next byte to `m1`, ..., high byte to `m7`. Current setup code passes `0xff`, which makes `m0` all-lanes active and leaves `m1..m7` zero. |
| `mova.x.m rd` | Concatenate `m7..m0` into one integer. Useful for diagnostics or preserving all masks. |
| `maskand`, `maskor`, `maskxor`, `masknot` | Boolean mask algebra. |
| `maskpopc`, `maskpopcz` | Count active or inactive lanes. Useful when a compare writes a mask and code needs scalar loop bounds or validation. |

Inactive-lane consequences:

- For `flw.ps`, inactive lanes do not generate memory exceptions and do not
  update the destination lane.
- For `fsw.ps`, inactive lanes do not generate memory exceptions and do not
  update memory.
- For computational instructions, inactive destination lanes are unchanged.
- If a destination register is reused, inactive lanes keep old data. Initialize
  or overwrite the destination before using partial masks.
- Tensor `tensor_mask` is a different CSR. Do not confuse a vector lane mask
  (`m0`) with a Tensor row mask (`tensor_mask`).

Minimal all-lanes row copy idiom:

```c
static inline void copy_16_fp32_ps(float dst[16], const float src[16])
{
    uint32_t mask = 0xffu;

    asm volatile(
        "mov.m.x m0, %[mask], 0\n"
        "flw.ps f0, 0(%[src])\n"
        "flw.ps f1, 32(%[src])\n"
        "fsw.ps f0, 0(%[dst])\n"
        "fsw.ps f1, 32(%[dst])\n"
        :
        : [mask] "r"(mask), [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "memory");
}
```

This mirrors the style in `attention/attention_et.h`. Keep snippets small and
validate generated code because compiler register allocation, clobber lists, and
ET binutils decoding are all part of the evidence.

## 5. Packed-single instruction families

Use instruction families as a design checklist.

### Load and store

`FLW.PS` and `FSW.PS` move eight contiguous 32-bit lanes under `m0`.
`FLQ2` and `FSQ2` move an unmasked 256-bit register. Use masked load/store when
edge rows may be partial; use all-lanes masks for full 8-lane chunks.

Current local ET header macros:

```c
#define flw_ps(fd, ptr) __asm__ volatile("flw.ps f" #fd ", (%0)" :: "r"(ptr))
#define fsw_ps(fd, ptr) __asm__ volatile("fsw.ps f" #fd ", (%0)" :: "r"(ptr) : "memory")
```

The project sources currently use explicit inline assembly instead of these
macros in several places. Both approaches require the same mask discipline.

### Broadcast

`FBC.PS`, `FBCX.PS`, and `FBCI.PS` broadcast one 32-bit memory, integer, or
immediate-derived value into active lanes. Broadcasting a scalar to a temporary
aligned eight-float array and loading it with `flw.ps` is simple and visible in
source, but a direct broadcast instruction can avoid the memory temporary if the
toolchain path is validated.

### Gather and scatter

`FGW.PS`, `FGH.PS`, `FGB.PS` gather arbitrary word/halfword/byte elements using
indices in a second vector register. Halfword and byte gathers sign-extend to
32-bit lanes. `FSCW.PS`, `FSCH.PS`, `FSCB.PS` scatter low lane portions.

Restricted forms `FG32*.PS` and `FSC32*.PS` operate within a 32-byte aligned
block and use smaller per-lane indices. Use them only when the layout contract
really is a block-indexed layout.

Trap/restart implication: arbitrary gather/scatter operations update
`GSC_PROGRESS` on partial trap. A robust expected-failure or page-boundary test
should check restart behavior instead of assuming the instruction is atomic over
all lanes.

### Arithmetic and fused operations

`FADD.PS`, `FSUB.PS`, `FMUL.PS`, `FMADD.PS`, `FMSUB.PS`, `FNMADD.PS`,
`FNMSUB.PS`, `FDIV.PS`, `FSQRT.PS`, `FMIN.PS`, and `FMAX.PS` operate lane-wise
under `m0`. Most encode a rounding mode; `FMIN.PS` and `FMAX.PS` do not.
Inactive lanes are unchanged.

Use fused multiply-add only when the fused rounding behavior is intended. Do not
silently substitute scalar `a*b + c` reference results without tolerance policy.

### Moves, conversions, and swizzles

- `FCMOV.PS` selects from two source registers based on a third source value.
- `FCMOVM.PS` selects from two source registers based on `m0`.
- `FSWIZZ.PS` permutes each 4-lane half according to four 2-bit selectors.
- `FMVZ.X.PS` and `FMVS.X.PS` extract one lane to an integer register with zero
  or sign extension.
- `FCVT.*.PS` and `FCVT.PS.*` convert between FP32 and integer, FP16, and
  normalized or small-float formats depending on the instruction.

Use lane extraction for debug assertions sparingly; it serializes the vector
mental model back into scalar code.

### Comparisons and mask writes

`FEQ.PS`, `FLE.PS`, and `FLT.PS` write all-ones/all-zero lane values to an FP
register. `FEQM.PS`, `FLEM.PS`, and `FLTM.PS` write the boolean result to a mask
register. `FCLASS.PS` writes a 10-bit FP class result per lane.

Prefer mask-writing comparisons when the next operation is predicated. Read the
mask with `mova.x.m` or count with `maskpopc` only when scalar control flow
needs it.

### Transcendentals and reciprocal operations

PRM section 5.7 is easy to misuse:

- `FEXP.PS` computes `2^x`, not `e^x`.
- `FLOG.PS` computes `log2(x)`.
- `FSIN.PS` computes `sin(2*pi*x)` and expects the source in the appropriate
  reduced range; the PRM suggests `FFRC.PS` for range reduction.
- `FRCP.PS` computes reciprocal and rounds toward zero.
- `FRSQ.PS` computes reciprocal square root and is emulated on ET-SoC-1.

A row-wise softmax cannot use `FEXP.PS` as `exp(x)` unless it first multiplies
by `log2(e)` or documents a separately validated approximation.

## 6. Tensor mental model

PRM chapter 9 defines the Tensor extension.

A Tensor is a small two-dimensional tile in a larger row-major matrix or in a
2-D projection of a higher-dimensional object. Consecutive rows do not have to
be contiguous; Tensor load/store instructions carry a row stride.

Matrix multiply model:

```text
A: M x K
B: K x N
C: M x N
M <= 16, N <= 16, K <= 64 / element_size(A,B)
C elements are 4 bytes
```

For FP32, `element_size=4`, so the natural maximum tile is `16 x 16`, and one
row is exactly 64 bytes. The operation is:

```text
for k in 0..K-1
  for m in 0..M-1
    for n in 0..N-1
      C[m][n] += A[m][k] * B[k][n]
```

For FP16 and INT8, the PRM describes interleaved B layouts so the innermost
vectorized dot products consume 4-byte groups efficiently. Do not reuse an FP32
B layout for FP16/INT8 TensorFMA.

Tensor state locations:

- **L1SCP**: L1 scratchpad lines used by Tensor loads and TensorFMA operands.
  The local simulator defines 48 logical scratchpad entries, each 64 bytes.
- **TenB**: logical 16 registers of 64 bytes for B-streaming. It is not a normal
  software-visible register file. A `TensorLoadB` must be paired with a
  consuming TensorFMA or behavior becomes undefined.
- **TenC**: logical 16 registers of 64 bytes used by integer Tensor matrix
  multiplication. Current FP32 examples write results to the vector register
  file, not TenC.
- **Vector register file**: TensorFMA32 and Tensor store paths use `f0..f31` as
  rows of result data. A 16-column FP32 row uses two FP registers.

Tensor instructions are part of the hart instruction stream, but their effects
do not follow ordinary scalar program order. Tensor memory access exceptions do
not trap in the usual way; many are recorded in `tensor_error`. Software must
wait explicitly before consuming results or reading error state.

## 7. Tensor CSRs

All Tensor instructions are encoded as CSR writes. Current local wrappers in
`<etsoc/isa/tensors.h>` map directly to those CSR numbers.

| CSR | Number | Role |
| --- | ---: | --- |
| `tensor_reduce` | `0x800` | `TensorSend`, `TensorRecv`, `TensorBroadcast`, `TensorReduce` selected by bits `1:0`. |
| `tensor_fma` | `0x801` | `TensorFMA32`, `TensorFMA16A32`, `TensorIMA8A32` selected by bits `3:1`. |
| `tensor_conv_size` | `0x802` | Array dimensions and row/column steps; writing updates `tensor_mask`. |
| `tensor_conv_ctrl` | `0x803` | Tensor start row/column inside a larger array; writing updates `tensor_mask`. |
| `tensor_coop` | `0x804` | Cooperative TensorLoad neighborhoods, minions, and group id. |
| `tensor_mask` | `0x805` | Sixteen row-mask bits for TensorLoad, TensorFMA, and optional cacheop masking. |
| `tensor_quant` | `0x806` | Up to ten TensorQuant transformations. |
| `tensor_error` | `0x808` | Sticky software-cleared error accumulation for Tensor and cache management. |
| `tensor_wait` | `0x830` | Wait/fence events for Tensor/cache completion. |
| `tensor_load` | `0x83f` | TensorLoad, TensorLoadB, interleave, and transpose variants. |
| `tensor_load_l2` | `0x85f` | TensorLoadL2Scp. |
| `tensor_store` | `0x87f` | TensorStore and TensorStoreFromScp. |

### `tensor_mask`

`tensor_mask` has 16 meaningful low bits. When bit `n` is zero, a masked Tensor
load/FMA/cacheop skips row `n`. The row mask can be written directly or computed
by writing `tensor_conv_size` and `tensor_conv_ctrl`.

Use a direct all-rows mask for dense 16-row tiles. Use the convolution CSRs when
boundary handling is naturally expressed as a start coordinate plus array shape.
Remember that writing either convolution CSR can update `tensor_mask` as a side
effect.

### `tensor_coop`

`tensor_coop` fields are:

```text
bits 19:16  NEIGHS   neighborhood mask
bits 15:8   MINIONS  minion mask inside selected neighborhoods
bits 4:0    GROUP    cooperation id
```

All harts in a cooperation group must specify the same `NEIGHS`, `MINIONS`, and
`GROUP`, execute the same TensorLoad variant, and name the same physical
addresses. The PRM says mismatches are undefined. Reusing a group id before all
previous cooperative loads for that group have completed also requires external
synchronization.

Do not use cooperative Tensor operations as a local optimization afterthought.
They are graph-level coordination operations even though the CSR is local.

### `tensor_error`

`tensor_error` is sticky and software-cleared. Hardware never clears it for you.
Clear it before a Tensor/cache sequence, wait for relevant operations, then read
it. Reading before the wait can miss the error for the operation you care about.

Meaningful current bits:

| Bit | Name in PRM/local headers | Meaning |
| ---: | --- | --- |
| 1 | `IV` / `TENSOR_ERROR_LOAD_TRANSFORM` | Illegal `tensor_load` command or TenB field. |
| 3 | `FCCO` / `TENSOR_ERROR_FCC_OVERFLOW` | Fast credit counter overflow. |
| 4 | `L1SCPDIS` / `TENSOR_ERROR_SCP_DISABLED` | Tensor instruction issued while L1 scratchpad is disabled. |
| 5 | `LF` / `TENSOR_ERROR_LOCKSW` | `LockSW` failed to lock the cache line. |
| 6 | `ILLTP` / `TENSOR_ERROR_TL1_FMA` | Illegal TensorLoadB and TensorFMA pairing. |
| 7 | `TMF` / `TENSOR_ERROR_MEM_FAULT` | Tensor/cache memory-related exception such as page or access fault. |
| 8 | `IVNT` / `TENSOR_ERROR_STORE_COOP` | Illegal TensorStore size/cooperation combination. |
| 9 | `ILLT` / `TENSOR_ERROR_REDUCE` | Illegal TensorReduce target or function field. |

The PRM notes that some interrupts, such as bus errors due to Tensor or cache
management instructions, are not captured in `tensor_error` and are not
suppressed. Treat `tensor_error == 0` as necessary, not as the only possible
runtime health signal.

## 8. Cache control and L1 scratchpad setup

PRM chapter 8 defines cache control. Tensor work depends on L1 data cache mode.

`mcache_control` bit layout:

```text
bit 1  ScpEnable  1 enables L1 scratchpad
bit 0  D1Split    1 hard-partitions L1D between harts
```

Modes:

| D1Split | ScpEnable | Mode | Consequence |
| ---: | ---: | --- | --- |
| 0 | x | Shared | L1D dynamically shared; scratchpad disabled. |
| 1 | 0 | Split | Harts have hard-partitioned L1D sets; scratchpad disabled. |
| 1 | 1 | Scratchpad | H0 has cache sets 12-13, H1 has sets 14-15, sets 0-11 become H0-only Tensor scratchpad. |

Transition hazards:

- Leaving or returning to shared mode invalidates the entire L1 and clears locks.
- Toggling `ScpEnable` while split invalidates and zeroes sets 0-13.
- The `{ScpEnable,D1Split} == {1,0}` state is illegal and ignored by the PRM.
- User-mode `ucache_control` can set `ScpEnable` only from thread 0 when
  `D1Split` is already set; thread 1 writes to `ScpEnable` are ignored.

Current project setup pattern from tensor matmul and attention:

```c
static inline void enable_tensor_scratchpad_once(void)
{
    uint64_t pmask = 0xffu;

    excl_mode(1);
    et_cache_evict_l1d_to_l2();
    asm volatile("fence rw, rw" ::: "memory");
    mcache_control(1, 0, 0, 0);  /* split */
    mcache_control(1, 1, 0, 0);  /* split + scratchpad */
    asm volatile("csrwi tensor_mask, 0\n"
                 "csrwi tensor_coop, 0\n"
                 "mova.m.x %0\n"
                 :
                 : "r"(pmask)
                 : "memory");
    asm volatile("csrwi tensor_error, 0" ::: "memory");
    excl_mode(0);
}
```

Do not cargo-cult this sequence without understanding it:

- `et_cache_evict_l1d_to_l2()` plus `fence rw,rw` protects dirty cache state
  before a mode transition that invalidates/zeroes sets.
- Two `mcache_control` writes follow a valid transition path: shared/split to
  split, then split to scratchpad.
- `mova.m.x` seeds all mask registers from one 64-bit value. With the current
  `0xff` value, only `m0` becomes all-lanes active. This is not a Tensor row
  mask write.
- `tensor_error` is cleared after setup because setup and cacheops can also
  report errors there.

PRM cacheop ordering rule: execute a `fence` before cacheops when previous
memory operations touch the same cache lines, and use `TensorWait 6` before
subsequent memory operations that read or write lines affected by cacheops.

## 9. Tensor load, transpose, interleave, and TenB

`TensorLoad` variants are writes to CSR `0x83f`. The instruction variant is
encoded in bits `[61:59,52]`; illegal encodings set `tensor_error[1]` and perform
no operation.

The `x31` implicit source register carries row stride and load id:

```text
x31[47:6]  STRIDE bits for 64-byte row stride
x31[0]     ID used by TensorWait 0 or 1
```

The current wrapper constructs it as:

```c
register uint64_t x31_enc asm("x31") = (stride & 0xFFFFFFFFFFC0ULL) | (id & 1);
```

Normal FP32 tile load:

- rows = `ROWS + 1`, up to 16;
- each row is 64 consecutive bytes;
- base address omits low 6 bits, so effective row addresses are 64-byte aligned;
- destination is consecutive L1SCP lines starting at `START`, modulo the logical
  scratchpad line count;
- if `MSK` is set, zero bits in `tensor_mask` suppress memory access and
  scratchpad write for the corresponding row.

`TensorLoadTranspose32` is the usual B-transpose tool for 16-by-16 FP32 `A * B^T`
patterns. It loads a 16-row, FP32 matrix, transposes while loading, and writes
transposed rows to L1SCP lines. It still uses 64-byte aligned row addresses.

Interleave variants prepare lower-precision B layouts:

| Variant | Source element size | Alignment consequence | Consumer |
| --- | ---: | --- | --- |
| `TensorLoadInterleave8` | 8-bit | base uses 16-byte alignment; stride still encoded in 64-byte units | `TensorIMA8A32` |
| `TensorLoadInterleave16` | 16-bit | base uses 32-byte alignment; stride still encoded in 64-byte units | `TensorFMA16A32` |
| `TensorLoadTranspose8/16/32` | 8/16/32-bit | transposes during load; FP32 form uses 64-byte rows | Transposed B or layout transforms |

`TensorLoadB` is special:

- it loads a B matrix into TenB, not regular L1SCP lines;
- it must be paired with a subsequent TensorFMA that consumes TenB;
- multiple unpaired `TensorLoadB` operations may cancel/replace earlier ones,
  but an unpaired TenB load can leave the Tensor co-processor in undefined state;
- cooperative `TensorLoadB` must also be paired correctly;
- wrong TenB pairing sets `tensor_error[6]` in the documented cases.

Current repository examples avoid TenB and load A/B into L1SCP. That is the
right first pattern when teaching and debugging.

## 10. TensorFMA32 and result paths

`TensorFMA32` is a write to CSR `0x801` with type bits `3:1 == 000`.

Important fields for FP32:

```text
bit 63      MSK: use tensor_mask row mask
bits 56:55  BCOLS: result columns encoded as groups of 4 columns
bits 54:51  AROWS: A rows minus 1
bits 50:47  ACOLS: A columns minus 1
bits 46:43  AOFFSET: starting FP32 column offset inside each A scratchpad row
bit 20      TENB: 0 means B in L1SCP, 1 means B in TenB
bits 17:12  BSTART: L1SCP line for B when TENB=0
bits 9:4    ASTART: L1SCP line for A
bit 0       MUL: 1 means product only, 0 means add to existing C
```

FP32 result layout:

- `bcols = (BCOLS + 1) * 4` result columns;
- row `i` of C is stored in `f2*i` and `f2*i+1` when `bcols > 8`;
- for a 16-column FP32 result, rows occupy `f0/f1`, `f2/f3`, ..., `f30/f31`;
- `MUL=1` starts from zero product; `MUL=0` accumulates into existing C.

Current examples use this sequence:

1. Store the current accumulator matrix into the vector register file using
   `flw.ps` pairs.
2. Load A into L1SCP id 0 and B into L1SCP id 1.
3. Wait for the loads that the following TensorFMA consumes.
4. Issue `tensor_fma(..., opcode=0, first_pass=0 or 1)`.
5. Wait for `TENSOR_FMA_WAIT`.
6. Store the vector register file result back with `fsw.ps` pairs.
7. Read `tensor_error` and fail the process if nonzero.

In `kernels/tpa_tensor_matmul.c`, the inner loop waits for load id 0 but not id
1 before FMA. That is current validated source behavior. A conservative new
kernel should wait for every load it depends on unless PRM source/destination
rules and reviewed evidence justify omitting a wait.

## 11. Secondary Tensor families: PRM facts and validation boundary

The current in-repository TPA evidence is narrow and should be treated that
way. Validated examples cover normal FP32 TensorLoad to L1SCP, FP32
TensorLoadTranspose32, TensorFMA32, TensorWait, `tensor_error` clear/read, the
L1SCP-disabled expected-error path, packed-single row transforms, and attention
packed-single substeps. They do not make every PRM-described Tensor family a
reusable TPA recipe.

Use the summaries below as PRM-derived design notes until a small target proves
the exact family under the TPA build, mapper/place, emulator, disassembly, trace,
wait, and error-handling path.

| Family | PRM role | Why it is risky in TPA process graphs | Current repository evidence | Minimum evidence before treating as a recipe |
| --- | --- | --- | --- | --- |
| TensorStore / TensorStoreFromScp | CSR `0x87f` writes vector-register rows or L1SCP rows to memory, bypassing normal caches; completion is waited with TensorWait event 8. | Store results are consumed through ordinary TPA memory/edge paths, so cache visibility, `TensorWait 8`, row-size encoding, stride, and write-after-write hazards matter. Cooperative store modes also encode placement assumptions through `mhartid` relationships. | No current TPA evidence target relies on TensorStore or TensorStoreFromScp for PASS. Existing TensorFMA examples store vector-register results with `fsw.ps` pairs after `TensorWait 7`. | One positive H0 target for TensorStore and one for TensorStoreFromScp that writes known rows, waits event 8, reads via ordinary memory, validates values, checks `tensor_error`, and includes objdump evidence for `csrw 0x87f`. A separate expected-error target should cover an invalid cooperative store mode without hanging. |
| TensorQuant | CSR `0x806` applies a sequence of up to ten transformations to matrix A in the vector register file, with optional L1SCP operands for some transforms; completion is waited with event 10. | Transform semantics are data-type and rounding-mode dependent. Some transforms require L1SCP, some can trap on invalid `frm`, and useful claims need scalar references plus tolerance policy. | No current TPA evidence target executes TensorQuant. Existing quant discussion is PRM/header-derived only. | A focused target for one transform family at a time: initialize vector-register rows, issue TensorQuant, wait event 10, store/read back results, compare against scalar reference, check `tensor_error`, and record disassembly plus tolerance rationale. |
| TensorReduce / TensorBroadcast | CSR `0x800` transfers vector-register values between harts and optionally reduces them through explicit send/receive or tree actions; completion is waited with event 9. | Participating harts must issue compatible operations. A missing sender/receiver or wrong tree step can stall the reduction graph, so this is graph-level synchronization, not a local arithmetic optimization. | No current TPA target validates TensorReduce or TensorBroadcast. The guide's reduce/broadcast material is PRM-derived. | A two-H0 or four-H0 fixed-placement target with explicit send/receive or tree schedule, watchdog-friendly timeout policy, trace tags on every participant, TensorWait event 9, scalar validation of the collective result, and objdump evidence for `csrw 0x800`. |
| Cooperative TensorLoad / cooperative TensorLoadB | `tensor_coop` plus `COOP=1` lets selected harts cooperate on the same physical TensorLoad address and operation. | All participants must agree on group id, neighborhood/minion masks, instruction variant, and physical addresses. Group reuse needs synchronization. Mismatches are undefined and can corrupt data or hang. | No current TPA target validates cooperative TensorLoad. Current examples use non-cooperative loads. | A fixed-placement multi-H0 target proving one cooperative load with identical address/mask/group on all participants, trace tags before/after the cooperative operation, waits on the correct load id, scalar validation of the loaded/FMA result, and a documented policy for not running unsafe mismatch negatives by default. |
| Cooperative TensorStore | PRM describes pair/quad cooperative stores that coalesce multiple Minion requests to the same cache line. | Unlike `tensor_coop`, the store cooperation encoding depends on valid `COOP/SIZE` combinations and hart address relationships. It also has memory visibility hazards with downstream TPA readers. | No current TPA target validates cooperative TensorStore; only the PRM error bit for invalid store cooperation is summarized. | Start with non-cooperative TensorStore. Then add a fixed two-H0 store target with explicit placement/address relationships, TensorWait event 8, post-store ordinary-memory validation, and an expected-error test for one invalid `COOP/SIZE` encoding. |
| TensorLoadInterleave8/16 with lower-precision FMA | Interleave loads reshape 8-bit or 16-bit memory rows into the L1SCP B layout required by `TensorIMA8A32` or `TensorFMA16A32`. | Layout is not a cosmetic transpose: address alignment, row count interpretation, interleaving order, signedness, FMA opcode, and scalar reference math all change. | Current TPA Tensor evidence is FP32-only (`TensorLoad`, `TensorLoadTranspose32`, `TensorFMA32`). | A generated 8-bit or 16-bit packet with static layout assertions, one lower-precision FMA opcode, wait events for both loads and FMA, scalar reference including signedness/rounding, `tensor_error` check, and objdump evidence for the interleave load encoding. |
| TensorLoadB / TenB-heavy path | TensorLoadB streams B through TenB and must pair with a following TensorFMA that consumes TenB. | Unpaired or incorrectly paired TenB loads can leave the Tensor co-processor in undefined state. Pairing rules differ from L1SCP B loads and are easy to violate when continuations branch or fail. | Current TPA examples intentionally avoid TenB and keep B in L1SCP. TenB behavior is documented from the PRM, headers, and simulator inspection only. | One positive paired TensorLoadB/TensorFMA target that proves the B-in-TenB path, plus one carefully isolated expected-error target for a documented `tensor_error[6]` pairing failure. The target must clear errors, avoid leaving unpaired TenB state before PASS/FAIL, and record waits/objdump. |
| TensorLoadL2Scp | CSR `0x85f` copies memory to L2 scratchpad and uses TensorWait ids 2/3. It is one of the Tensor instructions allowed on H1 by the PRM. | The TPA ownership, visibility, and lifetime of L2 scratchpad data must be defined before it can be a process-local staging mechanism. H1 legality does not by itself define a safe graph contract. | No current TPA evidence target validates TensorLoadL2Scp or L2 scratchpad ownership. | A small target that defines which process/hart owns the L2 scratchpad region, issues `TensorLoadL2Scp`, waits id 2 or 3, consumes the data through a documented path, validates values, checks `tensor_error`, and states whether H1 participation is intentionally covered. |

Promotion checklist for any secondary Tensor family:

1. Name the exact PRM instruction variant and CSR encoding under test.
2. State placement requirements, including H0/H1 legality and any participating
   hart set.
3. Declare row layout, alignment, stride, packet offsets, edge capacities, and
   scratch/workspace budgets with static assertions where possible.
4. Clear `tensor_error`, issue the operation, wait for the correct event id, and
   read `tensor_error` only after the wait.
5. Validate data against a scalar reference or an exact expected error value.
6. Record ET build/run evidence, PASS/FAIL marker behavior, objdump/CSR grep,
   and trace tags sufficient to prove the path executed.
7. Keep positive recipes separate from expected-error or hang-risk experiments.

### TensorQuant

`TensorQuant` is a write to CSR `0x806`. It applies up to ten transformations to
matrix A in the vector register file, stopping at transform `0` (`LAST`). The
local header names transforms such as:

```text
INT32_TO_FP32, FP32_TO_INT32, RELU,
INT32_ADD_ROW, INT32_ADD_COL,
FP32_MUL_ROW, FP32_MUL_COL,
SATINT8, SATUINT8, PACK_128B
```

Some transforms read L1SCP lines. If L1SCP is disabled and such a transform is
requested, the operation sets `tensor_error[4]`. If `frm` is invalid for a
floating transform, the PRM allows an illegal instruction trap.

### TensorStore

`TensorStore` writes vector-register rows to memory through CSR `0x87f` and
bypasses L1D/L2 caches. Row byte count is encoded as `16*(SIZE+1)` for valid
sizes 16, 32, or 64 bytes. The base address omits low 4 bits; individual row
stores are aligned to row size. Cooperation modes are tightly constrained:

| COOP | SIZE | Meaning |
| ---: | ---: | --- |
| 0 | 0,1,3 | one hart stores 16, 32, or 64 bytes/row |
| 1 | 0 | two harts cooperate as 2x16 |
| 1 | 1 | two harts cooperate as 2x32 |
| 3 | 0 | four harts cooperate as 4x16 |

Invalid `COOP/SIZE` combinations set `tensor_error[8]`. Cooperative stores do
not use `tensor_coop`; they depend on `mhartid` address relationships. Treat
cooperative store as a placement contract, not as a local store flag.

`TensorStoreFromScp` writes 64-byte rows from L1SCP to memory. It requires L1SCP
enabled and uses 64-byte aligned base addresses.

### TensorReduce and TensorBroadcast

`TensorSend`, `TensorRecv`, `TensorBroadcast`, and `TensorReduce` are writes to
CSR `0x800` with action bits `1:0`. They transfer vector-register values between
harts and optionally combine them with FADD/FMAX/FMIN/integer ADD/MAX/MIN/MOVE.

Key hazards:

- Send and receive sides must match counts and partner relationships.
- Tree reductions use increasing heights; broadcasts use decreasing heights.
- Only harts participating at a given height need to issue that step.
- Invalid function or target fields set `tensor_error[9]`.
- FADD with invalid `frm` can trap even when count is zero.
- The PRM describes stalls on vector-register access while a send/receive graph
  waits for its counterpart.

Use reduction/broadcast only when the TPA graph and placement make cross-hart
coordination explicit. A local per-process matrix multiply does not need these
instructions.

## 12. TensorWait and ordering hazards

`TensorWait` is a write to CSR `0x830`. It is a fence for Tensor and cache
management effects. One write waits for one event only; wait for multiple events
with multiple writes.

Event ids from the PRM and local headers:

| ID | Event | Local name if present |
| ---: | --- | --- |
| 0 | TensorLoad id 0 complete | `TENSOR_LOAD_WAIT_0` |
| 1 | TensorLoad id 1 complete | `TENSOR_LOAD_WAIT_1` |
| 2 | TensorLoadL2 id 0 complete | not currently used by local examples |
| 3 | TensorLoadL2 id 1 complete | not currently used by local examples |
| 4 | Prefetch id 0 complete | cacheop event |
| 5 | Prefetch id 1 complete | cacheop event |
| 6 | Previous cacheops complete | cacheop wait |
| 7 | Previous Tensor matrix multiplications complete | `TENSOR_FMA_WAIT` |
| 8 | Previous Tensor stores complete | `TENSOR_STORE_WAIT` |
| 9 | Previous Tensor reductions complete | `TENSOR_REDUCE_WAIT` |
| 10 | TensorQuant complete | `TENSOR_QUANT_WAIT` |
| 11-15 | no event / no stall | none |

PRM hazard classes:

1. **Producer -> consumer:** wait after TensorLoad before reading L1SCP; wait
   after TensorFMA/Reduce/Quant before scalar or SIMD FP consumes vector-register
   results; wait after TensorStore before memory reads the written line.
2. **Consumer -> overwrite:** wait before overwriting L1SCP lines or memory that
   an older Tensor/cache instruction still reads.
3. **Write -> write:** wait when two operations may write the same cache line or
   L1SCP line.
4. **Shared resources:** wait between TensorStoreFromScp and TensorFMA/Quant,
   between TensorFMA/Quant and TensorStoreFromScp, and between TensorFMA with B
   in L1SCP and a following TensorLoadB.
5. **Error visibility:** wait for the operation before reading `tensor_error`.

If a bug disappears when an extra `tensor_wait` is added, assume the original
sequence was missing an architectural ordering edge until proven otherwise.

## 13. Alignment, packet layout, and static assertions

Tensor alignment is not a local micro-optimization; it is often an edge-payload
contract.

For FP32 TensorLoad/TensorLoadB/TensorLoadTranspose32:

- base row address is 64-byte aligned;
- row stride is encoded with low six bits clear;
- one 16-float row is exactly one 64-byte cache line;
- a 16-by-16 tile is exactly 1024 bytes;
- if the tile is inside a packet, the matrix field offset must also be 64-byte
  aligned, not only the beginning of the containing struct.

For packed-single row helpers:

- one `flw.ps` / `fsw.ps` covers 32 bytes;
- a 16-float row uses two operations at offsets `0` and `32`;
- using 64-byte row alignment lets the same row serve both Tensor and SIMD code;
- a temporary scalar broadcast array should be at least 32-byte aligned and is
  usually declared 64-byte aligned to preserve the Tensor row invariant.

Current packet-layout pattern:

```c
#define ROW_BYTES (16u * sizeof(float))
#define HEADER_BYTES 64u

struct packet {
    uint32_t id;
    uint8_t header_pad[HEADER_BYTES - sizeof(uint32_t)];
    float a[16][16];
    float b[16][16];
} __attribute__((aligned(64)));

TPA_STATIC_ASSERT(ROW_BYTES == TPA_CACHELINE_BYTES,
                  "rows must be one cacheline");
TPA_STATIC_ASSERT(offsetof(struct packet, a) == HEADER_BYTES,
                  "matrix a must start after aligned header");
TPA_STATIC_ASSERT(offsetof(struct packet, b) % TPA_CACHELINE_BYTES == 0,
                  "matrix b must be cacheline aligned");
```

This mirrors the assertions in `attention/attention_common.h`: fixed packet
headers are padded to one cache line, matrix fields begin on cache-line
boundaries, and payload sizes are asserted so graph-channel capacity does not
silently drift away from C layout.

`kernels/tpa_tensor_alignment.c` is the current minimal target for this
contract. It sends a 2112-byte edge packet with a 64-byte header/reserved region
followed by two 16-by-16 FP32 matrices. The checker requires the received packet
and both matrix bases to be 64-byte aligned before issuing Tensor instructions.

That target deliberately does not claim a misaligned-address TensorLoad error.
TensorLoad encodes row addresses with the low six bits omitted, and the PRM does
not define a separate misalignment bit for that case. Its negative subtest uses
the defined L1-scratchpad-disabled path instead: disable scratchpad, clear
`tensor_error`, issue TensorLoad, wait, require `tensor_error == 0x10`
(`tensor_error[4]`, `L1SCPDIS`), then restore scratchpad mode.

## 14. TPA process integration

Vector/Tensor code belongs inside a process continuation's compute phase:

```text
receive edge payload -> local scalar/SIMD/Tensor compute -> send edge payload
```

Do not blur these storage classes:

| Storage class | Lifetime | Vector/Tensor consequence |
| --- | --- | --- |
| Process state | persistent across continuations | Keep high-level counters, phases, and durable state. Do not treat L1SCP or FP registers as persistent process state. |
| Scratch workspace | per process instance, mapper-budgeted | Use for local aligned staging when it must survive a continuation yield. Declare sizes in manifests and metadata. |
| Immutable model/data image | static image payload | May feed Tensor loads if layout/alignment is declared and generated artifacts are validated. |
| Edge/channel buffers | communication payload | Must satisfy packet layout and capacity contracts; not persistent process state. |
| L1SCP, TenB, vector registers | transient hart-local compute state | Initialize, load, wait, consume, store, and validate within the continuation that owns the compute step. |

A Tensor-capable process kind may require H0-only placement. Express that through
machine descriptions, placement, or mapper metadata; do not assume every context
in every platform accepts Tensor instructions.

Build integration remains the normal TPA path:

1. Write continuation-style C process code.
2. Declare process kinds and ports in `.tpm`.
3. Instantiate/connect in `.tpp`.
4. Use hand `.place` for small examples or mapper output for larger graphs.
5. Integrate with `add_tpa_process()` and `add_tpa_program()`.
6. Build through the top-level ET superbuild.
7. Validate on `erbium_emu` or the target launcher mode required by the job.

Host smoke-test doubles are syntax/unit checks, not ET platform validation. A
host compiler will not execute ET `.ps` or Tensor CSR semantics.

Tensor alignment/error evidence target:

```text
tensor_alignment_source -> tensor_alignment_check
```

`kernels/tpa_tensor_alignment.c` demonstrates a Tensor-ready edge payload and a
controlled expected-error path in a structured TPA program. The source process
fills A with `1.0f` and B with `2.0f`; the checker enables Tensor scratchpad,
clears `tensor_error`, issues two aligned TensorLoad operations, waits for load
ids 0 and 1, issues TensorFMA32, waits for FMA, requires `tensor_error == 0`,
stores the FP register-file result, and validates every element as exactly
`32.0f`. The same checker then runs the L1SCP-disabled negative subtest
described above. Application PASS requires both the aligned success path and the
expected error value.

Memory categories remain separate: the packet is edge/channel payload, the
checker workspace stores only receive pointer/length continuation state, and the
advertised transient scratch peak is one 16-by-16 FP32 result matrix.

## 15. Case study: Tensor matrix multiply process

`kernels/tpa_tensor_matmul.c` demonstrates a compact TensorFMA process.
Important constants:

```c
#define TM_TENSOR_N 16u
#define TM_TENSOR_TL0_START 0u
#define TM_TENSOR_TL1_START 32u
#define TM_TENSOR_ROWS (TM_TENSOR_N - 1u)
#define TM_TENSOR_COLS (TM_TENSOR_N - 1u)
#define TM_TENSOR_BCOLS ((TM_TENSOR_N / 4u) - 1u)
```

The source establishes three useful patterns:

1. **Workspace and status storage are explicit.** Aligned input/accumulator
   buffers live in process workspace or static aligned words. Static assertions
   cap workspace size.
2. **Tensor setup is once-per-hart.** A static `tm_tensor_ready[ARCH_NR_HARTS]`
   array guards scratchpad setup. The code uses `arch_hart_id()` only to index
   readiness state, not to decide graph placement.
3. **TensorFMA lives inside an ordinary continuation.** Feed/check processes
   still use TPA receive/send operations. Tensor instructions are only a local
   compute implementation.

Representative inner loop shape:

```c
tensor_load(0, 0, TM_TENSOR_TL0_START, 0, 0,
            (uint64_t)a_tile, 0, TM_TENSOR_ROWS, row_bytes, 0);
tensor_load(0, 0, TM_TENSOR_TL1_START, 0, 1,
            (uint64_t)b_tile, 0, TM_TENSOR_ROWS, row_bytes, 1);
tensor_wait(TENSOR_LOAD_WAIT_0);
tensor_fma(0, TM_TENSOR_BCOLS, TM_TENSOR_ROWS, TM_TENSOR_COLS,
           0, 0, 0, 0, 1,
           TM_TENSOR_TL1_START, TM_TENSOR_TL0_START,
           TM_TENSOR_FP32, 0);
tensor_wait(TENSOR_FMA_WAIT);
```

Read this as a pattern source, not as proof of a globally optimal wait schedule.
If new code consumes both load ids, wait for both ids unless the PRM and local
validation packet justify otherwise.

## 16. Case study: fixed 16-by-16 attention helpers

`attention/attention_et.h` is useful because it packages ET-specific helpers in
one file while leaving process graph semantics in the `.tpm/.tpp` path.

Key helper roles:

- `attention_et_enable_tensor_scratchpad()` performs the
  cache/scratchpad/mask/error setup.
- `attention_et_store_rf_16x16()` stores `f0..f31` back to memory with `fsw.ps`
  pairs.
- `attention_et_matmul_16x16()` loads A and B into L1SCP, waits for both load
  ids, issues TensorFMA32, waits for FMA, stores the result, and checks
  `tensor_error`.
- `attention_et_scale_matrix_ps()` applies row-wise scalar scaling with
  packed-single multiply after the TensorFMA product.
- `attention_et_sub_max_row_ps()` is used only by the opt-in
  `tpa_fast_attention_ps_softmax_subtract.elf` target. It broadcasts one scalar
  row maximum into a packed row, subtracts it from two eight-lane halves with
  `fsub.ps`, and stores a prepared row for the existing scalar exponent/sum
  loop.

This shows a good separation of concerns:

```text
process source:
  receive packet, call helper, validate/fail/send

ET helper:
  masks, L1SCP, TensorLoad/FMA/Wait, vector-register store,
  packed-single row transform, tensor_error check

common header:
  layout sizes, alignment assertions, trace tags, scalar reference math
```

The evidence packet shows extension use in disassembly and trace markers, but it
also documents that speedup is not claimed. The packed-single subtract variant
is similarly a correctness/evidence experiment: it isolates one softmax substep
and keeps baseline/optimized trace evidence separate. Use the source as an
instruction pattern and validation example, not as a performance conclusion.

## 17. Writing inline assembly that survives review

Rules for inline packed-single assembly:

1. Set `m0` in the same helper or establish a documented caller contract.
2. List every FP register clobbered by explicit register names.
3. Add `memory` when loads/stores or CSRs must not move across C memory
   operations.
4. Keep the assembly block small enough that objdump and code review can match
   it to source intent.
5. Avoid silently mixing scalar FP and packed FP on the same register live range.
6. Prefer static alignment assertions over comments.
7. Provide a scalar reference path for validation when numerical behavior is not
   bit-identical.

Tensor wrapper calls are already inline assembly. Still review them as CSR
writes. For example, `tensor_fma()` has no `memory` clobber in the current local
header, so caller-side fences/waits and surrounding memory clobbers matter when
C memory and Tensor state interact.

Objdump caveat: ET binutils support may lag mnemonics. Some packed-single
instructions can appear as `.insn` or raw encodings while Tensor wrappers appear
as CSR writes such as `csrw 0x83f`, `csrw 0x801`, and `csrw 0x830`. Absence of a
pretty mnemonic is not absence of the instruction.

## 18. Error-handling template

A minimal Tensor sequence should look like this in structure:

```c
static int run_tensor_tile(float out[16][16],
                           const float a[16][16],
                           const float b[16][16])
{
    asm volatile("csrwi tensor_error, 0" ::: "memory");

    tensor_load(0, 0, A_START, 0, 0,
                (uint64_t)a, 0, 15, 64, 0);
    tensor_load(0, 0, B_START, 0, 0,
                (uint64_t)b, 0, 15, 64, 1);
    tensor_wait(TENSOR_LOAD_WAIT_0);
    tensor_wait(TENSOR_LOAD_WAIT_1);

    tensor_fma(0, 3, 15, 15, 0, 0, 0, 0, 0,
               B_START, A_START, 0, 1);
    tensor_wait(TENSOR_FMA_WAIT);

    store_vector_regs(out);

    tensor_wait(TENSOR_FMA_WAIT);
    unsigned long err = get_tensor_error();
    if (err != 0)
        return -1;
    return 0;
}
```

Notes:

- The second FMA wait before reading `tensor_error` is redundant if no operation
  intervened after the first FMA wait. It is shown to stress that the error read
  belongs after the event wait, not before it.
- Use symbolic constants for `A_START`, `B_START`, and wait ids. Raw numbers are
  acceptable in wrappers but poor review targets in process code.
- On TPA failure paths, emit a trace or PASS/FAIL marker consistent with nearby
  tests before invoking the project failure macro.

## 19. Disassembly and evidence workflow

A reviewable ET vector/Tensor change needs more than a successful compile.
Collect the smallest evidence appropriate for the change.

Recommended commands on an Erbium ET build:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_tensor_matmul.elf
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_tensor_matmul.elf \
  -max_cycles 1000000
```

For a new process target, substitute its ELF path and target. A PASS marker or
explicit process-level validation is required for runtime claims.

Disassembly checks:

```sh
riscv64-unknown-elf-objdump -d path/to/program.elf > /tmp/program.dis
rg -n 'flw\.ps|fsw\.ps|fmul\.ps|fadd\.ps|frcp\.ps|\.insn' /tmp/program.dis
rg -n 'csrw\s+0x83f|csrw\s+0x801|csrw\s+0x830|csrw\s+0x808' /tmp/program.dis
```

Interpretation:

- `flw.ps` / `fsw.ps` evidence proves packed-single load/store selection when
  decoded by the toolchain.
- `.insn` evidence may still be valid if the source inline assembly and opcode
  encoding are reviewed.
- `csrw 0x83f` is TensorLoad family.
- `csrw 0x801` is TensorFMA family.
- `csrw 0x830` is TensorWait.
- `csrw 0x808` or `csrwi tensor_error, 0` shows error clear; `csrr 0x808` shows
  error read.

Validated packed-single row micro-example:

- `tpa_packed_single_row.elf` is the current minimal structured row-local
  packed-single target. It is built through the top-level ET superbuild and the
  `.c + .tpm + .tpp + .place` TPA path.
- The device-subbuild CTest `tpa_packed_single_row` passes by observing the
  application PASS marker. A direct Erbium run reports PASS at cycle `10888` and
  then returns raw exit `1` because the emulator reports sleeping harts after
  PASS; treat that as PASS-marker evidence with the normal direct-emulator
  caveat.
- Current objdump output for that target decodes the expected row sequence:

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

That target proves a narrow fact: the current Erbium toolchain can build, run,
and decode this one packed-single row transform. It does not prove a speedup,
platform portability, or any broader kernel optimization.

Tensor alignment/error micro-example evidence:

- `tpa_tensor_alignment.elf` is built through the top-level ET superbuild and
  the `.c + .tpm + .tpp + .place` TPA path.
- The device-subbuild CTest `tpa_tensor_alignment` passes by observing the
  application PASS marker. A direct Erbium DEBUG run reports aligned begin/end
  tags `0x7a100001`/`0x7a100002`, negative begin `0x7a100003`, the expected
  `TensorLoad with L1SCP disabled!!` warning, negative error tag `0x7a100410`,
  negative end `0x7a100005`, PASS tag `0x7a1000ff`, and application PASS at
  cycle `11775`; raw exit is `1` afterward because of the normal post-PASS
  sleeping-harts caveat.
- Objdump for the target decodes Tensor CSR names and packed-single result
  stores, including `csrw tensor_load` (CSR `0x83f`), `csrw tensor_fma` (CSR
  `0x801`), `csrw tensor_wait` (CSR `0x830`), `csrwi`/`csrr tensor_error`, and
  `fsw.ps` stores.

This supports a narrow claim: a 64-byte-header Tensor-ready edge packet can feed
an aligned TensorLoad/TensorFMA path, and a scratchpad-disabled TensorLoad can
be treated as an expected PASS subcondition when it reports `tensor_error[4]`.
It does not prove a misaligned-address TensorLoad error, performance speedup, or
platform portability.

Attention packed-single subtract-max evidence:

- `tpa_fast_attention.elf` remains the baseline mapped target.
- `tpa_fast_attention_ps_softmax_subtract.elf` is the opt-in variant that uses
  the same graph/mapping path but compiles `attention_softmax.c` with
  `ATTENTION_ENABLE_PS_SOFTMAX_SUBTRACT=1`.
- Runtime validation under Erbium reports PASS markers for both mapped targets
  and for `tpa_fast_attention_serial.elf`; direct emulator raw exit code may
  still be `1` after PASS due the known sleeping-harts caveat.
- Objdump for the variant decodes the new helper as `flw.ps`, `fsub.ps`, and
  `fsw.ps` around the softmax path. This proves instruction selection for the
  subtract-max preparation step only.
- Filtered trace extraction with `tools/trace/parse_attention_trace.py` keeps
  baseline and variant measurements separate. The current observed mapped trace
  spans are effectively unchanged at the program level and are not a speedup
  claim; the output scalar validation span still dominates PASS time.

PMU counter sanity evidence:

- `tpa_pmu_counter_sanity.elf` is the current narrow structured Erbium PMU CSR
  sanity target. It is built through the top-level ET superbuild and the
  `.c + .tpm + .tpp + .place` TPA path.
- The target validates only a documented M-mode CSR subset: `mcycle` and
  `minstret` read as zero, `mhpmcounter9` reads as zero, `mhpmevent3 = NONE`
  leaves `mhpmcounter3` unchanged across deterministic work, and
  `mhpmevent3 = RETIRED_INST0` makes `mhpmcounter3` advance across two fixed
  instruction loops.
- The Erbium CTest uses `-minions 0x1` to reduce shared-counter aggregation
  hazards. A direct DEBUG run records raw `validation0` trace values showing
  `retired_delta1 = 648` and `retired_delta2 = 1289`, followed by an
  application PASS marker at cycle `10363`; the raw direct-emulator exit is
  still `1` afterward because of the normal post-PASS sleeping-harts caveat.
- The target intentionally does not write a PMU-control ESR. The local PRM says
  the PMU requires ESR control, but the Erbium and ET-SoC-1 ESR addresses and
  bit semantics are not yet documented in this repository as a safe portable
  process-level API. Treat this target as Erbium emulator PMU-access sanity, not
  as a complete operational recipe for ET-SoC-1, silicon, or published
  per-kernel measurement.

Trace checks should prove that the optimized path actually ran. Trace tags are
application-specific; the pattern is not:

```text
begin tag -> Tensor/SIMD region begin -> Tensor/SIMD region end -> validate -> PASS
```

Do not claim speedup unless the evidence includes all of:

1. ET build and target identity.
2. Runtime PASS/FAIL or equivalent correctness result.
3. Disassembly or trace evidence that extension instructions executed.
4. Comparable baseline and optimized measurements under the same conditions.
5. Explanation of measurement source, cycles, warmup, and noise.

## 20. Debugging playbook

| Symptom | Likely causes | First checks |
| --- | --- | --- |
| Illegal instruction on target | H1 placement; unsupported mnemonic; invalid Tensor round mode; toolchain mismatch | Check machine context name, `mhartid`, objdump, and wrapper encoding. |
| `tensor_error[4]` | L1SCP disabled | Confirm cache-control transition and H0 execution. |
| `tensor_error[1]` | Illegal TensorLoad transform or TenB command | Decode `tensor_load` transformation and `use_tenb` fields. |
| `tensor_error[6]` | TensorLoadB/FMA pairing error | Remove TenB first; use L1SCP B; then reintroduce paired TenB. |
| `tensor_error[7]` | Tensor/cache memory fault | Check row alignment, page mapping, row count, stride, and masked rows. |
| `tensor_error[8]` | Invalid cooperative TensorStore size | Decode `COOP/SIZE`; avoid cooperative store until placement is explicit. |
| Wrong inactive SIMD lanes | `m0` partial mask left stale destination lanes | Initialize destination or force all-lanes mask. |
| All-zero upper SIMD lanes | Scalar FP instruction wrote the same FP register | Separate scalar and packed register lifetimes. |
| First tile correct, later tiles wrong | Missing wait before overwriting L1SCP or memory | Add waits on load/store/FMA/cacheop events. |
| Works with scalar reference only | Packet layout or row stride mismatch | Assert offsets, sizes, and `row_bytes == 64`. |
| Error read is zero but result stale | Read before event completion or missing result store | Place `tensor_wait` before `get_tensor_error()` and before scalar/SIMD readback. |
| Disassembly lacks pretty `.ps` mnemonic | Binutils decoder gap | Compare source inline assembly and raw `.insn`; do not assume compile removed it. |
| Cooperative load hangs or corrupts data | Group mismatch or reused group id | Verify every participating hart executes same variant/address/group and has external synchronization. |

When in doubt, reduce to:

1. one H0 process;
2. one aligned 16-by-16 tile;
3. non-cooperative TensorLoad to L1SCP;
4. wait both load ids;
5. TensorFMA32 product-only;
6. wait FMA;
7. store vector registers;
8. compare against scalar reference;
9. read `tensor_error` after waits.

## 21. Code review checklist

Before approving vector/Tensor code, ask:

- Does the process kind have a placement story compatible with Tensor H0 rules?
- Are row bases, field offsets, and strides asserted for Tensor alignment?
- Are edge/channel payload sizes updated when structs change?
- Is `m0` initialized before every packed-single helper that depends on it?
- Are inactive packed lanes either irrelevant or initialized?
- Are scalar FP instructions kept away from live packed registers?
- Are Tensor loads, FMA, stores, quant, reductions, broadcasts, and cacheops
  followed by the needed `tensor_wait` events?
- Is `tensor_error` cleared before the sequence and read after waits?
- Are L1 scratchpad mode transitions guarded by cache eviction/fence rules?
- Are TenB and cooperative operations avoided unless the pairing/group contract
  is documented and tested?
- Does disassembly show expected `.ps`, `.insn`, or Tensor CSR writes?
- Does runtime validation include PASS/FAIL evidence on an ET target when ET
  behavior is claimed?
- Does documentation state exactly what was measured, and avoid speedup claims
  without comparable baseline data?

## 22. Common design patterns

### Pattern A: SIMD row transform

Use when each row is 8- or 16-lane independent and no cross-row operation is
needed.

```text
set m0 -> flw.ps low/high -> compute .ps -> fsw.ps low/high
```

Good for row scale, clamp, elementwise add/multiply, and copy/convert kernels.

`kernels/tpa_packed_single_row.c` is the smallest current structured example of this pattern. It uses a source -> compute -> check graph, channel-backed 64-byte row payloads, all-lane `m0`, two `flw.ps` loads, row-local `fmul.ps` plus `fadd.ps`, two `fsw.ps` stores, and scalar validation of every lane. Treat it as correctness and tooling evidence, not as a benchmark.

### Pattern B: Tensor local tile product

Use when each process owns an aligned MxK and KxN tile and writes a local MxN
result.

```text
setup scratchpad -> clear error -> load A -> load B -> wait loads ->
TensorFMA32 -> wait FMA -> store vector regs -> read error -> send
```

Good for fixed 16-by-16 FP32 products and as the baseline for lower-precision
experiments.

`tpa_tensor_alignment.elf` is the current smallest validated target for the
layout/error side of this pattern: it proves one aligned FP32 TensorLoad/FMA
packet path and one expected `tensor_error[4]` path, not a misaligned-address
error.

### Pattern C: Tensor plus SIMD post-process

Use when Tensor computes a tile and a lane-wise transform follows.

```text
TensorFMA -> wait FMA -> fsw.ps to aligned matrix ->
for rows: flw.ps -> lane transform -> fsw.ps -> scalar/reference validation
```

Do not run packed-single instructions against vector-register rows still owned
by an in-flight TensorFMA.

### Pattern D: Cross-hart reduction or broadcast

Use only when the dataflow graph actually wants a cross-hart collective. This is
a design sketch, not a validated repository recipe yet.

```text
explicit placement/grouping -> matching send/recv or tree actions -> wait 9 ->
error check -> continue
```

This is not a replacement for local accumulation unless placement and
synchronization are part of the design. Promote it to a recipe only after a
fixed-placement evidence target validates compatible participant behavior,
trace visibility, scalar results, wait event 9, and `tensor_error` handling.

## 23. Open questions and evidence gaps

These are not blockers for current documented examples, but they should be
answered before turning patterns into a reusable ET math library:

- Which packed-single mnemonics are fully decoded by the currently pinned ET
  binutils, and which still appear as `.insn`?
- What project-level PMU-control API and validation evidence are needed before
  the current Erbium CSR sanity target can become a reusable measurement recipe
  for broader platforms or published per-kernel measurements?
- What is the preferred project-level abstraction for H0-only process-kind
  requirements when future machine descriptions expose H1 contexts?
- Should conservative new TensorFMA helpers always wait for both load ids even
  when older source waited for one id in a validated example?
- Is there any deterministic, documented way to observe a misaligned-address
  TensorLoad failure, given that the row address encoding omits low six bits?
- Which TensorStore and TensorStoreFromScp positive/negative subtests should be
  validated first, and how should ordinary-memory visibility be checked after
  TensorWait event 8?
- Which cooperative TensorLoad/TensorStore patterns are worth validating first,
  and what graph-level synchronization should they require?
- Which TensorLoadInterleave8/16 layout should be the first lower-precision FMA
  evidence target, and what signedness/rounding reference should it use?
- Which TenB paired-load target can prove the B-in-TenB path without leaving
  undefined unpaired state on failure?
- Which TensorQuant transformations are needed by current kernels, and what
  scalar references should define their tolerance?
- What TPA ownership rule would make TensorLoadL2Scp safe to document as a
  process-local staging primitive?

Document answers in source-adjacent comments, tests, or this guide when they are
validated.

## 24. Minimal glossary

| Term | Meaning in this guide |
| --- | --- |
| H0/H1 | The two harts in one ET Minion. Most Tensor instructions are H0-only. |
| `.ps` | Packed-single SIMD instruction operating on eight FP32 lanes in one widened FP register under `m0`. |
| `.pi` | Packed-integer instruction using the widened FP register file as int lanes. |
| `m0` | Mask register that predicates packed-single and packed-integer lanes. |
| `tensor_mask` | CSR `0x805`; row mask for TensorLoad/TensorFMA/cacheop masking. |
| L1SCP | L1 scratchpad mode storage used by Tensor load/FMA/store operations. |
| TenB | Tensor B operand storage loaded by TensorLoadB and consumed by TensorFMA. |
| TenC | Tensor C storage used by integer Tensor matrix operations. |
| TensorFMA32 | FP32 Tensor matrix multiply/accumulate instruction encoded by CSR `0x801`. |
| TensorWait | CSR `0x830` write that waits for one Tensor/cache event. |
| `tensor_error` | Sticky software-cleared Tensor/cache error CSR `0x808`. |
| Edge buffer | TPA communication payload storage; not the same as process state or Tensor scratchpad. |
| Scratch workspace | Mapper-budgeted per-process storage declared in process metadata; not the same as L1SCP. |
