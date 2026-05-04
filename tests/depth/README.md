# Depth Block Tests

This directory contains structured TPA block tests for the no-weights stereo SAD
fallback. The tests consume deterministic generated headers from
`tests/depth/generated/`; they do not add external images, datasets, model
weights, or third-party stereo code.

Targets:

- `tpa_depth_sad_cost_5x5.elf` checks the generated 5x5 SAD probe and valid
  pixels in `sad_cost_5x5.h`.
- `tpa_depth_sad_argmin_16x8.elf` checks the 16x8/D8 argmin and invalid-mask
  case in `sad_argmin_16x8.h`.
- `tpa_depth_sad_stripe_96x16.elf` checks the first 96x16 stripe for the
  approved 96x64/D32 demo using `sad_demo_96x64_stripe0.h`.

Each target is a one-process `.c + .tpm + .tpp + .place` TPA program that emits
`TEST_PASS` only after comparing computed SAD/argmin results with the generated
expected data.

Example Erbium build from the repository root:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target \
  tpa_depth_sad_cost_5x5.elf \
  tpa_depth_sad_argmin_16x8.elf \
  tpa_depth_sad_stripe_96x16.elf
ctest --test-dir build-et-erbium/tpa-device-prefix/src/tpa-device-build \
  --output-on-failure \
  -R 'tpa_depth_sad_(cost_5x5|argmin_16x8|stripe_96x16)_erbium'
```

These block tests validate arithmetic and generated reference contracts. They are
not the full four-stripe depth demo.
