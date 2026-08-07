#pragma once

#include "Freya/Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Mouse button enumeration.
     */
    enum class MouseButton
    {
        Left = 1, ///< Left mouse button
        Middle,   ///< Middle mouse button (scroll wheel)
        Right,    ///< Right mouse button
        Button4,  ///< Additional button 4
        Button5   ///< Additional button 5
    };

    /**
     * @brief Event fired when mouse moves.
     *
     * @param x      Absolute X position
     * @param y      Absolute Y position
     * @param deltaX Relative X movement
     * @param deltaY Relative Y movement
     */
    struct MouseMoveEvent : Event
    {
        float x;      ///< Absolute X position
        float y;      ///< Absolute Y position
        float deltaX; ///< Relative X movement
        float deltaY; ///< Relative Y movement
    };

    /**
     * @brief Event fired when mouse button is pressed.
     *
     * @param button Which button was pressed
     */
    struct MouseButtonPressedEvent : Event
    {
        MouseButton button;
    };

    /**
     * @brief Event fired when mouse button is released.
     *
     * @param button Which button was released
     */
    struct MouseButtonReleasedEvent : Event
    {
        MouseButton button;
    };
} // namespace FREYA_NAMESPACE