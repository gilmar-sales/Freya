#pragma once

#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

    class DeferredCompressedPassBuilder
    {
      public:
        DeferredCompressedPassBuilder() :
            mSamples(vk::SampleCountFlagBits::e1), mFrameCount(0)
        {
        }

        DeferredCompressedPassBuilder& SetDevice(const Ref<Device>& device)
        {
            mDevice = device;
            return *this;
        }

        DeferredCompressedPassBuilder& SetSurface(const Ref<Surface>& surface)
        {
            mSurface = surface;
            return *this;
        }

        DeferredCompressedPassBuilder& SetSamples(
            const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        DeferredCompressedPassBuilder& SetFrameCount(
            const std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        [[nodiscard]] Ref<DeferredCompressedPass> Build() const;
        vk::RenderPass                            createRenderPass() const;

      private:
        Ref<Device>  mDevice;
        Ref<Surface> mSurface;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mFrameCount;
    };
} // namespace FREYA_NAMESPACE
