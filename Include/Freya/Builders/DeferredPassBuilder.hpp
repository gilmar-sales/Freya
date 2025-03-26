#pragma once

#include "Freya/Core/DeferredPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

    class DeferredPassBuilder
    {
      public:
        DeferredPassBuilder() :
            mSamples(vk::SampleCountFlagBits::e1), mFrameCount(0)
        {
        }

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

        [[nodiscard]] Ref<DeferredPass> Build() const;

      private:
        vk::RenderPass                               createRenderPass() const;
        std::tuple<vk::PipelineLayout, vk::Pipeline> createPipeline() const;
        DeferredPassDescriptors                      createDescriptors() const;

        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;
        std::uint32_t           mFrameCount;
    };
} // namespace FREYA_NAMESPACE
