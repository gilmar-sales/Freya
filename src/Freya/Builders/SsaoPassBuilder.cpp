#include "SsaoPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <random>

namespace FREYA_NAMESPACE
{
    SsaoPassBuilder::SsaoPassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<SsaoPass> SsaoPassBuilder::Build(const skr::Arc<SwapChain>&,
                                              vk::Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
            extent = mSurface->QueryExtent();

        const vk::Extent2D ssaoExtent {
            std::max(1u, extent.width / 2),
            std::max(1u, extent.height / 2),
        };

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto ssaoShader = loadShader("DeferredCompressed/ssao.comp.spv");
        auto blurShader = loadShader("DeferredCompressed/ssao_blur.comp.spv");

        auto imageBuilder = mServiceProvider->GetService<ImageBuilder>();

        auto createSsaoImage = [&]() {
            return imageBuilder->SetUsage(ImageUsage::Ssao)
                .SetWidth(ssaoExtent.width)
                .SetHeight(ssaoExtent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();
        };

        auto       ssaoRawImage = createSsaoImage();
        auto       blurImage0   = createSsaoImage();
        auto       blurImage1   = createSsaoImage();
        std::array blurImages   = { blurImage0, blurImage1 };

        constexpr std::uint32_t                               kNoiseSize = 4;
        std::array<std::uint8_t, kNoiseSize * kNoiseSize * 3> noiseData {};
        std::mt19937                                          rng(42);
        std::uniform_real_distribution<float>                 dist(-1.0f, 1.0f);
        for (std::uint32_t i = 0; i < kNoiseSize * kNoiseSize; ++i)
        {
            glm::vec3 v(dist(rng), dist(rng), 0.0f);
            if (glm::length(v) < 1e-4f)
                v = glm::vec3(1.0f, 0.0f, 0.0f);
            v = glm::normalize(v);
            noiseData[i * 3 + 0] =
                static_cast<std::uint8_t>((v.x * 0.5f + 0.5f) * 255.0f);
            noiseData[i * 3 + 1] =
                static_cast<std::uint8_t>((v.y * 0.5f + 0.5f) * 255.0f);
            noiseData[i * 3 + 2] = 128;
        }

        auto noiseImage =
            imageBuilder->SetUsage(ImageUsage::Texture)
                .SetFormat(vk::Format::eR8G8B8Unorm)
                .SetWidth(kNoiseSize)
                .SetHeight(kNoiseSize)
                .SetChannels(3)
                .SetData(noiseData.data())
                .Build();

        // Depth sampling (clamp). Noise reuses nearest with REPEAT via
        // a second sampler so AO tiling does not bleed at screen edges.
        auto nearestSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        auto noiseSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat));

        auto linearSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        auto cameraBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(SsaoCameraBuffer))
                .Build();

        auto ssaoBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };
        auto ssaoSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(ssaoBindings));
        auto ssaoPipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setSetLayouts(ssaoSetLayout));
        auto ssaoStage = vk::PipelineShaderStageCreateInfo()
                             .setStage(vk::ShaderStageFlagBits::eCompute)
                             .setModule(ssaoShader->Get())
                             .setPName("main");
        auto ssaoPipeline =
            mDevice->Get()
                .createComputePipeline(nullptr,
                                       vk::ComputePipelineCreateInfo()
                                           .setStage(ssaoStage)
                                           .setLayout(ssaoPipelineLayout))
                .value;

        auto blurBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };
        auto blurSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(blurBindings));
        auto blurPushRange =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setOffset(0)
                .setSize(sizeof(float) * 8);
        auto blurPipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(blurSetLayout)
                .setPushConstantRanges(blurPushRange));
        auto blurStage = vk::PipelineShaderStageCreateInfo()
                             .setStage(vk::ShaderStageFlagBits::eCompute)
                             .setModule(blurShader->Get())
                             .setPName("main");
        auto blurPipeline =
            mDevice->Get()
                .createComputePipeline(nullptr,
                                       vk::ComputePipelineCreateInfo()
                                           .setStage(blurStage)
                                           .setLayout(blurPipelineLayout))
                .value;

        auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(16),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(4),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1),
        };
        auto descriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo().setPoolSizes(poolSizes).setMaxSets(
                3));

        auto layouts = std::vector {
            ssaoSetLayout,
            blurSetLayout,
            blurSetLayout,
        };
        auto sets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(descriptorPool)
                .setSetLayouts(layouts));
        auto ssaoSet  = sets[0];
        auto blurSetH = sets[1];
        auto blurSetV = sets[2];

        mDevice->Get().destroyShaderModule(ssaoShader->Get());
        mDevice->Get().destroyShaderModule(blurShader->Get());

        return skr::MakeArc<SsaoPass>(
            mDevice, mFreyaOptions, ssaoPipelineLayout, ssaoPipeline,
            blurPipelineLayout, blurPipeline, ssaoSetLayout, blurSetLayout,
            descriptorPool, ssaoSet, blurSetH, blurSetV, cameraBuffer,
            nearestSampler, noiseSampler, linearSampler, ssaoRawImage,
            blurImages, noiseImage, extent, ssaoExtent);
    }

} // namespace FREYA_NAMESPACE
