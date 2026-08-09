#include "Freya/Core/DebugDrawPass.hpp"

#include "Freya/Core/DebugLabels.hpp"

#include <algorithm>
#include <cstring>

namespace FREYA_NAMESPACE
{
    DebugDrawPass::DebugDrawPass(
        const skr::Arc<Device>&       device,
        const skr::Arc<FreyaOptions>& freyaOptions, vk::RenderPass renderPass,
        vk::PipelineLayout pipelineLayout, vk::Pipeline pipeline,
        std::vector<vk::Framebuffer>  framebuffers,
        std::vector<skr::Arc<Buffer>> vertexBuffers,
        const std::uint32_t           maxVertices) :
        mDevice(device), mFreyaOptions(freyaOptions), mRenderPass(renderPass),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mFramebuffers(std::move(framebuffers)),
        mVertexBuffers(std::move(vertexBuffers)), mMaxVertices(maxVertices)
    {
    }

    DebugDrawPass::~DebugDrawPass()
    {
        if (!mDevice)
            return;
        const auto& d = mDevice->Get();
        for (auto fb : mFramebuffers)
        {
            if (fb)
                d.destroyFramebuffer(fb);
        }
        if (mPipeline)
            d.destroyPipeline(mPipeline);
        if (mPipelineLayout)
            d.destroyPipelineLayout(mPipelineLayout);
        if (mRenderPass)
            d.destroyRenderPass(mRenderPass);
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
        if (frameIndex >= mVertexBuffers.size() ||
            imageIndex >= mFramebuffers.size() || !mVertexBuffers[frameIndex])
            return;

        const auto bytes =
            static_cast<std::uint64_t>(count) * sizeof(DebugDrawVertex);
        mVertexBuffers[frameIndex]->Copy(verts.data(), bytes);

        const auto cmd    = commandPool->GetCommandBuffer();
        const auto extent = swapChain->GetExtent();

        mDevice->BeginDebugLabel(cmd, DebugLabel::DebugDraw);

        auto beginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass)
                .setFramebuffer(mFramebuffers[imageIndex])
                .setRenderArea({ { 0, 0 }, extent })
                .setClearValueCount(0);

        cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

        cmd.setViewport(
            0,
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(extent.width))
                .setHeight(static_cast<float>(extent.height))
                .setMinDepth(0.f)
                .setMaxDepth(1.f));
        cmd.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
        cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                          sizeof(glm::mat4), &viewProj);
        mVertexBuffers[frameIndex]->Bind(commandPool);
        cmd.draw(count, 1, 0, 0);

        cmd.endRenderPass();
        mDevice->EndDebugLabel(cmd);
    }

} // namespace FREYA_NAMESPACE
