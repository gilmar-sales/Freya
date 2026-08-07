#include "SsaoPass.hpp"

namespace FREYA_NAMESPACE
{
    SsaoPass::SsaoPass(
        const skr::Arc<Device>&               device,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const vk::PipelineLayout              ssaoPipelineLayout,
        const vk::Pipeline                    ssaoPipeline,
        const vk::PipelineLayout              blurPipelineLayout,
        const vk::Pipeline                    blurPipeline,
        const vk::DescriptorSetLayout         ssaoSetLayout,
        const vk::DescriptorSetLayout         blurSetLayout,
        const vk::DescriptorPool              descriptorPool,
        const vk::DescriptorSet               ssaoSet,
        const vk::DescriptorSet               blurSetH,
        const vk::DescriptorSet               blurSetV,
        const skr::Arc<Buffer>&               cameraBuffer,
        const vk::Sampler                     nearestSampler,
        const vk::Sampler                     noiseSampler,
        const vk::Sampler                     linearSampler,
        const skr::Arc<Image>&                ssaoRawImage,
        const std::array<skr::Arc<Image>, 2>& blurImages,
        const skr::Arc<Image>&                noiseImage,
        const vk::Extent2D                    fullExtent,
        const vk::Extent2D                    ssaoExtent) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mSsaoPipelineLayout(ssaoPipelineLayout), mSsaoPipeline(ssaoPipeline),
        mBlurPipelineLayout(blurPipelineLayout), mBlurPipeline(blurPipeline),
        mSsaoSetLayout(ssaoSetLayout), mBlurSetLayout(blurSetLayout),
        mDescriptorPool(descriptorPool), mSsaoSet(ssaoSet),
        mBlurSetH(blurSetH), mBlurSetV(blurSetV), mCameraBuffer(cameraBuffer),
        mNearestSampler(nearestSampler), mNoiseSampler(noiseSampler),
        mLinearSampler(linearSampler),
        mSsaoRawImage(ssaoRawImage), mBlurImages(blurImages),
        mNoiseImage(noiseImage), mFullExtent(fullExtent),
        mSsaoExtent(ssaoExtent)
    {
    }

    SsaoPass::~SsaoPass()
    {
        auto& vkDevice = mDevice->Get();

        vkDevice.destroyPipeline(mSsaoPipeline);
        vkDevice.destroyPipeline(mBlurPipeline);
        vkDevice.destroyPipelineLayout(mSsaoPipelineLayout);
        vkDevice.destroyPipelineLayout(mBlurPipelineLayout);
        vkDevice.destroyDescriptorPool(mDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mSsaoSetLayout);
        vkDevice.destroyDescriptorSetLayout(mBlurSetLayout);
        vkDevice.destroySampler(mNearestSampler);
        vkDevice.destroySampler(mNoiseSampler);
        vkDevice.destroySampler(mLinearSampler);

        mSsaoRawImage.reset();
        mBlurImages[0].reset();
        mBlurImages[1].reset();
        mNoiseImage.reset();
        mCameraBuffer.reset();
    }

    void SsaoPass::barrierColor(
        const skr::Arc<CommandPool>& commandPool,
        const vk::Image              image,
        const vk::ImageLayout        oldLayout,
        const vk::ImageLayout        newLayout,
        const vk::AccessFlags        srcAccess,
        const vk::AccessFlags        dstAccess,
        const vk::PipelineStageFlags srcStage,
        const vk::PipelineStageFlags dstStage) const
    {
        auto range =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto barrier =
            vk::ImageMemoryBarrier()
                .setOldLayout(oldLayout)
                .setNewLayout(newLayout)
                .setSrcAccessMask(srcAccess)
                .setDstAccessMask(dstAccess)
                .setImage(image)
                .setSubresourceRange(range);

        commandPool->GetCommandBuffer().pipelineBarrier(
            srcStage, dstStage, {}, nullptr, nullptr, barrier);
    }

    void SsaoPass::Dispatch(
        const skr::Arc<CommandPool>& commandPool,
        const skr::Arc<Image>&       depthImage,
        const skr::Arc<Image>&       normalImage,
        const glm::mat4&             view,
        const glm::mat4&             unjitteredProjection,
        const bool                   reverseZ,
        const float                  radius,
        const float                  bias,
        const float                  power,
        const float                  intensity) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, "SSAO");

        SsaoCameraBuffer cam {};
        cam.invProjection = glm::inverse(unjitteredProjection);
        cam.view          = view;
        cam.projection    = unjitteredProjection;
        cam.params        = glm::vec4(radius, bias, power, intensity);
        cam.res           = glm::vec4(
            1.0f / static_cast<float>(mSsaoExtent.width),
            1.0f / static_cast<float>(mSsaoExtent.height),
            reverseZ ? 1.0f : 0.0f,
            0.0f);
        mCameraBuffer->Copy(&cam, sizeof(SsaoCameraBuffer), 0);

        // Geometry depth/normal -> compute
        {
            auto depthRange =
                vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1);
            auto depthBarrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(
                        vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                    .setNewLayout(
                        vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                    .setSrcAccessMask(
                        vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setImage(depthImage->GetImage())
                    .setSubresourceRange(depthRange);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eLateFragmentTests,
                vk::PipelineStageFlagBits::eComputeShader, {}, nullptr, nullptr,
                depthBarrier);

            barrierColor(commandPool,
                         normalImage->GetImage(),
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::AccessFlagBits::eColorAttachmentWrite,
                         vk::AccessFlagBits::eShaderRead,
                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                         vk::PipelineStageFlagBits::eComputeShader);
        }

        auto depthInfo =
            vk::DescriptorImageInfo()
                .setSampler(mNearestSampler)
                .setImageView(depthImage->GetImageView())
                .setImageLayout(
                    vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        auto normalInfo =
            vk::DescriptorImageInfo()
                .setSampler(mLinearSampler)
                .setImageView(normalImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto noiseInfo =
            vk::DescriptorImageInfo()
                .setSampler(mNoiseSampler)
                .setImageView(mNoiseImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto ssaoOutInfo =
            vk::DescriptorImageInfo()
                .setImageView(mSsaoRawImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eGeneral);
        auto cameraBufInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mCameraBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(SsaoCameraBuffer));

        auto ssaoWrites = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(mSsaoSet)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(depthInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSsaoSet)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(normalInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSsaoSet)
                .setDstBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(noiseInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSsaoSet)
                .setDstBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setImageInfo(ssaoOutInfo),
            vk::WriteDescriptorSet()
                .setDstSet(mSsaoSet)
                .setDstBinding(4)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(cameraBufInfo),
        };
        mDevice->Get().updateDescriptorSets(
            static_cast<std::uint32_t>(ssaoWrites.size()), ssaoWrites.data(), 0,
            nullptr);

        barrierColor(commandPool,
                     mSsaoRawImage->GetImage(),
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eGeneral,
                     {},
                     vk::AccessFlagBits::eShaderWrite,
                     vk::PipelineStageFlagBits::eTopOfPipe,
                     vk::PipelineStageFlagBits::eComputeShader);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                                   mSsaoPipeline);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mSsaoPipelineLayout, 0, 1,
            &mSsaoSet, 0, nullptr);

        const auto groupsX = (mSsaoExtent.width + 7) / 8;
        const auto groupsY = (mSsaoExtent.height + 7) / 8;
        commandBuffer.dispatch(groupsX, groupsY, 1);

        barrierColor(commandPool,
                     mSsaoRawImage->GetImage(),
                     vk::ImageLayout::eGeneral,
                     vk::ImageLayout::eShaderReadOnlyOptimal,
                     vk::AccessFlagBits::eShaderWrite,
                     vk::AccessFlagBits::eShaderRead,
                     vk::PipelineStageFlagBits::eComputeShader,
                     vk::PipelineStageFlagBits::eComputeShader);

        struct BlurPush
        {
            float invResolution[2];
            float direction[2];
            float depthSigma;
            float normalSigma;
            float reverseZ;
            float pad;
        };

        auto runBlur = [&](const vk::DescriptorSet set,
                           const skr::Arc<Image>&  src,
                           const skr::Arc<Image>&  dst,
                           const float             dirX,
                           const float             dirY) {
            barrierColor(commandPool,
                         dst->GetImage(),
                         vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eGeneral,
                         {},
                         vk::AccessFlagBits::eShaderWrite,
                         vk::PipelineStageFlagBits::eTopOfPipe,
                         vk::PipelineStageFlagBits::eComputeShader);

            auto srcInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mLinearSampler)
                    .setImageView(src->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            auto dstInfo =
                vk::DescriptorImageInfo()
                    .setImageView(dst->GetImageView())
                    .setImageLayout(vk::ImageLayout::eGeneral);

            auto blurWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(srcInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(3)
                    .setDescriptorType(vk::DescriptorType::eStorageImage)
                    .setDescriptorCount(1)
                    .setImageInfo(dstInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(blurWrites.size()),
                blurWrites.data(), 0, nullptr);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                                       mBlurPipeline);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute, mBlurPipelineLayout, 0, 1,
                &set, 0, nullptr);

            BlurPush push {
                { cam.res.x, cam.res.y },
                { dirX, dirY },
                100.0f,
                32.0f,
                reverseZ ? 1.0f : 0.0f,
                0.0f,
            };
            commandBuffer.pushConstants(mBlurPipelineLayout,
                                        vk::ShaderStageFlagBits::eCompute, 0,
                                        sizeof(BlurPush), &push);
            commandBuffer.dispatch(groupsX, groupsY, 1);

            barrierColor(commandPool,
                         dst->GetImage(),
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::AccessFlagBits::eShaderWrite,
                         vk::AccessFlagBits::eShaderRead,
                         vk::PipelineStageFlagBits::eComputeShader,
                         vk::PipelineStageFlagBits::eFragmentShader);
        };

        runBlur(mBlurSetH, mSsaoRawImage, mBlurImages[0], 1.0f, 0.0f);
        runBlur(mBlurSetV, mBlurImages[0], mBlurImages[1], 0.0f, 1.0f);

        mDevice->EndDebugLabel(commandBuffer);
    }

} // namespace FREYA_NAMESPACE
