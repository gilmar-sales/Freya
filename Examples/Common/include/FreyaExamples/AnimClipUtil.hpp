#pragma once

#include <Freya/Freya.hpp>

#include <string_view>

namespace FreyaExamples
{
    /**
     * @brief First animation clip whose name contains @p needle, or nullptr.
     */
    [[nodiscard]] inline const fra::AnimationClip* FindClipContaining(
        const fra::SkinnedModel& model, std::string_view needle)
    {
        for (const auto& clip : model.clips)
        {
            if (clip.name.find(needle) != std::string::npos)
                return &clip;
        }
        return nullptr;
    }
} // namespace FreyaExamples
