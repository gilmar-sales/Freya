#pragma once

#include "Core/Renderer.hpp"
#include "Events/EventManager.hpp"

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
               Ref<EventManager>
                   eventManager) :
            mWindow(window),
            mTitle(title), mWidth(width), mHeight(height),
            mVSync(vSync), mRunning(true), mEventManager(eventManager)
        {
            mEventManager->Subscribe<WindowResizeEvent>([this](WindowResizeEvent event) {
                if (!event.handled)
                {
                    Resize(event.width, event.height);
                }
            });
        }

        ~Window();

        void Update();

        std::uint32_t GetWidth() { return mWidth; }
        std::uint32_t GetHeight() { return mHeight; }

        void Resize(std::uint32_t width, std::uint32_t height)
        {
            mWidth  = width;
            mHeight = height;
        }

        bool IsRunning() { return mRunning; }
        void Close() { mRunning = false; }

        float GetDeltaTime() { return mDeltaTime; }

      private:
        friend class ApplicationBuilder;
        void pollEvents();

        Ref<EventManager> mEventManager;
        SDL_Window*       mWindow;
        std::string       mTitle;
        std::uint32_t     mWidth;
        std::uint32_t     mHeight;
        bool              mVSync;
        bool              mRunning;
        float             mDeltaTime;
    };

} // namespace FREYA_NAMESPACE