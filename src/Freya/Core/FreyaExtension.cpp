#include "FreyaExtension.hpp"

#include "Freya/Builders/BillboardPassBuilder.hpp"
#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/BoneMatrixResourcesBuilder.hpp"
#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DebugDrawPassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/GpuAnimPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/IndirectDrawSystemBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/LightServiceBuilder.hpp"
#include "Freya/Builders/MaterialDescriptorResourcesBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/PickPassBuilder.hpp"
#include "Freya/Builders/PostProcessBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/ShadowPassBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Builders/TranslucentPassBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MaterialTechniqueRegistry.hpp"
#include "Freya/Asset/LightingTechniqueRegistry.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/IndirectDrawSystem.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/ShadowPass.hpp"

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
        services.AddTransient<SwapChainBuilder>();
        services.AddTransient<ImageBuilder>();
        services.AddTransient<RenderTargetBuilder>();
        services.AddTransient<RendererBuilder>();
        services.AddTransient<ShaderModuleBuilder>();
        services.AddTransient<CommandPoolBuilder>();
        services.AddTransient<MaterialDescriptorResourcesBuilder>();
        services.AddTransient<BoneMatrixResourcesBuilder>();
        services.AddTransient<DeferredCompressedPassBuilder>();
        services.AddTransient<BloomPassBuilder>();
        services.AddTransient<TaaPassBuilder>();
        services.AddTransient<SsaoPassBuilder>();
        services.AddTransient<CompositePassBuilder>();
        services.AddTransient<PostProcessBuilder>();
        services.AddTransient<DebugDrawPassBuilder>();
        services.AddTransient<BillboardPassBuilder>();
        services.AddTransient<GpuAnimPassBuilder>();
        services.AddTransient<TranslucentPassBuilder>();
        services.AddTransient<ShadowPassBuilder>();
        services.AddTransient<PickPassBuilder>();
        services.AddTransient<IndirectDrawSystemBuilder>();

        services.AddSingleton<Instance>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();
                auto instanceBuilder =
                    serviceProvider.GetService<InstanceBuilder>();

                instanceBuilder->SetApplicationName(freyaOptions->title);

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
                auto swapChainBuilder =
                    serviceProvider.GetService<SwapChainBuilder>();

                return swapChainBuilder->Build();
            });

        services.AddSingleton<EventManager>();

        services.AddSingleton<MaterialDescriptorResources>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider
                    .GetService<MaterialDescriptorResourcesBuilder>()
                    ->Build();
            });

        services.AddSingleton<BoneMatrixResources>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<BoneMatrixResourcesBuilder>()
                    ->Build();
            });

        services.AddSingleton<TexturePool>();
        services.AddSingleton<MaterialPool>();
        services.AddSingleton<MaterialTechniqueRegistry>();
        services.AddSingleton<LightingTechniqueRegistry>();
        services.AddSingleton<MeshPool>();

        services.AddSingleton<LightService>(
            [](skr::ServiceProvider& serviceProvider) {
                auto device       = serviceProvider.GetService<Device>();
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto lights = skr::MakeArc<LightService>(
                    device, freyaOptions->frameCount, freyaOptions->maxLights);
                lights->SetIblIntensity(freyaOptions->iblIntensity);
                lights->SetExposure(freyaOptions->exposure);
                return lights;
            });

        services.AddSingleton<IBLService>();

        services.AddSingleton<ShadowPass>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<ShadowPassBuilder>()->Build();
            });

        services.AddSingleton<PickPass>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<PickPassBuilder>()->Build();
            });

        services.AddSingleton<IndirectDrawSystem>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<IndirectDrawSystemBuilder>()
                    ->Build();
            });

        services.AddSingleton<Window>(
            [](skr::ServiceProvider& serviceProvider) {
                auto windowBuilder =
                    serviceProvider.GetService<WindowBuilder>();

                return windowBuilder->Build();
            });

        services.AddSingleton<Renderer>(
            [](skr::ServiceProvider& serviceProvider) {
                auto rendererBuilder =
                    serviceProvider.GetService<RendererBuilder>();

                return rendererBuilder->Build();
            });
    }

} // namespace FREYA_NAMESPACE
