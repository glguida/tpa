# Trace Analysis Tools

These scripts are ported from the original TPA repository for working with large
Erbium emulator logs.

## Split logs by hart

```sh
tools/trace/split_trace_by_hart.sh /tmp/erbium.log /tmp/tpa-trace-by-hart
```

This writes one full log and one instruction-only log per Erbium hart using
`m<minion>_h<lane>` filenames.

For very large instruction traces, write gzip-compressed per-hart instruction
logs directly:

```sh
tools/trace/split_inst_trace_by_hart.sh /tmp/erbium.log /tmp/tpa-inst-by-hart
```

## Attribute instruction traces to symbols

```sh
tools/trace/analyze_trace_by_hart.sh \
  build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  /tmp/tpa-trace-by-hart/m0_h0.inst.log \
  --top
```

`analyze_trace_by_hart.sh` uses `riscv64-unknown-elf-nm` to map instruction PCs
back to text symbols and supports optional cycle windows:

```sh
tools/trace/analyze_trace_by_hart.sh ELF TRACE_LOG --from 1000 --to 5000
```

`TRACE_LOG` may be plain text or `.gz`.

## Requirements

- ET RISC-V binutils (`riscv64-unknown-elf-nm`) on `PATH`.
- Standard POSIX shell tools (`awk`, `grep`, `gzip`).
