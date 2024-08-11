#pragma once

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
            mWindowBuilder(WindowBuilder())
        {
        }

        ApplicationBuilder& WithWindow(std::function<void(WindowBuilder&)> windowBuilderFunc);
        ApplicationBuilder& WithRenderer(std::function<void(RendererBuilder&)> rendererBuilderFunc);

        template <typename T>
            requires IsApplication<T>
        std::shared_ptr<T> Build()
        {
            auto app = std::make_shared<T>();

            auto eventManager = std::make_shared<EventManager>();

            auto window = mWindowBuilder.SetEventManager(eventManager).Build();

            auto renderer = mRendererBuilder
                                .WithInstance([](InstanceBuilder& instanceBuilder) {
                                    uint32_t extensionCount;
                                    auto     extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

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
                                .SetEventManager(eventManager)
                                .Build();

            ((AbstractApplication*) app.get())->mWindow       = window;
            ((AbstractApplication*) app.get())->mRenderer     = renderer;
            ((AbstractApplication*) app.get())->mEventManager = eventManager;

            return app;
        }

      protected:
        WindowBuilder   mWindowBuilder;
        RendererBuilder mRendererBuilder;
    };

} // namespace FREYA_NAMESPACE
