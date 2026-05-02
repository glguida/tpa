# Generation Flow

This note describes the **existing TPA generation plumbing**.

It is intentionally about:

- the files
- the current CMake utilities
- the mapper boundary

It is **not** about how a planner happens to be implemented.

## The Existing Contract

TPA already has a real generation path.

The inputs are:

- process implementation code
- process manifests: `.tpm`
- one program manifest: `.tpp`
- one placement file: `.place`

The existing utilities are:

- [`add_tpa_process()`](../../cmake/tpa-kernel.cmake)
- [`add_tpa_program()`](../../cmake/tpa-kernel.cmake)
- [`gen_tpa_image.cmake`](../../cmake/gen_tpa_image.cmake)

That is the path that produces the final mapped program image.

## What Each File Means

### `.tpm`

`.tpm` defines **process kinds**.

It says:

- what a process kind is called
- whether it is `user` or `sys`
- its numeric id
- its start symbol
- its workspace size
- its ports

This is tied to the process implementation code and belongs with that code.

### `.tpp`

`.tpp` defines the **program graph**.

It says:

- which process instances exist
- which instance ports are connected
- the capacity of each channel

This is a program-level graph description.

### `.place`

`.place` defines the **mapping**.

It says:

- which instance goes to which runtime hart id
- optional channel-kind overrides for connections

This is the explicit hardware placement.

## What The Existing Utilities Do

### `add_tpa_process()`

[`add_tpa_process()`](../../cmake/tpa-kernel.cmake):

- compiles the process implementation sources
- records the associated `.tpm`

It does **not** build the graph.

### `add_tpa_program()`

[`add_tpa_program()`](../../cmake/tpa-kernel.cmake):

- collects all process manifests from the process targets
- takes one `.tpp`
- takes one `.place`
- invokes [`gen_tpa_image.cmake`](../../cmake/gen_tpa_image.cmake)

This is the real handoff from source-level graph description to mapped image generation.

### `gen_tpa_image.cmake`

[`gen_tpa_image.cmake`](../../cmake/gen_tpa_image.cmake):

- parses all `.tpm`
- parses the `.tpp`
- parses the `.place`
- checks consistency
- emits transport kind from explicit placement overrides or the arch interface
- materializes:
  - process definitions
  - process instances
  - port bindings
  - channels
  - channel capacities
  - fabric buffers where needed
  - boot sections
- emits the generated image C used in the final link

This is the mapper.

## The Real Flow

The existing flow is:

```text
process code + .tpm
        \
         +-- .tpp -----------+
                              +--> add_tpa_program()
         +-- .place ---------+        |
                                       +--> gen_tpa_image.cmake
                                       +--> generated image C
                                       +--> final ELF
```

So the important rule is:

- `.tpp` and `.place` feed the mapper
- `gen_tpa_image.cmake` is the mapper
- nothing else should try to replace that step

## What Should Be Generated

If a model-specific flow needs generation, the correct outputs are:

- `.tpp`
- `.place`
- optional inventories or notes

The wrong outputs are:

- generated image C directly
- a second mapper
- a parallel build path that bypasses `add_tpa_program()`

## What Stays With The Process Code

Process-specific artifacts stay with the process code:

- implementation sources
- `.tpm`

That is because `.tpm` describes the process kind itself:

- start symbol
- workspace size
- ports

Those are process-code properties, not program-graph properties.

## How This Applies To YOLO

For a YOLO graph, the correct split is:

- YOLO block implementations own their `.tpm`
- the model graph emits `.tpp`
- the model mapping emits `.place`
- the existing TPA build path turns those into the final image

So for YOLO, the correct question is:

- “what `.tpp` should this graph produce?”
- “what `.place` should this mapping produce?”

Not:

- “how do we build an image without the mapper?”

## Practical Rule

Going forward:

1. Keep `.tpm` with the process code.
2. Generate `.tpp` and `.place` for model graphs.
3. Feed them into [`add_tpa_program()`](../../cmake/tpa-kernel.cmake).
4. Let [`gen_tpa_image.cmake`](../../cmake/gen_tpa_image.cmake) do the actual mapping.

That is the existing plumbing, and it is enough.
