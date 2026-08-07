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

    void TaaPass::updateDescriptors(const skr::Arc<Image>& sceneColor,
                                    const skr::Arc<Image>& velocity) const
    {
        const auto readIndex = 1u - mWriteIndex;
        const auto set       = mDescriptorSets[mWriteIndex];

        auto currentInfo =
            vk::DescriptorImageInfo()
                .setSampler(mSampler)
                .setImageView(sceneColor->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto velocityInfo =
            vk::DescriptorImageInfo()
                .setSampler(mSampler)
                .setImageView(velocity->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto historyInfo =
            vk::DescriptorImageInfo()
                .setSampler(mSampler)
                .setImageView(mHistoryImages[readIndex]->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto outputInfo =
            vk::DescriptorImageInfo()
                .setImageView(mHistoryImages[mWriteIndex]->GetImageView())
                .setImageLayout(vk::ImageLayout::eGeneral);

        auto writes = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(set)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(currentInfo),
            vk::WriteDescriptorSet()
                .setDstSet(set)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(velocityInfo),
            vk::WriteDescriptorSet()
                .setDstSet(set)
                .setDstBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(historyInfo),
            vk::WriteDescriptorSet()
                .setDstSet(set)
                .setDstBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setImageInfo(outputInfo),
        };
        mDevice->Get().updateDescriptorSets(
            static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
            nullptr);
    }

    void TaaPass::Dispatch(const skr::Arc<CommandPool>& commandPool,
                           const skr::Arc<Image>&       sceneColor,
                           const skr::Arc<Image>&       velocity) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, "TAA Resolve");

        updateDescriptors(sceneColor, velocity);

        const auto readIndex = 1u - mWriteIndex;

        auto range =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        // History read: ShaderRead; history write: General
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
            0.1f,
            mHistoryValid ? 1.0f : 0.0f,
        };

        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push),
            &push);

        const auto groupsX = (mExtent.width + 7) / 8;
        const auto groupsY = (mExtent.height + 7) / 8;
        commandBuffer.dispatch(groupsX, groupsY, 1);

        // Output → shader read for composite
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
