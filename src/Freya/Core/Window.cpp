#include "Freya/Core/Window.hpp"

#include "Freya/Events/Events.hpp"
#include "Freya/Internal/WindowNative.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>

namespace FREYA_NAMESPACE
{
    Window::Window(std::unique_ptr<Impl> impl) : mImpl(std::move(impl))
    {
    }

    Window::~Window()
    {
        if (!mImpl)
            return;

        SDL_DestroyWindow(mImpl->window);
        SDL_Vulkan_UnloadLibrary();

        for (const auto gamepad : mImpl->gamepads)
        {
            SDL_CloseGamepad(gamepad);
        }

        SDL_Quit();
    }

    SDL_Window* WindowNative::Get(const Window& window)
    {
        return window.mImpl->window;
    }

    void Window::Update()
    {
        static unsigned frames          = 0;
        static auto     previousCounter = SDL_GetPerformanceCounter();
        static double   secondTime      = 0;

        const auto currentCount = SDL_GetPerformanceCounter();

        mImpl->deltaTime = static_cast<float>(currentCount - previousCounter) /
                           static_cast<float>(SDL_GetPerformanceFrequency());

        secondTime += mImpl->deltaTime;
        previousCounter = currentCount;

        mImpl->pollEvents();

        if (secondTime >= 1.0f)
        {
            secondTime       = 0;
            auto titleStream = std::stringstream();

            titleStream << mImpl->freyaOptions->title << " - " << frames
                        << " FPS";
            frames = 0;
            SDL_SetWindowTitle(mImpl->window, titleStream.str().c_str());
        }
        frames++;
    }

    std::uint32_t Window::GetWidth() const
    {
        return mImpl->freyaOptions->width;
    }

    std::uint32_t Window::GetHeight() const
    {
        return mImpl->freyaOptions->height;
    }

    float Window::GetScale() const
    {
        SDL_DisplayID display_id = SDL_GetDisplayForWindow(mImpl->window);
        return SDL_GetDisplayContentScale(display_id);
    }

    void Window::Resize(const std::uint32_t width, const std::uint32_t height)
    {
        mImpl->freyaOptions->width  = width;
        mImpl->freyaOptions->height = height;
    }

    bool Window::IsRunning() const
    {
        return mImpl->running;
    }

    void Window::Close()
    {
        mImpl->running = false;
    }

    float Window::GetDeltaTime() const
    {
        return mImpl->deltaTime;
    }

    void* Window::NativeWindow() const
    {
        return reinterpret_cast<void*>(mImpl->window);
    }

    bool Window::IsFullscreen() const
    {
        return SDL_GetWindowFlags(mImpl->window) & SDL_WINDOW_FULLSCREEN;
    }

    void Window::SetFullscreen(const bool fullscreen)
    {
        SDL_SetWindowFullscreen(mImpl->window, fullscreen);
    }

    bool Window::IsMouseGrab() const
    {
        return SDL_GetWindowFlags(mImpl->window) & SDL_WINDOW_MOUSE_GRABBED;
    }

    void Window::SetMouseGrab(const bool grab) const
    {
        SDL_SetWindowRelativeMouseMode(mImpl->window, grab);
        SDL_SetWindowMouseGrab(mImpl->window, grab);
    }

    void Window::Impl::pollEvents()
    {
        SDL_Event sdlEvent;

        while (SDL_PollEvent(&sdlEvent))
        {
            switch (sdlEvent.type)
            {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                    running = false;
                    break;
                }
                case SDL_EVENT_WINDOW_MINIMIZED: {
                    while (sdlEvent.type != SDL_EVENT_WINDOW_RESTORED)
                    {
                        SDL_WaitEvent(&sdlEvent);
                    }
                }
                case SDL_EVENT_WINDOW_RESIZED: {
                    int width, height;
                    SDL_GetWindowSizeInPixels(window, &width, &height);

                    logger->LogInformation(
                        "Window size: {}, {}", width, height);

                    const auto resizeEvent =
                        WindowResizeEvent { .width = width, .height = height };

                    eventManager->Send(resizeEvent);
                    break;
                }
                case SDL_EVENT_KEY_DOWN: {
                    const auto keyEvent = KeyPressedEvent {
                        .key = static_cast<KeyCode>(sdlEvent.key.scancode)
                    };
                    eventManager->Send(keyEvent);
                    break;
                }
                case SDL_EVENT_KEY_UP: {
                    const auto keyEvent = KeyReleasedEvent {
                        .key = static_cast<KeyCode>(sdlEvent.key.scancode)
                    };
                    eventManager->Send(keyEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    const auto mouseEvent =
                        MouseMoveEvent { .x      = sdlEvent.motion.x,
                                         .y      = sdlEvent.motion.y,
                                         .deltaX = sdlEvent.motion.xrel,
                                         .deltaY = sdlEvent.motion.yrel };
                    eventManager->Send(mouseEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    const auto mouseEvent = MouseButtonPressedEvent {
                        .button =
                            static_cast<MouseButton>(sdlEvent.button.button)
                    };
                    eventManager->Send(mouseEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    const auto mouseEvent = MouseButtonReleasedEvent {
                        .button =
                            static_cast<MouseButton>(sdlEvent.button.button)
                    };
                    eventManager->Send(mouseEvent);
                    break;
                }
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                    const auto gamePadEvent = GamepadButtonPressedEvent {
                        .button =
                            static_cast<GamepadButton>(sdlEvent.gbutton.button)
                    };
                    eventManager->Send(gamePadEvent);
                    break;
                }
                case SDL_EVENT_GAMEPAD_ADDED: {
                    SDL_Gamepad* gamepad =
                        SDL_OpenGamepad(sdlEvent.gdevice.which);
                    if (gamepad)
                    {
                        gamepads.push_back(gamepad);
                    }
                }
                case SDL_EVENT_GAMEPAD_REMOVED: {
                    if (auto gamepad = SDL_OpenGamepad(sdlEvent.gdevice.which))
                    {
                        SDL_CloseGamepad(gamepad);
                        const auto it = std::ranges::find(gamepads, gamepad);
                        gamepads.erase(it);
                    }
                }
                case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                    const auto gamePadEvent = GamepadButtonReleasedEvent {
                        .button =
                            static_cast<GamepadButton>(sdlEvent.gbutton.button)
                    };
                    eventManager->Send(gamePadEvent);
                    break;
                }
                case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                    const auto gamePadEvent = GamepadAxisMotionEvent {
                        .axis  = static_cast<GamepadAxis>(sdlEvent.gaxis.axis),
                        .value = static_cast<double>(sdlEvent.gaxis.value) /
                                 std::numeric_limits<std::int16_t>::max()
                    };
                    eventManager->Send(gamePadEvent);
                }
                break;
                default:
                    break;
            }
        }
    }

} // namespace FREYA_NAMESPACE
