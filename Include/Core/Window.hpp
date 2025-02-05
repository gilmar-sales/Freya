#pragma once

#include "Core/Renderer.hpp"
#include "Events/EventManager.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace FREYA_NAMESPACE
{

    class Window
    {
      public:
        Window(SDL_Window*              window,
               std::string              title,
               const std::uint32_t      width,
               const std::uint32_t      height,
               const bool               vSync,
               const Ref<EventManager>& eventManager) :
            mEventManager(eventManager), mWindow(window),
            mTitle(std::move(title)), mWidth(width), mHeight(height),
            mVSync(vSync), mRunning(true), mDeltaTime(0)
        {
            mEventManager->Subscribe<WindowResizeEvent>(
                [this](const WindowResizeEvent event) {
                    if (!event.handled)
                    {
                        Resize(event.width, event.height);
                    }
                });

            mGamepads               = std::vector<SDL_Gamepad*>();
            auto       gamepadCount = 0;
            const auto gamepadIds   = SDL_GetGamepads(&gamepadCount);

            for (int i = 0; i < gamepadCount; ++i)
            {
                if (SDL_IsGamepad(gamepadIds[i]))
                {
                    if (auto* controller = SDL_OpenGamepad(gamepadIds[i]))
                    {
                        mGamepads.push_back(controller);
                    }
                }
            }
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

        void SetMouseGrab(bool grab) const;

      private:
        friend class ApplicationBuilder;
        void pollEvents();

        Ref<EventManager>         mEventManager;
        std::vector<SDL_Gamepad*> mGamepads;
        SDL_Window*               mWindow;
        std::string               mTitle;
        std::uint32_t             mWidth;
        std::uint32_t             mHeight;
        bool                      mVSync;
        bool                      mRunning;
        float                     mDeltaTime;
    };

} // namespace FREYA_NAMESPACE