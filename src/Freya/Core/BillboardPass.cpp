#include "Freya/Core/BillboardPass.hpp"

#include "Freya/Core/BillboardGpu.hpp"
#include "Freya/Core/DebugLabels.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct BillboardPush
        {
            glm::mat4 view { 1.f };
            glm::mat4 proj { 1.f };
        };

    } // namespace

    BillboardPass::BillboardPass(
        const skr::Arc<Device>&                      device,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const vk::RenderPass                         hdrRenderPass,
        const vk::RenderPass                         ldrRenderPass,
        const vk::RenderPass                         offscreenLdrRenderPass,
        const vk::PipelineLayout                     pipelineLayout,
        const vk::DescriptorSetLayout                setLayout,
        const vk::DescriptorPool                     descriptorPool,
        const std::vector<vk::DescriptorSet>&        instanceSets,
        std::vector<skr::Arc<Buffer>>
            instanceBuffers,
        std::vector<skr::Arc<Buffer>>
                        connectedBuffers,
        const Pipelines hdrPipelines,
        const Pipelines ldrPipelines,
        const Pipelines offscreenLdrPipelines,
        const Pipelines hdrConnectedPipelines,
        const Pipelines ldrConnectedPipelines,
        const Pipelines offscreenLdrConnectedPipelines,
        std::vector<vk::Framebuffer>
                            ldrFramebuffers,
        const vk::Extent2D  extent,
        const std::uint32_t maxQuads,
        const std::uint32_t maxConnectedQuads,
        DepthInputResources depthInput) :
        mDevice(device), mFreyaOptions(freyaOptions), mMaterials(materials),
        mHdrRenderPass(hdrRenderPass), mLdrRenderPass(ldrRenderPass),
        mOffscreenLdrRenderPass(offscreenLdrRenderPass),
        mPipelineLayout(pipelineLayout), mSetLayout(setLayout),
        mDescriptorPool(descriptorPool), mInstanceSets(instanceSets),
        mInstanceBuffers(std::move(instanceBuffers)),
        mConnectedBuffers(std::move(connectedBuffers)),
        mHdrPipelines(hdrPipelines), mLdrPipelines(ldrPipelines),
        mOffscreenLdrPipelines(offscreenLdrPipelines),
        mHdrConnectedPipelines(hdrConnectedPipelines),
        mLdrConnectedPipelines(ldrConnectedPipelines),
        mOffscreenLdrConnectedPipelines(offscreenLdrConnectedPipelines),
        mLdrFramebuffers(std::move(ldrFramebuffers)), mHdrExtent(extent),
        mLdrExtent(extent), mMaxQuads(maxQuads),
        mMaxConnectedQuads(maxConnectedQuads),
        mDepthInputSetLayout(depthInput.setLayout),
        mDepthInputPool(depthInput.pool), mDepthInputSet(depthInput.set)
    {
    }

    BillboardPass::~BillboardPass()
    {
        if (!mDevice)
            return;
        const auto& d = mDevice->Get();
        mDevice->Get().waitIdle();
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
        destroyPipe(mOffscreenLdrPipelines.alphaDepth);
        destroyPipe(mOffscreenLdrPipelines.alphaNoDepth);
        destroyPipe(mOffscreenLdrPipelines.addDepth);
        destroyPipe(mOffscreenLdrPipelines.addNoDepth);
        destroyPipe(mHdrConnectedPipelines.alphaDepth);
        destroyPipe(mHdrConnectedPipelines.alphaNoDepth);
        destroyPipe(mHdrConnectedPipelines.addDepth);
        destroyPipe(mHdrConnectedPipelines.addNoDepth);
        destroyPipe(mLdrConnectedPipelines.alphaDepth);
        destroyPipe(mLdrConnectedPipelines.alphaNoDepth);
        destroyPipe(mLdrConnectedPipelines.addDepth);
        destroyPipe(mLdrConnectedPipelines.addNoDepth);
        destroyPipe(mOffscreenLdrConnectedPipelines.alphaDepth);
        destroyPipe(mOffscreenLdrConnectedPipelines.alphaNoDepth);
        destroyPipe(mOffscreenLdrConnectedPipelines.addDepth);
        destroyPipe(mOffscreenLdrConnectedPipelines.addNoDepth);
        if (mPipelineLayout)
            d.destroyPipelineLayout(mPipelineLayout);
        if (mHdrRenderPass)
            d.destroyRenderPass(mHdrRenderPass);
        if (mLdrRenderPass)
            d.destroyRenderPass(mLdrRenderPass);
        if (mOffscreenLdrRenderPass)
            d.destroyRenderPass(mOffscreenLdrRenderPass);
        if (mDescriptorPool)
            d.destroyDescriptorPool(mDescriptorPool);
        if (mSetLayout)
            d.destroyDescriptorSetLayout(mSetLayout);
        if (mDepthInputPool)
            d.destroyDescriptorPool(mDepthInputPool);
        if (mDepthInputSetLayout)
            d.destroyDescriptorSetLayout(mDepthInputSetLayout);
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
        mLdrOffscreen = false;
    }

    void BillboardPass::UpdateHdrTargets(
        const std::span<const skr::Arc<Image>> colors,
        const skr::Arc<Image>& depth, const vk::Extent2D extent)
    {
        mDevice->Get().waitIdle();
        destroyHdrFramebuffers();
        mHdrExtent = extent;
        if (!depth || colors.empty() || !mHdrRenderPass)
            return;

        mHdrDepthView = depth->GetImageView();
        UpdateDepthInput(mHdrDepthView);
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
        mDevice->Get().waitIdle();
        destroyLdrFramebuffers();
        if (!depth || !swapChain || !mLdrRenderPass)
            return;

        mLdrOffscreen = false;
        mLdrDepthView = depth->GetImageView();
        UpdateDepthInput(mLdrDepthView);
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

    void BillboardPass::UpdateLdrOffscreen(const skr::Arc<Image>& color,
                                           const skr::Arc<Image>& depth,
                                           const vk::Extent2D     extent)
    {
        mDevice->Get().waitIdle();
        destroyLdrFramebuffers();
        if (!color || !depth || !mOffscreenLdrRenderPass)
            return;
        if (extent.width == 0 || extent.height == 0)
            return;

        mLdrOffscreen = true;
        mLdrDepthView = depth->GetImageView();
        UpdateDepthInput(mLdrDepthView);
        mLdrExtent = extent;
        mLdrFramebuffers.resize(1);
        auto views = std::array { color->GetImageView(), mLdrDepthView };
        mLdrFramebuffers[0] = mDevice->Get().createFramebuffer(
            vk::FramebufferCreateInfo()
                .setRenderPass(mOffscreenLdrRenderPass)
                .setAttachments(views)
                .setWidth(extent.width)
                .setHeight(extent.height)
                .setLayers(1));
    }

    void BillboardPass::UpdateDepthInput(const vk::ImageView depthView)
    {
        if (!mDepthInputSet || !depthView)
            return;
        auto imgInfo =
            vk::DescriptorImageInfo().setImageView(depthView).setImageLayout(
                vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        auto write =
            vk::WriteDescriptorSet()
                .setDstSet(mDepthInputSet)
                .setDstBinding(0)
                .setDescriptorCount(1)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setImageInfo(imgInfo);
        mDevice->Get().updateDescriptorSets(write, nullptr);
    }

    vk::Pipeline BillboardPass::pickPipeline(const BillboardTarget target,
                                             const BillboardBlend  blend,
                                             const bool depthTest) const
    {
        const Pipelines* p = &mHdrPipelines;
        if (target == BillboardTarget::Ldr)
            p = mLdrOffscreen ? &mOffscreenLdrPipelines : &mLdrPipelines;
        if (blend == BillboardBlend::Additive)
            return depthTest ? p->addDepth : p->addNoDepth;
        return depthTest ? p->alphaDepth : p->alphaNoDepth;
    }

    vk::Pipeline BillboardPass::pickConnectedPipeline(
        const BillboardTarget target, const BillboardBlend blend,
        const bool depthTest) const
    {
        const Pipelines* p = &mHdrConnectedPipelines;
        if (target == BillboardTarget::Ldr)
        {
            p = mLdrOffscreen ? &mOffscreenLdrConnectedPipelines
                              : &mLdrConnectedPipelines;
        }
        if (blend == BillboardBlend::Additive)
            return depthTest ? p->addDepth : p->addNoDepth;
        return depthTest ? p->alphaDepth : p->alphaNoDepth;
    }

    void BillboardPass::Draw(
        const skr::Arc<CommandPool>& commandPool,
        const skr::Arc<SwapChain>& swapChain, const BillboardTarget target,
        const BillboardLayer layer, const BillboardDraw& source,
        const glm::mat4& view, const glm::mat4& proj) const
    {
        if (!commandPool || !swapChain)
            return;

        std::vector<Billboard> quads;
        source.Snapshot(quads);
        std::vector<ConnectedBillboard> connected;
        source.SnapshotConnected(connected);
        if (quads.empty() && connected.empty())
            return;

        const auto frameIndex = swapChain->GetCurrentFrameIndex();
        const auto imageIndex = swapChain->GetCurrentImageIndex();
        if (frameIndex >= mInstanceBuffers.size() ||
            frameIndex >= mConnectedBuffers.size() ||
            frameIndex >= mInstanceSets.size())
            return;

        vk::Framebuffer framebuffer {};
        vk::RenderPass  renderPass {};
        if (target == BillboardTarget::Hdr)
        {
            framebuffer = frameIndex < mHdrFramebuffers.size()
                              ? mHdrFramebuffers[frameIndex]
                              : vk::Framebuffer {};
            renderPass  = mHdrRenderPass;
        }
        else if (mLdrOffscreen)
        {
            framebuffer = !mLdrFramebuffers.empty() ? mLdrFramebuffers[0]
                                                    : vk::Framebuffer {};
            renderPass  = mOffscreenLdrRenderPass;
        }
        else
        {
            framebuffer = imageIndex < mLdrFramebuffers.size()
                              ? mLdrFramebuffers[imageIndex]
                              : vk::Framebuffer {};
            renderPass  = mLdrRenderPass;
        }
        if (!framebuffer || !renderPass)
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
        for (const auto& q : quads)
        {
            if (q.layer != layer)
                continue;
            const int bi = (q.blend == BillboardBlend::Additive ? 2 : 0) +
                           (HasFlag(q.flags, BillboardFlags::DepthTest)
                                   ? 0
                                   : 1);
            batches[bi].gpu.push_back(ToBillboardGpu(q));
            ++total;
        }

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
        if (!packed.empty())
            mInstanceBuffers[frameIndex]->Copy(packed.data(), bytes);

        struct ConnectedBatch
        {
            BillboardBlend                             blend;
            bool                                       depthTest;
            std::vector<ConnectedBillboardGpuInstance> gpu;
        };
        ConnectedBatch connectedBatches[4] = {
            { BillboardBlend::Alpha, true, {} },
            { BillboardBlend::Alpha, false, {} },
            { BillboardBlend::Additive, true, {} },
            { BillboardBlend::Additive, false, {} },
        };

        std::uint32_t connectedTotal = 0;
        for (const auto& q : connected)
        {
            if (q.layer != layer)
                continue;
            const int bi = (q.blend == BillboardBlend::Additive ? 2 : 0) +
                           (HasFlag(q.flags, BillboardFlags::DepthTest)
                                   ? 0
                                   : 1);
            connectedBatches[bi].gpu.push_back(ToConnectedBillboardGpu(q));
            ++connectedTotal;
        }

        connectedTotal = std::min(connectedTotal, mMaxConnectedQuads);
        std::uint32_t connectedWritten = 0;
        for (auto& b : connectedBatches)
        {
            if (connectedWritten >= mMaxConnectedQuads)
            {
                b.gpu.clear();
                continue;
            }
            if (connectedWritten + b.gpu.size() > mMaxConnectedQuads)
                b.gpu.resize(mMaxConnectedQuads - connectedWritten);
            connectedWritten += static_cast<std::uint32_t>(b.gpu.size());
        }

        std::vector<ConnectedBillboardGpuInstance> connectedPacked;
        connectedPacked.reserve(connectedWritten);
        struct ConnectedDrawRange
        {
            BillboardBlend blend;
            bool           depthTest;
            std::uint32_t  first;
            std::uint32_t  count;
        };
        std::vector<ConnectedDrawRange> connectedRanges;
        for (auto& b : connectedBatches)
        {
            if (b.gpu.empty())
                continue;
            ConnectedDrawRange r {};
            r.blend     = b.blend;
            r.depthTest = b.depthTest;
            r.first     = static_cast<std::uint32_t>(connectedPacked.size());
            r.count     = static_cast<std::uint32_t>(b.gpu.size());
            connectedPacked.insert(
                connectedPacked.end(), b.gpu.begin(), b.gpu.end());
            connectedRanges.push_back(r);
        }

        if (packed.empty() && connectedPacked.empty())
            return;

        if (!connectedPacked.empty())
        {
            const auto connectedBytes =
                connectedPacked.size() * sizeof(ConnectedBillboardGpuInstance);
            mConnectedBuffers[frameIndex]->Copy(connectedPacked.data(),
                                                connectedBytes);
        }

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
        if (mDepthInputSet)
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, mPipelineLayout, 2, 1,
                &mDepthInputSet, 0, nullptr);

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

        // Keep tracking across both loops: the connected pipelines
        // may resolve to the same handle as the last bound one.
        for (const auto& r : connectedRanges)
        {
            auto pipe = pickConnectedPipeline(target, r.blend, r.depthTest);
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
