#include "Freya/Core/UiPass.hpp"

#include "Freya/Core/DebugLabels.hpp"
#include "Freya/Core/UiGpu.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct UiPush
        {
            glm::vec2 framebufferSize { 1.f, 1.f };
        };
    } // namespace

    UiPass::UiPass(
        const skr::Arc<Device>&                      device,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const vk::RenderPass                         swapchainRenderPass,
        const vk::RenderPass                         offscreenRenderPass,
        const vk::PipelineLayout                     pipelineLayout,
        const vk::DescriptorSetLayout                setLayout,
        const vk::DescriptorPool                     descriptorPool,
        const std::vector<vk::DescriptorSet>&        instanceSets,
        std::vector<skr::Arc<Buffer>>
                           instanceBuffers,
        const vk::Pipeline swapchainPipeline,
        const vk::Pipeline offscreenPipeline,
        std::vector<vk::Framebuffer>
                            framebuffers,
        const std::uint32_t maxQuads) :
        mDevice(device), mFreyaOptions(freyaOptions), mMaterials(materials),
        mSwapchainRenderPass(swapchainRenderPass),
        mOffscreenRenderPass(offscreenRenderPass),
        mPipelineLayout(pipelineLayout), mSetLayout(setLayout),
        mDescriptorPool(descriptorPool), mInstanceSets(instanceSets),
        mInstanceBuffers(std::move(instanceBuffers)),
        mSwapchainPipeline(swapchainPipeline),
        mOffscreenPipeline(offscreenPipeline),
        mFramebuffers(std::move(framebuffers)), mMaxQuads(maxQuads)
    {
    }

    UiPass::~UiPass()
    {
        if (!mDevice)
            return;
        mDevice->Get().waitIdle();
        const auto& d = mDevice->Get();
        destroyFramebuffers();
        if (mSwapchainPipeline)
            d.destroyPipeline(mSwapchainPipeline);
        if (mOffscreenPipeline)
            d.destroyPipeline(mOffscreenPipeline);
        if (mPipelineLayout)
            d.destroyPipelineLayout(mPipelineLayout);
        if (mSwapchainRenderPass)
            d.destroyRenderPass(mSwapchainRenderPass);
        if (mOffscreenRenderPass)
            d.destroyRenderPass(mOffscreenRenderPass);
        if (mDescriptorPool)
            d.destroyDescriptorPool(mDescriptorPool);
        if (mSetLayout)
            d.destroyDescriptorSetLayout(mSetLayout);
    }

    void UiPass::destroyFramebuffers()
    {
        for (auto fb : mFramebuffers)
        {
            if (fb)
                mDevice->Get().destroyFramebuffer(fb);
        }
        mFramebuffers.clear();
        mOffscreen = false;
        mExtent    = vk::Extent2D {};
    }

    void UiPass::UpdateSwapchain(const skr::Arc<SwapChain>& swapChain)
    {
        mDevice->Get().waitIdle();
        destroyFramebuffers();
        if (!swapChain || !mSwapchainRenderPass)
            return;

        const auto& frames = swapChain->GetFrames();
        mExtent            = swapChain->GetExtent();
        mOffscreen         = false;
        mFramebuffers.resize(frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            auto views       = std::array { frames[i].imageView };
            mFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(mSwapchainRenderPass)
                    .setAttachments(views)
                    .setWidth(mExtent.width)
                    .setHeight(mExtent.height)
                    .setLayers(1));
        }
    }

    void UiPass::UpdateOffscreen(const skr::Arc<Image>& color,
                                 const vk::Extent2D     extent)
    {
        mDevice->Get().waitIdle();
        destroyFramebuffers();
        if (!color || !mOffscreenRenderPass || extent.width == 0 ||
            extent.height == 0)
            return;

        mExtent    = extent;
        mOffscreen = true;
        mFramebuffers.resize(1);
        auto views       = std::array { color->GetImageView() };
        mFramebuffers[0] = mDevice->Get().createFramebuffer(
            vk::FramebufferCreateInfo()
                .setRenderPass(mOffscreenRenderPass)
                .setAttachments(views)
                .setWidth(extent.width)
                .setHeight(extent.height)
                .setLayers(1));
    }

    void UiPass::Draw(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<SwapChain>& swapChain, UiDraw& source,
                      const vk::Extent2D extent, const float logicalScale) const
    {
        if (!commandPool || !swapChain)
            return;

        std::vector<UiQuad> quads;
        source.Snapshot(quads);
        if (quads.empty())
            return;

        const auto frameIndex = swapChain->GetCurrentFrameIndex();
        const auto imageIndex = swapChain->GetCurrentImageIndex();
        if (frameIndex >= mInstanceBuffers.size() ||
            frameIndex >= mInstanceSets.size() || !mInstanceBuffers[frameIndex])
            return;

        const vk::RenderPass renderPass =
            mOffscreen ? mOffscreenRenderPass : mSwapchainRenderPass;
        const vk::Pipeline pipeline =
            mOffscreen ? mOffscreenPipeline : mSwapchainPipeline;
        const std::size_t fbIndex = mOffscreen ? 0u : imageIndex;
        if (!renderPass || !pipeline || fbIndex >= mFramebuffers.size() ||
            !mFramebuffers[fbIndex])
            return;

        const auto drawExtent =
            (extent.width > 0 && extent.height > 0) ? extent : mExtent;
        if (drawExtent.width == 0 || drawExtent.height == 0)
            return;

        const auto count =
            std::min(static_cast<std::uint32_t>(quads.size()), mMaxQuads);
        if (count == 0)
            return;

        const float scale = logicalScale > 0.f ? logicalScale : 1.f;
        std::vector<UiGpuInstance> gpu;
        gpu.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto g = ToUiGpu(quads[i]);
            g.rect.x *= scale;
            g.rect.y *= scale;
            g.rect.z *= scale;
            g.rect.w *= scale;
            g.rounding *= scale;
            g.borderWidth *= scale;
            gpu.push_back(g);
        }

        const auto bytes = gpu.size() * sizeof(UiGpuInstance);
        mInstanceBuffers[frameIndex]->Copy(gpu.data(), bytes);

        const auto cmd = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(cmd, DebugLabel::UI);

        cmd.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(renderPass)
                .setFramebuffer(mFramebuffers[fbIndex])
                .setRenderArea({ { 0, 0 }, drawExtent })
                .setClearValueCount(0),
            vk::SubpassContents::eInline);

        cmd.setViewport(
            0,
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(drawExtent.width))
                .setHeight(static_cast<float>(drawExtent.height))
                .setMinDepth(0.f)
                .setMaxDepth(1.f));
        cmd.setScissor(0, vk::Rect2D({ 0, 0 }, drawExtent));

        auto bindless = mMaterials->GetBindlessSet();
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, 1,
            &mInstanceSets[frameIndex], 0, nullptr);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               mPipelineLayout, 1, 1, &bindless, 0, nullptr);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

        UiPush push {};
        push.framebufferSize = { static_cast<float>(drawExtent.width),
                                 static_cast<float>(drawExtent.height) };
        cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                          sizeof(UiPush), &push);

        cmd.draw(6, count, 0, 0);

        cmd.endRenderPass();
        mDevice->EndDebugLabel(cmd);
    }

} // namespace FREYA_NAMESPACE
