#include "TaaPass.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    TaaPass::TaaPass(
        const skr::Arc<Device>&                              device,
        const skr::Arc<FreyaOptions>&                        freyaOptions,
        const vk::PipelineLayout                             pipelineLayout,
        const vk::Pipeline                                   pipeline,
        const vk::DescriptorSetLayout                        setLayout,
        const vk::DescriptorPool                             descriptorPool,
        const std::vector<std::array<vk::DescriptorSet, 2>>& descriptorSets,
        const std::vector<std::array<skr::Arc<Image>, 2>>&   historyImages,
        const std::vector<std::array<skr::Arc<Image>, 2>>&   depthHistoryImages,
        vk::Sampler                                          colorSampler,
        vk::Sampler                                          nearestSampler,
        vk::Extent2D                                         extent,
        std::uint32_t                                        frameCount) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mSetLayout(setLayout), mDescriptorPool(descriptorPool),
        mDescriptorSets(descriptorSets), mHistoryImages(historyImages),
        mDepthHistoryImages(depthHistoryImages), mColorSampler(colorSampler),
        mNearestSampler(nearestSampler), mExtent(extent),
        mWriteIndex(frameCount, 0), mHistoryValid(frameCount, 0),
        mBoundViews(frameCount)
    {
        // Wire history/output for both ping-pong entries of each flight slot. Scene/velocity/
        // depth are filled on first Dispatch (stable until rebuild).
        for (std::uint32_t slot = 0; slot < frameCount; ++slot)
        {
            for (std::uint32_t writeIndex = 0; writeIndex < 2; ++writeIndex)
            {
                const auto set        = mDescriptorSets[slot][writeIndex];
                const auto readIndex  = 1u - writeIndex;
                const auto imageInfos = std::array {
                    vk::DescriptorImageInfo {}
                        .setSampler(mColorSampler)
                        .setImageView(
                            mHistoryImages[slot][readIndex]->GetImageView())
                        .setImageLayout(
                            vk::ImageLayout::eShaderReadOnlyOptimal),
                    vk::DescriptorImageInfo {}
                        .setImageView(
                            mHistoryImages[slot][writeIndex]->GetImageView())
                        .setImageLayout(vk::ImageLayout::eGeneral),
                    vk::DescriptorImageInfo {}
                        .setSampler(mNearestSampler)
                        .setImageView(mDepthHistoryImages[slot][readIndex]
                                          ->GetImageView())
                        .setImageLayout(
                            vk::ImageLayout::eShaderReadOnlyOptimal),
                    vk::DescriptorImageInfo {}
                        .setImageView(mDepthHistoryImages[slot][writeIndex]
                                          ->GetImageView())
                        .setImageLayout(vk::ImageLayout::eGeneral),
                };
                const auto writes = std::array {
                    vk::WriteDescriptorSet {}
                        .setDstSet(set)
                        .setDstBinding(2)
                        .setDescriptorType(
                            vk::DescriptorType::eCombinedImageSampler)
                        .setImageInfo(imageInfos[0]),
                    vk::WriteDescriptorSet {}
                        .setDstSet(set)
                        .setDstBinding(3)
                        .setDescriptorType(vk::DescriptorType::eStorageImage)
                        .setImageInfo(imageInfos[1]),
                    vk::WriteDescriptorSet {}
                        .setDstSet(set)
                        .setDstBinding(5)
                        .setDescriptorType(
                            vk::DescriptorType::eCombinedImageSampler)
                        .setImageInfo(imageInfos[2]),
                    vk::WriteDescriptorSet {}
                        .setDstSet(set)
                        .setDstBinding(6)
                        .setDescriptorType(vk::DescriptorType::eStorageImage)
                        .setImageInfo(imageInfos[3]),
                };
                mDevice->Get().updateDescriptorSets(writes, nullptr);
            }
        }
    }

    TaaPass::~TaaPass()
    {
        mDevice->Get().waitIdle();
        auto& vkDevice = mDevice->Get();
        vkDevice.destroyPipeline(mPipeline);
        vkDevice.destroyPipelineLayout(mPipelineLayout);
        vkDevice.destroyDescriptorPool(mDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSetLayout);
        vkDevice.destroySampler(mColorSampler);
        vkDevice.destroySampler(mNearestSampler);
        for (auto& pair : mHistoryImages)
            for (auto& img : pair)
                img.reset();
        for (auto& pair : mDepthHistoryImages)
            for (auto& img : pair)
                img.reset();
    }

    void TaaPass::ensureSceneDescriptors(const skr::Arc<Image>& sceneColor,
                                         const skr::Arc<Image>& velocity,
                                         const skr::Arc<Image>& depth,
                                         std::uint32_t frameIndex) const
    {
        const auto slot         = frameIndex % FrameCount();
        const auto sceneView    = sceneColor->GetImageView();
        const auto velocityView = velocity->GetImageView();
        const auto depthView    = depth->GetImageView();
        if (mBoundViews[slot].scene == sceneView &&
            mBoundViews[slot].velocity == velocityView &&
            mBoundViews[slot].depth == depthView)
            return;

        const auto imageInfos = std::array {
            vk::DescriptorImageInfo {}
                .setSampler(mColorSampler)
                .setImageView(sceneView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::DescriptorImageInfo {}
                .setSampler(mNearestSampler)
                .setImageView(velocityView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::DescriptorImageInfo {}
                .setSampler(mNearestSampler)
                .setImageView(depthView)
                .setImageLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
        };

        for (auto set : mDescriptorSets[slot])
        {
            const auto writes = std::array {
                vk::WriteDescriptorSet {}
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setImageInfo(imageInfos[0]),
                vk::WriteDescriptorSet {}
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setImageInfo(imageInfos[1]),
                vk::WriteDescriptorSet {}
                    .setDstSet(set)
                    .setDstBinding(4)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setImageInfo(imageInfos[2]),
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        }

        mBoundViews[slot].scene    = sceneView;
        mBoundViews[slot].velocity = velocityView;
        mBoundViews[slot].depth    = depthView;
    }

    void TaaPass::ResetHistory()
    {
        std::fill(mHistoryValid.begin(), mHistoryValid.end(), 0);
    }

    void TaaPass::Dispatch(const skr::Arc<CommandPool>& commandPool,
                           const skr::Arc<Image>&       sceneColor,
                           const skr::Arc<Image>&       velocity,
                           const skr::Arc<Image>&       depth,
                           std::uint32_t                frameIndex) const
    {
        const auto slot          = frameIndex % FrameCount();
        auto       commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Taa);

        ensureSceneDescriptors(sceneColor, velocity, depth, frameIndex);

        const auto readIndex = 1u - mWriteIndex[slot];

        auto colorRange =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto barriers = std::array {
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eGeneral)
                .setSrcAccessMask({})
                .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setImage(mHistoryImages[slot][mWriteIndex[slot]]->GetImage())
                .setSubresourceRange(colorRange),
            vk::ImageMemoryBarrier()
                .setOldLayout(mHistoryValid[slot]
                                  ? vk::ImageLayout::eShaderReadOnlyOptimal
                                  : vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(mHistoryValid[slot]
                                      ? vk::AccessFlagBits::eShaderRead
                                      : vk::AccessFlags {})
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(mHistoryImages[slot][readIndex]->GetImage())
                .setSubresourceRange(colorRange),
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eGeneral)
                .setSrcAccessMask({})
                .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setImage(
                    mDepthHistoryImages[slot][mWriteIndex[slot]]->GetImage())
                .setSubresourceRange(colorRange),
            vk::ImageMemoryBarrier()
                .setOldLayout(mHistoryValid[slot]
                                  ? vk::ImageLayout::eShaderReadOnlyOptimal
                                  : vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(mHistoryValid[slot]
                                      ? vk::AccessFlagBits::eShaderRead
                                      : vk::AccessFlags {})
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(mDepthHistoryImages[slot][readIndex]->GetImage())
                .setSubresourceRange(colorRange),
        };

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eEarlyFragmentTests |
                vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::PipelineStageFlagBits::eComputeShader, {}, nullptr, nullptr,
            barriers);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mPipelineLayout, 0, 1,
            &mDescriptorSets[slot][mWriteIndex[slot]], 0, nullptr);

        struct Push
        {
            float invResX;
            float invResY;
            float currentWeight;
            float historyValid;
            float varianceGammaY;
            float varianceGammaC;
            float depthReject;
            float sharpen;
            float quality;
            float reverseZ;
            float pad0;
            float pad1;
        } push {
            1.0f / static_cast<float>(mExtent.width),
            1.0f / static_cast<float>(mExtent.height),
            mFreyaOptions->taaCurrentWeight,
            mHistoryValid[slot] ? 1.0f : 0.0f,
            mFreyaOptions->taaVarianceGammaY,
            mFreyaOptions->taaVarianceGammaC,
            mFreyaOptions->taaDepthRejectThreshold,
            mFreyaOptions->taaSharpen,
            static_cast<float>(mFreyaOptions->taaQualityLevel),
            mFreyaOptions->ReverseZ ? 1.0f : 0.0f,
            0.0f,
            0.0f,
        };

        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push),
            &push);

        const auto groupsX = (mExtent.width + 7) / 8;
        const auto groupsY = (mExtent.height + 7) / 8;
        commandBuffer.dispatch(groupsX, groupsY, 1);

        auto toSampledColor =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eGeneral)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(mHistoryImages[slot][mWriteIndex[slot]]->GetImage())
                .setSubresourceRange(colorRange);
        auto toSampledDepth =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eGeneral)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(
                    mDepthHistoryImages[slot][mWriteIndex[slot]]->GetImage())
                .setSubresourceRange(colorRange);

        auto after = std::array { toSampledColor, toSampledDepth };

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eComputeShader,
            {}, nullptr, nullptr, after);

        mHistoryValid[slot] = true;
        mWriteIndex[slot]   = 1u - mWriteIndex[slot];

        mDevice->EndDebugLabel(commandBuffer);
    }

} // namespace FREYA_NAMESPACE
