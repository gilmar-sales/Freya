#include "Freya/Builders/IndirectDrawSystemBuilder.hpp"

#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

#include <array>

namespace FREYA_NAMESPACE
{
    IndirectDrawSystemBuilder::IndirectDrawSystemBuilder(
        const skr::Arc<Device>&                      device,
        const skr::Arc<PhysicalDevice>&              physicalDevice,
        const skr::Arc<CommandPool>&                 commandPool,
        const skr::Arc<MeshPool>&                    meshPool,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<skr::ServiceProvider>&        serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mCommandPool(commandPool), mMeshPool(meshPool), mMaterials(materials),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<IndirectDrawSystem> IndirectDrawSystemBuilder::Build()
    {
        const auto& root = mFreyaOptions->shaderRoot;
        auto        cullShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/GpuDriven/CullFrustum.comp.spv")
                .Build();

        const auto bindings = std::array {
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
        };

        const auto setLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));

        const auto pushRange =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setOffset(0)
                .setSize(sizeof(CullPushConstants));

        const auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayout)
                .setPushConstantRanges(pushRange));

        const auto stage =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eCompute)
                .setModule(cullShader->Get())
                .setPName("main");

        const auto cullPipeline =
            mDevice->Get()
                .createComputePipeline(
                    nullptr,
                    vk::ComputePipelineCreateInfo()
                        .setStage(stage)
                        .setLayout(pipelineLayout))
                .value;

        const auto frameCount = std::max(1u, mFreyaOptions->frameCount);

        const auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(3 * frameCount),
        };
        const auto descriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(poolSizes)
                .setMaxSets(frameCount));

        std::vector<vk::DescriptorSetLayout> layouts(frameCount, setLayout);
        auto descriptorSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(descriptorPool)
                .setSetLayouts(layouts));

        return skr::MakeArc<IndirectDrawSystem>(
            mDevice, mCommandPool, mMeshPool, mMaterials, frameCount,
            cullPipeline, pipelineLayout, setLayout, descriptorPool,
            std::move(descriptorSets));
    }
} // namespace FREYA_NAMESPACE
