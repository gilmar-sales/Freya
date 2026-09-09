#include "Freya/Core/ShadowMaskPass.hpp"

#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/Internal/LightServiceGpu.hpp"

namespace FREYA_NAMESPACE
{
    ShadowMaskPass::ShadowMaskPass(
        const skr::Arc<Device>&       device,
        const skr::Arc<FreyaOptions>& freyaOptions,
        const vk::PipelineLayout      pipelineLayout,
        const vk::Pipeline            pipeline,
        const vk::DescriptorSetLayout setLayout,
        const vk::DescriptorPool      descriptorPool,
        const vk::DescriptorSet       set,
        const vk::Sampler             nearestSampler,
        const vk::Sampler             linearSampler,
        const skr::Arc<Image>&        maskImage,
        const vk::Extent2D            fullExtent,
        const vk::Extent2D            maskExtent) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mSetLayout(setLayout), mDescriptorPool(descriptorPool), mSet(set),
        mNearestSampler(nearestSampler), mLinearSampler(linearSampler),
        mMaskImage(maskImage), mFullExtent(fullExtent), mMaskExtent(maskExtent)
    {
    }

    ShadowMaskPass::~ShadowMaskPass()
    {
        mDevice->Get().waitIdle();
        auto& dev = mDevice->Get();
        if (mPipeline)
            dev.destroyPipeline(mPipeline);
        if (mPipelineLayout)
            dev.destroyPipelineLayout(mPipelineLayout);
        if (mDescriptorPool)
            dev.destroyDescriptorPool(mDescriptorPool);
        if (mSetLayout)
            dev.destroyDescriptorSetLayout(mSetLayout);
        if (mNearestSampler)
            dev.destroySampler(mNearestSampler);
        if (mLinearSampler)
            dev.destroySampler(mLinearSampler);
        mMaskImage.reset();
    }

    void ShadowMaskPass::ensureDescriptors(
        const skr::Arc<Image>&      depthImage,
        const skr::Arc<Image>&      normalImage,
        const skr::Arc<ShadowPass>& shadowPass,
        const LightService&         lights,
        const std::uint32_t         frameIndex) const
    {
        const auto depthView   = depthImage->GetImageView();
        const auto normalView  = normalImage->GetImageView();
        const auto cascadeView = shadowPass->GetCascadeView();

        if (mStaticBound && mBoundDepthView == depthView &&
            mBoundNormalView == normalView && mBoundCascadeView == cascadeView &&
            mBoundFrame == frameIndex)
            return;

        constexpr auto shadowMapLayout =
            vk::ImageLayout::eShaderReadOnlyOptimal;

        const auto depthInfo =
            vk::DescriptorImageInfo()
                .setSampler(mNearestSampler)
                .setImageView(depthView)
                .setImageLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        const auto normalInfo =
            vk::DescriptorImageInfo()
                .setSampler(mLinearSampler)
                .setImageView(normalView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        const auto lightBuf =
            vk::DescriptorBufferInfo()
                .setBuffer(LightServiceGpu::Buffer(lights)->Get())
                .setOffset(frameIndex * sizeof(LightUniformBuffer))
                .setRange(sizeof(LightUniformBuffer));
        const auto shadowBuf =
            vk::DescriptorBufferInfo()
                .setBuffer(shadowPass->GetUniformBuffer()->Get())
                .setOffset(shadowPass->GetUniformBufferOffset(frameIndex))
                .setRange(sizeof(ShadowUniformBuffer));
        const auto cascadeInfo =
            vk::DescriptorImageInfo()
                .setSampler(shadowPass->GetCompareSampler())
                .setImageView(cascadeView)
                .setImageLayout(shadowMapLayout);
        const auto spotInfo =
            vk::DescriptorImageInfo()
                .setSampler(shadowPass->GetCompareSampler())
                .setImageView(shadowPass->GetSpotView())
                .setImageLayout(shadowMapLayout);
        const auto pointInfo =
            vk::DescriptorImageInfo()
                .setSampler(shadowPass->GetCompareSampler())
                .setImageView(shadowPass->GetPointView())
                .setImageLayout(shadowMapLayout);
        const auto maskInfo =
            vk::DescriptorImageInfo()
                .setImageView(mMaskImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eGeneral);

        const auto writes = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(depthInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(normalInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(2)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(lightBuf),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(3)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(shadowBuf),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(4)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(cascadeInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(5)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(spotInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(6)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pointInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSet)
                .setDstBinding(7)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setImageInfo(maskInfo),
        };
        mDevice->Get().updateDescriptorSets(writes, nullptr);

        mBoundDepthView   = depthView;
        mBoundNormalView  = normalView;
        mBoundCascadeView = cascadeView;
        mBoundFrame      = frameIndex;
        mStaticBound     = true;
    }

    void ShadowMaskPass::Dispatch(
        const skr::Arc<CommandPool>& commandPool,
        const skr::Arc<Image>&       depthImage,
        const skr::Arc<Image>&       normalImage,
        const skr::Arc<ShadowPass>&  shadowPass,
        const LightService&          lights,
        const glm::mat4&             view,
        const glm::mat4&             unjitteredProjection,
        const bool                   reverseZ,
        const std::uint32_t          frameIndex) const
    {
        if (!mMaskImage || !depthImage || !normalImage || !shadowPass)
            return;

        ensureDescriptors(depthImage, normalImage, shadowPass, lights,
                          frameIndex);

        auto& cb = commandPool->GetCommandBuffer();

        {
            const auto barrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eGeneral)
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setImage(mMaskImage->GetImage())
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::PipelineStageFlagBits::eComputeShader, {}, 0,
                               nullptr, 0, nullptr, 1, &barrier);
        }

        struct PC
        {
            glm::mat4     invViewProjection;
            std::uint32_t extentX;
            std::uint32_t extentY;
            std::uint32_t reverseZFlag;
            std::uint32_t pad;
        } pc {};
        pc.invViewProjection = glm::inverse(unjitteredProjection * view);
        pc.extentX           = mMaskExtent.width;
        pc.extentY           = mMaskExtent.height;
        pc.reverseZFlag      = reverseZ ? 1u : 0u;

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mPipelineLayout,
                              0, 1, &mSet, 0, nullptr);
        cb.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                         sizeof(PC), &pc);
        cb.dispatch((mMaskExtent.width + 7u) / 8u,
                    (mMaskExtent.height + 7u) / 8u, 1);

        {
            const auto barrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eGeneral)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setImage(mMaskImage->GetImage())
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eFragmentShader, {},
                               0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

} // namespace FREYA_NAMESPACE
