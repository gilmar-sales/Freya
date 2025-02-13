#pragma once

#include "Freya/Events/Event.hpp"
#include "Freya/Events/KeyCode.hpp"

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