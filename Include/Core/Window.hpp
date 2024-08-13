#pragma once

#include "Core/Renderer.hpp"
#include "Events/EventManager.hpp"

#include <SDL3/SDL.h>

namespace FREYA_NAMESPACE
{

    class Window
    {
      public:
        Window(SDL_Window*              window,
               const std::string&       title,
               const std::uint32_t      width,
               const std::uint32_t      height,
               const bool               vSync,
               const Ref<EventManager>& eventManager) :
            mEventManager(eventManager),
            mWindow(window), mTitle(title), mWidth(width),
            mHeight(height), mVSync(vSync), mRunning(true)
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

        [[nodiscard]] std::uint32_t GetWidth() const { return mWidth; }
        [[nodiscard]] std::uint32_t GetHeight() const { return mHeight; }

        void Resize(const std::uint32_t width, const std::uint32_t height)
        {
            mWidth  = width;
            mHeight = height;
        }

        [[nodiscard]] bool IsRunning() const { return mRunning; }
        void               Close() { mRunning = false; }

        [[nodiscard]] float GetDeltaTime() const { return mDeltaTime; }

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