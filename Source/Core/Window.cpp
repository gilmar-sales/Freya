#include "Freya/Core/Window.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_vulkan.h>

#include <sstream>

namespace FREYA_NAMESPACE
{
    Window::~Window()
    {
        SDL_DestroyWindow(mWindow);
        SDL_Vulkan_UnloadLibrary();

        for (const auto gamepad : mGamepads)
        {
            SDL_CloseGamepad(gamepad);
        }

        SDL_Quit();
    }

    void Window::Update()
    {
        static unsigned frames          = 0;
        static auto     previousCounter = SDL_GetPerformanceCounter();
        static double   secondTime      = 0;

        const auto currentCount = SDL_GetPerformanceCounter();

        mDeltaTime = static_cast<float>(currentCount - previousCounter) /
                     static_cast<float>(SDL_GetPerformanceFrequency());

        secondTime += mDeltaTime;
        previousCounter = currentCount;

        pollEvents();

        if (secondTime >= 1.0f)
        {
            secondTime       = 0;
            auto titleStream = std::stringstream();

            titleStream << mTitle << " - " << frames << " FPS";
            frames = 0;
            SDL_SetWindowTitle(mWindow, titleStream.str().c_str());
        }

        frames++;
    }

    void Window::SetMouseGrab(const bool grab) const
    {
        SDL_SetWindowRelativeMouseMode(mWindow, grab);
        SDL_SetWindowMouseGrab(mWindow, grab);
    }

    void Window::pollEvents()
    {
        SDL_Event sdlEvent;

        while (SDL_PollEvent(&sdlEvent))
        {
            switch (sdlEvent.type)
            {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {

                    Close();
                    break;
                }
                case SDL_EVENT_WINDOW_MINIMIZED: {
                    while (sdlEvent.type != SDL_EVENT_WINDOW_RESTORED)
                    {
                        SDL_WaitEvent(&sdlEvent);
                    }
                }
                case SDL_EVENT_WINDOW_RESIZED: {
                    const auto scale = SDL_GetWindowDisplayScale(mWindow);

                    const auto resizeEvent = WindowResizeEvent {
                        .width =
                            sdlEvent.window.data1 * static_cast<int>(scale),
                        .height =
                            sdlEvent.window.data2 * static_cast<int>(scale)
                    };

                    mEventManager->Send(resizeEvent);
                    break;
                }
                case SDL_EVENT_KEY_DOWN: {
                    const auto keyEvent = KeyPressedEvent {
                        .key = static_cast<KeyCode>(sdlEvent.key.scancode)
                    };
                    mEventManager->Send(keyEvent);
                    break;
                }
                case SDL_EVENT_KEY_UP: {
                    const auto keyEvent = KeyReleasedEvent {
                        .key = static_cast<KeyCode>(sdlEvent.key.scancode)
                    };
                    mEventManager->Send(keyEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    const auto mouseEvent =
                        MouseMoveEvent { .x      = sdlEvent.motion.x,
                                         .y      = sdlEvent.motion.y,
                                         .deltaX = sdlEvent.motion.xrel,
                                         .deltaY = sdlEvent.motion.yrel };
                    mEventManager->Send(mouseEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    const auto mouseEvent = MouseButtonPressedEvent {
                        .button =
                            static_cast<MouseButton>(sdlEvent.button.button)
                    };
                    mEventManager->Send(mouseEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    const auto mouseEvent = MouseButtonReleasedEvent {
                        .button =
                            static_cast<MouseButton>(sdlEvent.button.button)
                    };
                    mEventManager->Send(mouseEvent);
                    break;
                }
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                    const auto gamePadEvent = GamepadButtonPressedEvent {
                        .button =
                            static_cast<GamepadButton>(sdlEvent.gbutton.button)
                    };
                    mEventManager->Send(gamePadEvent);
                    break;
                }
                case SDL_EVENT_GAMEPAD_ADDED: {
                    SDL_Gamepad* gamepad =
                        SDL_OpenGamepad(sdlEvent.gdevice.which);
                    if (gamepad)
                    {
                        mGamepads.push_back(gamepad);
                    }
                }
                case SDL_EVENT_GAMEPAD_REMOVED: {
                    if (auto gamepad = SDL_OpenGamepad(sdlEvent.gdevice.which))
                    {
                        SDL_CloseGamepad(gamepad);
                        const auto it = std::ranges::find(mGamepads, gamepad);
                        mGamepads.erase(it);
                    }
                }
                case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                    const auto gamePadEvent = GamepadButtonReleasedEvent {
                        .button =
                            static_cast<GamepadButton>(sdlEvent.gbutton.button)
                    };
                    mEventManager->Send(gamePadEvent);
                    break;
                }
                case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                    const auto gamePadEvent = GamepadAxisMotionEvent {
                        .axis  = static_cast<GamepadAxis>(sdlEvent.gaxis.axis),
                        .value = static_cast<double>(sdlEvent.gaxis.value) /
                                 std::numeric_limits<std::int16_t>::max()
                    };
                    mEventManager->Send(gamePadEvent);
                }
                break;
                default:
                    break;
            }
        };
    }

} // namespace FREYA_NAMESPACE
