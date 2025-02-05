#pragma once

#include "Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    enum class GamepadAxis
    {
        GamepadAxisInvalid = -1,
        GamepadAxisLeftX,
        GamepadAxisLeftY,
        GamepadAxisRightX,
        GamepadAxisRightY,
        GamepadAxisLeftTrigger,
        GamepadAxisRightTrigger,
        GamepadAxisCount
    };

    enum class GamepadButton
    {
        GamepadButtonInvalid = -1,
        GamepadButtonSouth,
        GamepadButtonEast,
        GamepadButtonWest,
        GamepadButtonNorth,
        GamepadButtonBack,
        GamepadButtonGuide,
        GamepadButtonStart,
        GamepadButtonLeftStick,
        GamepadButtonRightStick,
        GamepadButtonLeftShoulder,
        GamepadButtonRightShoulder,
        GamepadButtonDpadUp,
        GamepadButtonDpadDown,
        GamepadButtonDpadLeft,
        GamepadButtonDpadRight,
        GamepadButtonMisc1,
        GamepadButtonRightPaddle1,
        GamepadButtonLeftPaddle1,
        GamepadButtonRightPaddle2,
        GamepadButtonLeftPaddle2,
        GamepadButtonTouchpad,
        GamepadButtonMisc2,
        GamepadButtonMisc3,
        GamepadButtonMisc4,
        GamepadButtonMisc5,
        GamepadButtonMisc6,
        GamepadButtonCount
    };

    struct GamepadButtonPressedEvent : Event
    {
        GamepadButton button;
    };

    struct GamepadButtonReleasedEvent : Event
    {
        GamepadButton button;
    };
    struct GamepadAxisMotionEvent : Event
    {
        GamepadAxis axis;
        double value;
    };
} // namespace FREYA_NAMESPACE