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
| `GetRenderTargetBuilder()` | Get a render target builder |
| `SetOutputTarget(RenderTarget)` | Redirect scene composite to an offscreen target |
| `ClearOutputTarget()` | Restore composite destination to the swapchain |
| `GetOutputTarget()` | Current output target, or null if swapchain |
| `BindBuffer(Buffer)` | Bind a buffer for rendering |
| `GetCurrentFrameIndex()` | Get current frame index |
| `GetFrameCount()` | Get total frame count |
| `CalculateProjectionMatrix(float near, float far)` | Calculate projection matrix |
| `ClearProjections()` | Clear all projection matrices |
| `UpdateProjection(ProjectionUniformBuffer&)` | Update projection data |
| `UpdateModel(const glm::mat4&)` | Update model matrix |

### RenderTarget / output target

By default the scene composites onto the window swapchain. To render into a
sampled texture (for example an ImGui viewport), build a `RenderTarget` and bind
it on the renderer. Scene, bloom, and composite then use the target extent
(independent of the window size). Freya does not integrate ImGui; it only
produces the texture. With an output target bound, `EndFrame` still presents a
cleared swapchain so the application can draw UI afterward.

```cpp
auto viewport = mRenderer->GetRenderTargetBuilder()
    .SetWidth(panelW)
    .SetHeight(panelH)
    .Build();

mRenderer->SetOutputTarget(viewport);

mRenderer->BeginFrame();
// ... draw scene ...
mRenderer->EndFrame();

// Sample in UI (e.g. ImGui) via viewport->GetColorImageView()
// and viewport->GetSampler()
```

Call `ClearOutputTarget()` to restore composite-to-swapchain behavior.

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
