#pragma once

#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ForwardPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"
#include "Freya/FreyaOptions.hpp"

#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"

#include "Freya/Core/AbstractApplication.hpp"

namespace FREYA_NAMESPACE
{
    template <typename T>
    concept IsApplication = std::is_base_of_v<AbstractApplication, T>;

    class ApplicationBuilder
    {
      public:
        ApplicationBuilder() :
            mServiceCollection(std::make_shared<skr::ServiceCollection>())
        {
            mFreyaOptionsFunc = [](FreyaOptions& freyaOptions) {};
        }

        [[nodiscard]] Ref<skr::ServiceCollection> GetServiceCollection() const
        {
            return mServiceCollection;
        }

        ApplicationBuilder& WithOptions(
            std::function<void(FreyaOptions&)> freyaOptionsFunc);

        template <typename T>
            requires IsApplication<T>
        Ref<T> Build()
        {
            mServiceCollection->AddSingleton<T>();

            mServiceCollection->AddSingleton<FreyaOptions>(
                [freyaOptionsFunc =
                     mFreyaOptionsFunc](skr::ServiceProvider& serviceProvider) {
                    auto freyaOptions = skr::MakeRef<FreyaOptions>();

                    if (freyaOptionsFunc)
                    {
                        freyaOptionsFunc(*freyaOptions);
                    }

                    return freyaOptions;
                });

            mServiceCollection->AddTransient<WindowBuilder>();
            mServiceCollection->AddTransient<InstanceBuilder>();
            mServiceCollection->AddTransient<PhysicalDeviceBuilder>();
            mServiceCollection->AddTransient<DeviceBuilder>();
            mServiceCollection->AddTransient<SurfaceBuilder>();
            mServiceCollection->AddTransient<ForwardPassBuilder>();
            mServiceCollection->AddTransient<SwapChainBuilder>();
            mServiceCollection->AddTransient<ImageBuilder>();
            mServiceCollection->AddTransient<RendererBuilder>();
            mServiceCollection->AddTransient<ShaderModuleBuilder>();
            mServiceCollection->AddTransient<CommandPoolBuilder>();

            mServiceCollection->AddSingleton<Instance>(
                [](skr::ServiceProvider& serviceProvider) {
                    auto freyaOptions =
                        serviceProvider.GetService<FreyaOptions>();

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
                    auto freyaOptions =
                        serviceProvider.GetService<FreyaOptions>();

                    auto deviceBuilder =
                        serviceProvider.GetService<DeviceBuilder>();

                    return deviceBuilder->Build();
                });

            mServiceCollection->AddSingleton<CommandPool>(
                [](skr::ServiceProvider serviceProvider) {
                    auto freyaOptions =
                        serviceProvider.GetService<FreyaOptions>();

                    return serviceProvider.GetService<CommandPoolBuilder>()
                        ->SetCount(freyaOptions->frameCount)
                        .Build();
                });

            mServiceCollection->AddSingleton<SwapChain>(
                [](skr::ServiceProvider& serviceProvider) {
                    auto swapChainBuilder =
                        serviceProvider.GetService<SwapChainBuilder>();

                    return swapChainBuilder->Build();
                });

            mServiceCollection->AddSingleton<EventManager>();
            mServiceCollection->AddSingleton<MeshPool>();
            mServiceCollection->AddSingleton<TexturePool>();

            mServiceCollection->AddSingleton<Window>(
                [](skr::ServiceProvider& serviceProvider) {
                    auto windowBuilder =
                        serviceProvider.GetService<WindowBuilder>();

                    return windowBuilder->Build();
                });

            mServiceCollection->AddSingleton<ForwardPass>(
                [](skr::ServiceProvider& serviceProvider) {
                    auto forwardPassBuilder =
                        serviceProvider.GetService<ForwardPassBuilder>();

                    return forwardPassBuilder->Build();
                });

            mServiceCollection->AddSingleton<Renderer>(
                [](skr::ServiceProvider& serviceProvider) {
                    auto freyaOptions =
                        serviceProvider.GetService<FreyaOptions>();

                    auto rendererBuilder =
                        serviceProvider.GetService<RendererBuilder>();

                    return rendererBuilder->Build();
                });

            const auto serviceProvider =
                mServiceCollection->CreateServiceProvider();

            return serviceProvider->GetService<T>();
        }

      protected:
        std::function<void(FreyaOptions&)> mFreyaOptionsFunc;

        Ref<skr::ServiceCollection> mServiceCollection;
    };
} // namespace FREYA_NAMESPACE
