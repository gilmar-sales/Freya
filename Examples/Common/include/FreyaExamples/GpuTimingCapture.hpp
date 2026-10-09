#pragma once

#include <Freya/Freya.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace FreyaExamples
{
    /**
     * @brief Headless GPU stage-timing capture for repeatable baselines.
     *
     * Disabled unless @c FREYA_BENCH_FRAMES is set. Environment:
     *  - @c FREYA_BENCH_FRAMES  measured frames (enables the capture)
     *  - @c FREYA_BENCH_WARMUP  frames discarded first (default 120)
     *  - @c FREYA_BENCH_OUT     CSV file to append rows to (optional)
     *  - @c FREYA_BENCH_LABEL   free-form tag stored in every row (optional)
     *
     * Usage: call Tick() at the start of Update(); when it returns true the
     * report has been printed and the app should close its window.
     */
    class GpuTimingCapture
    {
      public:
        GpuTimingCapture();

        [[nodiscard]] bool Active() const { return mFrames > 0; }

        /**
         * @brief Sample the renderer's latest per-stage GPU times.
         * @param extra Appended to the label (e.g. "lights=64").
         * @return true once warmup + measured frames are done and the report
         * was written.
         */
        bool Tick(fra::Renderer& renderer, const std::string& extra = {});

      private:
        struct StageSeries
        {
            std::string        name;
            std::vector<float> samples;
        };

        void Report(const std::string& extra) const;

        std::uint32_t            mFrames {};
        std::uint32_t            mWarmup {};
        std::uint32_t            mSeen {};
        std::string              mOutPath;
        std::string              mLabel;
        std::vector<StageSeries> mStages;
        std::vector<float>       mTotals;
    };
} // namespace FreyaExamples
