#include "Core/RenderPass.hpp"

#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace FREYA_NAMESPACE
{

    RenderPass::~RenderPass()
    {
        mDevice->Get().destroyDescriptorPool(mSamplerDescriptorPool);

        mDevice->Get().destroySampler(mSampler);

        mDevice->Get().destroyDescriptorPool(mDescriptorPool);

        for (auto& uniformBuffer : mUniformBuffers)
        {
            uniformBuffer.reset();
        }

        for (const auto& descriptorSetLayout : mDescriptorSetLayouts)
        {
            mDevice->Get().destroyDescriptorSetLayout(descriptorSetLayout);
        }

        mDevice->Get().destroyPipeline(mGraphicsPipeline);
        mDevice->Get().destroyPipelineLayout(mPipelineLayout);

        mDevice->Get().destroyRenderPass(mRenderPass);
    }

    void RenderPass::UpdateProjection(ProjectionUniformBuffer& buffer,
                                      const std::uint32_t      frameIndex) const
    {
        mUniformBuffers[frameIndex]->Copy(&buffer, sizeof(ProjectionUniformBuffer));

        auto bufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mUniformBuffers[frameIndex]->Get())
                .setOffset(0)
                .setRange(sizeof(ProjectionUniformBuffer));

        const auto descriptorWriter =
            vk::WriteDescriptorSet()
                .setDstSet(mDescriptorSets[frameIndex])
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(bufferInfo);

        mDevice->Get().updateDescriptorSets(1, &descriptorWriter, 0, nullptr);
    }

    void RenderPass::UpdateModel(glm::mat4 model, const std::uint32_t frameIndex) const
    {
        mUniformBuffers[frameIndex]->Copy(&model, sizeof(model));
    }

    void RenderPass::BindDescriptorSet(const Ref<CommandPool>& commandPool,
                                       const std::uint32_t     frameIndex) const
    {
        commandPool->GetCommandBuffer().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mPipelineLayout,
            0,
            1,
            &mDescriptorSets[frameIndex],
            0,
            nullptr);
    }

} // namespace FREYA_NAMESPACE
