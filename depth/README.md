# Stereo SAD depth demo

This directory contains the first full no-weights depth-vision demo: a structured
TPA graph for deterministic 96x64 grayscale stereo, 5x5 SAD, and max disparity
32.

The graph is hardware-independent:

```text
stereo_source
  -> sad_worker stripe 0 -> stereo_checker port 0
  -> sad_worker stripe 1 -> stereo_checker port 1
  -> sad_worker stripe 2 -> stereo_checker port 2
  -> sad_worker stripe 3 -> stereo_checker port 3
```

`stereo_sad.place` is a small reviewed Erbium hand placement for the validated
`tpa_stereo_sad.elf` demo path. Runtime hart choices remain outside process code
and outside `stereo_sad.tpp`.

The build also exposes report-only mapper targets for inspection:

- `tpa_stereo_sad_hand_plan_planner_json` extracts process metadata and plans the
  current hand placement;
- `tpa_stereo_sad_map_mapped_program` maps the same graph with
  `machines/erbium.json`, writes a map report, mapped-program JSON,
  mapper-generated `.place`, scratch header, and edge-buffer config header, but
  does not replace the hand-placed runtime ELF.

The mapper cost hints in `stereo_sad_compute_costs.json` intentionally make
workers much more expensive than the source/checker so the scheduler sees the
four SAD stripes as the dominant parallel work.

## Data policy

The source process regenerates the same synthetic `bands:6,12,18` scene policy
used by `tools/depth/gen_stereo_sad_case.py` and the block-test generated
manifest. It does not load external images, datasets, pretrained weights, or
third-party stereo code.

Worker processes receive fixed stripe+halo packets, compute streaming
winner-takes-all SAD without materializing a full cost volume, and send compact
`uint8_t` disparity stripes with `255` marking invalid pixels. The checker emits
`TEST_PASS` only after all four output stripes match the deterministic expected
disparity/invalid policy.

## Build and run

Build through the top-level ET superbuild:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_stereo_sad.elf
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/depth/tpa_stereo_sad.elf \
  -max_cycles 100000000
```

The device subbuild also registers the Erbium CTest `tpa_stereo_sad_erbium`
when `erbium_emu` is available.
