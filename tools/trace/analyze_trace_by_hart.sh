#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 7 ]; then
    echo "usage: $0 ELF TRACE_LOG [--top] [--from CYCLE] [--to CYCLE]" >&2
    exit 1
fi

elf=$1
log=$2
shift 2

top=0
from=0
to=0

while [ "$#" -gt 0 ]; do
    case "$1" in
    --top)
        top=1
        ;;
    --from)
        shift
        from=${1:?missing cycle after --from}
        ;;
    --to)
        shift
        to=${1:?missing cycle after --to}
        ;;
    *)
        echo "unknown option: $1" >&2
        exit 1
        ;;
    esac
    shift
done

nm_file=$(mktemp)
trap 'rm -f "$nm_file"' EXIT

riscv64-unknown-elf-nm -n "$elf" \
    | awk '$2 ~ /[tT]/ { print $1, $3 }' > "$nm_file"

run_awk()
{
    if [[ "$log" = *.gz ]]; then
        if [ "$top" -eq 1 ]; then
            awk -v want_top=1 -v from_cycle="$from" -v to_cycle="$to" \
                -f "$(dirname "$0")/analyze_trace_by_hart.awk" \
                "$nm_file" <(gzip -dc "$log")
        else
            awk -v from_cycle="$from" -v to_cycle="$to" \
                -f "$(dirname "$0")/analyze_trace_by_hart.awk" \
                "$nm_file" <(gzip -dc "$log")
        fi
        return
    fi

    if [ "$top" -eq 1 ]; then
        awk -v want_top=1 -v from_cycle="$from" -v to_cycle="$to" \
            -f "$(dirname "$0")/analyze_trace_by_hart.awk" \
            "$nm_file" "$log"
    else
        awk -v from_cycle="$from" -v to_cycle="$to" \
            -f "$(dirname "$0")/analyze_trace_by_hart.awk" \
            "$nm_file" "$log"
    fi
}

run_awk
