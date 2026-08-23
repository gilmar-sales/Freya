#include "Freya/Core/HiZPyramid.hpp"

#include <algorithm>
#include <cmath>

namespace FREYA_NAMESPACE
{
    HiZPyramid::HiZPyramid(
        const skr::Arc<Device>&       device,
        const vk::Pipeline            copyPipeline,
        const vk::PipelineLayout      copyLayout,
        const vk::Pipeline            reducePipeline,
        const vk::PipelineLayout      reduceLayout,
        const vk::DescriptorSetLayout copySetLayout,
        const vk::DescriptorSetLayout reduceSetLayout,
        const vk::DescriptorPool      descriptorPool,
        const vk::Sampler             depthSampler,
        const std::uint32_t           frameCount) :
        mDevice(device), mCopyPipeline(copyPipeline), mCopyLayout(copyLayout),
        mReducePipeline(reducePipeline), mReduceLayout(reduceLayout),
        mCopySetLayout(copySetLayout), mReduceSetLayout(reduceSetLayout),
        mDescriptorPool(descriptorPool), mDepthSampler(depthSampler),
        mFrameCount(std::max(1u, frameCount))
    {
        auto& vkDevice = mDevice->Get();

        std::vector<vk::DescriptorSetLayout> copyLayouts(mFrameCount,
                                                         mCopySetLayout);
        mCopySets = vkDevice.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mDescriptorPool)
                .setSetLayouts(copyLayouts));

        const auto reducePerFrame = kMaxMipLevels - 1u;
        const auto reduceTotal    = mFrameCount * reducePerFrame;
        std::vector<vk::DescriptorSetLayout> reduceLayouts(reduceTotal,
                                                           mReduceSetLayout);
        mReduceSets = vkDevice.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mDescriptorPool)
                .setSetLayouts(reduceLayouts));

        mFramePyramidGeneration.assign(mFrameCount, 0);
        mFrameBoundDepthView.assign(mFrameCount, vk::ImageView {});
    }

    HiZPyramid::~HiZPyramid()
    {
        mDevice->Get().waitIdle();
        destroyMipViews();
        auto& dev = mDevice->Get();
        if (mCopyPipeline)
            dev.destroyPipeline(mCopyPipeline);
        if (mCopyLayout)
            dev.destroyPipelineLayout(mCopyLayout);
        if (mReducePipeline)
            dev.destroyPipeline(mReducePipeline);
        if (mReduceLayout)
            dev.destroyPipelineLayout(mReduceLayout);
        if (mCopySetLayout)
            dev.destroyDescriptorSetLayout(mCopySetLayout);
        if (mReduceSetLayout)
            dev.destroyDescriptorSetLayout(mReduceSetLayout);
        if (mDescriptorPool)
            dev.destroyDescriptorPool(mDescriptorPool);
        if (mDepthSampler)
            dev.destroySampler(mDepthSampler);
    }

    void HiZPyramid::destroyMipViews()
    {
        for (auto view : mMipViews)
        {
            if (view)
                mDevice->Get().destroyImageView(view);
        }
        mMipViews.clear();
    }

    void HiZPyramid::Resize(const std::uint32_t width,
                            const std::uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        if (mImage && mWidth == width && mHeight == height)
            return;

        // Old mip views/image may be in use by an in-flight frame.
        mDevice->Get().waitIdle();
        destroyMipViews();
        mReady  = false;
        mWidth  = width;
        mHeight = height;
        mMipLevels =
            std::min(kMaxMipLevels,
                     static_cast<std::uint32_t>(
                         std::floor(std::log2(std::max(width, height)))) +
                         1u);
        ++mPyramidGeneration;
        if (mPyramidGeneration == 0)
            mPyramidGeneration = 1;
        for (auto& gen : mFramePyramidGeneration)
            gen = 0;
        for (auto& view : mFrameBoundDepthView)
            view = vk::ImageView {};

        auto& vkDevice = mDevice->Get();

        const auto imageInfo =
            vk::ImageCreateInfo()
                .setImageType(vk::ImageType::e2D)
                .setFormat(vk::Format::eR32Sfloat)
                .setExtent({ width, height, 1 })
                .setMipLevels(mMipLevels)
                .setArrayLayers(1)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setTiling(vk::ImageTiling::eOptimal)
                .setUsage(vk::ImageUsageFlagBits::eStorage |
                          vk::ImageUsageFlagBits::eSampled |
                          vk::ImageUsageFlagBits::eTransferSrc |
                          vk::ImageUsageFlagBits::eTransferDst)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setInitialLayout(vk::ImageLayout::eUndefined);

        auto       image = vkDevice.createImage(imageInfo);
        const auto reqs  = vkDevice.getImageMemoryRequirements(image);
        const auto memType =
            mDevice->GetPhysicalDevice()->QueryCompatibleMemoryType(
                reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        auto memory = vkDevice.allocateMemory(vk::MemoryAllocateInfo()
                                                  .setAllocationSize(reqs.size)
                                                  .setMemoryTypeIndex(memType));
        vkDevice.bindImageMemory(image, memory, 0);

        const auto fullViewInfo =
            vk::ImageViewCreateInfo()
                .setImage(image)
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(vk::Format::eR32Sfloat)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(mMipLevels)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));
        auto fullView = vkDevice.createImageView(fullViewInfo);

        mImage = skr::MakeArc<Image>(mDevice, image, fullView, memory,
                                     vk::Format::eR32Sfloat, mMipLevels);

        mMipViews.resize(mMipLevels);
        for (std::uint32_t mip = 0; mip < mMipLevels; ++mip)
        {
            mMipViews[mip] = vkDevice.createImageView(
                vk::ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(vk::Format::eR32Sfloat)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(mip)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1)));
        }
    }

    void HiZPyramid::writeFrameDescriptors(const std::uint32_t    frame,
                                           const skr::Arc<Image>& depthImage)
    {
        const auto copySet   = mCopySets[frame];
        const auto depthView = depthImage->GetImageView();

        const auto depthInfo =
            vk::DescriptorImageInfo()
                .setSampler(mDepthSampler)
                .setImageView(depthView)
                .setImageLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        const auto mip0Info = vk::DescriptorImageInfo()
                                  .setImageView(mMipViews[0])
                                  .setImageLayout(vk::ImageLayout::eGeneral);

        const auto copyWrites = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(copySet)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(depthInfo),
            vk::WriteDescriptorSet()
                .setDstSet(copySet)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setImageInfo(mip0Info),
        };
        mDevice->Get().updateDescriptorSets(copyWrites, nullptr);

        const auto reduceStride = kMaxMipLevels - 1u;
        for (std::uint32_t mip = 1; mip < mMipLevels; ++mip)
        {
            const auto reduceSet =
                mReduceSets[frame * reduceStride + (mip - 1u)];
            const auto srcInfo = vk::DescriptorImageInfo()
                                     .setImageView(mMipViews[mip - 1])
                                     .setImageLayout(vk::ImageLayout::eGeneral);
            const auto dstInfo = vk::DescriptorImageInfo()
                                     .setImageView(mMipViews[mip])
                                     .setImageLayout(vk::ImageLayout::eGeneral);
            const auto reduceWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(reduceSet)
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageImage)
                    .setDescriptorCount(1)
                    .setImageInfo(srcInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(reduceSet)
                    .setDstBinding(1)
                    .setDescriptorType(vk::DescriptorType::eStorageImage)
                    .setDescriptorCount(1)
                    .setImageInfo(dstInfo),
            };
            mDevice->Get().updateDescriptorSets(reduceWrites, nullptr);
        }

        mFramePyramidGeneration[frame] = mPyramidGeneration;
        mFrameBoundDepthView[frame]    = depthView;
    }

    void HiZPyramid::Build(const skr::Arc<CommandPool>& commandPool,
                           const skr::Arc<Image>&       depthImage,
                           const bool                   reverseZ,
                           const std::uint32_t          frameIndex)
    {
        if (!mImage || !depthImage || mMipLevels == 0 || mCopySets.empty())
            return;

        const auto frame = frameIndex % mFrameCount;
        auto&      cb    = commandPool->GetCommandBuffer();

        const auto depthView = depthImage->GetImageView();
        if (mFramePyramidGeneration[frame] != mPyramidGeneration ||
            mFrameBoundDepthView[frame] != depthView)
        {
            writeFrameDescriptors(frame, depthImage);
        }

        {
            const auto barrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eGeneral)
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setImage(mImage->GetImage())
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(mMipLevels)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::PipelineStageFlagBits::eComputeShader, {}, 0,
                               nullptr, 0, nullptr, 1, &barrier);
        }

        const auto copySet = mCopySets[frame];

        struct CopyPC
        {
            std::uint32_t extentX;
            std::uint32_t extentY;
            std::uint32_t reverseZ;
            std::uint32_t pad;
        } copyPc { mWidth, mHeight, reverseZ ? 1u : 0u, 0 };

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, mCopyPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mCopyLayout, 0,
                              1, &copySet, 0, nullptr);
        cb.pushConstants(mCopyLayout, vk::ShaderStageFlagBits::eCompute, 0,
                         sizeof(CopyPC), &copyPc);
        cb.dispatch((mWidth + 7u) / 8u, (mHeight + 7u) / 8u, 1);

        const auto reduceStride = kMaxMipLevels - 1u;
        for (std::uint32_t mip = 1; mip < mMipLevels; ++mip)
        {
            const auto srcBarrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eGeneral)
                    .setNewLayout(vk::ImageLayout::eGeneral)
                    .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setImage(mImage->GetImage())
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(mip - 1)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eComputeShader, {}, 0,
                               nullptr, 0, nullptr, 1, &srcBarrier);

            const auto reduceSet =
                mReduceSets[frame * reduceStride + (mip - 1u)];

            const auto dstW = std::max(1u, mWidth >> mip);
            const auto dstH = std::max(1u, mHeight >> mip);
            struct ReducePC
            {
                std::uint32_t extentX;
                std::uint32_t extentY;
                std::uint32_t reverseZ;
                std::uint32_t pad;
            } reducePc { dstW, dstH, reverseZ ? 1u : 0u, 0 };

            cb.bindPipeline(vk::PipelineBindPoint::eCompute, mReducePipeline);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                  mReduceLayout, 0, 1, &reduceSet, 0, nullptr);
            cb.pushConstants(mReduceLayout, vk::ShaderStageFlagBits::eCompute,
                             0, sizeof(ReducePC), &reducePc);
            cb.dispatch((dstW + 7u) / 8u, (dstH + 7u) / 8u, 1);
        }

        {
            const auto barrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eGeneral)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setImage(mImage->GetImage())
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(mMipLevels)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eComputeShader, {}, 0,
                               nullptr, 0, nullptr, 1, &barrier);
        }

        mReady = true;
    }

} // namespace FREYA_NAMESPACE
