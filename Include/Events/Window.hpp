#pragma once

#include "Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    struct WindowCloseEvent : Event
    {
    };

    struct WindowResizeEvent : Event
    {
        std::int32_t width;
        std::int32_t height;
    };
}; // namespace FREYA_NAMESPACE