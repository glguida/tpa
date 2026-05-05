# TPA fast-attention demo

This directory contains a structured TPA graph program for a fixed
scaled dot-product attention workload:

- sequence length: 16
- embedding dimension: 64
- heads: 4
- per-head dimension: 16

The graph fans out one deterministic Q/K/V source into four per-head pipelines,
then validates the concatenated logical `[16][64]` output against a serial
reference in `attention_output.c`.

For ET packed-single SIMD and Tensor guidance specific to this 16-by-16 per-head
shape, see `../docs/et-simd-tensor-kernel-notes.md`. The channel packets reserve
a 64-byte header before the first matrix so every 16-float row starts on a
64-byte boundary suitable for TensorLoad and TensorLoadTranspose32. Do not infer
speedup from placement or instruction candidates alone; update this README with
performance claims only after the optimized kernels have ET build, emulator PASS,
extension use, and baseline-vs-optimized measurement evidence.

## Targets

Build through the top-level ET superbuild. Pass `-DPYTHON` when selecting a
specific planner Python environment:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python3)
cmake --build build-et-erbium --target tpa_fast_attention_map_mapped_program
cmake --build build-et-erbium --target tpa_fast_attention.elf
cmake --build build-et-erbium --target tpa_fast_attention_ps_softmax_subtract_map_mapped_program
cmake --build build-et-erbium --target tpa_fast_attention_ps_softmax_subtract.elf
cmake --build build-et-erbium --target tpa_fast_attention_serial.elf
```

Run under Erbium emulator:

```sh
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/attention/tpa_fast_attention.elf \
  -max_cycles 5000000
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/attention/tpa_fast_attention_ps_softmax_subtract.elf \
  -max_cycles 5000000
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/attention/tpa_fast_attention_serial.elf \
  -max_cycles 5000000
```

The primary `tpa_fast_attention.elf` target uses `tpa-map-program` during the
CMake device build. The mapper reads `attention.tpp`, process metadata extracted
from the built process objects, `attention_compute_costs.json`, and
`machines/erbium.json`, then writes the consumed placement and map artifacts
under the build-tree `attention/planner/` directory. The current mapper output
distributes the four score/softmax head pipelines across four Erbium runtime
harts inside the documented `-minions 0x1f` mask.

`tpa_fast_attention_ps_softmax_subtract.elf` is an opt-in experiment target that
keeps the same graph and mapping path but compiles the softmax process with
`ATTENTION_ENABLE_PS_SOFTMAX_SUBTRACT=1`. Only the row-local
`score[col] - max_score` preparation step changes to packed-single SIMD; row
maximum, exponent approximation, row sum, reciprocal, and final row scaling stay
on the existing baseline paths. Keep its evidence separate from the baseline and
do not treat small trace differences as a speedup claim without a controlled
comparison.

`attention_serial.place` is a checked-in baseline placement: it keeps
source/output on hart `0` and maps every score/softmax stage to hart `2` for a
trace-inspectable comparison target. The repository must not treat any of the
mapped, packed-single-subtract, or serial layouts as a measured speedup claim
unless traces are measured and compared separately.

## Trace tags

The demo emits stable 32-bit trace tags with `arch_trace()`:

| Tag | Meaning |
| --- | --- |
| `0xa7700000` | QKV generator/program begin |
| `0xa7710000 | head` | score compute begin for head `0..3` |
| `0xa7720000 | head` | score compute end for head `0..3` |
| `0xa7730000 | head` | softmax compute begin for head `0..3` |
| `0xa7740000 | head` | softmax compute end for head `0..3` |
| `0xa7750000` | output/check process begin; includes waiting for softmax packets |
| `0xa7750001` | output/check process end after product and scalar validation |
| `0xa7750002` | all four softmax packets received and checked; product phase can begin |
| `0xa7760000 | head` | output product begin for head `0..3` (`softmax * V` TensorFMA path) |
| `0xa7770000 | head` | output product end for head `0..3` |
| `0xa7780000 | head` | scalar reference validation begin for head `0..3` |
| `0xa7790000 | head` | scalar reference validation end for head `0..3` |
| `0xa77f00ff` | validation passed immediately before `TEST_PASS` |
| `0xa77f00ee` | validation failed immediately before `TEST_FAIL` |

The emulator PASS/FAIL markers still come from `TEST_PASS` / `TEST_FAIL`; the
trace tags are for inspection and timing comparison only. The output process
currently computes all per-head TensorFMA output products before starting the
independent scalar reference validation loop, so `0xa776...` spans measure
production output work while `0xa778...` spans measure validation work.

### Extracting trace-tag cycles

INFO-level `erbium_emu` output shows PASS/FAIL markers but not all `arch_trace()`
tag writes. To recover tag cycles, enable DEBUG logging and filter the
`validation0` CSR writes. Raw DEBUG logs can exceed 1 GiB per logged hart, so use
`-lt` to limit harts and stream-filter when only tag timing is needed:

```sh
/opt/et/bin/erbium_emu \
  -l -lt 0 -lt 2 -lt 4 -lt 6 -lt 8 \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/attention/tpa_fast_attention.elf \
  -max_cycles 5000000 2>&1 \
  | awk '/validation0 =/ || /Signal end test/ { print }'
```

The cycle is the leading number before the first colon. The printed
`validation0` value may be sign-extended; compare the low 32 bits with the tag
values above. To turn filtered logs into JSON/Markdown timing tables, use
`tools/trace/parse_attention_trace.py` from the repository root.
