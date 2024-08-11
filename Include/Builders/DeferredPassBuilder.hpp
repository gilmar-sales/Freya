#pragma once

#include "Core/DeferredPass.hpp"
#include "Core/Device.hpp"
#include "Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

    class DeferredPassBuilder
    {
      public:
        DeferredPassBuilder& SetDevice(Ref<Device> device)
        {
            mDevice = device;
            return *this;
        }

        DeferredPassBuilder& SetSurface(Ref<Surface> surface)
        {
            mSurface = surface;
            return *this;
        }

        DeferredPassBuilder& SetSamples(vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        DeferredPassBuilder& SetFrameCount(std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        Ref<DeferredPass> Build();

      protected:
        vk::Format getDepthFormat();

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
