#include "XessPass.hpp"

#include "Freya/Core/DebugLabels.hpp"

namespace FREYA_NAMESPACE
{
#if FREYA_HAS_XESS
    namespace
    {
        xess_vk_image_view_info MakeViewInfo(const skr::Arc<Image>& image,
                                             vk::Extent2D           extent,
                                             vk::ImageAspectFlags   aspect)
        {
            xess_vk_image_view_info info {};
            info.imageView = image->GetImageView();
            info.image     = image->GetImage();
            info.subresourceRange =
                VkImageSubresourceRange { static_cast<VkImageAspectFlags>(
                                              aspect),
                                          0, 1, 0, 1 };
            info.format = static_cast<VkFormat>(image->GetFormat());
            info.width  = extent.width;
            info.height = extent.height;
            return info;
        }
    } // namespace

    XessPass::XessPass(const skr::Arc<Instance>&, const skr::Arc<PhysicalDevice>&,
                       const skr::Arc<Device>&         device,
                       const skr::Arc<FreyaOptions>&   freyaOptions,
                       skr::Arc<Image>                 outputImage,
                       vk::Extent2D                    inputExtent,
                       vk::Extent2D                    outputExtent,
                       xess_quality_settings_t         quality,
                       xess_context_handle_t           context) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mOutputImage(std::move(outputImage)), mInputExtent(inputExtent),
        mOutputExtent(outputExtent), mContext(context), mQuality(quality)
    {
        xess_version_t version {};
        if (xessGetVersion(&version) == XESS_RESULT_SUCCESS)
        {
            std::cout << "Freya: XeSS SDK v" << version.major << '.'
                      << version.minor << '.' << version.patch << '\n';
        }

        if (mContext)
        {
            xessSetVelocityScale(mContext,
                                 static_cast<float>(mInputExtent.width),
                                 static_cast<float>(mInputExtent.height));
            xessSetJitterScale(mContext, 1.0f, 1.0f);
        }
    }

    XessPass::~XessPass()
    {
        if (mContext)
        {
            mDevice->Get().waitIdle();
            xessDestroyContext(mContext);
            mContext = nullptr;
        }
    }

    void XessPass::Dispatch(const skr::Arc<CommandPool>& commandPool,
                            const skr::Arc<Image>&       sceneColor,
                            const skr::Arc<Image>&       velocity,
                            const skr::Arc<Image>&       depth) const
    {
        if (!mContext || !sceneColor || !velocity || !depth || !mOutputImage)
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Xess);

        const auto colorRange =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);
        const auto depthRange =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto barriers = std::array {
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(sceneColor->GetImage())
                .setSubresourceRange(colorRange),
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(velocity->GetImage())
                .setSubresourceRange(colorRange),
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentRead)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(depth->GetImage())
                .setSubresourceRange(depthRange),
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eGeneral)
                .setSrcAccessMask({})
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setImage(mOutputImage->GetImage())
                .setSubresourceRange(colorRange),
        };

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput |
                vk::PipelineStageFlagBits::eEarlyFragmentTests |
                vk::PipelineStageFlagBits::eLateFragmentTests |
                vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader, {}, nullptr, nullptr,
            barriers);

        xess_vk_execute_params_t exec {};
        exec.colorTexture =
            MakeViewInfo(sceneColor, mInputExtent,
                         vk::ImageAspectFlagBits::eColor);
        exec.velocityTexture =
            MakeViewInfo(velocity, mInputExtent,
                         vk::ImageAspectFlagBits::eColor);
        exec.depthTexture =
            MakeViewInfo(depth, mInputExtent, vk::ImageAspectFlagBits::eDepth);
        exec.outputTexture =
            MakeViewInfo(mOutputImage, mOutputExtent,
                         vk::ImageAspectFlagBits::eColor);
        exec.jitterOffsetX  = mJitterX;
        exec.jitterOffsetY  = -mJitterY;
        exec.exposureScale  = 1.0f;
        exec.resetHistory   = (!mHistoryPrimed || mResetHistory) ? 1u : 0u;
        exec.inputWidth     = mInputExtent.width;
        exec.inputHeight    = mInputExtent.height;

        const auto result =
            xessVKExecute(mContext, commandBuffer, &exec);
        if (result != XESS_RESULT_SUCCESS)
        {
            std::cerr << "Freya: xessVKExecute failed ("
                      << static_cast<int>(result) << ")\n";
        }
        else
        {
            mHistoryPrimed = true;
            mResetHistory  = false;
        }

        auto toSampled =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eGeneral)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(mOutputImage->GetImage())
                .setSubresourceRange(colorRange);
        auto depthBack =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentRead)
                .setImage(depth->GetImage())
                .setSubresourceRange(depthRange);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eEarlyFragmentTests,
            {}, nullptr, nullptr,
            std::array { toSampled, depthBack });

        mDevice->EndDebugLabel(commandBuffer);
    }

#else

    XessPass::XessPass(const skr::Arc<Instance>&, const skr::Arc<PhysicalDevice>&,
                       const skr::Arc<Device>& device,
                       const skr::Arc<FreyaOptions>& freyaOptions,
                       skr::Arc<Image>               outputImage,
                       vk::Extent2D                  inputExtent,
                       vk::Extent2D                  outputExtent) :
        mDevice(device), mFreyaOptions(freyaOptions),
        mOutputImage(std::move(outputImage)), mInputExtent(inputExtent),
        mOutputExtent(outputExtent)
    {
    }

    XessPass::~XessPass() = default;

    void XessPass::Dispatch(const skr::Arc<CommandPool>&,
                            const skr::Arc<Image>&, const skr::Arc<Image>&,
                            const skr::Arc<Image>&) const
    {
    }

#endif
} // namespace FREYA_NAMESPACE
