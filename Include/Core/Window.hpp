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
               bool          vSync) :
            mWindow(window),
            mTitle(title), mWidth(width), mHeight(height),
            mVSync(vSync), mRunning(true)
        {
        }

        ~Window();

        void Update();

        bool IsRunning() { return mRunning; }
        float GetDeltaTime() { return mDeltaTime; }

      private:
        friend class ApplicationBuilder;

        SDL_Window*               mWindow;
        std::string               mTitle;
        std::uint32_t             mWidth;
        std::uint32_t             mHeight;
        bool                      mVSync;
        bool                      mRunning;
        float                     mDeltaTime;
    };

} // namespace FREYA_NAMESPACE