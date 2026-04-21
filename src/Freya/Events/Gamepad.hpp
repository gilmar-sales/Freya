#pragma once

#include "Freya/Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Gamepad axis enumeration (sticks and triggers).
     */
    enum class GamepadAxis
    {
        GamepadAxisInvalid = -1, ///< Invalid/unset axis
        GamepadAxisLeftX,        ///< Left stick X axis
        GamepadAxisLeftY,        ///< Left stick Y axis
        GamepadAxisRightX,       ///< Right stick X axis
        GamepadAxisRightY,       ///< Right stick Y axis
        GamepadAxisLeftTrigger,  ///< Left trigger
        GamepadAxisRightTrigger, ///< Right trigger
        GamepadAxisCount         ///< Number of defined axes
    };

    /**
     * @brief Gamepad button enumeration.
     */
    enum class GamepadButton
    {
        GamepadButtonInvalid = -1,  ///< Invalid/unset button
        GamepadButtonSouth,         ///< A/Cross button (bottom)
        GamepadButtonEast,          ///< B/Circle button (right)
        GamepadButtonWest,          ///< X/Square button (left)
        GamepadButtonNorth,         ///< Y/Triangle button (top)
        GamepadButtonBack,          ///< Back/Share button
        GamepadButtonGuide,         ///< Guide/Home button
        GamepadButtonStart,         ///< Start/Options button
        GamepadButtonLeftStick,     ///< Left stick button (press)
        GamepadButtonRightStick,    ///< Right stick button (press)
        GamepadButtonLeftShoulder,  ///< Left bumper
        GamepadButtonRightShoulder, ///< Right bumper
        GamepadButtonDpadUp,        ///< D-pad up
        GamepadButtonDpadDown,      ///< D-pad down
        GamepadButtonDpadLeft,      ///< D-pad left
        GamepadButtonDpadRight,     ///< D-pad right
        GamepadButtonMisc1,         ///< Miscellaneous button 1
        GamepadButtonRightPaddle1,  ///< Right paddle 1
        GamepadButtonLeftPaddle1,   ///< Left paddle 1
        GamepadButtonRightPaddle2,  ///< Right paddle 2
        GamepadButtonLeftPaddle2,   ///< Left paddle 2
        GamepadButtonTouchpad,      ///< Touchpad button
        GamepadButtonMisc2,         ///< Miscellaneous button 2
        GamepadButtonMisc3,         ///< Miscellaneous button 3
        GamepadButtonMisc4,         ///< Miscellaneous button 4
        GamepadButtonMisc5,         ///< Miscellaneous button 5
        GamepadButtonMisc6,         ///< Miscellaneous button 6
        GamepadButtonCount          ///< Number of defined buttons
    };

    /**
     * @brief Event fired when gamepad button is pressed.
     *
     * @param button Which button was pressed
     */
    struct GamepadButtonPressedEvent : Event
    {
        GamepadButton button; ///< Button that was pressed
    };

    /**
     * @brief Event fired when gamepad button is released.
     *
     * @param button Which button was released
     */
    struct GamepadButtonReleasedEvent : Event
    {
        GamepadButton button; ///< Button that was released
    };

    /**
     * @brief Event fired when gamepad axis moves.
     *
     * @param axis  Which axis moved
     * @param value Axis value (-1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers)
     */
    struct GamepadAxisMotionEvent : Event
    {
        GamepadAxis axis;  ///< Which axis
        double      value; ///< Axis value
    };
} // namespace FREYA_NAMESPACE