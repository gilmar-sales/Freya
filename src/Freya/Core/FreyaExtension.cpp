#include "FreyaExtension.hpp"

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

        services.AddSingleton<LODPool>();
        services.AddSingleton<LODService>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();
                auto lodPool      = serviceProvider.GetService<LODPool>();
                auto meshPool     = serviceProvider.GetService<MeshPool>();
                auto renderer     = serviceProvider.GetService<Renderer>();
                auto device       = serviceProvider.GetService<Device>();
                auto physicalDevice =
                    serviceProvider.GetService<PhysicalDevice>();
                auto commandPool = serviceProvider.GetService<CommandPool>();

                LODBuilder lodBuilder;
                lodBuilder.SetDevice(device);
                lodBuilder.SetPhysicalDevice(physicalDevice);
                lodBuilder.SetCommandPool(commandPool);
                lodBuilder.SetLODPool(lodPool);
                lodBuilder.SetMeshPool(meshPool);
                lodBuilder.SetRenderer(renderer);
                lodBuilder.SetFreyaOptions(freyaOptions);
                lodBuilder.SetServiceProvider(
                    serviceProvider.GetService<skr::ServiceProvider>());

                return lodBuilder.Build();
            });

        services.AddSingleton<EventManager>();
        services.AddSingleton<MeshPool>();
        services.AddSingleton<TexturePool>();
        services.AddSingleton<MaterialPool>();

        services.AddSingleton<Window>(
            [](skr::ServiceProvider& serviceProvider) {
                auto windowBuilder =
                    serviceProvider.GetService<WindowBuilder>();

                return windowBuilder->Build();
            });

        services.AddSingleton<RenderPass>(
            [](skr::ServiceProvider& serviceProvider) {
                auto renderPassBuilder =
                    serviceProvider.GetService<RenderPassBuilder>();

                return renderPassBuilder->Build();
            });

        services.AddSingleton<Renderer>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto rendererBuilder =
                    serviceProvider.GetService<RendererBuilder>();

                return rendererBuilder->Build();
            });
    }

} // namespace FREYA_NAMESPACE