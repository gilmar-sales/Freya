# Builders

Freya uses the builder pattern to provide a fluent API for constructing engine components.

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

Creates Vulkan logical `Device`.

```cpp
auto device = DeviceBuilder()
    .SetPhysicalDevice(physicalDevice)
    .SetSurface(surface)
    .SetCommandPool(commandPool)
    .Build();
```

## PhysicalDeviceBuilder

Selects and configures Vulkan `PhysicalDevice` (GPU).

```cpp
auto physicalDevice = PhysicalDeviceBuilder()
    .SetSurface(surface)
    .SetDeviceTypePreference(vk::PhysicalDeviceType::eDiscreteGpu)
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

## RenderPassBuilder

Creates Vulkan `RenderPass`.

```cpp
auto renderPass = RenderPassBuilder()
    .SetSurface(surface)
    .SetPhysicalDevice(physicalDevice)
    .SetDevice(device)
    .SetSamples(4)
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

Creates the main `Renderer` instance.

```cpp
auto renderer = RendererBuilder(
    instance, surface, physicalDevice, device,
    commandPool, swapChain, renderPass, eventManager,
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
        .SetRenderingStrategy(fra::RenderingStrategy::Forward);
});
```

## DeferredCompressedPassBuilder

Creates deferred rendering passes with G-buffer compression.

```cpp
auto deferredPass = DeferredCompressedPassBuilder(device, surface, freyaOptions, serviceProvider)
    .Build();
```

Used for deferred rendering strategies to efficiently compress G-buffer data.
