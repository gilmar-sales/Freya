# Flexibility

Freya exposes configuration and composition points so apps can tune the
deferred stack without forking the renderer.

## What you can customize

| Area | Mechanism |
|------|-----------|
| Window / lighting / shadows | `FreyaOptions` / `FreyaOptionsBuilder` |
| Post features | `SetEnableShadows` / `SetEnableSsao` / `SetEnableTaa` / `SetEnableBloom` |
| SPIR-V root | `SetShaderRoot("./Resources/Shaders")` |
| Vulkan builders | `FreyaExtension::WithInstance` / `WithDevice` / `WithPhysicalDevice` / `WithSwapChain` / `WithRenderer` |
| Frame graph | `Renderer::InsertFrameStage` / `ReplaceFrameStage` |
| Custom shaders | `FullscreenEffectBuilder` (SPIR-V fullscreen, G-buffer inputs) |
| Textures from memory | `TexturePool::CreateTextureFromMemory` |
| Meshes from memory | `MeshPool::CreateMesh(vertices, indices)` |
| Materials | `MaterialCreateInfo` (+ `aoFactor`, `alphaCutoff`, `AlphaMode`) |

## Feature flags

```cpp
freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
    o.SetEnableSsao(true)
     .SetEnableTaa(true)
     .SetEnableBloom(false)
     .SetShaderRoot("./Resources/Shaders")
     .SetShadowQuality(fra::ShadowQuality::High)
     .SetSsaoQuality(fra::SsaoQuality::Medium)
     .SetTaaQuality(fra::TaaQuality::High)
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
| `SetTaaQuality` | current-frame blend weight, Halton period |
| `SetBloomQuality` | resolution divisor, threshold, extract scale, composite strength |

Runtime (after build):

```cpp
mRenderer->SetSsaoQuality(fra::SsaoQuality::Ultra);
mRenderer->SetTaaQuality(fra::TaaQuality::Off);
mRenderer->SetBloomQuality(fra::BloomQuality::High);
mRenderer->SetShadowQuality(fra::ShadowQuality::Off);
```

Resolution changes rebuild the related pass; TAA weight / bloom strength
update without a full rebuild when the divisor is unchanged. Toggling
`Off` ↔ any quality rebuilds SSAO/TAA/Bloom as needed; shadow `Off` only
gates the pass and clears `castShadows` in the light UBO.

Defaults match the previous always-on post stack. Disabling SSAO still runs
lighting with a white AO fallback. Disabling TAA skips Halton jitter.
Disabling bloom clears the bloom tap so composite stays dark for that input.

TAA motion vectors use G-buffer velocity (current unjittered VP vs
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

`GpuAnim → Pick → Shadow → DeferredGeometry → SsaoLighting → Taa → Translucent → Bloom → Composite → DebugDraw`

```cpp
#include <Freya/Vulkan.hpp>

mRenderer->ReplaceFrameStage("Bloom", std::make_shared<MyBloomStage>());
mRenderer->InsertFrameStage("Composite", std::make_shared<MyCustomStage>());
```

Stages receive a `RenderFrameContext` with pass Arcs and draw/blit callbacks.
Pass GPU objects themselves are unchanged; stages only wrap orchestration.

## Custom fullscreen shaders

The deferred stack uses one G-buffer pipeline for all opaque draws. User
shaders are fullscreen (or compute) effects that sample those targets via
`FullscreenEffect` + `InsertFrameStage`. SPIR-V is still compiled at CMake
time (`glslc`); there is no runtime GLSL.

Put the fragment under `Shaders/<Name>/` (or your own `add_shader_target`)
and insert **before Bloom** so ACES / bloom see the result:

```cpp
#include <Freya/Vulkan.hpp>

auto cell = serviceProvider->GetService<fra::FullscreenEffectBuilder>()
                ->SetName("Cell")
                .SetFragment("Cell/cell.frag.spv")
                .SetInputs({ fra::EffectInput::SceneColor,
                             fra::EffectInput::Depth,
                             fra::EffectInput::Normal })
                .SetPushConstantSize(sizeof(fra::CellPushConstants))
                .Build();

fra::CellPushConstants params {};
params.reverseZ = options->ReverseZ ? 1.0f : 0.0f;
cell->SetPushConstants(params);
mRenderer->InsertFrameStage("Bloom", cell->MakeStage());
```

`SetInputs` order is descriptor binding order (`set = 0`). Vertex defaults
to `DeferredCompressed/composing.vert.spv` (fullscreen triangle). The pass
writes an HDR ping-pong image and blits back onto the current scene HDR
(OIT composite, else TAA, else deferred scene color).

IndustrialPipeLamp is the PBR deferred reference. Cell + edges lives in
`Examples/CellBulbasaur/` (`F4` toggles the fullscreen effect).

## Residual roadmap

Not in this release (documented for planning):

- Forward / alternate full pipelines
- Dedicated AO CIS map / per-material shader variants
- HDR / wide color-space swapchain policy
- Event unsubscribe API
- Public PCH without Vulkan (headers are split; PCH still pulls vulkan.hpp)
- Physical glass (transmission / IOR / refraction) beyond Weighted Blended OIT

WBOIT translucency is implemented: `AlphaMode::Blend` instances use a
dedicated MDI cull (`CullMode::Translucent`), accumulate into weighted OIT
targets, resolve over TAA/opaque before Bloom, then Composite tonemaps the
combined HDR scene.

See also [API boundary](api-boundary.md).
