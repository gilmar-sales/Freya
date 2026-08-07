#include "TaaPass.hpp"

namespace FREYA_NAMESPACE
{
    TaaPass::TaaPass(const skr::Arc<Device>&                 device,
                     const skr::Arc<FreyaOptions>&           freyaOptions,
                     const vk::PipelineLayout                pipelineLayout,
                     const vk::Pipeline                      pipeline,
                     const vk::DescriptorSetLayout           setLayout,
                     const vk::DescriptorPool                descriptorPool,
                     const std::array<vk::DescriptorSet, 2>& descriptorSets,
                     const std::array<skr::Arc<Image>, 2>&   historyImages,
                     const vk::Sampler                       sampler,
                     const vk::Extent2D                      extent) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mSetLayout(setLayout), mDescriptorPool(descriptorPool),
        mDescriptorSets(descriptorSets), mHistoryImages(historyImages),
        mSampler(sampler), mExtent(extent)
    {
        // Wire history/output for both ping-pong slots once. Scene/velocity
        // are filled on first Dispatch (stable until rebuild).
        for (std::uint32_t writeIndex = 0; writeIndex < 2; ++writeIndex)
        {
            const auto readIndex  = 1u - writeIndex;
            const auto imageInfos = std::array {
                vk::DescriptorImageInfo {}
                    .setSampler(mSampler)
                    .setImageView(mHistoryImages[readIndex]->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
                vk::DescriptorImageInfo {}
                    .setImageView(mHistoryImages[writeIndex]->GetImageView())
                    .setImageLayout(vk::ImageLayout::eGeneral),
            };
            const auto writes = std::array {
                vk::WriteDescriptorSet {}
                    .setDstSet(mDescriptorSets[writeIndex])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setImageInfo(imageInfos[0]),
                vk::WriteDescriptorSet {}
                    .setDstSet(mDescriptorSets[writeIndex])
                    .setDstBinding(3)
                    .setDescriptorType(vk::DescriptorType::eStorageImage)
                    .setImageInfo(imageInfos[1]),
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        }
    }

    TaaPass::~TaaPass()
    {
        auto& vkDevice = mDevice->Get();
        vkDevice.destroyPipeline(mPipeline);
        vkDevice.destroyPipelineLayout(mPipelineLayout);
        vkDevice.destroyDescriptorPool(mDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSetLayout);
        vkDevice.destroySampler(mSampler);
        mHistoryImages[0].reset();
        mHistoryImages[1].reset();
    }

    void TaaPass::ensureSceneDescriptors(const skr::Arc<Image>& sceneColor,
                                         const skr::Arc<Image>& velocity) const
    {
        const auto sceneView    = sceneColor->GetImageView();
        const auto velocityView = velocity->GetImageView();
        if (mBoundSceneView == sceneView && mBoundVelocityView == velocityView)
            return;

        const auto imageInfos = std::array {
            vk::DescriptorImageInfo {}
                .setSampler(mSampler)
                .setImageView(sceneView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::DescriptorImageInfo {}
                .setSampler(mSampler)
                .setImageView(velocityView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        for (auto set : mDescriptorSets)
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
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        }

        mBoundSceneView    = sceneView;
        mBoundVelocityView = velocityView;
    }

    void TaaPass::Dispatch(const skr::Arc<CommandPool>& commandPool,
                           const skr::Arc<Image>&       sceneColor,
                           const skr::Arc<Image>&       velocity) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, "TAA Resolve");

        ensureSceneDescriptors(sceneColor, velocity);

        const auto readIndex = 1u - mWriteIndex;

        auto range =
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
                .setImage(mHistoryImages[mWriteIndex]->GetImage())
                .setSubresourceRange(range),
            vk::ImageMemoryBarrier()
                .setOldLayout(mHistoryValid
                                  ? vk::ImageLayout::eShaderReadOnlyOptimal
                                  : vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(mHistoryValid
                                      ? vk::AccessFlagBits::eShaderRead
                                      : vk::AccessFlags {})
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(mHistoryImages[readIndex]->GetImage())
                .setSubresourceRange(range),
        };

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eComputeShader, {}, nullptr, nullptr,
            barriers);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mPipelineLayout, 0, 1,
            &mDescriptorSets[mWriteIndex], 0, nullptr);

        struct Push
        {
            float invResX;
            float invResY;
            float currentWeight;
            float historyValid;
        } push {
            1.0f / static_cast<float>(mExtent.width),
            1.0f / static_cast<float>(mExtent.height),
            mFreyaOptions->taaCurrentWeight,
            mHistoryValid ? 1.0f : 0.0f,
        };

        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push),
            &push);

        const auto groupsX = (mExtent.width + 7) / 8;
        const auto groupsY = (mExtent.height + 7) / 8;
        commandBuffer.dispatch(groupsX, groupsY, 1);

        auto toSampled =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eGeneral)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(mHistoryImages[mWriteIndex]->GetImage())
                .setSubresourceRange(range);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr,
            toSampled);

        mHistoryValid = true;
        mWriteIndex   = 1u - mWriteIndex;

        mDevice->EndDebugLabel(commandBuffer);
    }

} // namespace FREYA_NAMESPACE
