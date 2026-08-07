# Flexibility

Freya exposes configuration and composition points so apps can tune the
deferred stack without forking the renderer.

## What you can customize

| Area | Mechanism |
|------|-----------|
| Window / lighting / shadows | `FreyaOptions` / `FreyaOptionsBuilder` |
| Post features | `SetEnableSsao` / `SetEnableTaa` / `SetEnableBloom` |
| SPIR-V root | `SetShaderRoot("./Resources/Shaders")` |
| Vulkan builders | `FreyaExtension::WithInstance` / `WithDevice` / `WithPhysicalDevice` / `WithSwapChain` / `WithRenderer` |
| Frame graph | `Renderer::InsertFrameStage` / `ReplaceFrameStage` |
| Textures from memory | `TexturePool::CreateTextureFromMemory` |
| Meshes from memory | `MeshPool::CreateMesh(vertices, indices)` |
| Materials | `MaterialCreateInfo` (+ `aoFactor`, `alphaCutoff`) |

## Feature flags

```cpp
freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
    o.SetEnableSsao(true)
     .SetEnableTaa(true)
     .SetEnableBloom(false)
     .SetShaderRoot("./Resources/Shaders");
});
```

Defaults match the previous always-on post stack. Disabling SSAO still runs
lighting with a white AO fallback. Disabling TAA skips Halton jitter.
Disabling bloom clears the bloom tap so composite stays dark for that input.

## DI configure hooks

```cpp
.WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
    freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
        o.SetTitle("My App");
    })
    .WithInstance([](fra::InstanceBuilder& b) {
        b.AddValidationLayers();
    })
    .WithPhysicalDevice([](fra::PhysicalDeviceBuilder& b) {
        b.PreferDeviceType(vk::PhysicalDeviceType::eIntegratedGpu);
    })
    .WithDevice([](fra::DeviceBuilder& b) {
        b.AddExtension("VK_KHR_swapchain");
    })
    .WithSwapChain([](fra::SwapChainBuilder& b) {
        b.PreferPresentMode(vk::PresentModeKHR::eMailbox);
    });
})
```

## Frame stages

`Renderer::EndScene` runs an ordered list of `IFrameStage` adapters:

`Pick → Shadow → DeferredGeometry → SsaoLighting → Taa → Bloom → Composite`

```cpp
#include <Freya/Vulkan.hpp>

mRenderer->ReplaceFrameStage("Bloom", std::make_shared<MyBloomStage>());
mRenderer->InsertFrameStage("Composite", std::make_shared<MyCustomStage>());
```

Stages receive a `RenderFrameContext` with pass Arcs and draw/blit callbacks.
Pass GPU objects themselves are unchanged; stages only wrap orchestration.

## Residual roadmap

Not in this release (documented for planning):

- Forward / alternate full pipelines
- Real translucency path (attachment exists; geometry path incomplete)
- Dedicated AO CIS map / material shader variants
- HDR / wide color-space swapchain policy
- Event unsubscribe API
- Public PCH without Vulkan (headers are split; PCH still pulls vulkan.hpp)

See also [API boundary](api-boundary.md).
