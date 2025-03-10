#pragma once

#include <Skirnir/ServiceCollection.hpp>

#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ForwardPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Core/AbstractApplication.hpp"

namespace FREYA_NAMESPACE
{
    class WindowBuilder;

    template <typename T>
    concept IsApplication = std::is_base_of_v<AbstractApplication, T>;

    class ApplicationBuilder
    {
      public:
        ApplicationBuilder() :
            mServiceCollection(std::make_shared<skr::ServiceCollection>())
        {
        }

        [[nodiscard]] Ref<skr::ServiceCollection> GetServiceCollection() const
        {
            return mServiceCollection;
        }

        ApplicationBuilder& WithWindow(
            const std::function<void(WindowBuilder&)>& windowBuilderFunc);

        ApplicationBuilder& WithRenderer(
            const std::function<void(RendererBuilder&)>& rendererBuilderFunc);

        template <typename T>
            requires IsApplication<T>
        Ref<T> Build()
        {
            mServiceCollection->AddSingleton<T>();

            mServiceCollection->AddTransient<WindowBuilder>();
            mServiceCollection->AddTransient<InstanceBuilder>();
            mServiceCollection->AddTransient<PhysicalDeviceBuilder>();
            mServiceCollection->AddTransient<DeviceBuilder>();
            mServiceCollection->AddTransient<SurfaceBuilder>();
            mServiceCollection->AddTransient<ForwardPassBuilder>();
            mServiceCollection->AddTransient<SwapChainBuilder>();
            mServiceCollection->AddTransient<ImageBuilder>();
            mServiceCollection->AddTransient<RendererBuilder>();

            mServiceCollection->AddSingleton<EventManager>();

            mServiceCollection->AddSingleton<Window>(
                [&](skr::ServiceProvider& serviceProvider) {
                    auto windowBuilder =
                        serviceProvider.GetService<WindowBuilder>();

                    mWindowBuilderFunc(*windowBuilder);

                    return windowBuilder
                        ->SetEventManager(
                            serviceProvider.GetService<EventManager>())
                        .Build();
                });

            mServiceCollection->AddSingleton<Renderer>(
                [&](skr::ServiceProvider& serviceProvider) {
                    auto rendererBuilder =
                        serviceProvider.GetService<RendererBuilder>();

                    mRendererBuilderFunc(*rendererBuilder);

                    auto window = serviceProvider.GetService<Window>();

                    return rendererBuilder
                        ->WithInstance([](InstanceBuilder& instanceBuilder) {
                            uint32_t extensionCount;
                            auto     extensionNames =
                                SDL_Vulkan_GetInstanceExtensions(
                                    &extensionCount);

                            instanceBuilder.AddValidationLayers();

                            for (auto i = 0; i < extensionCount; i++)
                            {
                                instanceBuilder.AddExtension(extensionNames[i]);
                            }
                        })
                        .SetWindow(window->mWindow)
                        .SetWidth(window->mWidth)
                        .SetHeight(window->mHeight)
                        .SetVSync(window->mVSync)
                        .SetEventManager(
                            serviceProvider.GetService<EventManager>())
                        .Build();
                });

            mServiceCollection->AddSingleton<MeshPool>(
                [&](skr::ServiceProvider& serviceProvider) {
                    return serviceProvider.GetService<Renderer>()
                        ->GetMeshPoolFactory()
                        ->CreateMeshPool();
                });

            mServiceCollection->AddSingleton<TexturePool>(
                [&](skr::ServiceProvider& serviceProvider) {
                    return serviceProvider.GetService<Renderer>()
                        ->GetTexturePoolFactory()
                        ->CreateTexturePool();
                });

            const auto serviceProvider =
                mServiceCollection->CreateServiceProvider();

            return serviceProvider->GetService<T>();
        }

      protected:
        std::function<void(WindowBuilder&)>   mWindowBuilderFunc;
        std::function<void(RendererBuilder&)> mRendererBuilderFunc;

        Ref<skr::ServiceCollection> mServiceCollection;
    };
} // namespace FREYA_NAMESPACE
