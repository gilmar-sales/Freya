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
| `mWindow` | `Ref<Window>` | Window instance |
| `mRenderer` | `Ref<Renderer>` | Renderer instance |
| `mEventManager` | `Ref<EventManager>` | Event manager instance |
| `mDeltaTime` | `float` | Time since last frame (seconds) |

## Renderer

Main rendering coordinator. Handles frame management, swap chain, and rendering commands.

```cpp
mRenderer->BeginFrame();
// ... rendering commands ...
mRenderer->EndFrame();
```

### Key Methods

| Method | Description |
|--------|-------------|
| `BeginFrame()` | Start a new frame |
| `EndFrame()` | End and present the current frame |
| `RebuildSwapChain()` | Recreate swap chain (e.g., on resize) |
| `SetVSync(bool)` | Enable/disable vertical sync |
| `SetSamples(uint32_t)` | Set MSAA sample count |
| `SetDrawDistance(float)` | Set render distance |
| `GetBufferBuilder()` | Get a buffer builder instance |
| `BindBuffer(Buffer)` | Bind a buffer for rendering |
| `GetCurrentFrameIndex()` | Get current frame index |
| `GetFrameCount()` | Get total frame count |
| `CalculateProjectionMatrix(float near, float far)` | Calculate projection matrix |
| `ClearProjections()` | Clear all projection matrices |
| `UpdateProjection(ProjectionUniformBuffer&)` | Update projection data |
| `UpdateModel(const glm::mat4&)` | Update model matrix |

## Window

Window management and input handling.

```cpp
mWindow->IsRunning();      // Check if window is running
mWindow->Update();         // Process window events
mWindow->GetDeltaTime();   // Get delta time in seconds
```

## Device

Vulkan logical device wrapper.

## Instance

Vulkan instance wrapper.

## Surface

Vulkan surface wrapper for window integration.

## SwapChain

Vulkan swap chain for framebuffer management.

## PhysicalDevice

Vulkan physical device (GPU) selection and properties.

## RenderPass

Vulkan render pass configuration.

## CommandPool

Command buffer pool for recording rendering commands.

## Buffer

GPU buffer abstraction (vertex, index, uniform, instance buffers).

## Image

GPU image/texture abstraction.

## UniformBuffer

Uniform buffer for shader data.

## DeferredCompressedPass

Deferred rendering pass with G-buffer compression.
