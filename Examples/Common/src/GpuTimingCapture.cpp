#include <FreyaExamples/GpuTimingCapture.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>

namespace FreyaExamples
{
    namespace
    {
        std::uint32_t EnvUint(const char* name, const std::uint32_t fallback)
        {
            const char* value = std::getenv(name);
            if (value == nullptr || *value == '\0')
                return fallback;
            return static_cast<std::uint32_t>(
                std::strtoul(value, nullptr, 10));
        }

        std::string EnvString(const char* name)
        {
            const char* value = std::getenv(name);
            return value != nullptr ? value : "";
        }

        struct Stats
        {
            float avg = 0.f;
            float min = 0.f;
            float p50 = 0.f;
            float p95 = 0.f;
            float max = 0.f;
        };

        Stats Summarize(std::vector<float> values)
        {
            if (values.empty())
                return {};
            std::ranges::sort(values);
            const auto at = [&](const float q) {
                const auto i = static_cast<std::size_t>(
                    q * static_cast<float>(values.size() - 1) + 0.5f);
                return values[i];
            };
            Stats s;
            s.avg = std::accumulate(values.begin(), values.end(), 0.f) /
                    static_cast<float>(values.size());
            s.min = values.front();
            s.p50 = at(0.5f);
            s.p95 = at(0.95f);
            s.max = values.back();
            return s;
        }
    } // namespace

    GpuTimingCapture::GpuTimingCapture() :
        mFrames(EnvUint("FREYA_BENCH_FRAMES", 0)),
        mWarmup(EnvUint("FREYA_BENCH_WARMUP", 120)),
        mOutPath(EnvString("FREYA_BENCH_OUT")),
        mLabel(EnvString("FREYA_BENCH_LABEL"))
    {
    }

    bool GpuTimingCapture::Tick(fra::Renderer&      renderer,
                                const std::string& extra)
    {
        if (!Active())
            return false;

        fra::FrameGpuTimingSample sample {};
        if (!renderer.PollFrameGpuTiming(sample) || !sample.enabled ||
            sample.stageCount == 0)
            return false;

        if (mSeen++ < mWarmup)
            return false;

        if (mStages.empty())
        {
            mStages.resize(sample.stageCount);
            for (std::uint32_t i = 0; i < sample.stageCount; ++i)
                mStages[i].name = sample.stages[i].name;
        }

        // A stage list change mid-run (renderer rebuild) invalidates the
        // series; skip the frame rather than mixing columns.
        if (sample.stageCount != mStages.size())
            return false;

        for (std::uint32_t i = 0; i < sample.stageCount; ++i)
            mStages[i].samples.push_back(sample.stages[i].gpuMs);
        mTotals.push_back(sample.totalGpuMs);

        if (mTotals.size() < mFrames)
            return false;

        Report(extra);
        return true;
    }

    void GpuTimingCapture::Report(const std::string& extra) const
    {
        std::string label = mLabel;
        if (!extra.empty())
            label += (label.empty() ? "" : " ") + extra;

        std::printf("\n=== GPU stage timing (%zu frames, warmup %u) %s ===\n",
                    mTotals.size(), mWarmup, label.c_str());
        std::printf("%-24s %8s %8s %8s %8s %8s\n", "stage", "avg", "min",
                    "p50", "p95", "max");

        FILE* csv = nullptr;
        if (!mOutPath.empty())
        {
            bool needsHeader = true;
            if (FILE* probe = std::fopen(mOutPath.c_str(), "rb"))
            {
                needsHeader = std::fgetc(probe) == EOF;
                std::fclose(probe);
            }
            csv = std::fopen(mOutPath.c_str(), "ab");
            if (csv != nullptr && needsHeader)
                std::fprintf(csv, "label,stage,avg_ms,min_ms,p50_ms,p95_ms,"
                                  "max_ms,frames\n");
        }

        const auto emit = [&](const std::string&        name,
                              const std::vector<float>& values) {
            const auto s = Summarize(values);
            std::printf("%-24s %8.3f %8.3f %8.3f %8.3f %8.3f\n", name.c_str(),
                        s.avg, s.min, s.p50, s.p95, s.max);
            if (csv != nullptr)
                std::fprintf(csv, "\"%s\",%s,%.4f,%.4f,%.4f,%.4f,%.4f,%zu\n",
                             label.c_str(), name.c_str(), s.avg, s.min, s.p50,
                             s.p95, s.max, values.size());
        };

        for (const auto& stage : mStages)
            emit(stage.name, stage.samples);
        emit("TOTAL", mTotals);
        std::fflush(stdout);

        if (csv != nullptr)
            std::fclose(csv);
    }
} // namespace FreyaExamples
