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
#include "Freya/Builders/MaterialDescriptorResourcesBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/PickPassBuilder.hpp"
#include "Freya/Builders/PostProcessBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/ShadowMaskPassBuilder.hpp"
#include "Freya/Builders/ShadowPassBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Builders/TranslucentPassBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/LightingTechniqueRegistry.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MaterialTechniqueRegistry.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/IPlatform.hpp"
#include "Freya/Core/IndirectDrawSystem.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/SdlPlatform.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/TransferCommandPool.hpp"
#include "Freya/Core/WindowConfigContext.hpp"

namespace FREYA_NAMESPACE
{

    void FreyaExtension::ConfigureServices(skr::ServiceCollection& services)
    {
        const auto baseOptions = mFreyaOptionsBuilder.Build();

        services.AddSingleton<FreyaOptionsTemplate>(
            [baseOptions](skr::ServiceProvider&) {
                return skr::MakeArc<FreyaOptionsTemplate>(
                    FreyaOptionsTemplate { .options = baseOptions });
            });

        services.AddScoped<WindowConfigContext>();

        services.AddScoped<FreyaOptions>(
            [](skr::ServiceProvider& serviceProvider) {
                const auto ctx =
                    serviceProvider.GetService<WindowConfigContext>();
                if (!ctx->options)
                {
                    serviceProvider.GetService<skr::Logger<FreyaExtension>>()
                        ->LogFatal(
                            "WindowConfigContext must be seeded with "
                            "FreyaOptions before resolving FreyaOptions.");
                }
                return ctx->options;
            });

        services.AddSingleton<IPlatform, SdlPlatform>();

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
        services.AddTransient<ShadowMaskPassBuilder>();
        services.AddTransient<CompositePassBuilder>();
        services.AddTransient<PostProcessBuilder>();
        services.AddTransient<DebugDrawPassBuilder>();
        services.AddTransient<BillboardPassBuilder>();
        services.AddTransient<GpuAnimPassBuilder>();
        services.AddTransient<TranslucentPassBuilder>();
        services.AddTransient<ShadowPassBuilder>();
        services.AddTransient<PickPassBuilder>();
        services.AddTransient<IndirectDrawSystemBuilder>();

        // Instance reads the process-wide template — never scoped FreyaOptions.
        services.AddSingleton<Instance>(
            [baseOptions](skr::ServiceProvider& serviceProvider) {
                auto instanceBuilder =
                    serviceProvider.GetService<InstanceBuilder>();

                instanceBuilder->SetApplicationName(baseOptions->title);

                return instanceBuilder->Build();
            });

        services.AddScoped<Surface>([](skr::ServiceProvider& serviceProvider) {
            return serviceProvider.GetService<SurfaceBuilder>()->Build();
        });

        // Device / PhysicalDevice are singletons but must be first resolved
        // from a window scope (DeviceBuilder needs Surface).
        services.AddSingleton<PhysicalDevice>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<PhysicalDeviceBuilder>()
                    ->Build();
            });

        services.AddSingleton<Device>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<DeviceBuilder>()->Build();
            });

        services.AddScoped<CommandPool>(
            [](skr::ServiceProvider serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                return serviceProvider.GetService<CommandPoolBuilder>()
                    ->SetCount(freyaOptions->frameCount)
                    .Build();
            });

        services.AddSingleton<TransferCommandPool>(
            [](skr::ServiceProvider& serviceProvider) {
                auto pool = serviceProvider.GetService<CommandPoolBuilder>()
                                ->SetCount(2)
                                .Build();
                return skr::MakeArc<TransferCommandPool>(std::move(pool));
            });

        services.AddTransient<SwapChain>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<SwapChainBuilder>()->Build();
            });

        services.AddScoped<EventManager>();

        services.AddSingleton<MaterialDescriptorResources>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider
                    .GetService<MaterialDescriptorResourcesBuilder>()
                    ->Build();
            });

        services.AddScoped<BoneMatrixResources>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<BoneMatrixResourcesBuilder>()
                    ->Build();
            });

        services.AddSingleton<TexturePool>();
        services.AddSingleton<MaterialPool>();
        services.AddSingleton<MaterialTechniqueRegistry>();
        services.AddSingleton<LightingTechniqueRegistry>();
        services.AddSingleton<MeshPool>();

        services.AddScoped<LightService>();

        services.AddScoped<IBLService>();

        services.AddScoped<ShadowPass>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<ShadowPassBuilder>()->Build();
            });

        services.AddScoped<PickPass>([](skr::ServiceProvider& serviceProvider) {
            return serviceProvider.GetService<PickPassBuilder>()->Build();
        });

        services.AddScoped<IndirectDrawSystem>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<IndirectDrawSystemBuilder>()
                    ->Build();
            });

        services.AddScoped<Window>([](skr::ServiceProvider& serviceProvider) {
            return serviceProvider.GetService<WindowBuilder>()->Build();
        });

        services.AddScoped<Renderer>([](skr::ServiceProvider& serviceProvider) {
            return serviceProvider.GetService<RendererBuilder>()->Build();
        });
    }

} // namespace FREYA_NAMESPACE
