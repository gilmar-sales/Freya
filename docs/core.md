# Core

The core module contains fundamental engine components.

## AbstractApplication

Base class for all Freya applications. Inherits from `skr::IApplication` and provides the main application loop.

```cpp
class MyApp final : public fra::AbstractApplication
{
    void StartUp() override;
    void Update() override;
    void ShutDown() override;
};
```

### Lifecycle Methods

- `StartUp()` - Called once before the main loop begins
- `Update()` - Called every frame (pure virtual, must be implemented)
- `ShutDown()` - Called once after the main loop ends
- `Run()` - Starts the main application loop

### Protected Members

| Member | Type | Description |
|--------|------|-------------|
| `mWindow` | `skr::Arc<Window>` | Window instance |
| `mRenderer` | `skr::Arc<Renderer>` | Renderer instance |
| `mEventManager` | `skr::Arc<EventManager>` | Event manager instance |
| `mDeltaTime` | `float` | Time since last frame (seconds) |

## Renderer

Main rendering coordinator. Handles frame management, swap chain, and an
ordered list of `IFrameStage` adapters that drive the deferred stack.

```cpp
mRenderer->BeginFrame();
// record draws via Draw / DrawInstanced
mRenderer->EndFrame(); // EndScene + Present
```

### Frame stages

Default order:

`Pick → Shadow → DeferredGeometry → SsaoLighting → Taa → Bloom → Composite`

```cpp
#include <Freya/Freya.hpp>

mRenderer->InsertFrameStage("Bloom", cell->MakeStage());
```

See [Flexibility](flexibility.md).

### Key Methods

| Method | Description |
|--------|-------------|
| `BeginFrame()` | Start a new frame |
| `EndScene()` | Run frame stages |
| `Present()` | Submit the command buffer and present |
| `EndFrame()` | `EndScene()` + `Present()` |
| `RebuildSwapChain()` | Recreate swap chain (e.g., on resize) |
| `InsertFrameStage` / `ReplaceFrameStage` | Insert a Freya-built stage |
| `SetVSync(bool)` | Enable/disable vertical sync |
| `SetSamples(uint32_t)` | Set MSAA sample count |
| `SetDrawDistance(float)` | Set render distance |
| `SetInstanceModels(const mat4*, size_t)` | Legacy instance matrices (with DrawInstanced) |
| `UploadSceneInstances(span)` | Prefer: GPU-driven scene table (batched MDI + cull) |
| `GetCurrentFrameIndex()` | Get current frame index |
| `GetFrameCount()` | Get total frame count |
| `CalculateProjectionMatrix(float near, float far)` | Calculate projection matrix |
| `ClearProjections()` | Clear all projection matrices |
| `UpdateProjection(ProjectionUniformBuffer&)` | Update projection data |
| `GetGpuAnimPass()` | GPU skinning pass (optional) |

Apps that do not customize the frame graph can keep calling `EndFrame()`.

## Window

Window management and input handling.

```cpp
mWindow->IsRunning();      // Check if window is running
mWindow->Update();         // Process window events
mWindow->GetDeltaTime();   // Get delta time in seconds
```

## UniformBuffer

Uniform buffer for shader data.

## LightService

Manages analytical lights (point, directional, spot, area) and uploads them
to a shared UBO used by DeferredCompressed lighting.

`FreyaOptions::maxLights` (default 16, see `MAX_LIGHTS`) caps how many lights
`AddLight` accepts. Shader arrays are fixed at 16 entries.

### Light types

| Type | Factory | Notes |
|------|---------|--------|
| Point | `MakePointLight(pos, color, radius, intensity)` | Attenuates by distance |
| Directional | `MakeDirectionalLight(dir, color, intensity)` | Direction is normalized |
| Spot | `MakeSpotLight(pos, dir, color, radius, innerRad, outerRad, intensity)` | Cone angles in radians; stored as cosines |
| Area | `MakeAreaLight(center, normal, tangent, halfW, halfH, color, intensity)` | Rect panel; LTC in Deferred lighting |

Spot/inner and outer cutoffs on `Light` are **cosines** of the cone half-angles.
The spot factory converts radians for you.

For area lights, `outerCutoff` stores half-width and `halfHeight` the half-extent
along the bitangent (`cross(normal, tangent)`).

### GPU packing (`LightUniformBuffer`, std140 SoA)

- `lightPositions[i]` — xyz position / area center, **w = LightType**
- `lightColorsAndRadius[i]` — rgb color, w radius
- `lightDirectionsAndCutoff[i]` — xyz direction / area normal, w innerCutoff (cos)
- `lightOuterCutoffAndIntensity[i]` — x outerCutoff (spot cos) or halfWidth (area), y intensity, z halfHeight (area), **w = castShadows** (0/1)
- `lightAreaTangents[i]` — xyz area tangent
- `viewPosition`, `lightCount`

Set `Light::castShadows = true` on directional / spot / point lights that should
write shadow maps. Area lights never cast shadows. Shadow budgets and quality
come from `FreyaOptions` (see Shadows below).

### Usage

```cpp
auto lights = serviceProvider->GetService<fra::LightService>();

lights->AddLight(fra::MakeDirectionalLight(
    glm::vec3(-0.4f, -1.0f, -0.3f), glm::vec3(1.0f), 0.4f));

const auto point =
    lights->AddLight(fra::MakePointLight(
        glm::vec3(0.0f, 5.0f, 0.0f),
        glm::vec3(1.0f, 0.4f, 0.3f),
        50.0f,
        0.5f));

lights->AddLight(fra::MakeSpotLight(
    glm::vec3(0.0f, 8.0f, 4.0f),
    glm::vec3(0.0f, -1.0f, -0.5f),
    glm::vec3(0.9f, 0.95f, 1.0f),
    60.0f,
    glm::radians(12.0f),
    glm::radians(22.0f),
    1.0f));

lights->AddLight(fra::MakeAreaLight(
    glm::vec3(0.0f, 6.0f, 0.0f),
    glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(1.0f, 0.0f, 0.0f),
    3.0f,
    1.5f,
    glm::vec3(1.0f, 0.95f, 0.9f),
    4.0f));

// Per-frame: position-only or full replace
lights->UpdateLightPosition(static_cast<std::uint32_t>(point),
                            glm::vec3(2.0f, 5.0f, 0.0f));

if (const auto* spot = lights->GetLight(2))
{
    fra::Light updated = *spot;
    updated.position   = glm::vec3(1.0f, 6.0f, 2.0f);
    updated.direction  = glm::normalize(-updated.position);
    lights->UpdateLight(2, updated);
}
```

`Renderer::UpdateCamera` refreshes the light UBO for the current frame when
the light service is present (also uploads `iblIntensity` for IBL).

## IBLService

Provides split-sum image-based lighting: an equirectangular environment map
with GGX importance-sampled specular mips (roughness → LOD), a convolved
irradiance map, a BRDF integration LUT, and parametric LTC LUTs used by
rectangular area lights. Built at startup from
`FreyaOptions::environmentMapPath` (Radiance `.hdr` via `stbi_loadf`; maps
wider than 1024px are downsampled for irradiance; specular prefilter bakes
at ≤512px wide). Default path is
`./Resources/Environments/studio_small_09_4k.hdr` (copied from the repo-root
`Resources/` into example binary dirs). Set the path to empty to force the
procedural sky; if the file is missing, the procedural sky is used as well.

| Resource | Role |
|----------|------|
| Environment | Specular IBL via `textureLod` (GGX prefiltered mips) |
| Irradiance | Diffuse IBL (CPU hemisphere convolution) |
| BRDF LUT | Specular split-sum scale/bias |
| LTC matrix/ampl | Linearly Transformed Cosines for area lights |

Configure with `SetIblIntensity` / `SetEnvironmentMapPath` on
`FreyaOptionsBuilder`. Deferred lighting bindings 7–9 sample IBL; 10–11 are LTC.

## Shadows

`ShadowPass` runs before deferred geometry each frame and produces:

| Target | Technique | Limit |
|--------|-----------|--------|
| Directional | Cascaded shadow maps (2D array) | `shadowCascadeCount` (1–4) |
| Spot | Perspective depth map (2D array) | `maxSpotShadows` (0–4) |
| Point | Cube array (6 faces each) | `maxPointShadows` (0–2) |

Configure via `FreyaOptionsBuilder`: `SetShadowQuality` presets
(`Low` / `Medium` / `High` / `Ultra`) or individual setters
(`SetShadowCascadeCount`, `SetShadowMapResolution`, `SetShadowBias`,
`SetMaxSpotShadows`, `SetMaxPointShadows`, `SetShadowSampleCount`).

| Preset | Resolution | Cascades | Spot | Point | Soft taps |
|--------|------------|----------|------|-------|-----------|
| Low | 512² | 2 | 2 | 2 | 4 |
| Medium | 1024² | 3 | 4 | 2 | 8 |
| High | 2048² | 4 | 4 | 2 | 16 |
| Ultra | 4096² | 4 | 4 | 2 | 16 |

Defaults without a preset: 4 cascades, 2048², bias `0.002`, 4 spot /
2 point slots, 16 soft-shadow taps. Spot/point budgets of `0` keep a 1×1
descriptor stub instead of a full-resolution atlas.

Lighting shaders multiply each light’s radiance by a PCF shadow factor
(hardware compare samplers). Deferred lighting bindings 12–15 hold the shadow
UBO and cascade / spot / point maps.

## DeferredCompressedPass

Deferred rendering pass with G-buffer compression.
