# Flexibility

Freya exposes configuration and composition points so apps can tune the
deferred stack without forking the renderer.

## What you can customize

| Area | Mechanism |
|------|-----------|
| Window / lighting / shadows | `FreyaOptions` / `FreyaOptionsBuilder` |
| Post features | `SetEnableShadows` / `SetEnableSsao` / `SetEnableFsr` / `SetEnableBloom` |
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
     .SetEnableFsr(true)
     .SetEnableBloom(false)
     .SetShaderRoot("./Resources/Shaders")
     .SetShadowQuality(fra::ShadowQuality::High)
     .SetSsaoQuality(fra::SsaoQuality::Medium)
     .SetFsrQuality(fra::FsrQuality::Quality)
     .SetBloomQuality(fra::BloomQuality::Medium);
});
```

## Quality presets

Each effect has a `Low` / `Medium` / `High` / `Ultra` / `Off` preset (same
idea as shadows). Presets write the per-effect knobs in `FreyaOptions`;
individual setters can override afterward. `Off` clears the matching
`enable*` flag without wiping resolution / strength knobs.

| API | Controls |
|-----|----------|
| `SetShadowQuality` | map res, cascades, spot/point slots, soft taps; `Off` skips maps |
| `SetSsaoQuality` | resolution divisor (1/2/4), radius, bias, power, intensity |
| `SetFsrQuality` | FSR 3 scale mode (NativeAA…UltraPerformance / Off) |
| `SetBloomQuality` | resolution divisor, threshold, extract scale, composite strength |

Runtime (after build):

```cpp
mRenderer->SetSsaoQuality(fra::SsaoQuality::Ultra);
mRenderer->SetFsrQuality(fra::FsrQuality::Off);
mRenderer->SetBloomQuality(fra::BloomQuality::High);
mRenderer->SetShadowQuality(fra::ShadowQuality::Off);
```

Resolution changes rebuild the related pass; FSR quality / bloom strength
update without a full rebuild when the divisor is unchanged. Toggling
`Off` ↔ any quality rebuilds SSAO/FSR/Bloom as needed; shadow `Off` only
gates the pass and clears `castShadows` in the light UBO.

Defaults match the previous always-on post stack. Disabling SSAO still runs
lighting with a white AO fallback. Disabling FSR skips jitter and renders
at native display resolution. Disabling bloom clears the bloom tap so
composite stays dark for that input.

FSR motion vectors use G-buffer velocity (current unjittered VP vs
`prevViewProjection`, plus per-instance previous model matrices). Prefer
`Renderer::SetInstanceModels` so Freya maintains that history; raw
`BindBuffer` instance data will not supply correct object motion alone.

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

`Pick → Shadow → DeferredGeometry → SsaoLighting → Fsr → Bloom → Composite`

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
