from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
from pathlib import Path


MUTABLE_PREFIXES = (".data", ".sdata", ".bss", ".sbss")
MODEL_PREFIXES = (".mram_data",)
IGNORE_PREFIXES = (".text", ".rela", ".comment", ".note", ".riscv.attributes")
MUTABLE_SYMBOL_TYPES = frozenset({"b", "B", "d", "D", "s", "S", "c", "C"})
META_SECTION = ".tpa.proc.meta"
META_MAGIC = 0x315F4154454D5054
META_VERSION = 1
META_STRUCT = struct.Struct("<QQQQ")


def run(tool: str, *args: str) -> str:
    return subprocess.check_output([tool, *args], text=True)


def parse_manifest(path: Path) -> dict:
    pdefs: list[dict] = []
    pdef_by_name: dict[str, dict] = {}

    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split()
        tag = fields[0]
        if tag == "pdef":
            if len(fields) != 6:
                raise ValueError(f"bad pdef line in {path}: {raw_line}")
            _, name, kind, pid, start, ws_sz = fields
            pdef = {
                "name": name,
                "kind": kind,
                "pid": int(pid, 10),
                "start": start,
                "declared_ws_bytes": int(ws_sz, 10),
                "ports": [],
            }
            pdefs.append(pdef)
            pdef_by_name[name] = pdef
        elif tag == "port":
            if len(fields) != 4:
                raise ValueError(f"bad port line in {path}: {raw_line}")
            _, pdef_name, port_id, direction = fields
            if pdef_name not in pdef_by_name:
                raise ValueError(f"unknown pdef {pdef_name} in {path}")
            pdef_by_name[pdef_name]["ports"].append(
                {
                    "id": int(port_id, 10),
                    "direction": direction,
                }
            )
        else:
            raise ValueError(f"unknown manifest tag {tag} in {path}")

    return {"manifest": str(path), "pdefs": pdefs}


def parse_size_sections(tool_prefix: str, objects: list[Path]) -> tuple[dict[str, int], dict[str, dict[str, int]]]:
    all_sections: dict[str, int] = {}
    per_object: dict[str, dict[str, int]] = {}
    size_tool = f"{tool_prefix}size"

    for obj in objects:
        sections: dict[str, int] = {}
        out = run(size_tool, "-A", str(obj))
        for line in out.splitlines():
            match = re.match(r"^(\S+)\s+(\d+)\s+\d+$", line.strip())
            if not match:
                continue
            section = match.group(1)
            size = int(match.group(2), 10)
            sections[section] = size
            all_sections[section] = all_sections.get(section, 0) + size
        per_object[str(obj)] = sections

    return all_sections, per_object


def parse_meta_records(tool_prefix: str, objects: list[Path]) -> list[dict]:
    objdump_tool = f"{tool_prefix}objdump"
    records: list[dict] = []

    for obj in objects:
        proc = subprocess.run(
            [objdump_tool, "-s", "-j", META_SECTION, str(obj)],
            text=True,
            capture_output=True,
        )
        if proc.returncode != 0:
            continue

        payload = bytearray()
        for line in proc.stdout.splitlines():
            match = re.match(r"^\s*[0-9a-fA-F]+\s+((?:[0-9a-fA-F]{8}\s+)+)", line)
            if not match:
                continue
            for group in match.group(1).split():
                payload.extend(bytes.fromhex(group))

        for off in range(0, len(payload), META_STRUCT.size):
            chunk = bytes(payload[off:off + META_STRUCT.size])
            if len(chunk) < META_STRUCT.size:
                break
            magic, version, pid, scratch_peak = META_STRUCT.unpack(chunk)
            if magic != META_MAGIC:
                raise ValueError(
                    f"{obj}: bad process metadata magic 0x{magic:x}"
                )
            if version != META_VERSION:
                raise ValueError(
                    f"{obj}: unsupported process metadata version {version}"
                )
            records.append(
                {
                    "pid": pid,
                    "scratch_peak_bytes": scratch_peak,
                    "object": str(obj),
                }
            )

    return records


def parse_symbols(tool_prefix: str, objects: list[Path]) -> dict[str, list[dict]]:
    nm_tool = f"{tool_prefix}nm"
    per_object: dict[str, list[dict]] = {}

    for obj in objects:
        symbols: list[dict] = []
        out = run(nm_tool, "-S", "-a", str(obj))
        for line in out.splitlines():
            match = re.match(
                r"^(?:[0-9A-Fa-f]+)?\s*([0-9A-Fa-f]+)\s+([A-Za-z])\s+(\S+)$",
                line.strip(),
            )
            if not match:
                continue
            size = int(match.group(1), 16)
            if size == 0:
                continue
            sym_type = match.group(2)
            name = match.group(3)
            symbols.append(
                {
                    "name": name,
                    "type": sym_type,
                    "size": size,
                }
            )
        per_object[str(obj)] = symbols

    return per_object


def is_embedded_edge_payload_symbol(symbol: dict) -> bool:
    if symbol["type"] not in MUTABLE_SYMBOL_TYPES:
        return False

    name = symbol["name"]
    return (
        name == "out_buf"
        or name.startswith("out_buf.")
        or name.startswith("yolov5n_input_")
    )


def classify_embedded_edge_payloads(per_object_symbols: dict[str, list[dict]]) -> dict:
    embedded_symbols: list[dict] = []
    embedded_total = 0

    for obj, symbols in per_object_symbols.items():
        for symbol in symbols:
            if not is_embedded_edge_payload_symbol(symbol):
                continue
            embedded_symbols.append(
                {
                    "object": obj,
                    "name": symbol["name"],
                    "size": symbol["size"],
                }
            )
            embedded_total += symbol["size"]

    return {
        "embedded_edge_payload_bytes": embedded_total,
        "embedded_edge_payload_symbols": embedded_symbols,
    }


def classify_sections(section_sizes: dict[str, int]) -> dict[str, int]:
    workspace = 0
    model = 0
    other_static = 0

    for section, size in section_sizes.items():
        if size == 0:
            continue
        if section.startswith(MUTABLE_PREFIXES):
            workspace += size
        elif section.startswith(MODEL_PREFIXES):
            model += size
        elif section.startswith(IGNORE_PREFIXES):
            continue
        elif section == META_SECTION:
            other_static += size
        elif section.startswith(".rodata") or section.startswith(".srodata") or section.startswith(".tpa."):
            other_static += size

    return {
        "workspace_bytes": workspace,
        "model_blob_bytes": model,
        "other_static_bytes": other_static,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--label", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--tool-prefix", default="riscv64-unknown-elf-")
    parser.add_argument("--objects", nargs="+", required=True)
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    output_path = Path(args.output)
    objects = [Path(obj) for obj in args.objects]

    manifest = parse_manifest(manifest_path)
    section_sizes, per_object_sections = parse_size_sections(args.tool_prefix, objects)
    proc_meta_records = parse_meta_records(args.tool_prefix, objects)
    per_object_symbols = parse_symbols(args.tool_prefix, objects)
    proc_meta_by_pid = {record["pid"]: record for record in proc_meta_records}

    categories = classify_sections(section_sizes)
    edge_payloads = classify_embedded_edge_payloads(per_object_symbols)

    pdefs: list[dict] = []
    for pdef in manifest["pdefs"]:
        record = proc_meta_by_pid.get(pdef["pid"])
        pdefs.append(
            {
                **pdef,
                "scratch_peak_bytes": 0 if record is None else record["scratch_peak_bytes"],
            }
        )

    result = {
        "label": args.label,
        "manifest": str(manifest_path),
        "objects": [str(obj) for obj in objects],
        "section_sizes": section_sizes,
        "per_object_sections": per_object_sections,
        "per_object_symbols": per_object_symbols,
        **categories,
        **edge_payloads,
        "pdefs": pdefs,
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
