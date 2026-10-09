#pragma once

#include "Freya/Config.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace FREYA_NAMESPACE
{
    // Dynamic storage offsets must be aligned and fit Vulkan's uint32_t API.
    class FrameUploadLayout
    {
      public:
        FrameUploadLayout(std::uint32_t frames, std::uint64_t bytes,
                          std::uint64_t alignment)
        {
            if (!frames || !bytes || !alignment)
                throw std::invalid_argument("Invalid frame upload layout");
            const auto max = std::numeric_limits<std::uint32_t>::max();
            if (bytes > max || alignment > max)
                throw std::overflow_error("Frame upload layout overflow");
            mStride = ((bytes + alignment - 1) / alignment) * alignment;
            if (mStride > max / frames)
                throw std::overflow_error("Frame upload offsets overflow");
            mFrames = frames;
        }

        [[nodiscard]] std::uint64_t Stride() const { return mStride; }
        [[nodiscard]] std::uint64_t Size() const { return mStride * mFrames; }
        [[nodiscard]] std::uint32_t Offset(std::uint32_t frame) const
        {
            if (frame >= mFrames)
                throw std::out_of_range("Invalid frame upload slot");
            return static_cast<std::uint32_t>(frame * mStride);
        }

      private:
        std::uint64_t mStride = 0;
        std::uint32_t mFrames = 0;
    };
} // namespace FREYA_NAMESPACE
