# Freya — C++23 Vulkan Rendering Framework

## Build

- CMake 3.29+, requires Vulkan SDK
- Find the actual build dir greping *build*, if found two or more asks what to use
- Single config: `cmake -B <build-dir> -S .` then `cmake --build <build-dir>`
- Examples auto-enable when building from root; disable with `-DFREYA_BUILD_EXAMPLES=OFF`
- CI builds on ubuntu/windows/macos with Debug only; Linux requires `xorg` dev packages

## Running Examples

- **Always change to the example's target directory before running.**
  Examples load shaders, textures, and models via relative paths like `./Resources/...`,
  so the working directory must be the executable's own directory.
  ```sh
  cd cmake-build-debug/Examples/Sofa
  ./Sofa
  ```
- Use `-WorkingDirectory` / `cwd` when launching from scripts.

## Shaders

- **Do NOT manually compile shaders with glslc.** The CMake build does it automatically.
- GLSL source files live in `Shaders/<variant>/` (`.vert` / `.frag`).
- The build system (`cmake/CompileShaders.cmake`) finds `glslc` from the Vulkan SDK,
  compiles every `.vert`/`.frag` into `.spv` at build time, and places them under
  `${CMAKE_BINARY_DIR}/Resources/Shaders/<variant>/`.
- **Incremental**: only changed source files are recompiled (timestamp check via
  `add_custom_command(OUTPUT ... DEPENDS ...)`). Just edit the `.vert`/`.frag` and
  `cmake --build` — the build system handles the rest.
- If `glslc` is not found, a warning is emitted and the old pre-compiled `.spv` files
  from the source tree are copied as fallback (configure-time only).
- The module provides `find_glslc()` and `add_shader_target(TARGET <name> FROM <dir> INTO <dir>)`.
- The `Shaders` target is a dependency of `Freya`, so shaders compile before the library.
- Examples use `add_dependencies(Example Shaders)` + `POST_BUILD` `copy_directory` to
  receive compiled shaders at their runtime resource paths.

## Project Layout

- `src/Freya/` — library source (builders, core, containers)
- `Examples/Sofa/` — only example; builds to `build/Examples/Sofa/Sofa`
- `Shaders/` — GLSL source files (`.vert` / `.frag`); compiled to `.spv` at build time
- `cmake/CompileShaders.cmake` — reusable shader compilation module
- Examples load resources via relative paths from binary (`./Resources/Textures/...`)

## Key Conventions

- Namespace: `FREYA_NAMESPACE` expands to `fra`
- Builder pattern for all objects (WindowBuilder, InstanceBuilder, RendererBuilder, etc.)
- `Pch.hpp` defines `FREYA_NAMESPACE`, GLM config, and validation layer flag
- Validation layers: enabled in Debug, disabled in Release (`NDEBUG`)
- Column limit: 80 (`.clang-format` uses Microsoft base style)
- C++23 throughout (`cxx_std_23`)

## Application Structure

- Extend `fra::AbstractApplication`; implement `StartUp()` and `Update()`
- Use `skr::ApplicationBuilder` with `AddExtension<fra::FreyaExtension>()` to configure
- Services (Renderer, Window, MeshPool, TexturePool, MaterialPool) obtained via `serviceProvider->GetService<T>()`
- FreyaOptions: title, dimensions, vSync, fullscreen, sampleCount, frameCount, clearColor, drawDistance, renderingStrategy