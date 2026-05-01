# Archived Original DNN Demos

This directory preserves the original TPA DNN tree and systolic demo sources as
reference material:

- `tpa_dnn_tree_demo.cpp/.tpm/.tpp/.place`
- `tpa_dnn_systolic_demo.cpp/.tpm`
- `gen_tpa_dnn_systolic_demo.cmake`

## Status

Archived/reference only. These demos are not wired into the structured ET build.

## Why not build them now?

The original DNN demos depend on APIs and headers outside the current structured
runtime surface, including:

- `dnn_lib/Operators.h`
- `dnn_lib/ElementBinaryInst.h`
- `dnn_lib/LibTensor.h`
- low-level tensor ISA headers such as `etsoc/isa/tensors.h`
- old `ARCH_NR_MINIONS` / `ARCH_NR_HARTS` style constants

The structured repo currently validates the portable HAL/core path, generated
process/program images, tensor-matmul demo, YOLO downstream path, and host
launcher. Reintroducing these DNN demos should be a separate implementation job
that first confirms the DNN library package/dependency policy.

## Future port steps

1. Decide whether the DNN library should be an ET package dependency, vendored
   source, or retired reference.
2. Add C++ support to `add_tpa_process()` / `add_tpa_program()` if needed for
   process objects using C++ sources.
3. Replace old architecture macros with structured HAL constants.
4. Generate or port systolic `.tpp` / `.place` outputs from
   `gen_tpa_dnn_systolic_demo.cmake` using current structured conventions.
5. Build under Erbium ET superbuild and validate under `erbium_emu` with
   documented pass/fail markers.
