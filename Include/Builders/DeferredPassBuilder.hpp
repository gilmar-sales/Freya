#pragma once

#include "Core/DeferredPass.hpp"
#include "Core/Device.hpp"
#include "Core/Surface.hpp"


namespace FREYA_NAMESPACE
{

    class DeferredPassBuilder
    {
      public:
        DeferredPassBuilder &SetDevice(std::shared_ptr<Device> device)
        {
            mDevice = device;
            return *this;
        }

        DeferredPassBuilder &SetSurface(std::shared_ptr<Surface> surface)
        {
            mSurface = surface;
            return *this;
        }

        DeferredPassBuilder &SetSamples(vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        DeferredPassBuilder &SetFrameCount(std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        std::shared_ptr<DeferredPass> Build();

      protected:
        vk::Format getDepthFormat();

      private:
        std::shared_ptr<Device> mDevice;
        std::shared_ptr<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
