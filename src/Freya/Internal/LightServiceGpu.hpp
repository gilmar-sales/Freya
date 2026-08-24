#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/LightService.hpp"

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    struct LightService::Impl
    {
        void createDescriptorResources();

        skr::Arc<Device> mDevice;
        std::uint32_t    mFrameCount {};
        std::uint32_t    mMaxLights {};
        std::uint32_t    mLightCount {};
        float            mIblIntensity   = 0.7f;
        float            mExposure       = 0.7f;
        bool             mShadowsEnabled = true;

        std::vector<Light> mLights;
        skr::Arc<Buffer>   mBuffer;

        vk::DescriptorSetLayout        mLayout;
        vk::DescriptorPool             mPool;
        std::vector<vk::DescriptorSet> mSets;
    };

    struct LightServiceGpu
    {
        static vk::DescriptorSetLayout Layout(const LightService& lights)
        {
            return lights.mImpl->mLayout;
        }

        static vk::DescriptorSet Set(const LightService& lights,
                                     std::uint32_t       frameIndex)
        {
            return lights.mImpl->mSets[frameIndex];
        }

        static skr::Arc<fra::Buffer> Buffer(const LightService& lights)
        {
            return lights.mImpl->mBuffer;
        }
    };
} // namespace FREYA_NAMESPACE
