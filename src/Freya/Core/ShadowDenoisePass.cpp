#include "ShadowDenoisePass.hpp"

#include "Freya/Core/UniformBuffer.hpp"

#include <vulkan/vulkan.h>

namespace
{
    void beginDebugLabel(const vk::CommandBuffer& cmd,
                         const char*              name,
                         const vk::Device&        device)
    {
        auto func = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            device.getProcAddr("vkCmdBeginDebugUtilsLabelEXT"));
        if (!func)
            return;
        VkDebugUtilsLabelEXT label {};
        label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        func(static_cast<VkCommandBuffer>(cmd), &label);
    }

    void endDebugLabel(const vk::CommandBuffer& cmd, const vk::Device& device)
    {
        auto func = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            device.getProcAddr("vkCmdEndDebugUtilsLabelEXT"));
        if (!func)
            return;
        func(static_cast<VkCommandBuffer>(cmd));
    }
} // namespace

namespace FREYA_NAMESPACE
{
    ShadowDenoisePass::ShadowDenoisePass(
        const skr::Arc<Device>&                     device,
        const skr::Arc<FreyaOptions>&               freyaOptions,
        const skr::Arc<Surface>&                    surface,
        vk::Extent2D                                fullExtent,
        vk::Extent2D                                halfExtent,
        const std::array<vk::RenderPass, 4>&        renderPasses,
        vk::PipelineLayout                          maskLayout,
        vk::PipelineLayout                          blurLayout,
        vk::PipelineLayout                          upsampleLayout,
        const std::array<vk::Pipeline, 4>&          pipelines,
        const skr::Arc<Image>&                      halfMaskImage,
        const skr::Arc<Image>&                      blurTempImage,
        const skr::Arc<Image>&                      resultImage,
        const std::array<vk::Framebuffer, 4>&       framebuffers,
        vk::DescriptorPool                          descriptorPool,
        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
        const std::array<vk::DescriptorSet, 4>&     descriptorSets,
        const skr::Arc<Buffer>&                     cameraBuffer,
        vk::Sampler                                 sampler) :
        mDevice(device), mFreyaOptions(freyaOptions), mSurface(surface),
        mFullExtent(fullExtent), mHalfExtent(halfExtent),
        mRenderPasses(renderPasses), mMaskLayout(maskLayout),
        mBlurLayout(blurLayout), mUpsampleLayout(upsampleLayout),
        mPipelines(pipelines), mHalfMaskImage(halfMaskImage),
        mBlurTempImage(blurTempImage), mResultImage(resultImage),
        mFramebuffers(framebuffers), mDescriptorPool(descriptorPool),
        mDescriptorSetLayouts(descriptorSetLayouts),
        mDescriptorSets(descriptorSets), mCameraBuffer(cameraBuffer),
        mSampler(sampler)
    {
    }

    ShadowDenoisePass::~ShadowDenoisePass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        vkDevice.destroyDescriptorPool(mDescriptorPool);

        for (auto& layout : mDescriptorSetLayouts)
            vkDevice.destroyDescriptorSetLayout(layout);

        for (auto& pipeline : mPipelines)
            vkDevice.destroyPipeline(pipeline);

        vkDevice.destroyPipelineLayout(mMaskLayout);
        vkDevice.destroyPipelineLayout(mBlurLayout);
        vkDevice.destroyPipelineLayout(mUpsampleLayout);

        for (auto& rp : mRenderPasses)
            vkDevice.destroyRenderPass(rp);

        vkDevice.destroySampler(mSampler);

        mHalfMaskImage.reset();
        mBlurTempImage.reset();
        mResultImage.reset();
        mCameraBuffer.reset();
    }

    void ShadowDenoisePass::UpdateDescriptors(
        const skr::Arc<Image>&  depthImage,
        const skr::Arc<Image>&  normalImage,
        vk::ImageView           cascadeCompareView,
        vk::ImageView           cascadeDepthView,
        vk::Sampler             compareSampler,
        vk::Sampler             depthSampler,
        const skr::Arc<Buffer>& shadowUniformBuffer)
    {
        constexpr auto shaderRead = vk::ImageLayout::eShaderReadOnlyOptimal;
        constexpr auto depthRead =
            vk::ImageLayout::eDepthStencilReadOnlyOptimal;

        auto depthInfo = vk::DescriptorImageInfo()
                             .setSampler(mSampler)
                             .setImageView(depthImage->GetImageView())
                             .setImageLayout(depthRead);

        auto normalInfo =
            vk::DescriptorImageInfo()
                .setSampler(mSampler)
                .setImageView(normalImage->GetImageView())
                .setImageLayout(shaderRead);

        auto cascadeCmpInfo =
            vk::DescriptorImageInfo()
                .setSampler(compareSampler)
                .setImageView(cascadeCompareView)
                .setImageLayout(shaderRead);

        auto cascadeDepthInfo =
            vk::DescriptorImageInfo()
                .setSampler(depthSampler)
                .setImageView(cascadeDepthView)
                .setImageLayout(shaderRead);

        auto shadowUboInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(shadowUniformBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(ShadowUniformBuffer));

        auto cameraUboInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mCameraBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(ShadowDenoiseCameraUBO));

        // Mask pass: depth, normal, cascade cmp, cascade depth, shadow UBO,
        // camera UBO
        {
            auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseMaskPass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseMaskPass])
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseMaskPass])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(cascadeCmpInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseMaskPass])
                    .setDstBinding(3)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(cascadeDepthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseMaskPass])
                    .setDstBinding(4)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(shadowUboInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseMaskPass])
                    .setDstBinding(5)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(cameraUboInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                nullptr);
        }

        auto halfMaskInfo =
            vk::DescriptorImageInfo()
                .setSampler(mSampler)
                .setImageView(mHalfMaskImage->GetImageView())
                .setImageLayout(shaderRead);

        auto blurTempInfo =
            vk::DescriptorImageInfo()
                .setSampler(mSampler)
                .setImageView(mBlurTempImage->GetImageView())
                .setImageLayout(shaderRead);

        // Blur H: halfMask -> blurTemp
        {
            auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseBlurHPass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(halfMaskInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseBlurHPass])
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseBlurHPass])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                nullptr);
        }

        // Blur V: blurTemp -> halfMask
        {
            auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseBlurVPass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(blurTempInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseBlurVPass])
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseBlurVPass])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                nullptr);
        }

        // Upsample: halfMask + depth/normal -> full result
        {
            auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseUpsamplePass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(halfMaskInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseUpsamplePass])
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseUpsamplePass])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseUpsamplePass])
                    .setDstBinding(3)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mDescriptorSets[ShadowDenoiseUpsamplePass])
                    .setDstBinding(4)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                nullptr);
        }
    }

    void ShadowDenoisePass::drawPass(
        const skr::Arc<CommandPool>&      commandPool,
        const std::uint32_t               passIndex,
        const vk::Extent2D                extent,
        const ShadowDenoisePushConstants& push) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        auto clearValue = vk::ClearValue().setColor({ 1.0f, 0.0f, 0.0f, 0.0f });

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPasses[passIndex])
                .setFramebuffer(mFramebuffers[passIndex])
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent))
                .setClearValueCount(1)
                .setPClearValues(&clearValue),
            vk::SubpassContents::eInline);

        auto viewport =
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(extent.width))
                .setHeight(static_cast<float>(extent.height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);
        auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent);
        commandBuffer.setViewport(0, 1, &viewport);
        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mPipelines[passIndex]);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            passIndex == ShadowDenoiseMaskPass       ? mMaskLayout
            : passIndex == ShadowDenoiseUpsamplePass ? mUpsampleLayout
                                                     : mBlurLayout,
            0, 1, &mDescriptorSets[passIndex], 0, nullptr);

        if (passIndex != ShadowDenoiseMaskPass)
        {
            commandBuffer.pushConstants(
                passIndex == ShadowDenoiseUpsamplePass ? mUpsampleLayout
                                                       : mBlurLayout,
                vk::ShaderStageFlagBits::eFragment, 0, sizeof(push), &push);
        }

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    void ShadowDenoisePass::Render(const skr::Arc<CommandPool>& commandPool,
                                   const glm::mat4&             invViewProj,
                                   const glm::vec3&             viewPos,
                                   const glm::vec3&             cameraForward,
                                   const glm::vec3&             lightDirection)
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        beginDebugLabel(commandBuffer, "Shadow Denoise", mDevice->Get());

        ShadowDenoiseCameraUBO camera {};
        camera.invViewProj    = invViewProj;
        camera.viewPos        = glm::vec4(viewPos, 1.0f);
        camera.cameraForward  = glm::vec4(glm::normalize(cameraForward), 0.0f);
        camera.lightDirection = glm::vec4(lightDirection, 0.0f);
        mCameraBuffer->Copy(&camera, sizeof(camera), 0);

        const float radius      = mFreyaOptions->shadowDenoiseRadius > 0.0f
                                      ? mFreyaOptions->shadowDenoiseRadius
                                      : 4.0f;
        const float depthSigma  = mFreyaOptions->shadowDenoiseDepthSigma;
        const float normalSigma = mFreyaOptions->shadowDenoiseNormalSigma;

        ShadowDenoisePushConstants blurH {};
        blurH.axisAndParams = glm::vec4(1.0f, 0.0f, radius, 0.0f);
        blurH.sigmas        = glm::vec4(depthSigma, normalSigma, 0.0f, 0.0f);

        ShadowDenoisePushConstants blurV = blurH;
        blurV.axisAndParams              = glm::vec4(0.0f, 1.0f, radius, 0.0f);

        ShadowDenoisePushConstants upsample {};
        upsample.axisAndParams = glm::vec4(0.0f);
        upsample.sigmas        = glm::vec4(depthSigma, normalSigma, 0.0f, 0.0f);

        drawPass(commandPool, ShadowDenoiseMaskPass, mHalfExtent, blurH);
        drawPass(commandPool, ShadowDenoiseBlurHPass, mHalfExtent, blurH);
        drawPass(commandPool, ShadowDenoiseBlurVPass, mHalfExtent, blurV);
        drawPass(commandPool, ShadowDenoiseUpsamplePass, mFullExtent, upsample);

        endDebugLabel(commandBuffer, mDevice->Get());
    }
} // namespace FREYA_NAMESPACE
