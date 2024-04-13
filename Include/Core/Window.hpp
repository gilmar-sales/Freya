#pragma once

#include "Core/Renderer.hpp"

#include <SDL3/SDL.h>

namespace FREYA_NAMESPACE
{

    class Window
    {
      public:
        Window(SDL_Window*   window,
               std::string   title,
               std::uint32_t width,
               std::uint32_t height,
               bool          vSync,
               std::shared_ptr<Renderer>
                   renderer) :
            mWindow(window),
            mTitle(title), mWidth(width), mHeight(height),
            mVSync(vSync), mRenderer(renderer)
        {
        }

        ~Window();

        void Run();

      private:
        SDL_Window*               mWindow;
        std::string               mTitle;
        std::uint32_t             mWidth;
        std::uint32_t             mHeight;
        bool                      mVSync;
        std::shared_ptr<Renderer> mRenderer;
    };

} // namespace FREYA_NAMESPACE