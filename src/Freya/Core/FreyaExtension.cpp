#include "FreyaExtension.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/LightServiceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Core/LightService.hpp"

namespace FREYA_NAMESPACE
{

    void FreyaExtension::ConfigureServices(skr::ServiceCollection& services)
    {
        services.AddSingleton<FreyaOptions>([this](skr::ServiceProvider&) {
            return mFreyaOptionsBuilder.Build();
        });

        services.AddTransient<WindowBuilder>();
        services.AddTransient<InstanceBuilder>();
        services.AddTransient<PhysicalDeviceBuilder>();
        services.AddTransient<DeviceBuilder>();
        services.AddTransient<SurfaceBuilder>();
        services.AddTransient<RenderPassBuilder>();
        services.AddTransient<SwapChainBuilder>();
        services.AddTransient<ImageBuilder>();
        services.AddTransient<RendererBuilder>();
        services.AddTransient<ShaderModuleBuilder>();
        services.AddTransient<CommandPoolBuilder>();
        services.AddTransient<BufferBuilder>();
        services.AddTransient<DeferredCompressedPassBuilder>();
        services.AddTransient<BloomPassBuilder>();
        services.AddTransient<CompositePassBuilder>();

        services.AddSingleton<Instance>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto instanceBuilder =
                    serviceProvider.GetService<InstanceBuilder>();

                return instanceBuilder->Build();
            });

        services.AddSingleton<Surface>(
            [](skr::ServiceProvider& serviceProvider) {
                auto surfaceBuilder =
                    serviceProvider.GetService<SurfaceBuilder>();

                return surfaceBuilder->Build();
            });

        services.AddSingleton<PhysicalDevice>(
            [](skr::ServiceProvider& serviceProvider) {
                auto physicalDeviceBuilder =
                    serviceProvider.GetService<PhysicalDeviceBuilder>();

                return physicalDeviceBuilder->Build();
            });

        services.AddSingleton<Device>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto deviceBuilder =
                    serviceProvider.GetService<DeviceBuilder>();

                return deviceBuilder->Build();
            });

        services.AddSingleton<CommandPool>(
            [](skr::ServiceProvider serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                return serviceProvider.GetService<CommandPoolBuilder>()
                    ->SetCount(freyaOptions->frameCount)
                    .Build();
            });

        services.AddTransient<SwapChain>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<SwapChainBuilder>()->Build();
            });

        services.AddSingleton<EventManager>();
        services.AddSingleton<MeshPool>();
        services.AddSingleton<TexturePool>();
        services.AddSingleton<MaterialPool>();

        // LightService singleton - uses IoC to resolve FreyaOptions dependency
        services.AddSingleton<LightService>(
            [](skr::ServiceProvider& serviceProvider) {
                auto device       = serviceProvider.GetService<Device>();
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                return skr::MakeRef<LightService>(
                    device,
                    serviceProvider.GetService<BufferBuilder>(),
                    freyaOptions->frameCount,
                    freyaOptions->maxLights);
            });

        services.AddSingleton<Window>(
            [](skr::ServiceProvider& serviceProvider) {
                auto windowBuilder =
                    serviceProvider.GetService<WindowBuilder>();

                return windowBuilder->Build();
            });

        // Always create forward RenderPass (needed by SwapChainBuilder for
        // framebuffer creation even in deferred mode)
        services.AddSingleton<RenderPass>(
            [](skr::ServiceProvider& serviceProvider) {
                auto renderPassBuilder =
                    serviceProvider.GetService<RenderPassBuilder>();

                return renderPassBuilder->Build();
            });

        // DeferredCompressedPass is NOT registered as a service — the
        // RendererBuilder creates it internally with the same SwapChain
        // that the Renderer uses, ensuring framebuffers reference the
        // correct swapchain images.

        services.AddSingleton<Renderer>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto rendererBuilder =
                    serviceProvider.GetService<RendererBuilder>();

                return rendererBuilder->Build();
            });
    }

} // namespace FREYA_NAMESPACE
