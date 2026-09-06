#pragma once

#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Process-wide template FreyaOptions from
     * FreyaExtension::WithOptions.
     *
     * Used to seed the main window scope and to clone options for secondary
     * windows. PhysicalDevice / Surface builders may mutate the main-window
     * options Arc (sampleCount / frameCount); secondary clones inherit those
     * clamps when created after the main window.
     */
    struct FreyaOptionsTemplate
    {
        skr::Arc<FreyaOptions> options;
    };

    /**
     * @brief Per-window-scope seed for FreyaOptions.
     *
     * Skirnir has no API to inject a ready instance into a scope, so
     * AbstractApplication resolves this scoped object, assigns @ref options,
     * then resolves FreyaOptions (and everything that depends on it).
     */
    struct WindowConfigContext
    {
        skr::Arc<FreyaOptions> options;
    };

} // namespace FREYA_NAMESPACE
