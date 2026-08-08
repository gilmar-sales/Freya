#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

#if FREYA_HAS_XESS
    #include <xess/xess.h>
    #include <xess/xess_vk.h>
#endif

namespace FREYA_NAMESPACE
{
    /**
     * @brief Intel XeSS-SR temporal upscaler (replaces Freya TAA when enabled).
     *
     * Renders at an optimal input resolution and upscales to the swapchain
     * (present) extent. Requires FREYA_HAS_XESS (Windows SDK runtime).
     */
    class XessPass
    {
      public:
#if FREYA_HAS_XESS
        XessPass(const skr::Arc<Instance>&       instance,
                 const skr::Arc<PhysicalDevice>& physicalDevice,
                 const skr::Arc<Device>&         device,
                 const skr::Arc<FreyaOptions>&   freyaOptions,
                 skr::Arc<Image>                 outputImage,
                 vk::Extent2D                    inputExtent,
                 vk::Extent2D                    outputExtent,
                 xess_quality_settings_t         quality,
                 xess_context_handle_t           context);
#else
        XessPass(const skr::Arc<Instance>&, const skr::Arc<PhysicalDevice>&,
                 const skr::Arc<Device>&, const skr::Arc<FreyaOptions>&,
                 skr::Arc<Image> outputImage, vk::Extent2D inputExtent,
                 vk::Extent2D outputExtent);
#endif

        ~XessPass();

        [[nodiscard]] skr::Arc<Image> GetOutputImage() const
        {
            return mOutputImage;
        }

        [[nodiscard]] vk::Extent2D GetInputExtent() const
        {
            return mInputExtent;
        }

        [[nodiscard]] vk::Extent2D GetOutputExtent() const
        {
            return mOutputExtent;
        }

        void ResetHistory() { mResetHistory = true; }

        void SetJitter(float offsetX, float offsetY)
        {
            mJitterX = offsetX;
            mJitterY = offsetY;
        }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<Image>&       sceneColor,
                      const skr::Arc<Image>&       velocity,
                      const skr::Arc<Image>&       depth) const;

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Image>        mOutputImage;
        vk::Extent2D           mInputExtent {};
        vk::Extent2D           mOutputExtent {};

        mutable float mJitterX       = 0.0f;
        mutable float mJitterY       = 0.0f;
        mutable bool  mResetHistory  = true;
        mutable bool  mHistoryPrimed = false;

#if FREYA_HAS_XESS
        xess_context_handle_t    mContext = nullptr;
        xess_quality_settings_t  mQuality = XESS_QUALITY_SETTING_BALANCED;
#endif
    };
} // namespace FREYA_NAMESPACE
