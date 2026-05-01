#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 TRACE_LOG OUT_DIR" >&2
    exit 1
fi

log=$1
out=$2

mkdir -p "$out"

for hart in $(seq 0 15); do
    minion=$((hart >> 1))
    lane=$((hart & 1))
    tag="\\[H${hart} "
    base="m${minion}_h${lane}"

    grep -E "${tag}" "$log" > "${out}/${base}.log" || true
    grep -E "^[0-9]+: DEBUG EMU: ${tag}.* I\\([A-Z]\\): 0x" "$log" \
        > "${out}/${base}.inst.log" || true
done
