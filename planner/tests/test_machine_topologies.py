from __future__ import annotations

import copy
import unittest
from pathlib import Path

from tpa_planner.map_program import (
    build_channel_placement_lines,
    channel_kind,
    make_machine,
)


ROOT = Path(__file__).resolve().parents[2]


def load_machine(name: str) -> dict:
    return make_machine(
        num_minions=8,
        hart="h0",
        machine_json_path=ROOT / "machines" / name,
    )


class MachineTopologyTests(unittest.TestCase):
    def test_single_minion_only_has_direct_mapping(self) -> None:
        machine = load_machine("single-minion.json")
        self.assertEqual(machine["kind"], "single-minion")
        self.assertEqual(len(machine["contexts"]), 1)

        ctx = machine["contexts"][0]
        self.assertEqual(ctx["hartid"], 0)
        self.assertEqual(channel_kind(ctx, ctx), "direct")

    def test_erbium_maps_all_contexts_to_one_local_domain(self) -> None:
        machine = load_machine("erbium.json")
        self.assertEqual(machine["kind"], "erbium")
        self.assertEqual(len(machine["contexts"]), 8)

        first = machine["contexts"][0]
        second = machine["contexts"][1]
        self.assertEqual(channel_kind(first, first), "direct")
        self.assertEqual(channel_kind(first, second), "local")
        self.assertEqual(first["local_domain"], second["local_domain"])
        self.assertEqual(first["device_domain"], second["device_domain"])

    def test_etsoc1_maps_shires_to_local_domains_on_one_fabric(self) -> None:
        machine = load_machine("etsoc1.json")
        self.assertEqual(machine["kind"], "etsoc1")
        self.assertEqual(len(machine["contexts"]), 32 * 32)

        shire0_minion0 = machine["contexts"][0]
        shire0_minion1 = machine["contexts"][1]
        shire1_minion0 = machine["contexts"][32]

        self.assertEqual(shire0_minion0["hartid"], 0)
        self.assertEqual(shire0_minion1["hartid"], 2)
        self.assertEqual(shire1_minion0["hartid"], 64)
        self.assertEqual(channel_kind(shire0_minion0, shire0_minion0), "direct")
        self.assertEqual(channel_kind(shire0_minion0, shire0_minion1), "local")
        self.assertEqual(channel_kind(shire0_minion0, shire1_minion0), "fabric")

    def test_device_boundary_is_external(self) -> None:
        machine = load_machine("etsoc1.json")
        src = machine["contexts"][0]
        dst = copy.deepcopy(src)
        dst["device_domain"] = "etsoc1:card1"

        self.assertEqual(channel_kind(src, dst), "external")

    def test_channel_placement_lines_use_machine_domains(self) -> None:
        machine = load_machine("etsoc1.json")
        placement = {
            0: machine["contexts"][0],
            1: machine["contexts"][1],
            32: machine["contexts"][32],
        }
        lines = build_channel_placement_lines(
            [
                {"src_inst": 0, "src_port": "out", "dst_inst": 1, "dst_port": "in"},
                {"src_inst": 1, "src_port": "out", "dst_inst": 32, "dst_port": "in"},
            ],
            placement,
        )

        self.assertEqual(
            lines,
            [
                "chan 0 out 1 in local",
                "chan 1 out 32 in fabric",
            ],
        )


if __name__ == "__main__":
    unittest.main()
