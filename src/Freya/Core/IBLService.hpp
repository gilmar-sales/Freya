#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Image-based lighting resources (split-sum approximation).
     *
     * Owns equirectangular HDR environment / irradiance maps and a BRDF
     * integration LUT. Specular uses environment mip LODs as a prefilter
     * approximation. Built at construction from FreyaOptions
     * (procedural sky or Radiance .hdr path).
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

        vk::Sampler GetEnvironmentSampler() const
        {
            return mEnvironmentSampler;
        }
        vk::Sampler GetIrradianceSampler() const { return mIrradianceSampler; }
        vk::Sampler GetBrdfSampler() const { return mBrdfSampler; }

        float         GetIntensity() const { return mIntensity; }
        void          SetIntensity(float intensity) { mIntensity = intensity; }
        std::uint32_t GetEnvironmentMipCount() const
        {
            return mEnvironment ? mEnvironment->GetMipLevels() : 1;
        }

        bool IsReady() const
        {
            return mEnvironment && mIrradiance && mBrdfLut;
        }

      private:
        void createSamplers();
        void buildFromEquirect(const std::vector<float>& src, int width,
                               int height);
        void generateProceduralSky(std::vector<float>& out, int width,
                                   int height) const;
        bool loadHdrFile(const std::string& path, std::vector<float>& out,
                         int& width, int& height) const;
        void convolveIrradiance(const std::vector<float>& src, int srcW,
                                int srcH, std::vector<float>& dst, int dstW,
                                int dstH) const;
        void generateBrdfLut(std::vector<float>& out, int size) const;
        skr::Arc<Image> uploadFloatRgb(const std::vector<float>& rgba,
                                       int width, int height,
                                       bool generateMips) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        float                          mIntensity = 1.0f;

        skr::Arc<Image> mEnvironment;
        skr::Arc<Image> mIrradiance;
        skr::Arc<Image> mBrdfLut;

        vk::Sampler mEnvironmentSampler = nullptr;
        vk::Sampler mIrradianceSampler  = nullptr;
        vk::Sampler mBrdfSampler        = nullptr;
    };

} // namespace FREYA_NAMESPACE
