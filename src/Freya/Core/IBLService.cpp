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
            float           u = std::atan2(n.z, n.x) / (2.0f * kPi) + 0.5f;
            float           v = std::acos(std::clamp(n.y, -1.0f, 1.0f)) / kPi;
            return { u, v };
        }

        glm::vec3 SampleEquirect(const std::vector<float>& data, int width,
                                 int height, const glm::vec3& dir)
        {
            const glm::vec2 uv = EquirectFromDirection(dir);
            const float     x  = uv.x * static_cast<float>(width - 1);
            const float     y  = uv.y * static_cast<float>(height - 1);
            const int       x0 = static_cast<int>(x);
            const int       y0 = static_cast<int>(y);
            const int       x1 = std::min(x0 + 1, width - 1);
            const int       y1 = std::min(y0 + 1, height - 1);
            const float     fx = x - static_cast<float>(x0);
            const float     fy = y - static_cast<float>(y0);

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
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
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
            const float a   = roughness * roughness;
            const float phi = 2.0f * kPi * Xi.x;
            const float cosTheta =
                std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
            const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

            const glm::vec3 H(std::cos(phi) * sinTheta,
                              std::sin(phi) * sinTheta, cosTheta);

            const glm::vec3 up = std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1)
                                                        : glm::vec3(1, 0, 0);
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

    IBLService::IBLService(
        const skr::Arc<Device>&               device,
        const skr::Arc<skr::ServiceProvider>& serviceProvider,
        const skr::Arc<FreyaOptions>&         options) :
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
            // Cap resolution for upload / CPU convolution cost (4k → 1k).
            downsampleEquirect(env, width, height, 1024);
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
        mDevice->Get().waitIdle();
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
        if (mLtcSampler)
        {
            vkDevice.destroySampler(mLtcSampler);
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
        mLtcSampler         = makeSampler(false);
    }

    void IBLService::generateLtcLuts(std::vector<float>& ltc1,
                                     std::vector<float>& ltc2, int size) const
    {
        // Parametric GGX→LTC approximation (inverse matrix packing matches
        // LearnOpenGL / Heitz layout). Sufficient for soft rect area lights;
        // can be swapped for tabulated Heitz fits later.
        ltc1.assign(static_cast<std::size_t>(size) * size * 4, 0.0f);
        ltc2.assign(static_cast<std::size_t>(size) * size * 4, 0.0f);

        for (int y = 0; y < size; ++y)
        {
            const float roughness =
                std::max((y + 0.5f) / static_cast<float>(size), 1e-3f);
            const float alpha  = std::max(roughness * roughness, 1e-4f);
            const float invA   = 1.0f / alpha;
            const float invASq = invA * invA;

            for (int x = 0; x < size; ++x)
            {
                // Sample as in the shader: uv = (roughness, sqrt(1-NdotV))
                const float ut    = (x + 0.5f) / static_cast<float>(size);
                const float NdotV = std::clamp(1.0f - ut * ut, 0.0f, 1.0f);
                const float theta = std::acos(NdotV);
                const float sinT  = std::sin(theta);

                // Stretch lobe with roughness; slight view-dependent skew.
                const float m00 =
                    std::clamp(invA + (1.0f - NdotV) * invA * 0.35f,
                               1.0f,
                               100.0f);
                const float m20 = sinT * (1.0f - alpha) * 0.15f;
                const float m02 = -m20 * 0.5f;
                const float m22 =
                    std::clamp(1.0f + (1.0f - alpha) * NdotV, 0.2f, 4.0f);

                const std::size_t i =
                    (static_cast<std::size_t>(y) * size + x) * 4;
                ltc1[i + 0] = m00;
                ltc1[i + 1] = m20;
                ltc1[i + 2] = m02;
                ltc1[i + 3] = m22;

                // Amplitude / fresnel helpers + horizon form-factor scale.
                const float fresnelBias = std::pow(1.0f - NdotV, 5.0f);
                ltc2[i + 0]             = 1.0f - fresnelBias * 0.5f;
                ltc2[i + 1]             = std::max(NdotV, 0.05f);
                ltc2[i + 2]             = invASq * 0.01f;
                ltc2[i + 3]             = 1.0f;
            }
        }
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

    bool IBLService::loadHdrFile(const std::string&  path,
                                 std::vector<float>& out, int& width,
                                 int& height) const
    {
        int    components = 0;
        float* pixels =
            stbi_loadf(path.c_str(), &width, &height, &components, 4);
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            return false;
        }

        const std::size_t count = static_cast<std::size_t>(width) * height * 4;
        out.assign(pixels, pixels + count);
        stbi_image_free(pixels);
        return true;
    }

    void IBLService::downsampleEquirect(std::vector<float>& data, int& width,
                                        int& height, int maxWidth) const
    {
        if (width <= maxWidth)
        {
            return;
        }

        const int          dstW = maxWidth;
        const int          dstH = std::max(1, (height * dstW) / width);
        std::vector<float> dst(static_cast<std::size_t>(dstW) * dstH * 4);

        for (int y = 0; y < dstH; ++y)
        {
            for (int x = 0; x < dstW; ++x)
            {
                const float u  = (x + 0.5f) / dstW;
                const float v  = (y + 0.5f) / dstH;
                const float sx = u * static_cast<float>(width - 1);
                const float sy = v * static_cast<float>(height - 1);
                const int   x0 = static_cast<int>(sx);
                const int   y0 = static_cast<int>(sy);
                const int   x1 = std::min(x0 + 1, width - 1);
                const int   y1 = std::min(y0 + 1, height - 1);
                const float fx = sx - static_cast<float>(x0);
                const float fy = sy - static_cast<float>(y0);

                auto fetch = [&](int px, int py) {
                    const std::size_t i =
                        (static_cast<std::size_t>(py) * width + px) * 4;
                    return glm::vec4(data[i], data[i + 1], data[i + 2],
                                     data[i + 3]);
                };

                const glm::vec4 c =
                    glm::mix(glm::mix(fetch(x0, y0), fetch(x1, y0), fx),
                             glm::mix(fetch(x0, y1), fetch(x1, y1), fx),
                             fy);
                const std::size_t i =
                    (static_cast<std::size_t>(y) * dstW + x) * 4;
                dst[i]     = c.r;
                dst[i + 1] = c.g;
                dst[i + 2] = c.b;
                dst[i + 3] = c.a;
            }
        }

        data   = std::move(dst);
        width  = dstW;
        height = dstH;
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

                glm::vec3 up = std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1)
                                                      : glm::vec3(1, 0, 0);
                const glm::vec3 tangent   = glm::normalize(glm::cross(up, N));
                const glm::vec3 bitangent = glm::cross(N, tangent);

                glm::vec3 irradiance(0.0f);
                float     weightSum = 0.0f;

                for (int phiI = 0; phiI < kPhiSamples; ++phiI)
                {
                    const float phi = (phiI + 0.5f) / kPhiSamples * 2.0f * kPi;
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
                const float NdotV     = std::max((x + 0.5f) / size, 1e-3f);
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
                        const float G = GeometrySmith(NdotV, NdotL, roughness);
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

    void IBLService::prefilterSpecular(const std::vector<float>& src, int srcW,
                                       int srcH, std::vector<float>& packed,
                                       int& outWidth, int& outHeight,
                                       int& mipCount) const
    {
        // Cap bake resolution so CPU GGX stays interactive at startup.
        std::vector<float> env    = src;
        int                width  = srcW;
        int                height = srcH;
        downsampleEquirect(env, width, height, 512);

        outWidth  = width;
        outHeight = height;

        mipCount =
            static_cast<int>(std::floor(std::log2(std::max(width, height)))) +
            1;
        mipCount = std::max(mipCount, 1);

        std::size_t totalFloats = 0;
        for (int mip = 0; mip < mipCount; ++mip)
        {
            const int mipW = std::max(1, width >> mip);
            const int mipH = std::max(1, height >> mip);
            totalFloats += static_cast<std::size_t>(mipW) * mipH * 4;
        }
        packed.assign(totalFloats, 0.0f);

        constexpr std::uint32_t kSampleCount = 64;

        std::size_t writeOffset = 0;
        for (int mip = 0; mip < mipCount; ++mip)
        {
            const int mipW = std::max(1, width >> mip);
            const int mipH = std::max(1, height >> mip);

            if (mip == 0)
            {
                // Sharp specular: copy source texels (already ≤512w).
                const std::size_t count =
                    static_cast<std::size_t>(mipW) * mipH * 4;
                std::copy_n(env.data(), count, packed.begin() + writeOffset);
                writeOffset += count;
                continue;
            }

            const float roughness =
                static_cast<float>(mip) / static_cast<float>(mipCount - 1);

            for (int y = 0; y < mipH; ++y)
            {
                for (int x = 0; x < mipW; ++x)
                {
                    const float u = (x + 0.5f) / static_cast<float>(mipW);
                    const float v = (y + 0.5f) / static_cast<float>(mipH);
                    // Split-sum: R = N = V for the prefilter lobe.
                    const glm::vec3 N = DirectionFromEquirect(u, v);
                    const glm::vec3 V = N;

                    glm::vec3 color(0.0f);
                    float     weightSum = 0.0f;

                    for (std::uint32_t i = 0; i < kSampleCount; ++i)
                    {
                        const glm::vec2 Xi = Hammersley(i, kSampleCount);
                        const glm::vec3 H =
                            ImportanceSampleGGX(Xi, N, roughness);
                        const glm::vec3 L =
                            glm::normalize(2.0f * glm::dot(V, H) * H - V);
                        const float NdotL = std::max(glm::dot(N, L), 0.0f);
                        if (NdotL <= 0.0f)
                        {
                            continue;
                        }

                        color += SampleEquirect(env, width, height, L) * NdotL;
                        weightSum += NdotL;
                    }

                    if (weightSum > 1e-4f)
                    {
                        color /= weightSum;
                    }

                    packed[writeOffset++] = color.r;
                    packed[writeOffset++] = color.g;
                    packed[writeOffset++] = color.b;
                    packed[writeOffset++] = 1.0f;
                }
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

    skr::Arc<Image> IBLService::uploadFloatRgbMipChain(
        const std::vector<float>& packed, int width, int height,
        int mipCount) const
    {
        auto* data = const_cast<float*>(packed.data());
        return mServiceProvider->GetService<ImageBuilder>()
            ->SetUsage(ImageUsage::Texture)
            .SetFormat(vk::Format::eR32G32B32A32Sfloat)
            .SetWidth(static_cast<std::uint32_t>(width))
            .SetHeight(static_cast<std::uint32_t>(height))
            .SetChannels(16)
            .SetMipLevels(static_cast<std::uint32_t>(std::max(mipCount, 1)))
            .SetUploadCustomMipChain(true)
            .SetData(data)
            .Build();
    }

    void IBLService::buildFromEquirect(const std::vector<float>& src, int width,
                                       int height)
    {
        std::vector<float> prefiltered;
        int                envW     = width;
        int                envH     = height;
        int                mipCount = 1;
        prefilterSpecular(src, width, height, prefiltered, envW, envH,
                          mipCount);
        mEnvironment =
            uploadFloatRgbMipChain(prefiltered, envW, envH, mipCount);

        constexpr int      kIrrW = 64;
        constexpr int      kIrrH = 32;
        std::vector<float> irradiance;
        convolveIrradiance(src, width, height, irradiance, kIrrW, kIrrH);
        mIrradiance = uploadFloatRgb(irradiance, kIrrW, kIrrH, false);

        constexpr int      kLutSize = 256;
        std::vector<float> lut;
        generateBrdfLut(lut, kLutSize);
        mBrdfLut = uploadFloatRgb(lut, kLutSize, kLutSize, false);

        constexpr int      kLtcSize = 64;
        std::vector<float> ltc1;
        std::vector<float> ltc2;
        generateLtcLuts(ltc1, ltc2, kLtcSize);
        mLtcMatrix = uploadFloatRgb(ltc1, kLtcSize, kLtcSize, false);
        mLtcAmpl   = uploadFloatRgb(ltc2, kLtcSize, kLtcSize, false);
    }

} // namespace FREYA_NAMESPACE
