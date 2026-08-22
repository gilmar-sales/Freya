# Flexibility

Freya exposes configuration and composition points so apps can tune the
deferred stack without forking the renderer.

## What you can customize

| Area | Mechanism |
|------|-----------|
| Window / lighting / shadows | `FreyaOptions` / `FreyaOptionsBuilder` |
| Post features | `SetEnableShadows` / `SetEnableSsao` / `SetEnableTaa` / `SetEnableBloom` |
| SPIR-V root | `SetShaderRoot("./Resources/Shaders")` |
| Frame graph | `Renderer::InsertFrameStage` / `ReplaceFrameStage` with Freya factories |
| Custom post shaders | `PostProcessBuilder` + `BindMaterial` (G-buffer albedo.a IDs) |
| Custom G-buffer shaders | `MaterialTechniqueRegistry` + `MaterialCreateInfo::techniqueId` |
| Textures from memory | `TexturePool::CreateTextureFromMemory` |
| Meshes from memory | `MeshPool::CreateMesh(vertices, indices)` |
| Materials | `MaterialCreateInfo` (AO map, packed MR, unlit, double-sided) |
| Model import | `MeshPool::CreateModelFromFile` (Assimp PBR + textures) |

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
`Renderer::UploadSceneInstances` so Freya maintains that history; drawing
without the scene-upload API will not supply correct object motion alone.

## DI configure hooks

```cpp
.WithExtension<fra::FreyaExtension>([](fra::FreyaExtension& freya) {
    freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
        o.SetTitle("My App")
         .SetVSync(true)
         .SetShadowQuality(fra::ShadowQuality::High);
    });
})
```

Present mode, GPU type, and device extensions are chosen inside the engine
from `FreyaOptions` (vsync, sample count, quality presets). Vulkan builders
are not part of the public extension API.

## Frame stages

`Renderer::EndScene` runs an ordered list of `IFrameStage` adapters:

`GpuAnim → Pick → Shadow → DeferredGeometry → SsaoLighting → Taa → Translucent → Bloom → Composite → DebugDraw`

```cpp
#include <Freya/Freya.hpp>

mRenderer->InsertFrameStage("Bloom", cell->MakeStage());
```

Apps may insert Freya factories (`PostProcess::MakeStage`) **or** implement
`IFrameStage` themselves. `Execute` / `Rebuild` receive a public
`StageContext` (Vulkan-free): options, extent, frame index, G-buffer / HDR
`GpuImageRef` taps, `DispatchCull` / `ExecuteDraws`, and opaque
`NativeCommandBuffer` / `NativeDevice` (`void*` → cast to Vulkan in the
app). `ReplaceFrameStage` rebuilds the new stage immediately (same as
insert).

`RenderFrameContext` (pass pointers, typed `vk::` fields) remains internal
to Freya.

## Custom lighting fragment

Deferred lighting uses stock `DeferredCompressed/lighting.frag.spv`. Override
the fullscreen lighting fragment globally:

```cpp
auto* lighting =
    serviceProvider->GetService<fra::LightingTechniqueRegistry>().get();
lighting->SetFragment("Cell/lighting_cell.frag.spv");
mRenderer->RebuildSwapChain();
```

The custom SPIR-V must keep the stock descriptor layout (bindings 0–15),
push constant `debugMode`, `lighting.vert`, and additive HDR output.
`Clear()` restores the default. Shadow / pick / OIT / G-buffer stay on stock
shaders unless also customized via `MaterialTechniqueRegistry`.

## Custom post-process shaders

The deferred stack uses one G-buffer pipeline for all opaque draws. User
shaders are post-process (fullscreen) effects that sample those targets via
`PostProcess` + `InsertFrameStage`. SPIR-V is still compiled at CMake
time (`glslc`); there is no runtime GLSL.

Put the fragment under `Shaders/<Name>/` (or your own `add_shader_target`)
and insert **before Bloom** so ACES / bloom see the result:

```cpp
#include <Freya/Freya.hpp>

struct CellPushConstants
{
    float     bands           = 4.0f;
    float     edgeDepthScale  = 80.0f;
    float     edgeNormalScale = 2.0f;
    float     strength        = 1.0f;
    glm::vec4 edgeColor { 0.02f, 0.02f, 0.04f, 1.0f };
    float     reverseZ   = 0.0f;
    float     shadowLift = 0.22f;
    float     edgeWidth  = 1.0f;
};

auto cell = serviceProvider->GetService<fra::PostProcessBuilder>()
                ->SetName("Cell")
                .SetFragment("Cell/cell.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth,
                             fra::PostProcessInput::Normal })
                .SetPushConstantSize(sizeof(CellPushConstants))
                .Build();

CellPushConstants params {};
params.reverseZ = options->ReverseZ ? 1.0f : 0.0f;
cell->SetPushConstants(params);
cell->BindMaterial(bodyMaterial); // albedo.a material ID; omit = all pixels
mRenderer->InsertFrameStage("Bloom", cell->MakeStage());
```

`SetInputs` order is descriptor binding order (`set = 0`). Vertex defaults
to `DeferredCompressed/composing.vert.spv` (fullscreen triangle). Set 1 is
always G-buffer albedo (binding 0, material ID in `.a`) and a
`PostProcessMaterialMask` UBO (binding 1). `BindMaterial` / `UnbindMaterial`
/ `ClearMaterials` fill that mask (`count == 0` means every pixel). IDs
are 0–255 (G-buffer `albedo.a` is `R8G8B8A8Unorm`; IDs ≥ 256 alias). The
pass writes an HDR ping-pong image and blits back onto the current scene
HDR (OIT composite, else TAA, else deferred scene color).

Push-constant layouts are app-defined POD matching the fragment shader;
Freya does not ship effect-specific constant structs.

IndustrialPipeLamp is the PBR deferred reference. Cell + edges lives in
`Examples/CellBulbasaur/` (`F4` toggles the effect on the Bulbasaur
materials, not the ground).

## Custom material G-buffer shaders

Opaque draws use stock `DeferredCompressed/gbuffer.frag` (technique 0).
Register alternate fragments that keep the same vertex inputs and G-buffer
attachments, then assign `MaterialCreateInfo::techniqueId`:

```cpp
auto* techniques =
    serviceProvider->GetService<fra::MaterialTechniqueRegistry>().get();
const auto cellTech =
    techniques->Register("CellGBuffer", "Cell/gbuffer_cell.frag.spv");
// Rebuild deferred pipelines after Register (e.g. RebuildSwapChain).

auto mat = materialPool->Create({
    .albedo      = albedoTex,
    .techniqueId = cellTech,
});
```

Up to `kMaxMaterialTechniques` (8) slots. Shadow / pick / OIT stay on stock
shaders; lighting uses `LightingTechniqueRegistry` (default stock). Custom
G-buffer fragments must still write albedo+matID, normal+flags, PBR,
emissive HDR, and velocity.

Stock technique frags under `Shaders/Material/`:

| SPIR-V | Role |
|--------|------|
| `Material/unlit_emissive.frag.spv` | Skip lighting; albedo+emissive → HDR |
| `Material/triplanar.frag.spv` | World-space triplanar albedo/normal |
| `Cell/gbuffer_cell.frag.spv` | Matte PBR (pairs with cell lighting / post) |
| `Cell/lighting_cell.frag.spv` | Cel-banded deferred lighting override |

Stock post frags under `Shaders/Post/` (and `Cell/`):

| SPIR-V | Inputs | Notes |
|--------|--------|-------|
| `Post/outline.frag.spv` | Scene, Depth, Normal | Depth/normal edges; `BindMaterial` optional |
| `Post/color_grade.frag.spv` | Scene | Contrast / sat / exposure / vignette / lift-gain |
| `Post/underwater.frag.spv` | Scene, Depth | Wave warp + tint + depth fog |
| `Post/heat_haze.frag.spv` | Scene, Depth | Shimmer; mask with `BindMaterial` |
| `Post/glow.frag.spv` | Scene, Depth | Item highlight aura; **requires** `BindMaterial` |
| `Post/mu_item_glow.frag.spv` | Scene, Depth | Mu Online +0…+13 tiers + wave flash; `BindMaterial` |

| `Cell/cell.frag.spv` | Scene, Depth, Normal | Cel bands + edges |

Animated effects (`underwater`, `heat_haze`) expect a `time` field in push
constants — update each frame via `SetPushConstants`.

## Residual roadmap

Not in this release (documented for planning):

- Forward / alternate full pipelines
- GGX-prefiltered IBL cubemap (CPU irradiance + raw HDR mips today)
- Clustered lighting (`MAX_LIGHTS` is still 16)
- Physical glass (transmission / IOR / refraction) beyond Weighted Blended OIT
- HDR / wide color-space swapchain policy
- Event unsubscribe API

WBOIT translucency is implemented: `AlphaMode::Blend` instances use a
dedicated MDI cull (`CullMode::Translucent`), accumulate into weighted OIT
targets, resolve over TAA/opaque before Bloom, then Composite tonemaps the
combined HDR scene.

See also [API boundary](api-boundary.md).
