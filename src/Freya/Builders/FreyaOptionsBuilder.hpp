#pragma once

#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    class FreyaOptionsBuilder
    {
      public:
        FreyaOptionsBuilder() : mFreyaOptions(skr::MakeRef<FreyaOptions>()) {};
        ~FreyaOptionsBuilder() = default;

        FreyaOptionsBuilder& SetTitle(const std::string& title)
        {
            mFreyaOptions->title = title;
            return *this;
        }

        FreyaOptionsBuilder& SetWidth(std::uint32_t width)
        {
            mFreyaOptions->width = width;
            return *this;
        }

        FreyaOptionsBuilder& SetHeight(std::uint32_t height)
        {
            mFreyaOptions->height = height;
            return *this;
        }

        FreyaOptionsBuilder& SetVSync(bool vSync)
        {
            mFreyaOptions->vSync = vSync;
            return *this;
        }

        FreyaOptionsBuilder& SetFullscreen(bool fullscreen)
        {
            mFreyaOptions->fullscreen = fullscreen;
            return *this;
        }

        FreyaOptionsBuilder& SetSampleCount(std::uint32_t sampleCount)
        {
            mFreyaOptions->sampleCount = sampleCount;
            return *this;
        }

        FreyaOptionsBuilder& SetFrameCount(std::uint32_t frameCount)
        {
            mFreyaOptions->frameCount = frameCount;
            return *this;
        }

        FreyaOptionsBuilder& SetClearColor(
            const vk::ClearColorValue& clearColor)
        {
            mFreyaOptions->clearColor = clearColor;
            return *this;
        }

        FreyaOptionsBuilder& SetDrawDistance(float drawDistance)
        {
            mFreyaOptions->drawDistance = drawDistance;
            return *this;
        }

        Ref<FreyaOptions> Build() { return mFreyaOptions; }

      private:
        Ref<FreyaOptions> mFreyaOptions;
    };

} // namespace FREYA_NAMESPACE
