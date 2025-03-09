#pragma once

#include "Freya/Core/ForwardPass.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Device;
    class Surface;

    class ForwardPassBuilder
    {
      public:
        ForwardPassBuilder() :
            mSamples(vk::SampleCountFlagBits::e1), mFrameCount(0)
        {
        }

        ForwardPassBuilder& SetDevice(const Ref<Device>& device)
        {
            mDevice = device;
            return *this;
        }

        ForwardPassBuilder& SetPhysicalDevice(
            const Ref<PhysicalDevice>& physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        ForwardPassBuilder& SetSurface(const Ref<Surface>& surface)
        {
            mSurface = surface;
            return *this;
        }

        ForwardPassBuilder& SetSamples(const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        ForwardPassBuilder& SetFrameCount(const std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        Ref<ForwardPass> Build();

      private:
        vk::RenderPass                               createRenderPass() const;
        std::tuple<vk::PipelineLayout, vk::Pipeline> createPipeline() const;

        Ref<Device>         mDevice;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Surface>        mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
