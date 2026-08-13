#pragma once

#include "Freya/Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Event fired when window requests close.
     */
    struct WindowCloseEvent : Event
    {
    };

    /**
     * @brief Event fired when window is resized.
     *
     * @param width  New window width
     * @param height New window height
     */
    struct WindowResizeEvent : Event
    {
        std::int32_t width;  ///< New window width
        std::int32_t height; ///< New window height
    };
}; // namespace FREYA_NAMESPACE