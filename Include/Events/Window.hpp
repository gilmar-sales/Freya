#pragma once

#include "Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    struct WindowEvent : Event
    {
    };

    struct WindowCloseEvent : WindowEvent
    {
    };

    struct WindowResizeEvent : WindowEvent
    {
        unsigned width;
        unsigned height;
    };
}; // namespace FREYA_NAMESPACE