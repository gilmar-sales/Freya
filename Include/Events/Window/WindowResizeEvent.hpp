#pragma once

#include "WindowEvent.hpp"

namespace FREYA_NAMESPACE
{
    struct WindowCloseEvent : WindowEvent
    {
        unsigned width;
        unsigned height;
    };
}; // namespace FREYA_NAMESPACE