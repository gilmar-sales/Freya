# Freya — C++26 Vulkan Rendering Framework

## Build

- CMake 3.29+, requires Vulkan SDK and **GCC 16+** (C++26;
  Clang/MSVC are not supported yet). Most deps fetched via
  `FetchContent`: SDL3, glm, assimp, skirnir, meshoptimizer
  (Dear ImGui and nlohmann_json only for examples, via
  `Examples/Common/`).
- Pinned dependency versions:

  | Dependency      | Version / Tag | Declared in |
  |-----------------|---------------|-------------|
  | SDL3            | `release-3.4.16` | root `CMakeLists.txt` |
  | glm             | `1.0.3` | root `CMakeLists.txt` |
  | assimp          | `v6.0.5` | root `CMakeLists.txt` |
  | skirnir         | `v0.23.3` | root `CMakeLists.txt` |
  | meshoptimizer   | `v0.25` | root `CMakeLists.txt` |
  | VMA (VulkanMemoryAllocator) | `v3.4.0` | root `CMakeLists.txt` |
  | Dear ImGui      | `v1.91.8` | `Examples/Common/CMakeLists.txt` |
  | nlohmann_json   | `v3.11.3` | `Examples/Common/CMakeLists.txt` (also `tests/` for GPU tests) |
  | googletest      | `v1.17.0` | `tests/CMakeLists.txt` |
  | google-benchmark | `v1.9.5` | `benchmarks/CMakeLists.txt` |
  | stb_image.h / stb_truetype.h | vendored in `src/Freya/Vendor/` | — |
- Static lib only (`BUILD_SHARED_LIBS OFF`).
- `build/` is the active build directory (Ninja, used by CI). It is
  **git-ignored** (`.gitignore:1`; `git ls-files build` is empty), so a
  fresh clone has no `build/` — create it with CMake.
- Generator: CI configures with `-G Ninja` (and installs Ninja on all
  runners), but `cmake/CompileShaders.cmake` only uses generic
  `add_custom_command`/`add_custom_target`, so any generator works.
- `cmake -B build -S . -G Ninja && cmake --build build --parallel`
- Examples auto-enable when building from root (detected via `CMAKE_SOURCE_DIR == CMAKE_CURRENT_SOURCE_DIR`);
  disable with `-DFREYA_BUILD_EXAMPLES=OFF`.
- CI builds on ubuntu/windows (GCC 16) with **Debug only**; Linux requires `xorg`/Wayland
  dev packages (see CI workflow). macOS/Clang/MSVC are excluded.
- `compile_commands.json` is generated in `build/` only
  (`CMAKE_EXPORT_COMPILE_COMMANDS ON` in the root `CMakeLists.txt`).
- Validation layers: enabled in Debug, disabled in Release (`NDEBUG` guard in `src/Freya/Pch.hpp`).

## Running the Example

- **Always `cd` to the example's target binary directory before running.** Examples load
  shaders/textures/models via relative paths (`./Resources/Shaders/...`, `./Resources/Textures/...`,
  `./Resources/Models/...`), so the working directory must be the executable's own directory.
  ```sh
  cd build/Examples/IndustrialPipeLamp
  ./IndustrialPipeLamp
  ```

## Shaders

- **Do NOT manually compile with glslc.** The build system (`cmake/CompileShaders.cmake`)
  compiles every `.vert`/`.frag`/`.comp` under `Shaders/` to `.spv` at build time
  (staging dir: `${CMAKE_BINARY_DIR}/Resources/Shaders`). `.inc` files are tracked as
  dependencies, both per-shader-directory and shared (`Shaders/Include/`). Only changed
  sources are recompiled. Edit the GLSL and rebuild.
- If `glslc` is missing, a warning is emitted and the shader sources are copied as-is
  at configure time as fallback (there is no pre-compiled `.spv` fallback).
- The `Shaders` CMake target is a dependency of `Freya`, so shaders compile before the library.
- Shader variants (12 dirs): `Anim/`, `Billboard/`, `Cell/`, `Debug/`,
  `DeferredCompressed/`, `GpuDriven/`, `Include/`, `Material/`, `Pick/`,
  `Post/`, `Shadow/`, `Ui/`.
- Each example receives compiled shaders via the centralized helper
  `add_freya_example()` in `Examples/Common/AddFreyaExample.cmake:30-33`, which calls
  `add_shader_outputs(Shaders <example_binary_dir>/Resources/Shaders)` then
  `add_dependencies(<ExampleTarget> ${Shaders_OUTPUT_TARGETS})`.
  This creates a per-example copy target that lands `.spv` files in the example's
  binary dir, preserving the `<variant>/` sub-directory layout. The helper also copies
  the example's `Resources/` dir and the requested IBL `.hdr` files.

## Tests

- GoogleTest unit tests live in `tests/` (18 `*Spec.cpp` files: header-only public API +
  UBO layouts). They do **not** create a Vulkan device.
- Enable with `-DFREYA_BUILD_TESTS=ON` (default when Freya is the top-level
  project). Disable with `-DFREYA_BUILD_TESTS=OFF`.
```sh
cmake -B build -S . -G Ninja
cmake --build build --parallel --target FreyaTests
ctest --test-dir build --output-on-failure
```
- GPU tests (`tests/gpu/`, fixtures in `tests/fixtures/gpu_cull/`) build as `FreyaGpuTests`,
  opt-in via `-DFREYA_BUILD_GPU_TESTS=ON` (default `OFF` in `tests/CMakeLists.txt`;
  requires a Vulkan ICD).

## Project Layout

| Path | Purpose |
|---|---|
| `include/Freya/` | Public headers. Umbrella `Freya.hpp` (app surface). Public `Builders/` holds only `FreyaOptionsBuilder.hpp` + `PostProcessBuilder.hpp`; every other builder is private under `src/`. |
| `src/Freya/` | Library `.cpp` plus private headers, split into `Asset/`, `Builders/`, `Containers/`, `Core/`, `Internal/`, `Scene/`, and `Vendor/` (`stb_image.h`, `stb_truetype.h`). Private PCH: `src/Freya/Pch.hpp`. |
| `Examples/` | 8 demo apps (`IndustrialPipeLamp`, `SsaoDebug`, `SkinnedFox`, `CellBulbasaur`, `GameUiShowcase`, `ParticleShowcase`, `BillboardShowcase`, `ChurchShowcase`) plus shared `Examples/Common/` (`FreyaExamplesCommon`, `add_freya_example()` helper). |
| `Shaders/` | GLSL sources in 12 variant dirs (see Shaders); shared snippets in `Shaders/Include/` (`.inc`). |
| `Resources/` | IBL environment maps (`*.hdr`: studio + outdoor), copied per-example into `Resources/Environments/`. There is no root-level `textures/` dir; example textures live under each example's own `Resources/`. |
| `benchmarks/` | google-benchmark suites, opt-in via `-DFREYA_BUILD_BENCHMARKS=OFF` (default off); currently `benchmarks/Animation/`. |
| `docs/` | MkDocs-material documentation, deployed via `mkdocs gh-deploy`. |
| `.kilo/` | Kilo CLI config: `agent/`, `command/`, `plans/` (`plans/` is git-ignored). |

## Key Conventions

- Namespace: `FREYA_NAMESPACE` expands to `fra`.
- Builder pattern for internal GPU objects (`src/Freya/Builders/`); the public builder
  surface is just `FreyaOptionsBuilder` + `PostProcessBuilder`.
- `FREYA_NAMESPACE` and the GLM config (`GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`,
  `GLM_ENABLE_EXPERIMENTAL`) live in `include/Freya/Config.hpp:8-20`; `src/Freya/Pch.hpp`
  only includes it and adds the validation-layer flag.
- Column limit: 80 (`.clang-format` — Microsoft base style, `NamespaceIndentation: All`).
- C++26 throughout (`cxx_std_26`). `-freflection` (GCC) is applied only to the
  `tests`/`benchmarks` targets, not to the `Freya` library itself.

## Code Formatting

- Style is enforced by `.clang-format` at the repo root.
- Format every tracked C/C++ file: `bash scripts/format.sh` (PowerShell: `scripts/format.ps1`).
- Verify without modifying: `bash scripts/format.sh --check`.
- Exclusions (`Shaders/`, `src/Freya/Vendor/`) are hard-coded inline in
  `scripts/format.sh:12-13` (mirrored in `format.ps1`); the `.clang-ignore` file at the
  repo root exists but is not consumed by the scripts. No format-check CI workflow exists.
- Optional local pre-commit hook: `pip install pre-commit && pre-commit install`
  (uses `.pre-commit-config.yaml`, which runs `bash scripts/format.sh --check` on staged files).

## Application Structure

- Extend `fra::AbstractApplication`; `StartUp()`/`ShutDown()` default to no-ops and only
  `Update()` is pure virtual (`include/Freya/Core/AbstractApplication.hpp:39-43`).
- Prefer the `fra::RunApp<AppT>(configureOptions, configureLogging)` helper
  (`include/Freya/Core/FreyaApp.hpp`) over hand-rolling `skr::ApplicationBuilder` with
  `WithExtension<fra::FreyaExtension>()`.
- Resolve **scoped** services (LightService, FreyaOptions, EventManager) via
  `GetMainServiceProvider()` (or `GetWindowServices(*window)` for secondary
  windows). Shared pools (`MeshPool` / `TexturePool` / `MaterialPool`) remain
  root singletons and can still be taken from the constructor `serviceProvider`.
- Open extra windows with `CreateWindow`; close with `Window::Close()`.
  Override `UpdateSecondaryWindow` and use `GetRenderer(*window)` to draw
  (IndustrialPipeLamp: F10).
- FreyaOptions: title, dimensions, vSync, fullscreen, sampleCount, frameCount,
  clearColor, drawDistance, maxLights, ReverseZ, shaderRoot,
  enableSsao/enableTaa/enableBloom.
- Application types: `#include <Freya/Freya.hpp>`.
