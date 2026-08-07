# Builders

Freya uses the builder pattern to provide a fluent API for constructing engine
components. When using `FreyaExtension`, prefer its configure hooks instead of
constructing builders manually.

## FreyaExtension hooks

```cpp
.WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
    freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
        o.SetTitle("My App")
         .SetShaderRoot("./Resources/Shaders")
         .SetEnableBloom(true);
    })
    .WithInstance([](fra::InstanceBuilder& b) {
        b.SetApplicationVersion(1, 0, 0);
    })
    .WithPhysicalDevice([](fra::PhysicalDeviceBuilder& b) {
        b.PreferDeviceType(vk::PhysicalDeviceType::eDiscreteGpu);
    })
    .WithDevice([](fra::DeviceBuilder& b) {
        b.AddExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    })
    .WithSwapChain([](fra::SwapChainBuilder& b) {
        b.PreferPresentMode(vk::PresentModeKHR::eFifo);
    });
})
```

Shader SPIR-V paths are `shaderRoot + "/DeferredCompressed|Shadow|Pick/…"`.

## Overview

All builders follow a consistent pattern:

```cpp
auto component = SomeBuilder(params)
    .SetProperty1(value1)
    .SetProperty2(value2)
    .Build();
```

## WindowBuilder

Creates `Window` instances. WindowBuilder requires an EventManager, FreyaOptions, and logger references.

```cpp
auto window = WindowBuilder(eventManager, freyaOptions, logger, windowLogger)
    .Build();
```

## InstanceBuilder

Creates Vulkan `Instance` objects. InstanceBuilder automatically enables validation layers and adds SDL Vulkan extensions.

```cpp
auto instance = InstanceBuilder(logger)
    .SetApplicationName("My App")
    .SetApplicationVersion(1, 0, 0)
    .Build();
```

## DeviceBuilder

Creates Vulkan logical `Device`. Supports `AddExtension` / `AddExtensions`.

```cpp
auto device = serviceProvider->GetService<fra::DeviceBuilder>()
    ->AddExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
    .Build();
```

## PhysicalDeviceBuilder

Selects and configures Vulkan `PhysicalDevice` (GPU). Use
`PreferDeviceType` or `SetTypePriorities`.

```cpp
auto physicalDevice = serviceProvider->GetService<fra::PhysicalDeviceBuilder>()
    ->PreferDeviceType(vk::PhysicalDeviceType::eDiscreteGpu)
    .Build();
```

## SurfaceBuilder

Creates Vulkan `Surface` for window integration.

```cpp
auto surface = SurfaceBuilder()
    .SetInstance(instance)
    .SetWindow(window)
    .SetPhysicalDevice(physicalDevice)
    .Build();
```

## SwapChainBuilder

Creates Vulkan `SwapChain`.

```cpp
auto swapChain = SwapChainBuilder()
    .SetSurface(surface)
    .SetPhysicalDevice(physicalDevice)
    .SetDevice(device)
    .SetWidth(1920)
    .SetHeight(1080)
    .SetVSync(true)
    .Build();
```

## CommandPoolBuilder

Creates `CommandPool` for command buffer management.

```cpp
auto commandPool = CommandPoolBuilder()
    .SetDevice(device)
    .SetPhysicalDevice(physicalDevice)
    .Build();
```

## RendererBuilder

Creates the main `Renderer` instance (deferred compressed scene path).

```cpp
auto renderer = RendererBuilder(
    instance, surface, physicalDevice, device,
    commandPool, swapChain, eventManager,
    window, freyaOptions, serviceProvider)
    .Build();
```

## ShaderModuleBuilder

Creates Vulkan `ShaderModule` from SPIR-V.

```cpp
auto shaderModule = ShaderModuleBuilder()
    .SetDevice(device)
    .SetShaderPath("shaders/vert.spv")
    .SetStage(vk::ShaderStageFlagBits::eVertex)
    .Build();
```

## BufferBuilder

Creates `Buffer` objects for GPU data storage.

```cpp
auto buffer = BufferBuilder(device)
    .SetData(&myData[0])
    .SetSize(sizeof(myData))
    .SetUsage(BufferUsage::Vertex)
    .Build();
```

### BufferUsage Enum

| Value | Description |
|-------|-------------|
| `Staging` | Staging buffer for data transfer |
| `Vertex` | Vertex data buffer |
| `Index` | Index buffer |
| `Uniform` | Uniform buffer object (UBO) |
| `Instance` | Per-instance data buffer |
| `Image` | Image buffer |

## RenderTargetBuilder

Creates offscreen `RenderTarget` objects for redirecting scene composite output
(for example to sample in an ImGui panel). Prefer obtaining the builder from
`Renderer::GetRenderTargetBuilder()`.

```cpp
auto target = mRenderer->GetRenderTargetBuilder()
    .SetWidth(1280)
    .SetHeight(720)
    .Build();

mRenderer->SetOutputTarget(target);
```

Optional `SetFormat` overrides the surface format (default). Bind or clear the
target via `Renderer::SetOutputTarget` / `ClearOutputTarget` (see [Core](core.md)).

## ImageBuilder

Creates `Image` objects (textures, render targets).

```cpp
auto image = ImageBuilder(surface, device, logger, serviceProvider)
    .SetUsage(ImageUsage::Texture)
    .SetFormat(vk::Format::eR8G8B8A8Srgb)
    .SetWidth(1024)
    .SetHeight(1024)
    .SetSamples(vk::SampleCountFlagBits::e1)
    .SetData(imageData)
    .Build();
```

### ImageUsage Enum

| Value | Description |
|-------|-------------|
| `Color` | Color attachment |
| `Depth` | Depth attachment |
| `Sampling` | Sampled image |
| `Texture` | Texture for sampling |
| `GBufferPosition` | G-buffer position |
| `GBufferNormal` | G-buffer normal |
| `GBufferAlbedo` | G-buffer albedo |

## FreyaOptionsBuilder

Configures engine options.

```cpp
freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
    freyaOptions.SetTitle("My App")
        .SetWidth(1920)
        .SetHeight(1080)
        .SetVSync(false)
        .SetSampleCount(8)
        .SetFullscreen(false)
        .SetDrawDistance(1000.0f)
        .SetShadowQuality(fra::ShadowQuality::Medium);
});
```

`SetShadowQuality` applies Low / Medium / High / Ultra budgets for shadow map
resolution, cascade count, spot/point slots, and soft-shadow tap count.
Individual setters can still override fields after the preset.

## DeferredCompressedPassBuilder

Creates the deferred geometry / lighting passes (G-buffer + Scene Color HDR).

```cpp
auto deferredPass = DeferredCompressedPassBuilder(device, surface, freyaOptions, serviceProvider)
    .Build(swapChain);
```

Built internally by `RendererBuilder`; not typically constructed by apps.
