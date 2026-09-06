#pragma once

#include "Freya/Config.hpp"

#include <array>
#include <cstdint>

namespace FREYA_NAMESPACE
{
    inline constexpr std::uint32_t kMaxFrameGpuStages = 16;

    /**
     * @brief GPU time for one frame stage (Vulkan timestamp delta).
     */
    struct FrameGpuStageTiming
    {
        char  name[32] = {};
        float gpuMs    = 0.f;
    };

    /**
     * @brief Sample of per-stage GPU times from the previous finished frame.
     *
     * Filled by Renderer::PollFrameGpuTiming. Values are wall-clock GPU ms
     * between timestamps around each IFrameStage::Execute (not Mali HWCPipe
     * counters such as PTILES / late-ZS).
     */
    struct FrameGpuTimingSample
    {
        bool                                                enabled    = false;
        float                                               totalGpuMs = 0.f;
        std::uint32_t                                       stageCount = 0;
        std::array<FrameGpuStageTiming, kMaxFrameGpuStages> stages {};
    };
} // namespace FREYA_NAMESPACE
