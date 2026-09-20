#pragma once

#include "Freya/Asset/GpuAnimation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Toolkit-agnostic view of #GpuAnimPass resident clip cache.
     */
    struct GpuAnimDebugSnapshot
    {
        struct ClipSlot
        {
            std::uint32_t slot      = 0;
            std::uint64_t key       = 0;
            bool          resident  = false;
            bool          pinned    = false;
            std::uint64_t lastTouch = 0;
            std::uint32_t frames    = 0;
            std::uint32_t joints    = 0; ///< per-frame joint count
        };

        struct SkeletonSlot
        {
            std::uint32_t slot      = 0;
            std::uint64_t key       = 0;
            bool          resident  = false;
            bool          pinned    = false;
            std::uint64_t lastTouch = 0;
            std::uint32_t joints    = 0;
            std::uint32_t rootJoint = 0xffffffffu;
        };

        bool          enabled                 = false;
        bool          quantizedJoints         = false;
        bool          timestampQueriesEnabled = false;
        std::uint32_t instanceCount           = 0;
        std::uint32_t skeletonJoints          = 0; ///< slot 0 joint count
        std::uint32_t maxClips                = 0;
        std::uint32_t maxSkeletons            = 0;
        std::uint32_t maxBakedJoints          = 0;
        std::uint32_t jointsPerClipSlot       = 0;
        std::uint32_t residentClips           = 0;
        std::uint32_t residentSkeletons       = 0;
        std::uint32_t extractRequests         = 0;

        std::vector<ClipSlot>     slots;
        std::vector<SkeletonSlot> skeletons;
    };

} // namespace FREYA_NAMESPACE
