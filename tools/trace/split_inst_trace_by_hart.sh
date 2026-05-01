#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 TRACE_LOG OUT_DIR" >&2
    exit 1
fi

log=$1
out=$2

mkdir -p "$out"

awk -v out="$out" '
/^[0-9]+: DEBUG EMU: \[H[0-9]+ .* I\([A-Z]\): 0x/ {
    if (!match($0, /\[H([0-9]+)/, m))
        next;

    hart = m[1] + 0;
    minion = int(hart / 2);
    lane = hart % 2;
    key = sprintf("%s/m%d_h%d.inst.log.gz", out, minion, lane);

    if (!(key in pipev))
        pipev[key] = "gzip -c > " key;

    print $0 | pipev[key];
}

END {
    for (k in pipev)
        close(pipev[k]);
}
' "$log"
