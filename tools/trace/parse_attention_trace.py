#!/usr/bin/env python3
"""Parse filtered Erbium attention validation0 traces.

The input should be a small filtered log produced from DEBUG `erbium_emu` output,
for example lines matching `validation0 =` and `Signal end test`. Raw DEBUG or
instruction logs can be multi-GiB; keep those out of the repository.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

ATTENTION_HEADS = 4

TAG_NAMES = {
    0xA7700000: "program_begin",
    0xA7750000: "output_begin",
    0xA7750001: "output_end",
    0xA7750002: "output_all_received",
    0xA77F00FF: "pass_tag",
    0xA77F00EE: "fail_tag",
}

HEAD_BASES = {
    0xA7710000: "score_begin",
    0xA7720000: "score_end",
    0xA7730000: "softmax_begin",
    0xA7740000: "softmax_end",
    0xA7760000: "product_begin",
    0xA7770000: "product_end",
    0xA7780000: "validate_begin",
    0xA7790000: "validate_end",
}

REQUIRED_EVENTS = (
    "program_begin",
    "output_begin",
    "output_all_received",
    "output_end",
    "pass_tag",
    "pass_marker",
)
REQUIRED_HEAD_EVENTS = (
    "score_begin",
    "score_end",
    "softmax_begin",
    "softmax_end",
    "product_begin",
    "product_end",
    "validate_begin",
    "validate_end",
)

VALIDATION_RE = re.compile(r"^(\d+): .*validation0 = (0x[0-9a-fA-F]+)")
PASS_RE = re.compile(r"^(\d+): .*Signal end test with PASS")


def span(events: dict[str, int], begin: str, end: str) -> int | None:
    if begin not in events or end not in events:
        return None
    return events[end] - events[begin]


def none_sum(values: list[int | None]) -> int | None:
    if any(v is None for v in values):
        return None
    return sum(v for v in values if v is not None)


def parse_log(path: Path) -> dict[str, Any]:
    events: dict[str, int] = {}
    head_events: dict[str, dict[str, int]] = {str(i): {} for i in range(ATTENTION_HEADS)}
    validation_events: list[dict[str, Any]] = []

    for line in path.read_text().splitlines():
        m = VALIDATION_RE.search(line)
        if m:
            cycle = int(m.group(1))
            value = int(m.group(2), 16) & 0xFFFFFFFF
            validation_events.append({"cycle": cycle, "value": f"0x{value:08x}"})
            if value in TAG_NAMES:
                events[TAG_NAMES[value]] = cycle
                continue

            base = value & 0xFFFF0000
            head = value & 0xFFFF
            if base in HEAD_BASES and head < ATTENTION_HEADS:
                head_events[str(head)][HEAD_BASES[base]] = cycle
            continue

        m = PASS_RE.search(line)
        if m:
            events["pass_marker"] = int(m.group(1))

    per_head: dict[str, dict[str, int | None]] = {}
    for head, ev in head_events.items():
        per_head[head] = {
            "score_span": span(ev, "score_begin", "score_end"),
            "softmax_span": span(ev, "softmax_begin", "softmax_end"),
            "product_span": span(ev, "product_begin", "product_end"),
            "validate_span": span(ev, "validate_begin", "validate_end"),
        }

    product_sum = none_sum([v["product_span"] for v in per_head.values()])
    validate_sum = none_sum([v["validate_span"] for v in per_head.values()])
    output_to_end = span(events, "output_begin", "output_end")
    wait_span = span(events, "output_begin", "output_all_received")
    residual = None
    if output_to_end is not None and wait_span is not None and product_sum is not None and validate_sum is not None:
        residual = output_to_end - wait_span - product_sum - validate_sum

    aggregate = {
        "program_begin_to_pass_tag": span(events, "program_begin", "pass_tag"),
        "program_begin_to_pass_marker": span(events, "program_begin", "pass_marker"),
        "output_begin_to_all_received": wait_span,
        "all_received_to_output_end": span(events, "output_all_received", "output_end"),
        "output_begin_to_output_end": output_to_end,
        "output_end_to_pass_tag": span(events, "output_end", "pass_tag"),
        "product_sum": product_sum,
        "validate_sum": validate_sum,
        "residual_output_overhead": residual,
    }

    missing: list[str] = []
    for name in REQUIRED_EVENTS:
        if name not in events:
            missing.append(name)
    for head, ev in head_events.items():
        for name in REQUIRED_HEAD_EVENTS:
            if name not in ev:
                missing.append(f"head{head}.{name}")

    return {
        "source_log": str(path),
        "events": events,
        "head_events": head_events,
        "per_head": per_head,
        "aggregate": aggregate,
        "validation_event_count": len(validation_events),
        "validation_events": validation_events,
        "missing_required": missing,
    }


def render_markdown(results: dict[str, dict[str, Any]]) -> str:
    lines = [
        "# Attention Trace Metrics",
        "",
        "> Product trace spans are timing checkpoints, not success criteria.",
        "> Default attention ELFs report `TEST_PASS` only after full scalar reference validation.",
        "",
        "## Aggregate spans",
        "",
        "| Log | program_begin→pass_tag | program_begin→PASS marker | output_begin→all_received | all_received→output_end | output_begin→output_end | product sum | validation sum | residual output overhead |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for name, data in results.items():
        a = data["aggregate"]
        lines.append(
            "| {name} | {program_begin_to_pass_tag} | {program_begin_to_pass_marker} | "
            "{output_begin_to_all_received} | {all_received_to_output_end} | "
            "{output_begin_to_output_end} | {product_sum} | {validate_sum} | "
            "{residual_output_overhead} |".format(name=name, **{k: fmt(v) for k, v in a.items()})
        )

    lines.extend([
        "",
        "## Per-head spans",
        "",
        "| Log | Head | score | softmax | product | scalar validation |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ])
    for name, data in results.items():
        for head in sorted(data["per_head"], key=int):
            h = {k: fmt(v) for k, v in data["per_head"][head].items()}
            lines.append(
                f"| {name} | {head} | {h['score_span']} | {h['softmax_span']} | "
                f"{h['product_span']} | {h['validate_span']} |"
            )

    lines.extend([
        "",
        "## Event cycles",
        "",
        "| Log | program_begin | output_begin | all_received | output_end | pass_tag | PASS marker |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ])
    for name, data in results.items():
        e = data["events"]
        lines.append(
            f"| {name} | {fmt(e.get('program_begin'))} | {fmt(e.get('output_begin'))} | "
            f"{fmt(e.get('output_all_received'))} | {fmt(e.get('output_end'))} | "
            f"{fmt(e.get('pass_tag'))} | {fmt(e.get('pass_marker'))} |"
        )

    return "\n".join(lines) + "\n"


def fmt(value: Any) -> str:
    if value is None:
        return "missing"
    if isinstance(value, int):
        return f"{value:,}"
    return str(value)


def write_outputs(results: dict[str, dict[str, Any]], json_out: Path | None, markdown_out: Path | None) -> None:
    if json_out:
        json_out.write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")
    markdown = render_markdown(results)
    if markdown_out:
        markdown_out.write_text(markdown)
    if not json_out and not markdown_out:
        print(markdown, end="")


def run_self_check() -> None:
    fixture = Path(__file__).with_name("fixtures") / "attention_trace_filter_sample.log"
    result = parse_log(fixture)
    agg = result["aggregate"]
    expected = {
        "program_begin_to_pass_tag": 705,
        "program_begin_to_pass_marker": 709,
        "output_begin_to_all_received": 150,
        "all_received_to_output_end": 560,
        "output_begin_to_output_end": 710,
        "product_sum": 43,
        "validate_sum": 460,
        "residual_output_overhead": 57,
    }
    errors = []
    for key, value in expected.items():
        if agg.get(key) != value:
            errors.append(f"{key}: got {agg.get(key)!r}, expected {value!r}")
    if result["missing_required"]:
        errors.append("missing required tags: " + ", ".join(result["missing_required"]))
    if result["per_head"]["3"]["product_span"] != 12:
        errors.append("head3 product span mismatch")
    if result["per_head"]["3"]["validate_span"] != 130:
        errors.append("head3 validate span mismatch")
    if errors:
        raise SystemExit("self-check failed:\n" + "\n".join(errors))
    print("attention trace parser self-check passed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="*", type=Path, help="filtered validation0 log(s)")
    parser.add_argument("--json-out", type=Path, help="write machine-readable JSON")
    parser.add_argument("--markdown-out", type=Path, help="write Markdown summary")
    parser.add_argument("--allow-missing", action="store_true", help="do not fail when required tags are missing")
    parser.add_argument("--self-check", action="store_true", help="run built-in fixture self-check")
    args = parser.parse_args(argv)

    if args.self_check:
        run_self_check()
        return 0
    if not args.logs:
        parser.error("provide at least one log or --self-check")

    results = {log.stem: parse_log(log) for log in args.logs}
    missing = {name: data["missing_required"] for name, data in results.items() if data["missing_required"]}
    write_outputs(results, args.json_out, args.markdown_out)
    if missing and not args.allow_missing:
        for name, tags in missing.items():
            print(f"error: {name} missing required tags: {', '.join(tags)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
