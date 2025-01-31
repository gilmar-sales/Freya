#pragma once

#include <ServiceCollection.hpp>

#include "Builders/RendererBuilder.hpp"
#include "Builders/WindowBuilder.hpp"
#include "Core/AbstractApplication.hpp"

namespace FREYA_NAMESPACE
{
    class WindowBuilder;

    template <typename T>
    concept IsApplication = std::is_base_of_v<AbstractApplication, T>;

    class ApplicationBuilder
    {
      public:
        ApplicationBuilder() :
            mServiceCollection(std::make_shared<ServiceCollection>()),
            mWindowBuilder(WindowBuilder())
        {
        }

        [[nodiscard]] std::shared_ptr<ServiceCollection> GetServiceCollection()
            const
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

            mServiceCollection->AddSingleton<EventManager>();

            mServiceCollection->AddSingleton<Window>(
                [&](ServiceProvider& serviceProvider) {
                    return mWindowBuilder
                        .SetEventManager(
                            serviceProvider.GetService<EventManager>())
                        .Build();
                });

            mServiceCollection->AddSingleton<Renderer>(
                [&](ServiceProvider& serviceProvider) {
                    auto window = serviceProvider.GetService<Window>();

                    return mRendererBuilder
                        .WithInstance([](InstanceBuilder& instanceBuilder) {
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
                [&](ServiceProvider& serviceProvider) {
                    return serviceProvider.GetService<Renderer>()
                        ->GetMeshPoolFactory()
                        ->CreateMeshPool();
                });

            mServiceCollection->AddSingleton<TexturePool>(
                [&](ServiceProvider& serviceProvider) {
                    return serviceProvider.GetService<Renderer>()
                        ->GetTexturePoolFactory()
                        ->CreateTexturePool();
                });

            const auto serviceProvider =
                mServiceCollection->CreateServiceProvider();

            return serviceProvider->GetService<T>();
        }

      protected:
        std::shared_ptr<ServiceCollection> mServiceCollection;
        WindowBuilder                      mWindowBuilder;
        RendererBuilder                    mRendererBuilder;
    };
} // namespace FREYA_NAMESPACE
