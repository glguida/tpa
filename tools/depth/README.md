# Depth Synthetic Data Tools

This directory contains deterministic depth-vision generation tools. The current
scope is the no-weights stereo SAD fallback while FastDepth trained artifacts are
blocked.

## Artifact policy

The stereo SAD cases are **internally generated synthetic data**:

- no pretrained model weights;
- no external image datasets;
- no copied OpenCV or third-party stereo code;
- no heavyweight Python dependencies.

Generated cases are block/demo validation inputs. They are not immutable model
weights and they are not ET platform validation by themselves. Future runtime
work must still keep generated sample data, process state, scratch, and
edge/channel buffers separate.

If external stereo images are ever introduced, add source URL, license, file
size, and SHA-256 before committing or fetching them. Do not mix external sample
policy with this synthetic-data policy.

## Generator

`gen_stereo_sad_case.py` uses only the Python standard library. It emits
C-friendly headers and a JSON manifest under `tests/depth/generated/`.

Regenerate all checked-in stereo SAD cases from the repository root:

```sh
tools/depth/gen_stereo_sad_case.py --out-dir tests/depth/generated
```

Run the generator's deterministic/reference self-check without writing files:

```sh
tools/depth/gen_stereo_sad_case.py --self-check
```

The generated `stereo_sad_cases_manifest.json` records the command, generator
version, synthetic formula, dimensions, seeds, per-payload SHA-256 values, and
file SHA-256 values.

## Synthetic texture and disparity policy

The generator computes each grayscale left-image pixel from a fixed 32-bit hash
mix of `(x, y, seed)`. The right image samples the same texture shifted by the
row's known disparity; out-of-bounds right-image pixels are filled by a separate
deterministic hash and masked invalid by the reference policy.

Small presets use constant disparity 3. The 96x64 demo uses horizontal disparity
bands `6,12,18`. Pixels are marked invalid (`255`) when the 5x5 window, full
`0..dmax-1` search range, image border, or disparity-band guard area would make
matching ambiguous or unsupported.

## Checked-in presets

- `sad_cost_5x5.h`: small 9x9 image with a 5x5 SAD radius (`R=2`), `dmax=5`,
  and a valid probe pixel for window-cost tests.
- `sad_argmin_16x8.h`: small whole-image case for argmin/invalid-mask tests,
  `R=2`, `dmax=8`, constant disparity 3.
- `sad_demo_96x64_stripe0.h`: first 16-row stripe of the approved 96x64 demo,
  `R=2`, disparities `0..31`, 18 input rows including bottom halo, and 16 rows
  of expected output. The JSON manifest also enumerates hashes and dimensions
  for all four 16-row full-demo stripes over 96x64 grayscale input.

The demo uses `uint8_t` left/right grayscale inputs and `uint8_t` output
where `255` marks invalid pixels. Runtime kernels, `.tpm`, `.tpp`, `.place`, and
CMake device targets are intentionally out of scope for this generator-only
step.
