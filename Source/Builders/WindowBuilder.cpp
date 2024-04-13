#include "Builders/WindowBuilder.hpp"

#include "Builders/InstanceBuilder.hpp"
#include "Builders/RendererBuilder.hpp"

namespace FREYA_NAMESPACE
{

    std::shared_ptr<Window> WindowBuilder::Build()
    {
        assert(SDL_Init(SDL_INIT_VIDEO) == 0 && "Failed to initialize SDL3");

        SDL_Vulkan_LoadLibrary(nullptr);

        auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        SDL_Window *window =
            SDL_CreateWindow(mTitle.c_str(), mWidth, mHeight, windowFlags);

        assert(window != nullptr && "Failed to create SDL3 Window");

        uint32_t extensionCount;
        SDL_Vulkan_GetInstanceExtensions(&extensionCount, nullptr);

        const char **extensionNames = new const char *[extensionCount];
        SDL_Vulkan_GetInstanceExtensions(&extensionCount, extensionNames);

        auto instanceBuilder = fra::InstanceBuilder().AddValidationLayers();

        for (auto i = 0; i < extensionCount; i++)
        {
            instanceBuilder.AddExtension(extensionNames[i]);
        }

        auto instance = instanceBuilder.Build();

        assert(instance && "Failed to create fra::Instance");

        auto renderer = RendererBuilder()
                            .SetInstance(instance)
                            .SetWindow(window)
                            .SetWidth(mWidth)
                            .SetHeight(mHeight)
                            .SetVSync(mVSync)
                            .Build();

        return std::make_shared<Window>(
            window, mTitle, mWidth, mHeight, mVSync, renderer);
    }

} // namespace FREYA_NAMESPACE