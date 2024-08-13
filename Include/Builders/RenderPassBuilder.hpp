#pragma once

#include "Core/RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Device;
    class Surface;

    class RenderPassBuilder
    {
      public:
        RenderPassBuilder &SetDevice(Ref<Device> device)
        {
            mDevice = device;
            return *this;
        }

        RenderPassBuilder &SetPhysicalDevice(
            Ref<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        RenderPassBuilder &SetSurface(Ref<Surface> surface)
        {
            mSurface = surface;
            return *this;
        }

        RenderPassBuilder &SetSamples(vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        RenderPassBuilder &SetFrameCount(std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        Ref<RenderPass> Build();

      protected:
        vk::Format getDepthFormat();

      private:
        Ref<Device> mDevice;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
