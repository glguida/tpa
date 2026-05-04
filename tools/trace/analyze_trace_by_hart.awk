function pct(part, total)
{
    if (!total)
        return "0.00";
    return sprintf("%.2f", (100.0 * part) / total);
}

function kind_of(sym)
{
    if (sym ~ /^(_start|main|send_resume|recv_resume|tpa_)/)
        return "runtime";
    if (sym ~ /^(systolic_|attention_)/)
        return "app";
    return "other";
}

function find_sym(pc,    i)
{
    for (i = 1; i < nsym; i++) {
        if (pc >= sym_addr[i] && pc < sym_addr[i + 1])
            return sym_name[i];
    }

    if (nsym && pc >= sym_addr[nsym])
        return sym_name[nsym];

    return "unknown";
}

BEGIN {
    OFS = "\t";
}

FNR == NR {
    nsym++;
    sym_addr[nsym] = strtonum("0x" $1);
    sym_name[nsym] = $2;
    next;
}

/^[0-9]+: DEBUG EMU: \[H[0-9]+ .* I\([A-Z]\): 0x[0-9a-f]+/ {
    cycle = $1;
    sub(/:$/, "", cycle);
    cycle += 0;

    if (from_cycle && cycle < from_cycle)
        next;
    if (to_cycle && cycle > to_cycle)
        next;

    if (match($0, /\[H([0-9]+)/, m))
        hart = m[1] + 0;
    else
        next;

    if (match($0, /I\([A-Z]\): 0x([0-9a-f]+)/, m))
        pc = strtonum("0x" m[1]);
    else
        next;

    sym = find_sym(pc);
    kind = kind_of(sym);

    instr_total[hart]++;
    instr_kind[hart, kind]++;
    instr_sym[hart, sym]++;

    if (seen[hart]) {
        dt = cycle - last_cycle[hart];
        if (dt < 0)
            dt = 0;

        span_total[hart] += dt;
        span_kind[hart, last_kind[hart]] += dt;
        span_sym[hart, last_sym[hart]] += dt;
    }

    seen[hart] = 1;
    last_cycle[hart] = cycle;
    last_kind[hart] = kind;
    last_sym[hart] = sym;
}

END {
    print "hart", "minion", "lane", \
          "instr_total", "rt_instr_pct", "nonrt_instr_pct", \
          "app_instr_pct", "other_instr_pct", \
          "span_total", "rt_span_pct", "nonrt_span_pct", \
          "app_span_pct", "other_span_pct";

    for (hart = 0; hart < 16; hart++) {
        if (!instr_total[hart])
            continue;

        minion = int(hart / 2);
        lane = hart % 2;

        print hart, minion, lane, \
              instr_total[hart], \
              pct(instr_kind[hart, "runtime"], instr_total[hart]), \
              pct(instr_kind[hart, "app"] + instr_kind[hart, "other"],
                  instr_total[hart]), \
              pct(instr_kind[hart, "app"], instr_total[hart]), \
              pct(instr_kind[hart, "other"], instr_total[hart]), \
              span_total[hart], \
              pct(span_kind[hart, "runtime"], span_total[hart]), \
              pct(span_kind[hart, "app"] + span_kind[hart, "other"],
                  span_total[hart]), \
              pct(span_kind[hart, "app"], span_total[hart]), \
              pct(span_kind[hart, "other"], span_total[hart]);

        if (want_top) {
            top_rt = "";
            top_rt_n = -1;
            top_app = "";
            top_app_n = -1;

            for (k in instr_sym) {
                split(k, a, SUBSEP);
                if (a[1] + 0 != hart)
                    continue;

                sym = a[2];
                n = instr_sym[k];

                if (kind_of(sym) == "runtime" && n > top_rt_n) {
                    top_rt = sym;
                    top_rt_n = n;
                }

                if (kind_of(sym) == "app" && n > top_app_n) {
                    top_app = sym;
                    top_app_n = n;
                }
            }

            printf("# hart %d top_runtime=%s(%d) top_app=%s(%d)\n",
                   hart, top_rt, top_rt_n, top_app, top_app_n);
        }
    }
}
