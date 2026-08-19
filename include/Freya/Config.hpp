#pragma once

/**
 * @file Config.hpp
 * @brief Public compile-time identity. No Vulkan, SDL, or third-party headers.
 */

#ifndef FREYA_NAMESPACE
    #define FREYA_NAMESPACE fra
#endif

#ifndef GLM_FORCE_RADIANS
    #define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
    #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif
