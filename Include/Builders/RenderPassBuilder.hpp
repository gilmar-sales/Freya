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
        RenderPassBuilder &SetDevice(std::shared_ptr<Device> device)
        {
            mDevice = device;
            return *this;
        }

        RenderPassBuilder &SetPhysicalDevice(
            std::shared_ptr<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        RenderPassBuilder &SetSurface(std::shared_ptr<Surface> surface)
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

        std::shared_ptr<RenderPass> Build();

      protected:
        vk::Format getDepthFormat();

      private:
        std::shared_ptr<Device> mDevice;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;
        std::shared_ptr<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
