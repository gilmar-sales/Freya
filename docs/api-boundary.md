# API boundary

## Headers

| Header | Audience |
|--------|----------|
| `<Freya/Freya.hpp>` | Apps: options, extension, pools, materials, events, `AbstractApplication`, `Renderer`, `FullscreenEffect`, GPU animation |
| `<Freya/Vulkan.hpp>` | Deprecated stub that includes `<Freya/Freya.hpp>` |
| Individual `Freya/...` | Prefer the umbrella; leaf headers still do not include Vulkan or SDL |

Public headers live under `include/Freya/` and compile without `vk::` or
`SDL_*`. Vulkan device, swapchain, passes, and builders live under
`src/Freya/` (CMake `PRIVATE`). Implementation `.cpp` files and vendored
`stb_image.h` stay in `src/Freya/`.

glm and Skirnir remain on the public surface (`skr::Arc`,
`skr::ApplicationBuilder`, `GetService`).

## Stability guidance

Treat as **app-stable**:

- `FreyaOptions` / `FreyaOptionsBuilder`
- `FreyaExtension::WithOptions`
- `MeshPool` / `TexturePool` / `MaterialPool` / `MaterialCreateInfo`
- `AbstractApplication` lifecycle
- `Renderer` frame loop, quality knobs, pick, debug draw, GPU anim methods
- `FullscreenEffect` + `FullscreenEffectBuilder` + `MakeStage()`
- `IFrameStage` as an opaque type for `InsertFrameStage` (apps use factories,
  they do not implement stages)

Treat as **internal** (not installed, not part of the app API):

- Concrete pass classes (`DeferredCompressedPass`, `BloomPass`, …)
- `Device`, `SwapChain`, `RenderFrameContext`, Vulkan builders
- Command-buffer / ImGui hooks (`GetCommandBuffer`, `GetUIRenderPass`)

## CMake

```cmake
target_include_directories(Freya
    PUBLIC  include
    PRIVATE src ${Vulkan_INCLUDE_DIRS})

target_link_libraries(Freya
    PUBLIC  glm skirnir::skirnir
    PRIVATE SDL3::SDL3 assimp meshoptimizer ${Vulkan_LIBRARIES})

target_precompile_headers(Freya PRIVATE
    <vulkan/vulkan.hpp> <SDL3/SDL.h> …)
```

Consumers link `Freya::Freya` and include from the public tree only. Freya
is a static library: the linker still sees Vulkan and SDL, but example
`Main.cpp` files do not get those include directories or the engine PCH.

## Compile firewall

A translation unit that only includes `<Freya/Freya.hpp>` cannot name
`vk::Device` or `SDL_Window`. Changing an internal pass header does not
rebuild application sources.
