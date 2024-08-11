#pragma once

#include "Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    enum class MouseButton
    {
        Left = 1,
        Middle,
        Right,
        Button4,
        Button5
    };

    struct MouseMoveEvent : Event
    {
        float x;
        float y;
        float deltaX;
        float deltaY;
    };

    struct MouseButtonPressedEvent : Event
    {
        MouseButton button;
    };

    struct MouseButtonReleasedEvent : Event
    {
        MouseButton button;
    };
} // namespace FREYA_NAMESPACE