#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Image-based lighting + LTC LUT resources.
     *
     * Owns equirectangular HDR environment / irradiance maps, a BRDF
     * integration LUT, and LTC matrices for rectangular area lights.
     * Specular env mips are GGX importance-sampled (split-sum prefilter).
     * Built at construction from FreyaOptions (procedural sky or Radiance
     * .hdr path).
     */
    class IBLService
    {
      public:
        IBLService(const skr::Arc<Device>&               device,
                   const skr::Arc<skr::ServiceProvider>& serviceProvider,
                   const skr::Arc<FreyaOptions>&         options);

        ~IBLService();

        IBLService(const IBLService&)            = delete;
        IBLService& operator=(const IBLService&) = delete;

        skr::Arc<Image> GetEnvironmentMap() const { return mEnvironment; }
        skr::Arc<Image> GetIrradianceMap() const { return mIrradiance; }
        skr::Arc<Image> GetBrdfLut() const { return mBrdfLut; }
        skr::Arc<Image> GetLtcMatrixMap() const { return mLtcMatrix; }
        skr::Arc<Image> GetLtcAmplMap() const { return mLtcAmpl; }

        vk::Sampler GetEnvironmentSampler() const
        {
            return mEnvironmentSampler;
        }
        vk::Sampler GetIrradianceSampler() const { return mIrradianceSampler; }
        vk::Sampler GetBrdfSampler() const { return mBrdfSampler; }
        vk::Sampler GetLtcSampler() const { return mLtcSampler; }

        float         GetIntensity() const { return mIntensity; }
        void          SetIntensity(float intensity) { mIntensity = intensity; }
        std::uint32_t GetEnvironmentMipCount() const
        {
            return mEnvironment ? mEnvironment->GetMipLevels() : 1;
        }

        bool IsReady() const
        {
            return mEnvironment && mIrradiance && mBrdfLut && mLtcMatrix &&
                   mLtcAmpl;
        }

      private:
        void createSamplers();
        void buildFromEquirect(const std::vector<float>& src, int width,
                               int height);
        void generateProceduralSky(std::vector<float>& out, int width,
                                   int height) const;
        bool loadHdrFile(const std::string& path, std::vector<float>& out,
                         int& width, int& height) const;
        void downsampleEquirect(std::vector<float>& data, int& width,
                                int& height, int maxWidth) const;
        void convolveIrradiance(const std::vector<float>& src, int srcW,
                                int srcH, std::vector<float>& dst, int dstW,
                                int dstH) const;
        void prefilterSpecular(const std::vector<float>& src, int srcW,
                               int srcH, std::vector<float>& packed,
                               int& outWidth, int& outHeight,
                               int& mipCount) const;
        void generateBrdfLut(std::vector<float>& out, int size) const;
        void generateLtcLuts(std::vector<float>& ltc1, std::vector<float>& ltc2,
                             int size) const;
        skr::Arc<Image> uploadFloatRgb(const std::vector<float>& rgba,
                                       int width, int height,
                                       bool generateMips) const;
        skr::Arc<Image> uploadFloatRgbMipChain(const std::vector<float>& packed,
                                               int width, int height,
                                               int mipCount) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        float                          mIntensity = 1.0f;

        skr::Arc<Image> mEnvironment;
        skr::Arc<Image> mIrradiance;
        skr::Arc<Image> mBrdfLut;
        skr::Arc<Image> mLtcMatrix;
        skr::Arc<Image> mLtcAmpl;

        vk::Sampler mEnvironmentSampler = nullptr;
        vk::Sampler mIrradianceSampler  = nullptr;
        vk::Sampler mBrdfSampler        = nullptr;
        vk::Sampler mLtcSampler         = nullptr;
    };

} // namespace FREYA_NAMESPACE
