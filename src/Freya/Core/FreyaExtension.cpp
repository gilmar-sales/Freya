#include "FreyaExtension.hpp"

namespace FREYA_NAMESPACE
{

    void FreyaExtension::Configure(skr::ServiceCollection& services) const
    {
        services.AddSingleton<FreyaOptions>(
            [freyaOptionsBuilderFunc = mFreyaOptionsBuilderFunc](
                skr::ServiceProvider& serviceProvider) {
                auto freyaOptionsBuilder = FreyaOptionsBuilder();

                if (freyaOptionsBuilderFunc)
                {
                    freyaOptionsBuilderFunc(freyaOptionsBuilder);
                }

                return freyaOptionsBuilder.Build();
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
