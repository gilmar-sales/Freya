#pragma once

#include "Freya/Events/EventManager.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    class Window
    {
      public:
        Window(SDL_Window*              window,
               const Ref<EventManager>& eventManager,
               const Ref<FreyaOptions>& freyaOptions) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mWindow(window), mRunning(true), mDeltaTime(0)
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

        [[nodiscard]] std::uint32_t GetWidth() const
        {
            return mFreyaOptions->width;
        }
        [[nodiscard]] std::uint32_t GetHeight() const
        {
            return mFreyaOptions->height;
        }

        void Resize(const std::uint32_t width, const std::uint32_t height)
        {
            mFreyaOptions->width  = width;
            mFreyaOptions->height = height;
        }

        [[nodiscard]] bool IsRunning() const { return mRunning; }
        void               Close() { mRunning = false; }

        [[nodiscard]] float GetDeltaTime() const { return mDeltaTime; }

        void SetMouseGrab(bool grab) const;

        SDL_Window* Get() const { return mWindow; }

      private:
        friend class ApplicationBuilder;
        void pollEvents();

        Ref<EventManager> mEventManager;
        Ref<FreyaOptions> mFreyaOptions;

        std::vector<SDL_Gamepad*> mGamepads;
        SDL_Window*               mWindow;
        bool                      mRunning;
        float                     mDeltaTime;
    };

} // namespace FREYA_NAMESPACE
