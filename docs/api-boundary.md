# API boundary

## Headers

| Header | Audience |
|--------|----------|
| `<Freya/Freya.hpp>` | Apps: options, extension, pools, materials, events, `AbstractApplication` |
| `<Freya/Vulkan.hpp>` | Advanced: `Renderer`, passes, device/swapchain builders, frame stages |
| Individual `Freya/...` | Prefer for slow compile or narrow dependency |

All public declarations live under `include/Freya/`. Implementation `.cpp`
files and vendored `stb_image.h` stay in `src/Freya/` (CMake `PRIVATE`).

## Stability guidance

Treat as **app-stable**:

- `FreyaOptions` / `FreyaOptionsBuilder`
- `FreyaExtension` configure hooks
- `MeshPool` / `TexturePool` / `MaterialPool` / `MaterialCreateInfo`
- `AbstractApplication` lifecycle
- `IFrameStage` name/insert/replace contract

Treat as **Vulkan-advanced** (may change shape across minors):

- Concrete pass classes (`DeferredCompressedPass`, `BloomPass`, …)
- `RenderFrameContext` field set
- Raw `vk::*` accessors on `Renderer` (`GetCommandBuffer`, `GetUIRenderPass`)

## CMake

```cmake
target_include_directories(Freya
    PUBLIC  include ${Vulkan_INCLUDE_DIRS}
    PRIVATE src)
```

Consumers link `Freya::Freya` and include from the public tree only.

## Known leak

The library PCH is still `PUBLIC` and includes `<vulkan/vulkan.hpp>` and SDL.
Fully opaque C++ API without Vulkan types in the transitive compile set is
future work.
