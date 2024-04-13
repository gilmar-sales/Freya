#pragma once

#include "Core/AbstractApplication.hpp"
#include "Builders/WindowBuilder.hpp"
#include "Builders/RendererBuilder.hpp"

namespace FREYA_NAMESPACE
{
    class WindowBuilder;
    
    template<typename T>
    concept IsApplication = std::is_base_of_v<AbstractApplication, T>; 

    class ApplicationBuilder
    {
        public:

        ApplicationBuilder():
        mWindowBuilder(WindowBuilder())
        {

        }

        ApplicationBuilder& WithWindow(std::function<void(WindowBuilder&)> windowBuilderFunc);

        template<typename T>
            requires IsApplication<T>
        std::shared_ptr<T> Build()
        
    {
        auto app = std::make_shared<T>();

        auto window = mWindowBuilder.Build();

        auto renderer = RendererBuilder()
                            .WithInstance([](InstanceBuilder& instanceBuilder)
                            {
                                uint32_t extensionCount;
                                SDL_Vulkan_GetInstanceExtensions(&extensionCount, nullptr);

                                const char **extensionNames = new const char *[extensionCount];
                                SDL_Vulkan_GetInstanceExtensions(&extensionCount, extensionNames);
                                
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
                            .Build();

        ((AbstractApplication*)app.get())->mWindow = window;
        ((AbstractApplication*)app.get())->mRenderer = renderer;

        return app;
    }

        protected:
            WindowBuilder mWindowBuilder;
    };

} // namespace FREYA_NAMESPACE
