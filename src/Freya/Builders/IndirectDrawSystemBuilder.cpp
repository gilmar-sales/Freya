#include "Freya/Builders/IndirectDrawSystemBuilder.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/HiZPyramid.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/ShaderModule.hpp"

#include <array>

namespace FREYA_NAMESPACE
{
    namespace
    {
        vk::Pipeline createComputePipeline(const skr::Arc<Device>& device,
                                           vk::PipelineLayout      layout,
                                           vk::ShaderModule        module)
        {
            const auto stage =
                vk::PipelineShaderStageCreateInfo()
                    .setStage(vk::ShaderStageFlagBits::eCompute)
                    .setModule(module)
                    .setPName("main");
            return device->Get()
                .createComputePipeline(
                    nullptr,
                    vk::ComputePipelineCreateInfo().setStage(stage).setLayout(
                        layout))
                .value;
        }
    } // namespace

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
        auto        shaderBuilder =
            mServiceProvider->GetService<ShaderModuleBuilder>();

        auto cullShader =
            shaderBuilder->SetFilePath(root + "/GpuDriven/CullFrustum.comp.spv")
                .Build();
        auto hizCopyShader =
            shaderBuilder
                ->SetFilePath(root + "/GpuDriven/HiZCopyDepth.comp.spv")
                .Build();
        auto hizReduceShader =
            shaderBuilder->SetFilePath(root + "/GpuDriven/HiZReduce.comp.spv")
                .Build();

        const auto cullBindings = std::array {
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
            vk::DescriptorSetLayoutBinding()
                .setBinding(7)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };

        const auto cullSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(cullBindings));

        const auto cullPush =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setOffset(0)
                .setSize(sizeof(CullPushConstants));

        const auto cullPipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(cullSetLayout)
                .setPushConstantRanges(cullPush));

        const auto cullPipeline = createComputePipeline(
            mDevice, cullPipelineLayout, cullShader->Get());

        const auto frameCount = std::max(1u, mFreyaOptions->frameCount);

        const auto cullPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(7 * frameCount *
                                    (1u + kMaxMaterialTechniques)),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(frameCount *
                                    (1u + kMaxMaterialTechniques)),
        };
        const auto cullSetsPerFrame = 1u + kMaxMaterialTechniques;
        const auto cullDescriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(cullPoolSizes)
                .setMaxSets(frameCount * cullSetsPerFrame));

        std::vector<vk::DescriptorSetLayout> cullLayouts(
            frameCount * cullSetsPerFrame, cullSetLayout);
        auto cullDescriptorSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(cullDescriptorPool)
                .setSetLayouts(cullLayouts));

        const auto copyBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };
        const auto reduceBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };

        const auto copySetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(copyBindings));
        const auto reduceSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(reduceBindings));

        const auto hizPush =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setOffset(0)
                .setSize(sizeof(std::uint32_t) * 4);

        const auto copyLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(copySetLayout)
                .setPushConstantRanges(hizPush));
        const auto reduceLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(reduceSetLayout)
                .setPushConstantRanges(hizPush));

        const auto copyPipeline =
            createComputePipeline(mDevice, copyLayout, hizCopyShader->Get());
        const auto reducePipeline = createComputePipeline(
            mDevice, reduceLayout, hizReduceShader->Get());

        const auto hizPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(frameCount),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(
                    frameCount +
                    frameCount * (HiZPyramid::kMaxMipLevels - 1) * 2),
        };
        const auto hizPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(hizPoolSizes)
                .setMaxSets(
                    frameCount + frameCount * (HiZPyramid::kMaxMipLevels - 1)));

        const auto depthSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setMinLod(0.0f)
                .setMaxLod(16.0f));

        auto hiz = skr::MakeArc<HiZPyramid>(
            mDevice, copyPipeline, copyLayout, reducePipeline, reduceLayout,
            copySetLayout, reduceSetLayout, hizPool, depthSampler, frameCount);

        auto&      vkDevice = mDevice->Get();
        const auto fallbackInfo =
            vk::ImageCreateInfo()
                .setImageType(vk::ImageType::e2D)
                .setFormat(vk::Format::eR32Sfloat)
                .setExtent({ 1, 1, 1 })
                .setMipLevels(1)
                .setArrayLayers(1)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setTiling(vk::ImageTiling::eOptimal)
                .setUsage(vk::ImageUsageFlagBits::eSampled |
                          vk::ImageUsageFlagBits::eTransferDst)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setInitialLayout(vk::ImageLayout::eUndefined);
        auto       fallbackImage = vkDevice.createImage(fallbackInfo);
        const auto reqs    = vkDevice.getImageMemoryRequirements(fallbackImage);
        const auto memType = mPhysicalDevice->QueryCompatibleMemoryType(
            reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        auto fallbackMem = vkDevice.allocateMemory(
            vk::MemoryAllocateInfo()
                .setAllocationSize(reqs.size)
                .setMemoryTypeIndex(memType));
        vkDevice.bindImageMemory(fallbackImage, fallbackMem, 0);
        auto fallbackView = vkDevice.createImageView(
            vk::ImageViewCreateInfo()
                .setImage(fallbackImage)
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(vk::Format::eR32Sfloat)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1)));

        {
            constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
            const auto cmd = mCommandPool->CreateCommandBuffer();
            cmd.begin(beginInfo);
            const auto barrier =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setImage(fallbackImage)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eComputeShader, {},
                                0, nullptr, 0, nullptr, 1, &barrier);
            cmd.end();
            const auto submit =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &cmd);
            mDevice->GetGraphicsQueue().submit(submit);
            mDevice->GetGraphicsQueue().waitIdle();
            mCommandPool->FreeCommandBuffer(cmd);
        }

        auto fallbackImageArc =
            skr::MakeArc<Image>(mDevice, fallbackImage, fallbackView,
                                fallbackMem, vk::Format::eR32Sfloat, 1);

        return skr::MakeArc<IndirectDrawSystem>(
            mDevice, mCommandPool, mMeshPool, mMaterials,
            mServiceProvider->GetService<MaterialPool>(), frameCount,
            cullPipeline, cullPipelineLayout, cullSetLayout, cullDescriptorPool,
            std::move(cullDescriptorSets), std::move(hiz),
            std::move(fallbackImageArc));
    }
} // namespace FREYA_NAMESPACE
