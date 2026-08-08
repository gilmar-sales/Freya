#include "XessPassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"

#if FREYA_HAS_XESS
    #include <xess/xess.h>
    #include <xess/xess_vk.h>
#endif

namespace FREYA_NAMESPACE
{
    namespace
    {
#if FREYA_HAS_XESS
        xess_quality_settings_t ToXessQuality(XessQuality quality)
        {
            switch (quality)
            {
                case XessQuality::UltraPerformance:
                    return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
                case XessQuality::Performance:
                    return XESS_QUALITY_SETTING_PERFORMANCE;
                case XessQuality::Balanced:
                    return XESS_QUALITY_SETTING_BALANCED;
                case XessQuality::Quality:
                    return XESS_QUALITY_SETTING_QUALITY;
                case XessQuality::UltraQuality:
                    return XESS_QUALITY_SETTING_ULTRA_QUALITY;
                case XessQuality::UltraQualityPlus:
                    return XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS;
                case XessQuality::AntiAliasing:
                    return XESS_QUALITY_SETTING_AA;
                case XessQuality::Off:
                    break;
            }
            return XESS_QUALITY_SETTING_BALANCED;
        }
#endif
    } // namespace

    XessPassBuilder::XessPassBuilder(
        const skr::Arc<Instance>&             instance,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Device>&               device,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mInstance(instance), mPhysicalDevice(physicalDevice), mDevice(device),
        mSurface(surface), mFreyaOptions(freyaOptions),
        mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<XessPass> XessPassBuilder::Build(const skr::Arc<SwapChain>&,
                                              vk::Extent2D outputExtent)
    {
        if (!mFreyaOptions->enableXess)
            return nullptr;

        if (outputExtent.width == 0 || outputExtent.height == 0)
            outputExtent = mSurface->QueryExtent();

#if !FREYA_HAS_XESS
        std::cerr
            << "Freya: enableXess is set but FREYA_HAS_XESS=0 "
               "(Intel XeSS runtime is Windows-only in SDK v3.0.2).\n";
        return nullptr;
#else
        xess_version_t version {};
        if (xessGetVersion(&version) == XESS_RESULT_SUCCESS)
        {
            std::cout << "Freya: linking XeSS v" << version.major << '.'
                      << version.minor << '.' << version.patch << '\n';
        }

        xess_context_handle_t context = nullptr;
        auto                  status  = xessVKCreateContext(
            mInstance->Get(), mPhysicalDevice->Get(), mDevice->Get(),
            &context);
        if (status != XESS_RESULT_SUCCESS || !context)
        {
            std::cerr << "Freya: xessVKCreateContext failed ("
                      << static_cast<int>(status) << ")\n";
            return nullptr;
        }

        if (xessIsOptimalDriver(context) == XESS_RESULT_WARNING_OLD_DRIVER)
        {
            std::cerr << "Freya: XeSS warns about an outdated graphics driver\n";
        }

        // Prefer the FreyaOptions quality; map defaults to Balanced.
        const auto qualityEnum = mFreyaOptions->xessQuality == XessQuality::Off
                                     ? XessQuality::Balanced
                                     : mFreyaOptions->xessQuality;
        const auto quality     = ToXessQuality(qualityEnum);

        uint32_t initFlags = XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE;
        if (mFreyaOptions->ReverseZ)
            initFlags |= XESS_INIT_FLAG_INVERTED_DEPTH;

        xess_vk_init_params_t params {};
        params.outputResolution = { outputExtent.width, outputExtent.height };
        params.qualitySetting   = quality;
        params.initFlags        = initFlags;
        params.creationNodeMask = 0;
        params.visibleNodeMask  = 0;
        params.tempBufferHeap   = VK_NULL_HANDLE;
        params.bufferHeapOffset = 0;
        params.tempTextureHeap  = VK_NULL_HANDLE;
        params.textureHeapOffset = 0;
        params.pipelineCache    = VK_NULL_HANDLE;

        status = xessVKInit(context, &params);
        if (status != XESS_RESULT_SUCCESS)
        {
            std::cerr << "Freya: xessVKInit failed ("
                      << static_cast<int>(status) << ")\n";
            xessDestroyContext(context);
            return nullptr;
        }

        xess_2d_t outputRes { outputExtent.width, outputExtent.height };
        xess_2d_t inputRes {};
        status = xessGetOptimalInputResolution(context, &outputRes, quality,
                                               &inputRes, nullptr, nullptr);
        if (status != XESS_RESULT_SUCCESS || inputRes.x == 0 || inputRes.y == 0)
        {
            // Fallback to non-optimal query.
            status = xessGetInputResolution(context, &outputRes, quality,
                                            &inputRes);
        }
        if (status != XESS_RESULT_SUCCESS || inputRes.x == 0 || inputRes.y == 0)
        {
            std::cerr << "Freya: XeSS input resolution query failed\n";
            xessDestroyContext(context);
            return nullptr;
        }

        const vk::Extent2D inputExtent { inputRes.x, inputRes.y };
        std::cout << "Freya: XeSS " << inputExtent.width << 'x'
                  << inputExtent.height << " → " << outputExtent.width << 'x'
                  << outputExtent.height << '\n';

        auto outputImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::TaaHistory)
                .SetWidth(outputExtent.width)
                .SetHeight(outputExtent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        return skr::MakeArc<XessPass>(
            mInstance, mPhysicalDevice, mDevice, mFreyaOptions,
            std::move(outputImage), inputExtent, outputExtent, quality,
            context);
#endif
    }

} // namespace FREYA_NAMESPACE
