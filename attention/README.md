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

## Targets

Build through the top-level ET superbuild:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_fast_attention.elf
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
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/attention/tpa_fast_attention_serial.elf \
  -max_cycles 5000000
```

`attention.place` distributes the four score/softmax head pipelines across
runtime harts `2`, `4`, `6`, and `8`, with source and output/check on hart `0`.
`attention_serial.place` keeps source/output on hart `0` but maps every
score/softmax stage to hart `2` for a trace-inspectable baseline. The repository
must not treat that layout as a measured speedup claim unless traces are measured
and compared separately.

## Trace tags

The demo emits stable 32-bit trace tags with `arch_trace()`:

| Tag | Meaning |
| --- | --- |
| `0xa7700000` | QKV generator/program begin |
| `0xa7710000 | head` | score compute begin for head `0..3` |
| `0xa7720000 | head` | score compute end for head `0..3` |
| `0xa7730000 | head` | softmax compute begin for head `0..3` |
| `0xa7740000 | head` | softmax compute end for head `0..3` |
| `0xa7750000` | output/check begin |
| `0xa7750001` | output/check end |
| `0xa77f00ff` | validation passed immediately before `TEST_PASS` |
| `0xa77f00ee` | validation failed immediately before `TEST_FAIL` |

The emulator PASS/FAIL markers still come from `TEST_PASS` / `TEST_FAIL`; the
trace tags are for inspection and timing comparison only.
