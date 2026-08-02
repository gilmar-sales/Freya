#include "Freya/Core/IBLService.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Vendor/stb_image.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr float kPi = std::numbers::pi_v<float>;

        glm::vec3 DirectionFromEquirect(float u, float v)
        {
            const float phi   = u * 2.0f * kPi;
            const float theta = v * kPi;
            const float sinT  = std::sin(theta);
            return glm::normalize(glm::vec3(sinT * std::cos(phi),
                                            std::cos(theta),
                                            sinT * std::sin(phi)));
        }

        glm::vec2 EquirectFromDirection(const glm::vec3& dir)
        {
            const glm::vec3 n = glm::normalize(dir);
            float           u =
                std::atan2(n.z, n.x) / (2.0f * kPi) + 0.5f;
            float v = std::acos(std::clamp(n.y, -1.0f, 1.0f)) / kPi;
            return { u, v };
        }

        glm::vec3 SampleEquirect(const std::vector<float>& data, int width,
                                 int height, const glm::vec3& dir)
        {
            const glm::vec2 uv = EquirectFromDirection(dir);
            const float     x = uv.x * static_cast<float>(width - 1);
            const float     y = uv.y * static_cast<float>(height - 1);
            const int       x0 = static_cast<int>(x);
            const int       y0 = static_cast<int>(y);
            const int x1 = std::min(x0 + 1, width - 1);
            const int y1 = std::min(y0 + 1, height - 1);
            const float fx = x - static_cast<float>(x0);
            const float fy = y - static_cast<float>(y0);

            auto fetch = [&](int px, int py) {
                const std::size_t i =
                    (static_cast<std::size_t>(py) * width + px) * 4;
                return glm::vec3(data[i], data[i + 1], data[i + 2]);
            };

            const glm::vec3 c00 = fetch(x0, y0);
            const glm::vec3 c10 = fetch(x1, y0);
            const glm::vec3 c01 = fetch(x0, y1);
            const glm::vec3 c11 = fetch(x1, y1);
            return glm::mix(glm::mix(c00, c10, fx), glm::mix(c01, c11, fx), fy);
        }

        float RadicalInverseVanDerCorput(std::uint32_t bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) |
                   ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) |
                   ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) |
                   ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) |
                   ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        }

        glm::vec2 Hammersley(std::uint32_t i, std::uint32_t n)
        {
            return { static_cast<float>(i) / static_cast<float>(n),
                     RadicalInverseVanDerCorput(i) };
        }

        glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N,
                                      float roughness)
        {
            const float a = roughness * roughness;
            const float phi = 2.0f * kPi * Xi.x;
            const float cosTheta =
                std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
            const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

            const glm::vec3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta,
                              cosTheta);

            const glm::vec3 up =
                std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
            const glm::vec3 tangent   = glm::normalize(glm::cross(up, N));
            const glm::vec3 bitangent = glm::cross(N, tangent);
            return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
        }

        float GeometrySchlickGGX(float NdotV, float roughness)
        {
            const float a = roughness;
            const float k = (a * a) / 2.0f;
            return NdotV / (NdotV * (1.0f - k) + k);
        }

        float GeometrySmith(float NdotV, float NdotL, float roughness)
        {
            return GeometrySchlickGGX(NdotV, roughness) *
                   GeometrySchlickGGX(NdotL, roughness);
        }
    } // namespace

    IBLService::IBLService(const skr::Arc<Device>&               device,
                           const skr::Arc<skr::ServiceProvider>& serviceProvider,
                           const skr::Arc<FreyaOptions>& options) :
        mDevice(device), mServiceProvider(serviceProvider),
        mIntensity(options->iblIntensity)
    {
        constexpr int kEnvW = 512;
        constexpr int kEnvH = 256;

        std::vector<float> env;
        int                width  = kEnvW;
        int                height = kEnvH;

        if (!options->environmentMapPath.empty() &&
            loadHdrFile(options->environmentMapPath, env, width, height))
        {
            // loaded
        }
        else
        {
            width  = kEnvW;
            height = kEnvH;
            generateProceduralSky(env, width, height);
        }

        buildFromEquirect(env, width, height);
        createSamplers();
    }

    IBLService::~IBLService()
    {
        auto& vkDevice = mDevice->Get();
        if (mEnvironmentSampler)
        {
            vkDevice.destroySampler(mEnvironmentSampler);
        }
        if (mIrradianceSampler)
        {
            vkDevice.destroySampler(mIrradianceSampler);
        }
        if (mBrdfSampler)
        {
            vkDevice.destroySampler(mBrdfSampler);
        }
    }

    void IBLService::createSamplers()
    {
        auto makeSampler = [&](bool enableMip) {
            auto info =
                vk::SamplerCreateInfo()
                    .setMagFilter(vk::Filter::eLinear)
                    .setMinFilter(vk::Filter::eLinear)
                    .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                    .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                    .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                    .setAnisotropyEnable(false)
                    .setMaxAnisotropy(1.0f)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueBlack)
                    .setUnnormalizedCoordinates(false)
                    .setCompareEnable(false)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setMinLod(0.0f)
                    .setMaxLod(enableMip ? VK_LOD_CLAMP_NONE : 0.0f)
                    .setMipLodBias(0.0f);
            return mDevice->Get().createSampler(info);
        };

        mEnvironmentSampler = makeSampler(true);
        mIrradianceSampler  = makeSampler(false);
        mBrdfSampler        = makeSampler(false);
    }

    void IBLService::generateProceduralSky(std::vector<float>& out, int width,
                                           int height) const
    {
        out.resize(static_cast<std::size_t>(width) * height * 4);
        const glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, 0.85f, 0.25f));
        const glm::vec3 sunColor(12.0f, 10.5f, 8.0f);
        const glm::vec3 zenith(0.15f, 0.35f, 0.85f);
        const glm::vec3 horizon(0.85f, 0.75f, 0.65f);
        const glm::vec3 ground(0.08f, 0.07f, 0.06f);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const float     u   = (x + 0.5f) / width;
                const float     v   = (y + 0.5f) / height;
                const glm::vec3 dir = DirectionFromEquirect(u, v);

                glm::vec3 color;
                if (dir.y < 0.0f)
                {
                    color = ground * (0.4f - 0.3f * dir.y);
                }
                else
                {
                    const float t = std::pow(1.0f - dir.y, 1.5f);
                    color         = glm::mix(zenith, horizon, t);
                    const float sun =
                        std::pow(std::max(glm::dot(dir, sunDir), 0.0f), 256.0f);
                    color += sunColor * sun;
                }

                const std::size_t i =
                    (static_cast<std::size_t>(y) * width + x) * 4;
                out[i]     = color.r;
                out[i + 1] = color.g;
                out[i + 2] = color.b;
                out[i + 3] = 1.0f;
            }
        }
    }

    bool IBLService::loadHdrFile(const std::string&   path,
                                 std::vector<float>&  out, int& width,
                                 int&                 height) const
    {
        int   components = 0;
        float* pixels =
            stbi_loadf(path.c_str(), &width, &height, &components, 4);
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            return false;
        }

        const std::size_t count =
            static_cast<std::size_t>(width) * height * 4;
        out.assign(pixels, pixels + count);
        stbi_image_free(pixels);
        return true;
    }

    void IBLService::convolveIrradiance(const std::vector<float>& src, int srcW,
                                        int srcH, std::vector<float>& dst,
                                        int dstW, int dstH) const
    {
        dst.assign(static_cast<std::size_t>(dstW) * dstH * 4, 0.0f);

        constexpr int kPhiSamples   = 48;
        constexpr int kThetaSamples = 16;

        for (int y = 0; y < dstH; ++y)
        {
            for (int x = 0; x < dstW; ++x)
            {
                const float     u = (x + 0.5f) / dstW;
                const float     v = (y + 0.5f) / dstH;
                const glm::vec3 N = DirectionFromEquirect(u, v);

                glm::vec3 up =
                    std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                const glm::vec3 tangent   = glm::normalize(glm::cross(up, N));
                const glm::vec3 bitangent = glm::cross(N, tangent);

                glm::vec3 irradiance(0.0f);
                float     weightSum = 0.0f;

                for (int phiI = 0; phiI < kPhiSamples; ++phiI)
                {
                    const float phi =
                        (phiI + 0.5f) / kPhiSamples * 2.0f * kPi;
                    for (int thetaI = 0; thetaI < kThetaSamples; ++thetaI)
                    {
                        const float theta =
                            (thetaI + 0.5f) / kThetaSamples * 0.5f * kPi;
                        const float sinT = std::sin(theta);
                        const float cosT = std::cos(theta);

                        const glm::vec3 tangentSample(
                            std::sin(phi) * cosT, std::cos(phi) * cosT, sinT);
                        const glm::vec3 sampleDir = glm::normalize(
                            tangent * tangentSample.x +
                            bitangent * tangentSample.y + N * tangentSample.z);

                        const glm::vec3 Li =
                            SampleEquirect(src, srcW, srcH, sampleDir);
                        // solid angle weight ~ cos(theta) * sin(theta)
                        const float w = cosT * sinT;
                        irradiance += Li * w;
                        weightSum += w;
                    }
                }

                irradiance = kPi * irradiance / std::max(weightSum, 1e-4f);

                const std::size_t i =
                    (static_cast<std::size_t>(y) * dstW + x) * 4;
                dst[i]     = irradiance.r;
                dst[i + 1] = irradiance.g;
                dst[i + 2] = irradiance.b;
                dst[i + 3] = 1.0f;
            }
        }
    }

    void IBLService::generateBrdfLut(std::vector<float>& out, int size) const
    {
        out.assign(static_cast<std::size_t>(size) * size * 4, 0.0f);
        constexpr std::uint32_t kSampleCount = 128;

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float NdotV =
                    std::max((x + 0.5f) / size, 1e-3f);
                const float roughness = std::max((y + 0.5f) / size, 0.045f);

                const glm::vec3 V(std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
                const glm::vec3 N(0.0f, 0.0f, 1.0f);

                float A = 0.0f;
                float B = 0.0f;

                for (std::uint32_t i = 0; i < kSampleCount; ++i)
                {
                    const glm::vec2 Xi = Hammersley(i, kSampleCount);
                    const glm::vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
                    const glm::vec3 L =
                        glm::normalize(2.0f * glm::dot(V, H) * H - V);

                    const float NdotL = std::max(L.z, 0.0f);
                    const float NdotH = std::max(H.z, 0.0f);
                    const float VdotH = std::max(glm::dot(V, H), 0.0f);

                    if (NdotL > 0.0f)
                    {
                        const float G =
                            GeometrySmith(NdotV, NdotL, roughness);
                        const float G_Vis =
                            (G * VdotH) / std::max(NdotH * NdotV, 1e-4f);
                        const float Fc = std::pow(1.0f - VdotH, 5.0f);
                        A += (1.0f - Fc) * G_Vis;
                        B += Fc * G_Vis;
                    }
                }

                A /= static_cast<float>(kSampleCount);
                B /= static_cast<float>(kSampleCount);

                const std::size_t i =
                    (static_cast<std::size_t>(y) * size + x) * 4;
                out[i]     = A;
                out[i + 1] = B;
                out[i + 2] = 0.0f;
                out[i + 3] = 1.0f;
            }
        }
    }

    skr::Arc<Image> IBLService::uploadFloatRgb(const std::vector<float>& rgba,
                                               int width, int height,
                                               bool generateMips) const
    {
        auto* data = const_cast<float*>(rgba.data());
        auto  builder =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Texture)
                .SetFormat(vk::Format::eR32G32B32A32Sfloat)
                .SetWidth(static_cast<std::uint32_t>(width))
                .SetHeight(static_cast<std::uint32_t>(height))
                .SetChannels(16)
                .SetData(data);

        if (!generateMips)
        {
            builder.SetMipLevels(1);
        }

        return builder.Build();
    }

    void IBLService::buildFromEquirect(const std::vector<float>& src, int width,
                                       int height)
    {
        // Specular IBL uses this map's mip chain as a prefilter stand-in.
        mEnvironment = uploadFloatRgb(src, width, height, true);

        constexpr int kIrrW = 64;
        constexpr int kIrrH = 32;
        std::vector<float> irradiance;
        convolveIrradiance(src, width, height, irradiance, kIrrW, kIrrH);
        mIrradiance = uploadFloatRgb(irradiance, kIrrW, kIrrH, false);

        constexpr int      kLutSize = 256;
        std::vector<float> lut;
        generateBrdfLut(lut, kLutSize);
        mBrdfLut = uploadFloatRgb(lut, kLutSize, kLutSize, false);
    }

} // namespace FREYA_NAMESPACE
