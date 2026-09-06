#include "Freya/Core/DebugDrawPass.hpp"

#include "Freya/Core/DebugLabels.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace FREYA_NAMESPACE
{
    DebugDrawPass::DebugDrawPass(
        const skr::Arc<Device>&       device,
        const skr::Arc<FreyaOptions>& freyaOptions,
        const vk::RenderPass          swapchainRenderPass,
        const vk::RenderPass          offscreenRenderPass,
        const vk::PipelineLayout      pipelineLayout,
        const vk::Pipeline            swapchainPipeline,
        const vk::Pipeline            offscreenPipeline,
        std::vector<vk::Framebuffer>
            framebuffers,
        std::vector<skr::Arc<Buffer>>
                            vertexBuffers,
        const std::uint32_t maxVertices) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mSwapchainRenderPass(swapchainRenderPass),
        mOffscreenRenderPass(offscreenRenderPass),
        mPipelineLayout(pipelineLayout), mSwapchainPipeline(swapchainPipeline),
        mOffscreenPipeline(offscreenPipeline),
        mFramebuffers(std::move(framebuffers)),
        mVertexBuffers(std::move(vertexBuffers)), mMaxVertices(maxVertices)
    {
    }

    DebugDrawPass::~DebugDrawPass()
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
    }

    void DebugDrawPass::destroyFramebuffers()
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

    void DebugDrawPass::UpdateSwapchain(const skr::Arc<SwapChain>& swapChain)
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

    void DebugDrawPass::UpdateOffscreen(const skr::Arc<Image>& color,
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

    void DebugDrawPass::Draw(const skr::Arc<SwapChain>&   swapChain,
                             const skr::Arc<CommandPool>& commandPool,
                             const std::span<const DebugDrawVertex>
                                              verts,
                             const glm::mat4& viewProj) const
    {
        if (verts.empty() || verts.size() < 2 || !swapChain || !commandPool)
            return;

        const auto count =
            std::min(static_cast<std::uint32_t>(verts.size()), mMaxVertices) &
            ~1u;
        if (count < 2)
            return;

        const auto frameIndex = swapChain->GetCurrentFrameIndex();
        const auto imageIndex = swapChain->GetCurrentImageIndex();
        if (frameIndex >= mVertexBuffers.size() || !mVertexBuffers[frameIndex])
            return;

        const vk::RenderPass renderPass =
            mOffscreen ? mOffscreenRenderPass : mSwapchainRenderPass;
        const vk::Pipeline pipeline =
            mOffscreen ? mOffscreenPipeline : mSwapchainPipeline;
        const std::size_t fbIndex = mOffscreen ? 0u : imageIndex;
        if (!renderPass || !pipeline || fbIndex >= mFramebuffers.size() ||
            !mFramebuffers[fbIndex])
            return;

        const auto bytes =
            static_cast<std::uint64_t>(count) * sizeof(DebugDrawVertex);
        mVertexBuffers[frameIndex]->Copy(verts.data(), bytes);

        const auto cmd = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(cmd, DebugLabel::DebugDraw);

        cmd.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(renderPass)
                .setFramebuffer(mFramebuffers[fbIndex])
                .setRenderArea({ { 0, 0 }, mExtent })
                .setClearValueCount(0),
            vk::SubpassContents::eInline);

        cmd.setViewport(
            0,
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(mExtent.width))
                .setHeight(static_cast<float>(mExtent.height))
                .setMinDepth(0.f)
                .setMaxDepth(1.f));
        cmd.setScissor(0, vk::Rect2D({ 0, 0 }, mExtent));

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                          sizeof(glm::mat4), &viewProj);
        mVertexBuffers[frameIndex]->Bind(commandPool);
        cmd.draw(count, 1, 0, 0);

        cmd.endRenderPass();
        mDevice->EndDebugLabel(cmd);
    }

} // namespace FREYA_NAMESPACE
