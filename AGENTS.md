# Freya — C++23 Vulkan Rendering Framework

## Build

- CMake 3.29+, requires Vulkan SDK. All deps fetched via `FetchContent`: SDL3, glm, assimp, skirnir.
- Pinned dependency versions:

  | Dependency  | Version / Tag    |
  |-------------|------------------|
  | SDL3        | `release-3.4.10` |
  | glm         | `1.0.3`          |
  | assimp      | `v6.0.5`         |
  | skirnir     | `v0.16.1`        |
  | stb_image.h | `v2.30` (vendored in `src/Freya/Vendor/`) |
- Static lib only (`BUILD_SHARED_LIBS OFF`).
- `build/` is the active build directory (Ninja, used by CI). `.gitignore` patterns `cmake-build-*/` and `build/` (but `build/` is committed — do not delete it).
- **Generator: Ninja is mandatory.** Always pass `-G Ninja` when configuring (CI uses Ninja, and the shader copy targets in `cmake/CompileShaders.cmake` rely on Ninja generator behavior). Do not use the default generator.
- `cmake -B build -S . -G Ninja && cmake --build build --parallel`
- Examples auto-enable when building from root (detected via `CMAKE_SOURCE_DIR == CMAKE_CURRENT_SOURCE_DIR`);
  disable with `-DFREYA_BUILD_EXAMPLES=OFF`.
- CI builds on ubuntu/windows/macos with **Debug only**; Linux requires `xorg` dev packages (see CI workflow).
- `compile_commands.json` generated in both build dirs for IDE tooling.
- Validation layers: enabled in Debug, disabled in Release (`NDEBUG` guard in `Pch.hpp`).

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
  compiles every `.vert`/`.frag` from `Shaders/<variant>/` to `.spv` at build time
  (staging dir: `${CMAKE_BINARY_DIR}/Resources/Shaders`). Only changed sources are
  recompiled (timestamp check). Edit the `.vert`/`.frag` and rebuild.
- If `glslc` is missing, a warning is emitted and pre-compiled `.spv` files are copied as fallback.
- The `Shaders` CMake target is a dependency of `Freya`, so shaders compile before the library.
- Each example opts in to receiving compiled shaders by calling
  `add_shader_outputs(Shaders <example_binary_dir>/Resources/Shaders)` in its
  `CMakeLists.txt`, then `add_dependencies(<ExampleTarget> ${Shaders_OUTPUT_TARGETS})`.
  This creates a per-example copy target that lands `.spv` files in the example's
  binary dir, preserving the `<variant>/` sub-directory layout.

## Tests

There are **no tests** in this repository. No test directory, no CTest suites configured.
The CI `ctest` step runs against an empty suite.

## Project Layout

| Path | Purpose |
|---|---|
| `src/Freya/` | Library source: `Core/` (Renderer, Window, RenderPass, DeferredCompressedPass, etc.), `Builders/` (builder for every core object), `Asset/` (MeshPool, TexturePool, MaterialPool), `Containers/` (SparseSet, MeshSet), `Events/` (input event system), `Vendor/` (stb_image.h). Also `Pch.hpp`, `FreyaOptions.hpp`. |
| `include/Freya/` | Public headers — umbrella `Freya.hpp` pulls in all public types. |
| `Examples/IndustrialPipeLamp/` | Only example; binary lands at `build/Examples/IndustrialPipeLamp/IndustrialPipeLamp`. |
| `Shaders/` | GLSL sources in three variants: `Forward/`, `Deferred/`, `DeferredCompressed/`. |
| `textures/` | Root-level texture (not used by the IndustrialPipeLamp example, which uses its own `Examples/IndustrialPipeLamp/Resources/Textures/`). |
| `docs/` | MkDocs-material documentation, deployed via `mkdocs gh-deploy`. |
| `.kilo/` | Kilo CLI config: agent definitions (`.kilo/agent/`) and plans (`.kilo/plans/`). |

## Key Conventions

- Namespace: `FREYA_NAMESPACE` expands to `fra`.
- Builder pattern for all objects (WindowBuilder, InstanceBuilder, RendererBuilder, etc.).
- `Pch.hpp` defines `FREYA_NAMESPACE`, GLM config (`GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_ENABLE_EXPERIMENTAL`), and the validation layer flag.
- Column limit: 80 (`.clang-format` — Microsoft base style, `NamespaceIndentation: All`).
- C++23 throughout (`cxx_std_23`).

## Code Formatting

- Style is enforced by `.clang-format` at the repo root.
- Format every tracked C/C++ file: `bash scripts/format.sh` (PowerShell: `scripts/format.ps1`).
- Verify without modifying: `bash scripts/format.sh --check`. The CI workflow
  `.github/workflows/format-check.yml` runs this on every push/PR that touches
  `src/`, `include/`, `Examples/`, `CMakeLists.txt`, `.clang-format`, or the script
  itself.
- Excluded paths are listed in `.clang-ignore` (build outputs, `Shaders/`,
  `src/Freya/Vendor/`, `.cache/`).
- Optional local pre-commit hook: `pip install pre-commit && pre-commit install`
  (uses `.pre-commit-config.yaml`, local hook that shells out to the format script).

## Application Structure

- Extend `fra::AbstractApplication`; implement `StartUp()` and `Update()`.
- Use `skr::ApplicationBuilder` with `AddExtension<fra::FreyaExtension>()` to configure.
- Services (Renderer, Window, MeshPool, TexturePool, MaterialPool, LightService) obtained via `serviceProvider->GetService<T>()`.
- FreyaOptions: title, dimensions, vSync, fullscreen, sampleCount, frameCount, clearColor, drawDistance, maxLights, ReverseZ, renderingStrategy (`Forward` | `Deferred`).
