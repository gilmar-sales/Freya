#pragma once

#include "Freya/Events/EventManager.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    class Window
    {
      public:
        Window(SDL_Window*                     window,
               const Ref<EventManager>&        eventManager,
               const Ref<FreyaOptions>&        freyaOptions,
               const Ref<skr::Logger<Window>>& logger) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mWindow(window), mRunning(true), mDeltaTime(0), mLogger(logger)
        {
            int width, height;
            SDL_GetWindowSizeInPixels(mWindow, &width, &height);

            freyaOptions->width  = width;
            freyaOptions->height = height;

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

        float GetScale() const
        {
            SDL_DisplayID display_id = SDL_GetDisplayForWindow(mWindow);
            float         scale      = SDL_GetDisplayContentScale(display_id);

            return scale;
        }

        void Resize(const std::uint32_t width, const std::uint32_t height)
        {
            mFreyaOptions->width  = width;
            mFreyaOptions->height = height;
        }

        [[nodiscard]] bool IsRunning() const { return mRunning; }
        void               Close() { mRunning = false; }

        [[nodiscard]] float GetDeltaTime() const { return mDeltaTime; }

        bool IsFullscreen() const
        {
            return SDL_GetWindowFlags(mWindow) & SDL_WINDOW_FULLSCREEN;
        }

        void SetFullscreen(bool fullscreen)
        {
            SDL_SetWindowFullscreen(mWindow, fullscreen);
        }

        [[nodiscard]] bool IsMouseGrab() const
        {
            return SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MOUSE_GRABBED;
        }

        void SetMouseGrab(bool grab) const;

        SDL_Window* Get() const { return mWindow; }

        void AddEventPollCallback(std::function<void(SDL_Event)> callback)
        {
            mEventPollCallbacks.push_back(callback);
        }

      private:
        friend class ApplicationBuilder;
        void pollEvents();

        Ref<EventManager>        mEventManager;
        Ref<FreyaOptions>        mFreyaOptions;
        Ref<skr::Logger<Window>> mLogger;

        std::vector<SDL_Gamepad*>                   mGamepads;
        std::vector<std::function<void(SDL_Event)>> mEventPollCallbacks;
        SDL_Window*                                 mWindow;
        bool                                        mRunning;
        float                                       mDeltaTime;
    };

} // namespace FREYA_NAMESPACE
