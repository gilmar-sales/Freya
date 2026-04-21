#pragma once

#include "Freya/Events/EventManager.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief SDL3-based window with event polling and gamepad management.
     *
     * Manages an SDL3 window, polls events, tracks gamepads, and provides
     * window state queries. Uses EventManager to dispatch input events.
     *
     * @param window       SDL_Window pointer
     * @param eventManager Event manager for dispatching input events
     * @param freyaOptions Freya options reference for dimensions
     * @param logger       Logger for window operations
     */
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

        /**
         * @brief Updates window state, polls events, and calculates delta time.
         * @note Updates FPS counter in window title every second
         */
        void Update();

        /**
         * @brief Returns window width in pixels.
         */
        [[nodiscard]] std::uint32_t GetWidth() const
        {
            return mFreyaOptions->width;
        }

        /**
         * @brief Returns window height in pixels.
         */
        [[nodiscard]] std::uint32_t GetHeight() const
        {
            return mFreyaOptions->height;
        }

        /**
         * @brief Returns the display content scale factor.
         * @return Scale factor (e.g., 1.0, 1.5, 2.0)
         */
        float GetScale() const
        {
            SDL_DisplayID display_id = SDL_GetDisplayForWindow(mWindow);
            float         scale      = SDL_GetDisplayContentScale(display_id);

            return scale;
        }

        /**
         * @brief Resizes the window dimensions in freyaOptions.
         * @param width  New width in pixels
         * @param height New height in pixels
         */
        void Resize(const std::uint32_t width, const std::uint32_t height)
        {
            mFreyaOptions->width  = width;
            mFreyaOptions->height = height;
        }

        /**
         * @brief Returns whether the window is running (not closed).
         */
        [[nodiscard]] bool IsRunning() const { return mRunning; }

        /**
         * @brief Closes the window, setting running to false.
         */
        void Close() { mRunning = false; }

        /**
         * @brief Returns time since last frame in seconds.
         */
        [[nodiscard]] float GetDeltaTime() const { return mDeltaTime; }

        /**
         * @brief Returns fullscreen state.
         */
        bool IsFullscreen() const
        {
            return SDL_GetWindowFlags(mWindow) & SDL_WINDOW_FULLSCREEN;
        }

        /**
         * @brief Sets fullscreen mode.
         * @param fullscreen true for fullscreen, false for windowed
         */
        void SetFullscreen(bool fullscreen)
        {
            SDL_SetWindowFullscreen(mWindow, fullscreen);
        }

        /**
         * @brief Returns whether mouse is grabbed.
         */
        [[nodiscard]] bool IsMouseGrab() const
        {
            return SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MOUSE_GRABBED;
        }

        /**
         * @brief Sets mouse grab state and relative mouse mode.
         * @param grab true to grab, false to release
         */
        void SetMouseGrab(bool grab) const;

        /**
         * @brief Returns the underlying SDL_Window pointer.
         */
        SDL_Window* Get() const { return mWindow; }

        /**
         * @brief Adds a callback for polling SDL events.
         * @param callback Function called for each SDL_Event
         */
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
