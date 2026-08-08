#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <FidelityFX/host/ffx_fsr3upscaler.h>

#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief AMD FidelityFX FSR 3.1 Upscaler (replaces Freya's custom TAA).
     *
     * Renders at renderExtent, writes HDR at displayExtent. Bloom/composite
     * should sample GetOutputImage() after Dispatch.
     */
    class FsrUpscalePass
    {
      public:
        FsrUpscalePass(const skr::Arc<Device>&         device,
                       const skr::Arc<PhysicalDevice>& physicalDevice,
                       const skr::Arc<FreyaOptions>&   freyaOptions,
                       skr::Arc<Image>
                                    outputImage,
                       vk::Extent2D renderExtent,
                       vk::Extent2D displayExtent);

        ~FsrUpscalePass();

        FsrUpscalePass(const FsrUpscalePass&)            = delete;
        FsrUpscalePass& operator=(const FsrUpscalePass&) = delete;

        [[nodiscard]] skr::Arc<Image> GetOutputImage() const
        {
            return mOutputImage;
        }

        [[nodiscard]] vk::Extent2D GetRenderExtent() const
        {
            return mRenderExtent;
        }

        [[nodiscard]] vk::Extent2D GetDisplayExtent() const
        {
            return mDisplayExtent;
        }

        [[nodiscard]] bool Valid() const { return mContextReady; }

        void ResetHistory() { mResetNextDispatch = true; }

        /// Sub-pixel jitter in unit-pixel space (same as FSR GetJitterOffset).
        void GetJitterOffset(std::uint32_t frameIndex, float& outX,
                             float& outY) const;

        [[nodiscard]] int GetJitterPhaseCount() const;

        /// Record the last jitter applied to the projection this frame
        /// (unit-pixel space from GetJitterOffset).
        void SetAppliedJitter(float x, float y)
        {
            mLastJitterX = x;
            mLastJitterY = y;
        }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<Image>&       sceneColor,
                      const skr::Arc<Image>&       velocity,
                      const skr::Arc<Image>&       depth,
                      float                        frameTimeDeltaMs,
                      float                        cameraNear,
                      float                        cameraFar,
                      float                        cameraFovYRadians);

        /// Map Freya quality → FFX quality / resolve render size.
        static FfxFsr3UpscalerQualityMode ToFfxQuality(FsrQuality quality);

        static vk::Extent2D QueryRenderExtent(vk::Extent2D display,
                                              FsrQuality   quality);

      private:
        void destroyContext();
        bool createContext();

        FfxResource wrapColor(VkImage image, VkFormat format, uint32_t w,
                              uint32_t h, bool storage,
                              FfxResourceStates state) const;

        skr::Arc<Device>         mDevice;
        skr::Arc<PhysicalDevice> mPhysicalDevice;
        skr::Arc<FreyaOptions>   mFreyaOptions;
        skr::Arc<Image>          mOutputImage;
        vk::Extent2D             mRenderExtent {};
        vk::Extent2D             mDisplayExtent {};

        FfxInterface              mBackendInterface {};
        std::vector<std::uint8_t> mScratchBuffer;
        FfxFsr3UpscalerContext    mContext {};
        bool                      mContextReady = false;

        FfxResourceInternal mDilatedDepth {};
        FfxResourceInternal mDilatedMotionVectors {};
        FfxResourceInternal mReconstructedPrevNearestDepth {};

        mutable bool  mResetNextDispatch = true;
        mutable float mLastJitterX       = 0.f;
        mutable float mLastJitterY       = 0.f;
    };
} // namespace FREYA_NAMESPACE
