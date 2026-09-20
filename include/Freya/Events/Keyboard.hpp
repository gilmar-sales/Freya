#pragma once

#include "Freya/Events/Event.hpp"
#include "Freya/Events/KeyCode.hpp"

#include <cstdint>
#include <string>

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

    /**
     * @brief UTF-8 text committed by the OS (IME / keyboard).
     */
    struct TextInputEvent : Event
    {
        std::string text;
    };

    /**
     * @brief IME composition in progress (optional).
     */
    struct TextEditingEvent : Event
    {
        std::string  text;
        std::int32_t start  = 0;
        std::int32_t length = 0;
    };
} // namespace FREYA_NAMESPACE
