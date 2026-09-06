#pragma once

#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"

#include <Skirnir/Skirnir.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Internal record for one Freya window scope.
     *
     * Owns the Skirnir ServiceScope. Not part of the public API — apps only
     * see fra::Window and resolve Renderer / services via AbstractApplication.
     */
    struct WindowSlot
    {
        skr::Arc<skr::ServiceScope> scope;
        skr::Arc<Window>            window;
        skr::Arc<Renderer>          renderer;
    };

} // namespace FREYA_NAMESPACE
