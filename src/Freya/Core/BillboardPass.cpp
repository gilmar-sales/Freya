#include "Freya/Core/BillboardPass.hpp"

#include "Freya/Core/DebugLabels.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct BillboardPush
        {
            glm::mat4 view { 1.f };
            glm::mat4 proj { 1.f };
        };

        BillboardGpuInstance ToGpu(const Billboard& b)
        {
            BillboardGpuInstance g {};
            g.worldPos     = b.worldPos;
            g.clipMax      = b.clipMax;
            g.size         = b.size;
            g.textureIndex = b.textureIndex;
            g.flags        = (b.align == BillboardAlign::Cylindrical
                                  ? kBillboardFlagCylindrical
                                  : 0u) |
                             (b.sdf ? kBillboardFlagSdf : 0u);
            g.color        = b.color;
            g.uvRect       = b.uvRect;
            g.localOffset  = b.localOffset;
            return g;
        }
    } // namespace

    BillboardPass::BillboardPass(
        const skr::Arc<Device>&                      device,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const vk::RenderPass hdrRenderPass, const vk::RenderPass ldrRenderPass,
        const vk::PipelineLayout              pipelineLayout,
        const vk::DescriptorSetLayout         setLayout,
        const vk::DescriptorPool              descriptorPool,
        const std::vector<vk::DescriptorSet>& instanceSets,
        std::vector<skr::Arc<Buffer>>         instanceBuffers,
        const Pipelines hdrPipelines, const Pipelines ldrPipelines,
        std::vector<vk::Framebuffer> ldrFramebuffers, const vk::Extent2D extent,
        const std::uint32_t maxQuads) :
        mDevice(device), mFreyaOptions(freyaOptions), mMaterials(materials),
        mHdrRenderPass(hdrRenderPass), mLdrRenderPass(ldrRenderPass),
        mPipelineLayout(pipelineLayout), mSetLayout(setLayout),
        mDescriptorPool(descriptorPool), mInstanceSets(instanceSets),
        mInstanceBuffers(std::move(instanceBuffers)),
        mHdrPipelines(hdrPipelines), mLdrPipelines(ldrPipelines),
        mLdrFramebuffers(std::move(ldrFramebuffers)), mHdrExtent(extent),
        mLdrExtent(extent), mMaxQuads(maxQuads)
    {
    }

    BillboardPass::~BillboardPass()
    {
        if (!mDevice)
            return;
        const auto& d = mDevice->Get();
        destroyHdrFramebuffers();
        destroyLdrFramebuffers();
        auto destroyPipe = [&](vk::Pipeline p) {
            if (p)
                d.destroyPipeline(p);
        };
        destroyPipe(mHdrPipelines.alphaDepth);
        destroyPipe(mHdrPipelines.alphaNoDepth);
        destroyPipe(mHdrPipelines.addDepth);
        destroyPipe(mHdrPipelines.addNoDepth);
        destroyPipe(mLdrPipelines.alphaDepth);
        destroyPipe(mLdrPipelines.alphaNoDepth);
        destroyPipe(mLdrPipelines.addDepth);
        destroyPipe(mLdrPipelines.addNoDepth);
        if (mPipelineLayout)
            d.destroyPipelineLayout(mPipelineLayout);
        if (mHdrRenderPass)
            d.destroyRenderPass(mHdrRenderPass);
        if (mLdrRenderPass)
            d.destroyRenderPass(mLdrRenderPass);
        if (mDescriptorPool)
            d.destroyDescriptorPool(mDescriptorPool);
        if (mSetLayout)
            d.destroyDescriptorSetLayout(mSetLayout);
    }

    void BillboardPass::destroyHdrFramebuffers()
    {
        for (auto fb : mHdrFramebuffers)
        {
            if (fb)
                mDevice->Get().destroyFramebuffer(fb);
        }
        mHdrFramebuffers.clear();
        mHdrColorViews.clear();
        mHdrDepthView = nullptr;
    }

    void BillboardPass::destroyLdrFramebuffers()
    {
        for (auto fb : mLdrFramebuffers)
        {
            if (fb)
                mDevice->Get().destroyFramebuffer(fb);
        }
        mLdrFramebuffers.clear();
        mLdrDepthView = nullptr;
    }

    void BillboardPass::UpdateHdrTargets(
        const std::span<const skr::Arc<Image>> colors,
        const skr::Arc<Image>& depth, const vk::Extent2D extent)
    {
        destroyHdrFramebuffers();
        mHdrExtent = extent;
        if (!depth || colors.empty() || !mHdrRenderPass)
            return;

        mHdrDepthView = depth->GetImageView();
        mHdrFramebuffers.resize(colors.size());
        mHdrColorViews.resize(colors.size());
        for (std::size_t i = 0; i < colors.size(); ++i)
        {
            if (!colors[i])
                continue;
            mHdrColorViews[i] = colors[i]->GetImageView();
            auto views        = std::array { mHdrColorViews[i], mHdrDepthView };
            mHdrFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(mHdrRenderPass)
                    .setAttachments(views)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }
    }

    void BillboardPass::UpdateLdrDepth(const skr::Arc<Image>&     depth,
                                       const skr::Arc<SwapChain>& swapChain)
    {
        destroyLdrFramebuffers();
        if (!depth || !swapChain || !mLdrRenderPass)
            return;

        mLdrDepthView      = depth->GetImageView();
        const auto& frames = swapChain->GetFrames();
        const auto  extent = swapChain->GetExtent();
        mLdrExtent         = extent;
        mLdrFramebuffers.resize(frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            auto views = std::array { frames[i].imageView, mLdrDepthView };
            mLdrFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(mLdrRenderPass)
                    .setAttachments(views)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }
    }

    vk::Pipeline BillboardPass::pickPipeline(const BillboardTarget target,
                                             const BillboardBlend  blend,
                                             const bool depthTest) const
    {
        const auto& p =
            target == BillboardTarget::Hdr ? mHdrPipelines : mLdrPipelines;
        if (blend == BillboardBlend::Additive)
            return depthTest ? p.addDepth : p.addNoDepth;
        return depthTest ? p.alphaDepth : p.alphaNoDepth;
    }

    void BillboardPass::Draw(
        const skr::Arc<CommandPool>& commandPool,
        const skr::Arc<SwapChain>& swapChain, const BillboardTarget target,
        const BillboardLayer layer, const BillboardDraw& source,
        const glm::mat4& view, const glm::mat4& proj) const
    {
        if (source.Empty() || !commandPool || !swapChain)
            return;

        const auto frameIndex = swapChain->GetCurrentFrameIndex();
        const auto imageIndex = swapChain->GetCurrentImageIndex();
        if (frameIndex >= mInstanceBuffers.size() ||
            frameIndex >= mInstanceSets.size())
            return;

        const vk::Framebuffer framebuffer =
            target == BillboardTarget::Hdr
                ? (frameIndex < mHdrFramebuffers.size()
                       ? mHdrFramebuffers[frameIndex]
                       : vk::Framebuffer {})
                : (imageIndex < mLdrFramebuffers.size()
                       ? mLdrFramebuffers[imageIndex]
                       : vk::Framebuffer {});
        if (!framebuffer)
            return;

        const auto renderPass =
            target == BillboardTarget::Hdr ? mHdrRenderPass : mLdrRenderPass;
        if (!renderPass)
            return;

        struct Batch
        {
            BillboardBlend                    blend;
            bool                              depthTest;
            std::vector<BillboardGpuInstance> gpu;
        };
        Batch batches[4] = {
            { BillboardBlend::Alpha, true, {} },
            { BillboardBlend::Alpha, false, {} },
            { BillboardBlend::Additive, true, {} },
            { BillboardBlend::Additive, false, {} },
        };

        std::uint32_t total = 0;
        for (const auto& q : source.Quads())
        {
            if (q.layer != layer)
                continue;
            const int bi = (q.blend == BillboardBlend::Additive ? 2 : 0) +
                           (q.depthTest ? 0 : 1);
            batches[bi].gpu.push_back(ToGpu(q));
            ++total;
        }
        if (total == 0)
            return;

        total                 = std::min(total, mMaxQuads);
        std::uint32_t written = 0;
        for (auto& b : batches)
        {
            if (written >= mMaxQuads)
            {
                b.gpu.clear();
                continue;
            }
            if (written + b.gpu.size() > mMaxQuads)
                b.gpu.resize(mMaxQuads - written);
            written += static_cast<std::uint32_t>(b.gpu.size());
        }

        std::vector<BillboardGpuInstance> packed;
        packed.reserve(written);
        struct DrawRange
        {
            BillboardBlend blend;
            bool           depthTest;
            std::uint32_t  first;
            std::uint32_t  count;
        };
        std::vector<DrawRange> ranges;
        for (auto& b : batches)
        {
            if (b.gpu.empty())
                continue;
            DrawRange r {};
            r.blend     = b.blend;
            r.depthTest = b.depthTest;
            r.first     = static_cast<std::uint32_t>(packed.size());
            r.count     = static_cast<std::uint32_t>(b.gpu.size());
            packed.insert(packed.end(), b.gpu.begin(), b.gpu.end());
            ranges.push_back(r);
        }

        const auto bytes = packed.size() * sizeof(BillboardGpuInstance);
        mInstanceBuffers[frameIndex]->Copy(packed.data(), bytes);

        const auto extent =
            target == BillboardTarget::Hdr ? mHdrExtent : mLdrExtent;
        if (extent.width == 0 || extent.height == 0)
            return;

        const auto cmd   = commandPool->GetCommandBuffer();
        const auto label = target == BillboardTarget::Hdr
                               ? DebugLabel::BillboardVfx
                               : DebugLabel::BillboardUi;
        mDevice->BeginDebugLabel(cmd, label);

        cmd.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(renderPass)
                .setFramebuffer(framebuffer)
                .setRenderArea({ { 0, 0 }, extent })
                .setClearValueCount(0),
            vk::SubpassContents::eInline);

        cmd.setViewport(0, vk::Viewport()
                               .setX(0)
                               .setY(0)
                               .setWidth(static_cast<float>(extent.width))
                               .setHeight(static_cast<float>(extent.height))
                               .setMinDepth(0.f)
                               .setMaxDepth(1.f));
        cmd.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

        auto bindless = mMaterials->GetBindlessSet();
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, 1,
            &mInstanceSets[frameIndex], 0, nullptr);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               mPipelineLayout, 1, 1, &bindless, 0, nullptr);

        BillboardPush push { view, proj };
        cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                          sizeof(BillboardPush), &push);

        vk::Pipeline bound {};
        for (const auto& r : ranges)
        {
            auto pipe = pickPipeline(target, r.blend, r.depthTest);
            if (!pipe)
                continue;
            if (pipe != bound)
            {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipe);
                bound = pipe;
            }
            cmd.draw(6, r.count, 0, r.first);
        }

        cmd.endRenderPass();
        mDevice->EndDebugLabel(cmd);
    }

} // namespace FREYA_NAMESPACE
