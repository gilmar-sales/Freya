#include "Freya/Builders/GpuAnimPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

#include <algorithm>
#include <array>

namespace FREYA_NAMESPACE
{
    skr::Arc<GpuAnimPass> GpuAnimPassBuilder::Build()
    {
        auto shader = mServiceProvider->GetService<ShaderModuleBuilder>()
                          ->SetFilePath(mFreyaOptions->shaderRoot +
                                        "/Anim/skin_bake.comp.spv")
                          .Build();

        const auto parentsBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxJoints * sizeof(std::int32_t));
        const auto invBindBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxJoints * sizeof(glm::mat4));
        const auto clipHdrBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxClips * sizeof(GpuClipHeader));
        const auto jointsBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxBakedJoints * sizeof(GpuBakedJoint));
        const auto instBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxInstances * sizeof(GpuAnimInstance));
        const auto maskBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxMaskFloats * sizeof(float));
        const auto restBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxJoints * sizeof(GpuBakedJoint));
        const auto readbackBytes = static_cast<std::uint32_t>(
            GpuAnimPass::kMaxJoints * sizeof(glm::mat4));

        auto parentsBuf =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(parentsBytes)
                .Build();
        auto invBindBuf =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(invBindBytes)
                .Build();
        auto clipHdrBuf =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(std::max(clipHdrBytes, 256u))
                .Build();
        auto jointsBuf = BufferBuilder(mDevice)
                             .SetUsage(BufferUsage::Storage)
                             .SetSize(jointsBytes)
                             .Build();
        auto instanceBuf =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(instBytes)
                .Build();
        auto maskBuf = BufferBuilder(mDevice)
                           .SetUsage(BufferUsage::Storage)
                           .SetSize(std::max(maskBytes, 256u))
                           .Build();
        auto restBuf = BufferBuilder(mDevice)
                           .SetUsage(BufferUsage::Storage)
                           .SetSize(restBytes)
                           .Build();
        auto readbackBuf =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Readback)
                .SetSize(readbackBytes)
                .Build();

        const auto animBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(6)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };
        auto animSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(animBindings));

        const auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(7),
        };
        auto animPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo().setMaxSets(1).setPoolSizes(
                poolSizes));

        auto animSet = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(animPool)
                .setSetLayouts(animSetLayout))[0];

        auto writeSsbo = [&](const std::uint32_t     binding,
                             const skr::Arc<Buffer>& buf,
                             const vk::DeviceSize    size) {
            auto info = vk::DescriptorBufferInfo()
                            .setBuffer(buf->Get())
                            .setOffset(0)
                            .setRange(size);
            auto write =
                vk::WriteDescriptorSet()
                    .setDstSet(animSet)
                    .setDstBinding(binding)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(info);
            mDevice->Get().updateDescriptorSets(write, {});
        };
        writeSsbo(0, parentsBuf, parentsBytes);
        writeSsbo(1, invBindBuf, invBindBytes);
        writeSsbo(2, clipHdrBuf, std::max(clipHdrBytes, 256u));
        writeSsbo(3, jointsBuf, jointsBytes);
        writeSsbo(4, instanceBuf, instBytes);
        writeSsbo(5, maskBuf, std::max(maskBytes, 256u));
        writeSsbo(6, restBuf, restBytes);

        const auto pushRange =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setOffset(0)
                .setSize(sizeof(std::uint32_t) * 4u);

        const auto setLayouts =
            std::array { mBoneResources->GetLayout(), animSetLayout };
        auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayouts)
                .setPushConstantRanges(pushRange));

        auto stage = vk::PipelineShaderStageCreateInfo()
                         .setStage(vk::ShaderStageFlagBits::eCompute)
                         .setModule(shader->Get())
                         .setPName("main");

        auto pipeline =
            mDevice->Get()
                .createComputePipeline(
                    nullptr,
                    vk::ComputePipelineCreateInfo().setStage(stage).setLayout(
                        pipelineLayout))
                .value;

        mDevice->Get().destroyShaderModule(shader->Get());

        return skr::MakeArc<GpuAnimPass>(
            mDevice, mBoneResources, pipelineLayout, pipeline, animSetLayout,
            animPool, animSet, parentsBuf, invBindBuf, clipHdrBuf, jointsBuf,
            instanceBuf, maskBuf, restBuf, readbackBuf);
    }

} // namespace FREYA_NAMESPACE
