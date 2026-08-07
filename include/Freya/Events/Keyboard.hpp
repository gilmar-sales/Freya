#pragma once

#include "Freya/Events/Event.hpp"
#include "Freya/Events/KeyCode.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Event fired when a key is pressed.
     *
     * @param key Scancode of pressed key
     */
    struct KeyPressedEvent : Event
    {
        KeyCode key;
    };

    /**
     * @brief Event fired when a key is released.
     *
     * @param key Scancode of released key
     */
    struct KeyReleasedEvent : Event
    {
        KeyCode key;
    };
} // namespace FREYA_NAMESPACE