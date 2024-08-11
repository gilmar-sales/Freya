#pragma once

#include "Events/Event.hpp"
#include "KeyCode.hpp"

namespace FREYA_NAMESPACE
{

    struct KeyboardEvent : Event
    {
        KeyCode key;
    };

    struct KeyPressedEvent : KeyboardEvent
    {
    };

    struct KeyReleasedEvent : KeyboardEvent
    {
    };
} // namespace FREYA_NAMESPACE