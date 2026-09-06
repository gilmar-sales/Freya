#pragma once

#include <Freya/Freya.hpp>

namespace FreyaExamples
{
    /**
     * @brief Cycle Low → Medium → High → Ultra → Off → Low for quality enums
     * that share that ordering (Shadow, SSAO, TAA, Bloom, Animation).
     */
    template <typename Quality>
    [[nodiscard]] constexpr Quality CycleQuality(Quality current)
    {
        switch (current)
        {
            case Quality::Low:
                return Quality::Medium;
            case Quality::Medium:
                return Quality::High;
            case Quality::High:
                return Quality::Ultra;
            case Quality::Ultra:
                return Quality::Off;
            case Quality::Off:
                return Quality::Low;
        }
        return Quality::Low;
    }

    [[nodiscard]] inline const char* QualityShortName(int index)
    {
        static constexpr const char* kNames[] = { "L", "M", "H", "U", "-" };
        return (index >= 0 && index <= 4) ? kNames[index] : "?";
    }

    template <typename Quality>
    [[nodiscard]] constexpr const char* QualityName(Quality q)
    {
        switch (q)
        {
            case Quality::Low:
                return "Low";
            case Quality::Medium:
                return "Medium";
            case Quality::High:
                return "High";
            case Quality::Ultra:
                return "Ultra";
            case Quality::Off:
                return "Off";
        }
        return "?";
    }
} // namespace FreyaExamples
