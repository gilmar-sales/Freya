#include "Freya/Core/IBLService.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Vendor/stb_image.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numbers>
#include <string>

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

        // Disk cache for CPU IBL bakes. Bump kIblCacheVersion when sample
        // counts, resolutions, or packing change.
        constexpr std::uint32_t kIblCacheVersion   = 1;
        constexpr int           kSrcDownsampleMaxW = 1024;
        constexpr int           kEnvPrefilterMaxW  = 512;
        constexpr int           kIrrW              = 64;
        constexpr int           kIrrH              = 32;
        constexpr int           kLutSize           = 256;
        constexpr const char*   kIblCacheDir =
            "./Resources/Environments/.ibl_cache";
        constexpr char kFloatMapMagic[4]  = { 'F', 'M', 'A', 'P' };
        constexpr char kEnvBundleMagic[4] = { 'F', 'I', 'B', 'E' };

        struct FloatMapHeader
        {
            char          magic[4];
            std::uint32_t version;
            std::uint32_t width;
            std::uint32_t height;
            std::uint32_t mipCount;
            std::uint32_t floatCount;
        };

        struct EnvBundleHeader
        {
            char          magic[4];
            std::uint32_t version;
            std::uint32_t envWidth;
            std::uint32_t envHeight;
            std::uint32_t mipCount;
            std::uint32_t envFloatCount;
            std::uint32_t irrWidth;
            std::uint32_t irrHeight;
            std::uint32_t irrFloatCount;
        };

        std::size_t MipChainFloatCount(int width, int height, int mipCount)
        {
            std::size_t total = 0;
            for (int mip = 0; mip < mipCount; ++mip)
            {
                const int mipW = std::max(1, width >> mip);
                const int mipH = std::max(1, height >> mip);
                total += static_cast<std::size_t>(mipW) * mipH * 4;
            }
            return total;
        }

        std::string SanitizeCacheToken(std::string token)
        {
            for (char& c : token)
            {
                const auto uc = static_cast<unsigned char>(c);
                if (!std::isalnum(uc) && c != '-' && c != '_')
                {
                    c = '_';
                }
            }
            return token;
        }

        std::filesystem::path CacheFilePath(const std::string& fileName)
        {
            return std::filesystem::path(kIblCacheDir) / fileName;
        }

        bool EnsureCacheDir()
        {
            std::error_code ec;
            std::filesystem::create_directories(kIblCacheDir, ec);
            return !ec;
        }

        std::string BrdfCacheFileName()
        {
            return "brdf_v" + std::to_string(kIblCacheVersion) + "_" +
                   std::to_string(kLutSize) + "_s128.fmap";
        }

        std::string EnvCacheFileName(const std::string& keyToken)
        {
            return "env_v" + std::to_string(kIblCacheVersion) + "_" +
                   SanitizeCacheToken(keyToken) + ".fibe";
        }

        std::string MakeHdrCacheToken(const std::string& path)
        {
            namespace fs = std::filesystem;
            std::error_code ec;
            const auto      fileSize = fs::file_size(path, ec);
            if (ec)
            {
                return {};
            }
            const auto mtime = fs::last_write_time(path, ec);
            if (ec)
            {
                return {};
            }
            const auto mtimeCount =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    mtime.time_since_epoch())
                    .count();
            return fs::path(path).stem().string() + "_" +
                   std::to_string(fileSize) + "_" + std::to_string(mtimeCount);
        }

        bool WriteAtomic(const std::filesystem::path&              path,
                         const std::function<bool(std::ostream&)>& writeBody)
        {
            if (!EnsureCacheDir())
            {
                return false;
            }

            const auto tmpPath = path.string() + ".tmp";
            {
                std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
                if (!out || !writeBody(out) || !out.flush())
                {
                    std::error_code ec;
                    std::filesystem::remove(tmpPath, ec);
                    return false;
                }
            }

            std::error_code ec;
            std::filesystem::rename(tmpPath, path, ec);
            if (ec)
            {
                std::filesystem::remove(path, ec);
                std::filesystem::rename(tmpPath, path, ec);
            }
            if (ec)
            {
                std::filesystem::remove(tmpPath, ec);
                return false;
            }
            return true;
        }

        bool LoadFloatMap(const std::filesystem::path& path,
                          std::vector<float>& out, int expectedW, int expectedH)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                return false;
            }

            FloatMapHeader header {};
            in.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!in || std::memcmp(header.magic, kFloatMapMagic, 4) != 0 ||
                header.version != kIblCacheVersion ||
                header.width != static_cast<std::uint32_t>(expectedW) ||
                header.height != static_cast<std::uint32_t>(expectedH) ||
                header.mipCount != 1)
            {
                return false;
            }

            const std::size_t expected =
                static_cast<std::size_t>(expectedW) * expectedH * 4;
            if (header.floatCount != expected)
            {
                return false;
            }

            out.resize(expected);
            in.read(reinterpret_cast<char*>(out.data()),
                    static_cast<std::streamsize>(expected * sizeof(float)));
            return static_cast<bool>(in);
        }

        bool SaveFloatMap(const std::filesystem::path& path,
                          const std::vector<float>& data, int width, int height)
        {
            const std::size_t expected =
                static_cast<std::size_t>(width) * height * 4;
            if (data.size() != expected)
            {
                return false;
            }

            FloatMapHeader header {};
            std::memcpy(header.magic, kFloatMapMagic, 4);
            header.version    = kIblCacheVersion;
            header.width      = static_cast<std::uint32_t>(width);
            header.height     = static_cast<std::uint32_t>(height);
            header.mipCount   = 1;
            header.floatCount = static_cast<std::uint32_t>(expected);

            return WriteAtomic(path, [&](std::ostream& out) {
                out.write(reinterpret_cast<const char*>(&header),
                          sizeof(header));
                out.write(
                    reinterpret_cast<const char*>(data.data()),
                    static_cast<std::streamsize>(expected * sizeof(float)));
                return static_cast<bool>(out);
            });
        }

        bool LoadEnvBundle(const std::filesystem::path& path,
                           std::vector<float>& envOut, int& envW, int& envH,
                           int& mipCount, std::vector<float>& irrOut)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                return false;
            }

            EnvBundleHeader header {};
            in.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!in || std::memcmp(header.magic, kEnvBundleMagic, 4) != 0 ||
                header.version != kIblCacheVersion ||
                header.irrWidth != static_cast<std::uint32_t>(kIrrW) ||
                header.irrHeight != static_cast<std::uint32_t>(kIrrH) ||
                header.mipCount == 0)
            {
                return false;
            }

            const auto envExpected = MipChainFloatCount(
                static_cast<int>(header.envWidth),
                static_cast<int>(header.envHeight),
                static_cast<int>(header.mipCount));
            const auto irrExpected =
                static_cast<std::size_t>(kIrrW) * kIrrH * 4;
            if (header.envFloatCount != envExpected ||
                header.irrFloatCount != irrExpected)
            {
                return false;
            }

            envOut.resize(envExpected);
            irrOut.resize(irrExpected);
            in.read(reinterpret_cast<char*>(envOut.data()),
                    static_cast<std::streamsize>(envExpected * sizeof(float)));
            in.read(reinterpret_cast<char*>(irrOut.data()),
                    static_cast<std::streamsize>(irrExpected * sizeof(float)));
            if (!in)
            {
                return false;
            }

            envW     = static_cast<int>(header.envWidth);
            envH     = static_cast<int>(header.envHeight);
            mipCount = static_cast<int>(header.mipCount);
            return true;
        }

        bool SaveEnvBundle(const std::filesystem::path& path,
                           const std::vector<float>& envData, int envW,
                           int envH, int mipCount,
                           const std::vector<float>& irrData)
        {
            const auto envExpected = MipChainFloatCount(envW, envH, mipCount);
            const auto irrExpected =
                static_cast<std::size_t>(kIrrW) * kIrrH * 4;
            if (envData.size() != envExpected || irrData.size() != irrExpected)
            {
                return false;
            }

            EnvBundleHeader header {};
            std::memcpy(header.magic, kEnvBundleMagic, 4);
            header.version       = kIblCacheVersion;
            header.envWidth      = static_cast<std::uint32_t>(envW);
            header.envHeight     = static_cast<std::uint32_t>(envH);
            header.mipCount      = static_cast<std::uint32_t>(mipCount);
            header.envFloatCount = static_cast<std::uint32_t>(envExpected);
            header.irrWidth      = static_cast<std::uint32_t>(kIrrW);
            header.irrHeight     = static_cast<std::uint32_t>(kIrrH);
            header.irrFloatCount = static_cast<std::uint32_t>(irrExpected);

            return WriteAtomic(path, [&](std::ostream& out) {
                out.write(reinterpret_cast<const char*>(&header),
                          sizeof(header));
                out.write(
                    reinterpret_cast<const char*>(envData.data()),
                    static_cast<std::streamsize>(envExpected * sizeof(float)));
                out.write(
                    reinterpret_cast<const char*>(irrData.data()),
                    static_cast<std::streamsize>(irrExpected * sizeof(float)));
                return static_cast<bool>(out);
            });
        }
    } // namespace

    IBLService::IBLService(
        const skr::Arc<Device>&                  device,
        const skr::Arc<skr::ServiceProvider>&    serviceProvider,
        const skr::Arc<FreyaOptions>&            options,
        const skr::Arc<skr::Logger<IBLService>>& logger) :
        mDevice(device), mServiceProvider(serviceProvider), mLogger(logger),
        mIntensity(options->iblIntensity)
    {
        mLogger->LogTrace("Building 'fra::IBLService':");
        buildFromEquirect(options->environmentMapPath);
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
        downsampleEquirect(env, width, height, kEnvPrefilterMaxW);

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

    void IBLService::buildFromEquirect(const std::string& environmentMapPath)
    {
        std::string envKey = "procedural_v1";
        if (!environmentMapPath.empty())
        {
            namespace fs = std::filesystem;
            std::error_code ec;
            if (fs::is_regular_file(environmentMapPath, ec))
            {
                const auto token = MakeHdrCacheToken(environmentMapPath);
                if (!token.empty())
                {
                    envKey = token;
                }
            }
        }

        mLogger->LogTrace("\tBuilding IBL (cache key '{}')", envKey);

        std::vector<float> prefiltered;
        std::vector<float> irradiance;
        int                envW     = 0;
        int                envH     = 0;
        int                mipCount = 1;
        const auto         envPath  = CacheFilePath(EnvCacheFileName(envKey));

        if (LoadEnvBundle(envPath, prefiltered, envW, envH, mipCount,
                          irradiance))
        {
            mLogger->LogTrace("\tEnv cache hit {}x{} mips={}", envW, envH,
                              mipCount);
        }
        else
        {
            std::vector<float> src;
            int                width  = 512;
            int                height = 256;

            if (!environmentMapPath.empty())
            {
                mLogger->LogTrace("\tLoading HDR: {}", environmentMapPath);
                if (loadHdrFile(environmentMapPath, src, width, height))
                {
                    mLogger->LogTrace("\tLoaded HDR {}x{}", width, height);
                    downsampleEquirect(src, width, height, kSrcDownsampleMaxW);
                    mLogger->LogTrace("\tDownsampled equirect to {}x{}", width,
                                      height);
                    const auto token = MakeHdrCacheToken(environmentMapPath);
                    if (!token.empty())
                    {
                        envKey = token;
                    }
                }
                else
                {
                    envKey = "procedural_v1";
                    mLogger->LogTrace(
                        "\tHDR load failed; generating procedural sky");
                    generateProceduralSky(src, width, height);
                }
            }
            else
            {
                mLogger->LogTrace("\tGenerating procedural sky");
                generateProceduralSky(src, width, height);
            }

            mLogger->LogTrace("\tEnv cache miss; baking from equirect {}x{}",
                              width, height);
            mLogger->LogTrace("\tPrefiltering specular environment");
            prefilterSpecular(src, width, height, prefiltered, envW, envH,
                              mipCount);
            mLogger->LogTrace("\tConvolving irradiance {}x{}", kIrrW, kIrrH);
            convolveIrradiance(src, width, height, irradiance, kIrrW, kIrrH);

            const auto writePath = CacheFilePath(EnvCacheFileName(envKey));
            if (SaveEnvBundle(writePath, prefiltered, envW, envH, mipCount,
                              irradiance))
            {
                mLogger->LogTrace("\tWrote env cache {}", writePath.string());
            }
            else
            {
                mLogger->LogTrace("\tFailed to write env cache {}",
                                  writePath.string());
            }
        }

        mLogger->LogTrace("\tUploading environment {}x{} mips={}", envW, envH,
                          mipCount);
        mEnvironment =
            uploadFloatRgbMipChain(prefiltered, envW, envH, mipCount);
        mLogger->LogTrace("\tUploading irradiance");
        mIrradiance = uploadFloatRgb(irradiance, kIrrW, kIrrH, false);

        std::vector<float> lut;
        const auto         brdfPath = CacheFilePath(BrdfCacheFileName());
        if (LoadFloatMap(brdfPath, lut, kLutSize, kLutSize))
        {
            mLogger->LogTrace("\tBRDF cache hit {}", brdfPath.string());
        }
        else
        {
            mLogger->LogTrace("\tGenerating BRDF LUT {}x{}", kLutSize,
                              kLutSize);
            generateBrdfLut(lut, kLutSize);
            if (SaveFloatMap(brdfPath, lut, kLutSize, kLutSize))
            {
                mLogger->LogTrace("\tWrote BRDF cache {}", brdfPath.string());
            }
            else
            {
                mLogger->LogTrace("\tFailed to write BRDF cache {}",
                                  brdfPath.string());
            }
        }
        mLogger->LogTrace("\tUploading BRDF LUT");
        mBrdfLut = uploadFloatRgb(lut, kLutSize, kLutSize, false);

        constexpr int      kLtcSize = 64;
        std::vector<float> ltc1;
        std::vector<float> ltc2;
        mLogger->LogTrace("\tGenerating LTC LUTs {}x{}", kLtcSize, kLtcSize);
        generateLtcLuts(ltc1, ltc2, kLtcSize);
        mLogger->LogTrace("\tUploading LTC LUTs");
        mLtcMatrix = uploadFloatRgb(ltc1, kLtcSize, kLtcSize, false);
        mLtcAmpl   = uploadFloatRgb(ltc2, kLtcSize, kLtcSize, false);
    }

} // namespace FREYA_NAMESPACE
