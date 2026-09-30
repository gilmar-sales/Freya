# Freya

A Vulkan-based rendering engine powered by [Skirnir](https://github.com/gilmar-sales/Skirnir) for dependency injection.

## Features

- **Vulkan-backed rendering** - Modern graphics API with high performance
- **Deferred rendering** - G-buffer path with SSAO, TAA, bloom, and HDR composite
- **Composable frame stages** - Insert or replace steps via
  `fra::Advanced(renderer)` (`<Freya/Advanced.hpp>`)
- **Feature flags** - Toggle shadows / SSAO / TAA / Bloom and set `shaderRoot`
  via options
- **Custom render targets** - Offscreen composite via
  `fra::Advanced(renderer).SetViewportTarget` / `ClearOutputTarget`, with
  opaque Vulkan/SDL handles (`GetImGuiNativeHandles`, `GetViewportImage`) for
  hosting the scene in an application-level ImGui viewport; quality and vsync
  via `FreyaOptions`
- **ImGui seam** - `fra::Advanced(renderer).BeginUI` / `EndUI` open the
  swapchain UI pass so apps draw Dear ImGui over an offscreen viewport without
  leaking `vk::`/SDL types into public headers
- **Asset management** - Meshes, textures (file or memory), and PBR materials
- **Skinned animation** - AnimGraph, bake, CPU or GPU skin palettes, LOD
  (see [Animation](animation.md))
- **Event system** - Flexible pub/sub event handling for window, keyboard, mouse, and gamepad
- **Builder pattern** - Fluent API plus `FreyaExtension` configure hooks
- **Skirnir integration** - IoC container for dependency injection

## Headers

```cpp
#include <Freya/Freya.hpp>    // app surface (no Vulkan / SDL types)
```

See [API boundary](api-boundary.md) and [Flexibility](flexibility.md).

## Dependencies

Fetched via `FetchContent` (see `CMakeLists.txt`) unless noted:

- Vulkan SDK (system install, `find_package`)
- SDL3 (`release-3.4.16`)
- GLM (`1.0.3`)
- Assimp (`v6.0.5`, model loading)
- Skirnir (`v0.23.3`, IoC container)
- meshoptimizer (`v0.25`, LOD / mesh optimization)
- stb_image.h / stb_truetype.h (vendored in `src/Freya/Vendor/`)
- Dear ImGui (`v1.91.8`, examples only via `Examples/Common/`)
- nlohmann_json (`v3.11.3`, examples and GPU tests)
- googletest (`v1.17.0`, tests only)

## Quick Start

```cpp
#include <Freya/Freya.hpp>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& serviceProvider)
        : AbstractApplication(serviceProvider)
    {
        // Root singletons (shared across windows).
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
        // Scoped to the main window — resolve via GetMainServiceProvider().
        auto windowServices = GetMainServiceProvider();
        mLightService = windowServices->GetService<fra::LightService>();
        mFreyaOptions = windowServices->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();
        // Initialize your assets here
    }

    void Update() override
    {
        mRenderer->BeginFrame();
        mCamera.Apply(*mRenderer);
        mScene.Upload(*mRenderer);
        // Record scene instances here
        mRenderer->EndFrame();
    }

  private:
    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::TexturePool>  mTexturePool;
    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;
    fra::Camera                 mCamera;
    fra::Scene                  mScene;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>([](fra::FreyaOptionsBuilder& freyaOptions) {
        freyaOptions.SetTitle("My App")
            .SetWidth(1920)
            .SetHeight(1080)
            .SetVSync(false)
            .SetFullscreen(false);
    });
}
```

## Project Structure

```
Freya/
├── include/Freya/      # Public headers (Freya.hpp app surface, Advanced.hpp,
│                       #   Core/, Scene/, Asset/, Events/, Builders/)
├── src/Freya/          # .cpp + private headers (Asset/, Builders/, Core/,
│                       #   Containers/, Internal/, Scene/) + Vendor/
├── Examples/           # 8 demo apps + shared Examples/Common/
├── Shaders/            # GLSL in 12 variant dirs (Anim/, Billboard/, Cell/,
│                       #   Debug/, DeferredCompressed/, GpuDriven/, Include/,
│                       #   Material/, Pick/, Post/, Shadow/, Ui/)
├── Resources/          # IBL environment maps (*.hdr)
├── tests/              # GoogleTest unit specs (no Vulkan device)
├── benchmarks/         # google-benchmark suites (opt-in)
├── cmake/              # CMake helpers (shader compilation)
└── docs/               # Documentation
```

## Configuration

Configure Freya using the `FreyaOptionsBuilder`:

```cpp
freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
    freyaOptions.SetTitle("My Window")
        .SetWidth(1920)
        .SetHeight(1080)
        .SetVSync(true)
        .SetFullscreen(false)
        .SetDrawDistance(1000.0f);
});
```

### Available Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `title` | `std::string` | `"Freya Window"` | Window title |
| `width` | `std::uint32_t` | `800` | Window width |
| `height` | `std::uint32_t` | `600` | Window height |
| `vSync` | `bool` | `true` | Vertical synchronization |
| `fullscreen` | `bool` | `true` | Fullscreen mode |
| `sampleCount` | `std::uint32_t` | `1` | Retained for compatibility (scene path is single-sample) |
| `frameCount` | `std::uint32_t` | `3` | Number of frames in flight |
| `clearColor` | `glm::vec4` | `{0,0,0,0}` | Clear color |
| `drawDistance` | `float` | `1000.0f` | Render distance |
| `maxLights` | `std::uint32_t` | `64` | Max analytical lights (`kMaxLights`) |
| `iblIntensity` | `float` | `0.7f` | Image-based lighting scale |
| `exposure` | `float` | `0.7f` | HDR exposure before ACES tonemap |
| `environmentMapPath` | `std::string` | `./Resources/Environments/studio_small_09_4k.hdr` | Radiance `.hdr` (empty = procedural sky) |
| `ambientColor` | `glm::vec3` | `{1,1,1}` | Flat ambient tint |
| `ambientIntensity` | `float` | `0.03f` | Flat ambient fill |
| `enableShadows` | `bool` | `true` | Master shadow-map switch |
| `shadowCascadeCount` | `std::uint32_t` | `4` | Directional CSM cascade count (1–4) |
| `shadowMapResolution` | `std::uint32_t` | `2048` | Shadow map resolution (square) |
| `shadowBias` | `float` | `0.002f` | Depth bias when sampling shadows |
| `shadowLightSize` | `float` | `0.03f` | World-space penumbra radius hint |
| `shadowMaxSoftness` | `float` | `8.0f` | Soft-shadow kernel clamp (texels) |
| `shadowMinVisibility` | `float` | `0.0f` | Minimum shadow visibility floor |
| `maxSpotShadows` | `std::uint32_t` | `4` | Max concurrent spot shadow maps |
| `maxPointShadows` | `std::uint32_t` | `2` | Max concurrent point cube shadows |
| `shadowSampleCount` | `std::uint32_t` | `16` | Soft-shadow Poisson taps (1–16) |
| `shadowCascadeBlend` | `float` | `0.0f` | Cascade overlap fraction (0 = off) |
| `shadowCascadeDistance` | `float` | `80.0f` | Max view-space CSM range (meters) |
| `enableShadowMask` | `bool` | `false` | Half-res CSM mask before lighting |
| `shadowMaskResolutionDivisor` | `std::uint32_t` | `2` | Mask res = full / N |
| `shadowCascadeUpdatePeriod` | `std::uint32_t` | `2` | Redraw CSM every N frames (1 = always) |
| `shadowPointResolution` | `std::uint32_t` | `0` | Point cube face size (0 = cascade / divisor) |
| `shadowPointResolutionDivisor` | `std::uint32_t` | `2` | Point res divisor when absolute is 0 |
| `shadowSpotResolution` | `std::uint32_t` | `0` | Spot map size (0 = cascade / divisor) |
| `shadowSpotResolutionDivisor` | `std::uint32_t` | `2` | Spot res divisor when absolute is 0 |
| `shadowPointUpdatePeriod` | `std::uint32_t` | `2` | Rebuild point cubes every N frames |
| `ReverseZ` | `bool` | `false` | Opt in via `WithReverseZ()` |
| `depthPrecision` | `DepthPrecision` | `Standard` | D24 default; `Low` for D16, `High` for D32F via `SetDepthPrecision()` / `SetHighPrecisionDepth()` |
| `shaderRoot` | `std::string` | `./Resources/Shaders` | SPIR-V directory root |
| `enableSsao` | `bool` | `true` | Run SSAO compute pass |
| `enableTaa` | `bool` | `true` | Run TAA resolve + jitter |
| `enableBloom` | `bool` | `true` | Run bloom extract/blur |
| `ssaoResolutionDivisor` | `std::uint32_t` | `2` | SSAO res = full / N (1,2,4) |
| `ssaoRadius` | `float` | `0.5f` | Hemisphere radius (view-space meters) |
| `ssaoBias` | `float` | `0.025f` | View-Z acne bias |
| `ssaoPower` | `float` | `1.5f` | Occlusion curve power |
| `ssaoIntensity` | `float` | `0.5f` | Occlusion strength |
| `deferredDebugView` | `DeferredDebugView` | `None` | G-buffer / SSAO / shadow visualization |
| `taaCurrentWeight` | `float` | `0.12f` | TAA blend toward current |
| `taaHaltonPeriod` | `std::uint32_t` | `16` | Halton jitter sequence length |
| `taaQualityLevel` | `std::uint32_t` | `2` | Shader feature tier 0=Low … 3=Ultra |
| `taaVarianceGammaY` | `float` | `1.35f` | Luminance variance AABB scale |
| `taaVarianceGammaC` | `float` | `1.5f` | Chroma variance AABB scale |
| `taaDepthRejectThreshold` | `float` | `0.03f` | History depth-reject threshold |
| `taaSharpen` | `float` | `0.15f` | Post-resolve sharpen (0 = off) |
| `bloomResolutionDivisor` | `std::uint32_t` | `2` | Bloom res = full / N |
| `bloomThreshold` | `float` | `0.75f` | Bright-pass threshold |
| `bloomExtractScale` | `float` | `1.0f` | Extract gain |
| `bloomStrength` | `float` | `0.8f` | Bloom mix in composite |
| `meshLodPixelRef` | `float` | `128.0f` | Screen diameter (px) keeping LOD0 |
| `meshLodStep` | `float` | `1.75f` | Diameter shrink per LOD step (> 1) |
| `enableAnimLod` | `bool` | `true` | Distance-based animation rate LOD |
| `animLodHz` | `float[4]` | `{60,30,15,8}` | Pose updates/sec per tier (Near→Far) |
| `animLodExitDist` | `float[3]` | `{20,38,55}` | Leave-tier distances (meters) |
| `animLodEnterDist` | `float[3]` | `{17,32,48}` | Enter-tier distances (hysteresis) |
| `animBakeHz` | `float` | `30.0f` | Clip bake rate for `BakeClip` callers |
| `quantizeGpuAnimJoints` | `bool` | `true` | 16 B quantized GPU joint storage |

Use `SetShadowQuality` / `SetSsaoQuality` / `SetTaaQuality` /
`SetBloomQuality` / `SetAnimationQuality` (`Low`–`Ultra`, plus `Off` to
disable the feature) for presets; individual setters still override fields
after the preset. Runtime mirrors live on `Renderer`.

## Services

Freya registers services via Skirnir. Resolve **scoped** services
(`Window`, `Renderer`, `EventManager`, `LightService`, `FreyaOptions`) from
the window scope — `GetMainServiceProvider()` (or `GetWindowServices(*window)`
for secondary windows). Asset pools are process-wide **singletons** and can be
taken from the root provider:

| Service | Lifetime | Description |
|---------|----------|-------------|
| `Window` | Scoped (per window) | Window management |
| `Renderer` | Scoped (per window) | Main renderer |
| `EventManager` | Scoped (per window) | Event system |
| `LightService` | Scoped (per window) | Point / directional / spot / area lights |
| `FreyaOptions` | Scoped (per window) | Engine configuration |
| `MeshPool` | Singleton | Mesh asset management |
| `TexturePool` | Singleton | Texture asset management |
| `MaterialPool` | Singleton | Material asset management |

Advanced operations — frame stages (`InsertFrameStage` / `ReplaceFrameStage`),
viewport targets (`SetViewportTarget` / `ClearOutputTarget`), the ImGui seam
(`BeginUI` / `EndUI`, `GetImGuiNativeHandles`, `GetViewportImage`) — are not
`Renderer` members: use `fra::Advanced(renderer)` from
`<Freya/Advanced.hpp>` (`Core/RendererAdvanced.hpp`). Environment lighting and
shadow-map internals live in `src/` only and are not named public services.
