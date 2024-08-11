#pragma once

#include "Events/Event.hpp"
#include "KeyCode.hpp"

namespace FREYA_NAMESPACE
{
    struct KeyPressedEvent : Event
    {
        KeyCode key;
    };

    struct KeyReleasedEvent : Event
    {
        KeyCode key;
    };
} // namespace FREYA_NAMESPACE