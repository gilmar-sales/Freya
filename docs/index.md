# Freya

A Vulkan-based rendering engine powered by [Skirnir](https://github.com/gilmar-sales/Skirnir) for dependency injection.

## Features

- **Vulkan-backed rendering** - Modern graphics API with high performance
- **Deferred & Forward rendering** - Choose between rendering strategies
- **Custom render targets** - Redirect scene output to an offscreen
  `RenderTarget` at runtime (see [Core](core.md)); not configured via FreyaOptions
- **Asset management** - Built-in support for meshes, textures, and materials
- **Event system** - Flexible pub/sub event handling for window, keyboard, mouse, and gamepad
- **Builder pattern** - Fluent API for constructing renderer components
- **Skirnir integration** - IoC container for dependency injection

## Dependencies

- Vulkan SDK
- SDL3
- GLM
- Assimp (for 3D model loading)
- Skirnir (IoC Container)

## Quick Start

```cpp
#include <Freya/Core/AbstractApplication.hpp>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider)
        : AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();
        // Initialize your assets here
    }

    void Update() override
    {
        mRenderer->BeginFrame();
        // Render your scene here
        mRenderer->EndFrame();
    }
};

int main(int argc, const char** argv)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension& freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions.SetTitle("My App")
                        .SetWidth(1920)
                        .SetHeight(1080)
                        .SetVSync(false)
                        .SetSampleCount(8);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
```

## Project Structure

```
Freya/
├── src/Freya/
│   ├── Core/           # Core engine components
│   ├── Builders/       # Builder classes for fluent construction
│   ├── Asset/          # Mesh, Texture, Material management
│   ├── Events/         # Event system (pub/sub)
│   └── Containers/     # Custom container data structures
├── include/Freya/      # Public headers
├── Examples/           # Example applications
├── Shaders/            # GLSL/Vulkan shaders
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
        .SetSampleCount(4)
        .SetDrawDistance(1000.0f)
        .SetRenderingStrategy(fra::RenderingStrategy::Forward);
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
| `sampleCount` | `std::uint32_t` | `8` | MSAA sample count |
| `frameCount` | `std::uint32_t` | `4` | Number of frames in flight |
| `drawDistance` | `float` | `1000.0f` | Render distance |
| `maxLights` | `std::uint32_t` | `16` | Max analytical lights (`MAX_LIGHTS`) |
| `iblIntensity` | `float` | `1.0f` | Image-based lighting scale |
| `environmentMapPath` | `std::string` | `""` | Optional Radiance `.hdr` (empty = procedural sky) |
| `renderingStrategy` | `RenderingStrategy` | `Forward` | Forward or Deferred |

## Services

Freya provides the following services via Skirnir's service provider:

| Service | Type | Description |
|---------|------|-------------|
| `Window` | `skr::Arc<Window>` | Window management |
| `Renderer` | `skr::Arc<Renderer>` | Main renderer |
| `EventManager` | `skr::Arc<EventManager>` | Event system |
| `MeshPool` | `skr::Arc<MeshPool>` | Mesh asset management |
| `TexturePool` | `skr::Arc<TexturePool>` | Texture asset management |
| `MaterialPool` | `skr::Arc<MaterialPool>` | Material asset management |
| `LightService` | `skr::Arc<LightService>` | Point / directional / spot lights |
| `IBLService` | `skr::Arc<IBLService>` | Environment / irradiance / BRDF LUT |
