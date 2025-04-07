#include "ApplicationBuilder.hpp"

namespace FREYA_NAMESPACE
{
    ApplicationBuilder& ApplicationBuilder::WithOptions(
        std::function<void(FreyaOptionsBuilder&)> freyaOptionsBuilderFunc)
    {
        mFreyaOptionsBuilderFunc = freyaOptionsBuilderFunc;

        return *this;
    }

    void ApplicationBuilder::AddServices()
    {

        mServiceCollection->AddSingleton<FreyaOptions>(
            [freyaOptionsBuilderFunc = mFreyaOptionsBuilderFunc](
                skr::ServiceProvider& serviceProvider) {
                auto freyaOptionsBuilder = FreyaOptionsBuilder();

                if (freyaOptionsBuilderFunc)
                {
                    freyaOptionsBuilderFunc(freyaOptionsBuilder);
                }

                return freyaOptionsBuilder.Build();
            });

        mServiceCollection->AddTransient<WindowBuilder>();
        mServiceCollection->AddTransient<InstanceBuilder>();
        mServiceCollection->AddTransient<PhysicalDeviceBuilder>();
        mServiceCollection->AddTransient<DeviceBuilder>();
        mServiceCollection->AddTransient<SurfaceBuilder>();
        mServiceCollection->AddTransient<RenderPassBuilder>();
        mServiceCollection->AddTransient<SwapChainBuilder>();
        mServiceCollection->AddTransient<ImageBuilder>();
        mServiceCollection->AddTransient<RendererBuilder>();
        mServiceCollection->AddTransient<ShaderModuleBuilder>();
        mServiceCollection->AddTransient<CommandPoolBuilder>();

        mServiceCollection->AddSingleton<Instance>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto instanceBuilder =
                    serviceProvider.GetService<InstanceBuilder>();

                return instanceBuilder->Build();
            });

        mServiceCollection->AddSingleton<Surface>(
            [](skr::ServiceProvider& serviceProvider) {
                auto surfaceBuilder =
                    serviceProvider.GetService<SurfaceBuilder>();

                return surfaceBuilder->Build();
            });

        mServiceCollection->AddSingleton<PhysicalDevice>(
            [](skr::ServiceProvider& serviceProvider) {
                auto physicalDeviceBuilder =
                    serviceProvider.GetService<PhysicalDeviceBuilder>();

                return physicalDeviceBuilder->Build();
            });

        mServiceCollection->AddSingleton<Device>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto deviceBuilder =
                    serviceProvider.GetService<DeviceBuilder>();

                return deviceBuilder->Build();
            });

        mServiceCollection->AddSingleton<CommandPool>(
            [](skr::ServiceProvider serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                return serviceProvider.GetService<CommandPoolBuilder>()
                    ->SetCount(freyaOptions->frameCount)
                    .Build();
            });

        mServiceCollection->AddTransient<SwapChain>(
            [](skr::ServiceProvider& serviceProvider) {
                return serviceProvider.GetService<SwapChainBuilder>()->Build();
            });

        mServiceCollection->AddSingleton<EventManager>();
        mServiceCollection->AddSingleton<MeshPool>();
        mServiceCollection->AddSingleton<TexturePool>();
        mServiceCollection->AddSingleton<MaterialPool>();

        mServiceCollection->AddSingleton<Window>(
            [](skr::ServiceProvider& serviceProvider) {
                auto windowBuilder =
                    serviceProvider.GetService<WindowBuilder>();

                return windowBuilder->Build();
            });

        mServiceCollection->AddSingleton<RenderPass>(
            [](skr::ServiceProvider& serviceProvider) {
                auto renderPassBuilder =
                    serviceProvider.GetService<RenderPassBuilder>();

                return renderPassBuilder->Build();
            });

        mServiceCollection->AddSingleton<Renderer>(
            [](skr::ServiceProvider& serviceProvider) {
                auto freyaOptions = serviceProvider.GetService<FreyaOptions>();

                auto rendererBuilder =
                    serviceProvider.GetService<RendererBuilder>();

                return rendererBuilder->Build();
            });
    }

} // namespace FREYA_NAMESPACE
