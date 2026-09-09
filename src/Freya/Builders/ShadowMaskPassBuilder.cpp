#include "ShadowMaskPassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    ShadowMaskPassBuilder::ShadowMaskPassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<ShadowMaskPass> ShadowMaskPassBuilder::Build(
        const skr::Arc<SwapChain>&, vk::Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
            extent = mSurface->QueryExtent();

        const auto scaled =
            ScaledExtent({ extent.width, extent.height },
                         mFreyaOptions->shadowMaskResolutionDivisor);
        const vk::Extent2D maskExtent { scaled.width, scaled.height };

        const auto& root = mFreyaOptions->shaderRoot;
        auto        shader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/DeferredCompressed/shadow_mask.comp.spv")
                .Build();

        auto maskImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Ssao)
                .SetWidth(maskExtent.width)
                .SetHeight(maskExtent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        const auto cis = [](std::uint32_t b) {
            return vk::DescriptorSetLayoutBinding()
                .setBinding(b)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute);
        };
        const auto ubo = [](std::uint32_t b) {
            return vk::DescriptorSetLayoutBinding()
                .setBinding(b)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute);
        };
        const auto storage = [](std::uint32_t b) {
            return vk::DescriptorSetLayoutBinding()
                .setBinding(b)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute);
        };

        const auto bindings = std::array {
            cis(0), cis(1), ubo(2), ubo(3), cis(4), cis(5), cis(6), storage(7),
        };

        const auto setLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));

        const auto push =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setOffset(0)
                .setSize(sizeof(glm::mat4) + sizeof(std::uint32_t) * 4);

        const auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayout)
                .setPushConstantRanges(push));

        const auto stage =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eCompute)
                .setModule(shader->Get())
                .setPName("main");
        const auto pipeline =
            mDevice->Get()
                .createComputePipeline(nullptr,
                                       vk::ComputePipelineCreateInfo()
                                           .setStage(stage)
                                           .setLayout(pipelineLayout))
                .value;

        const auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(8),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(2),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1),
        };
        const auto pool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(poolSizes)
                .setMaxSets(1));

        auto sets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(pool)
                .setSetLayouts(setLayout));

        const auto nearest = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));
        const auto linear = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        return skr::MakeArc<ShadowMaskPass>(
            mDevice, mFreyaOptions, pipelineLayout, pipeline, setLayout, pool,
            sets[0], nearest, linear, maskImage, extent, maskExtent);
    }
} // namespace FREYA_NAMESPACE
