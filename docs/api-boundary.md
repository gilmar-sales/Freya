# API boundary

## Headers

| Header | Audience |
|--------|----------|
| `<Freya/Freya.hpp>` | Apps: options, extension, pools, materials, events, `AbstractApplication`, `Renderer` frame loop, billboards, particles, UI draws |
| `<Freya/Advanced.hpp>` | Frame-stage plugins: `RendererAdvanced`, `PostProcess` + `PostProcessBuilder`, `IFrameStage` + `StageContext` + `GpuImageRef`, `MaterialTechniqueRegistry`, `LightingTechniqueRegistry`, GPU animation (`GpuAnimation`, `GpuAnimationSystem`, `GpuAnimDebug`) |
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
- `AbstractApplication` lifecycle and multi-window API (`CreateWindow`,
  `GetRenderer` / `GetWindowServices`, `GetMainServiceProvider`)
- `IPlatform` (process-wide window/event backend; default `SdlPlatform`)
- `Renderer` frame loop (`BeginFrame` / `EndScene` / `Present` / `EndFrame`),
  quality knobs (`SetShadowQuality` / `SetSsaoQuality` / `SetTaaQuality` /
  `SetBloomQuality`), pick, debug draw, billboards, scene-instance upload
- `RendererAdvanced` (`<Freya/Advanced.hpp>`, via `fra::Advanced(renderer)`):
  `InsertFrameStage` / `ReplaceFrameStage`, scene-instance upload trio,
  `NativeCommandBuffer` / `NativeDevice` (opaque `void*` =
  `VkCommandBuffer` / `VkDevice`), `BeginUI` / `EndUI`,
  `GetImGuiNativeHandles`, `GetViewportImage`, `SetViewportTarget` /
  `ClearOutputTarget` (offscreen viewport + swapchain UI pass; all
  Vulkan/SDL handles exposed as opaque `void*`), `GpuAnimation()` cull
  dumps (`RequestCullFrameDump` / `TryConsumeCullFrameDump`)
- `Renderer::GetBillboardDraw` + `BillboardDraw` (`Quad`, `Quads`,
  `ConnectedQuad`, `ConnectedQuads`, `Strip`, `HealthBar`, `Text`,
  `Snapshot`, `SnapshotConnected`; thread-safe concurrent submits via
  `SpinLock`) and `BillboardAlign` (`Screen` / `Cylindrical` / `Spherical`
  / `FixedAxis` / `Planar`)
- `ParticleEmitter` (`Tick`; one thread per emitter — draw queue is
  thread-safe)
- `Window::NativeWindow` (opaque `void*` = `SDL_Window*`)
- `PostProcess` + `PostProcessBuilder` (`<Freya/Advanced.hpp>`) +
  instance `postProcess->MakeStage()` (not a static factory)
- `MaterialTechniqueRegistry` + `MaterialCreateInfo::techniqueId`
  (`<Freya/Advanced.hpp>`)
- `LightingTechniqueRegistry` (global deferred lighting fragment override;
  `<Freya/Advanced.hpp>`)
- `IFrameStage` + `StageContext` + `GpuImageRef` (`<Freya/Advanced.hpp>`)
  — apps may implement stages and use `RendererAdvanced`
  `InsertFrameStage` / `ReplaceFrameStage`

Treat as **internal** (not installed, not part of the app API):

- Concrete pass classes (`DeferredCompressedPass`, `BloomPass`, …)
- `Device`, `SwapChain`, `RenderFrameContext`, Vulkan builders
- Command-buffer / ImGui hooks typed as `vk::` (`GetCommandBuffer`,
  `GetUIRenderPass`)

## CMake

```cmake
target_include_directories(Freya
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
rebuild application sources. Apps that need raw Vulkan for custom stages
include `vulkan.h` themselves and cast `StageContext::Native*` /
`GpuImageRef::Native*` handles.
