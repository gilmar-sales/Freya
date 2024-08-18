#pragma once

#include "Core/DeferredPass.hpp"
#include "Core/Device.hpp"
#include "Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

    class DeferredPassBuilder
    {
      public:
        DeferredPassBuilder() :
            mSamples(vk::SampleCountFlagBits::e1),
            mFrameCount(0) {}

        DeferredPassBuilder& SetDevice(const Ref<Device>& device)
        {
            mDevice = device;
            return *this;
        }

        DeferredPassBuilder& SetSurface(const Ref<Surface>& surface)
        {
            mSurface = surface;
            return *this;
        }

        DeferredPassBuilder& SetSamples(const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        DeferredPassBuilder& SetFrameCount(const std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        Ref<DeferredPass> Build() const;

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
