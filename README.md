# TPA Structured Build

This repository uses CMake to build one static TPA library per supported HAL
platform.

## Configure, build, and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The default configuration builds all currently supported platform variants:

- `tpa_erbium`
- `tpa_etsoc1`

## Selecting platforms

Use the CMake options below at configure time to enable or disable individual
platform variants:

```sh
cmake -S . -B build -DTPA_BUILD_ERBIUM=ON -DTPA_BUILD_ETSOC1=ON
cmake -S . -B build -DTPA_BUILD_ERBIUM=OFF
cmake -S . -B build -DTPA_BUILD_ETSOC1=OFF
```

Each platform library publishes the common TPA headers and the selected
platform's own HAL include directory. Platform configuration comes from the
per-platform headers under `tpa/hal/<platform>/include`, for example
`<tpa/hal/erbium.h>` and `<tpa/hal/etsoc1.h>`.
