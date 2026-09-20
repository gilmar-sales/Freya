#pragma once

#include "Freya/Core/Limits.hpp"
#include "Freya/Core/SpinLock.hpp"

#include <Skirnir/Skirnir.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct LightServiceGpu;

    /**
     * @brief Opaque light id (slot in LightService). Default is null.
     */
    class LightHandle
    {
      public:
        constexpr LightHandle() = default;

        constexpr explicit LightHandle(std::uint32_t index) :
            mIndex(index), mValid(true)
        {
        }

        [[nodiscard]] constexpr std::uint32_t Index() const { return mIndex; }

        [[nodiscard]] constexpr bool IsValid() const { return mValid; }

        constexpr explicit operator bool() const { return IsValid(); }

        constexpr auto operator<=>(const LightHandle&) const = default;

      private:
        std::uint32_t mIndex = 0;
        bool          mValid = false;
    };

    /**
     * @brief Light source data for the lighting system (host API).
     *
     * GPU UBO packing converts @c type to float; apps use LightType.
     */
    struct Light
    {
        glm::vec3 position    = glm::vec3(0.0f);
        LightType type        = LightType::Point;
        glm::vec3 color       = glm::vec3(1.0f);
        float     radius      = 10.0f;
        glm::vec3 direction   = glm::vec3(0.0f, -1.0f, 0.0f);
        float     innerCutoff = 0.9f;
        float     outerCutoff = 0.8f;
        float     intensity   = 1.0f;
        glm::vec3 tangent     = glm::vec3(1.0f, 0.0f, 0.0f);
        float     halfHeight  = 0.0f;
        bool      castShadows = true;
    };

    /**
     * @brief Host staging record for concurrent light updates.
     *
     * BeginLightUploads → Reserve → UploadLightUploads (any thread) →
     * EndLightUploads. Valid @c handle applies as UpdateLight at End.
     */
    struct LightUpload
    {
        LightHandle handle {};
        Light       light {};
    };

    inline Light MakePointLight(const glm::vec3& position,
                                const glm::vec3& color,
                                float            radius,
                                float            intensity = 1.0f)
    {
        Light light {};
        light.position  = position;
        light.type      = LightType::Point;
        light.color     = color;
        light.radius    = radius;
        light.intensity = intensity;
        return light;
    }

    inline Light MakeDirectionalLight(const glm::vec3& direction,
                                      const glm::vec3& color,
                                      float            intensity = 1.0f)
    {
        Light light {};
        light.type      = LightType::Directional;
        light.color     = color;
        light.direction = glm::normalize(direction);
        light.intensity = intensity;
        return light;
    }

    inline Light MakeSpotLight(const glm::vec3& position,
                               const glm::vec3& direction,
                               const glm::vec3& color,
                               float            radius,
                               float            innerAngleRad,
                               float            outerAngleRad,
                               float            intensity = 1.0f)
    {
        Light light {};
        light.position    = position;
        light.type        = LightType::Spot;
        light.color       = color;
        light.radius      = radius;
        light.direction   = glm::normalize(direction);
        light.innerCutoff = std::cos(innerAngleRad);
        light.outerCutoff = std::cos(outerAngleRad);
        light.intensity   = intensity;
        return light;
    }

    inline Light MakeAreaLight(const glm::vec3& center,
                               const glm::vec3& normal,
                               const glm::vec3& tangent,
                               float            halfWidth,
                               float            halfHeight,
                               const glm::vec3& color,
                               float            intensity = 1.0f)
    {
        Light light {};
        light.position    = center;
        light.type        = LightType::Area;
        light.color       = color;
        light.direction   = glm::normalize(normal);
        light.intensity   = intensity;
        light.outerCutoff = std::max(halfWidth, 1e-4f);
        light.halfHeight  = std::max(halfHeight, 1e-4f);

        auto T = tangent - light.direction * glm::dot(tangent, light.direction);
        if (glm::dot(T, T) < 1e-8f)
        {
            const glm::vec3 up = (std::abs(light.direction.y) < 0.99f)
                                     ? glm::vec3(0.0f, 1.0f, 0.0f)
                                     : glm::vec3(1.0f, 0.0f, 0.0f);
            T                  = glm::cross(up, light.direction);
        }
        light.tangent = glm::normalize(T);
        return light;
    }

    class LightService
    {
      public:
        struct Impl;

        LightService(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~LightService();

        LightService(const LightService&)            = delete;
        LightService& operator=(const LightService&) = delete;
        LightService(LightService&&) noexcept;
        LightService& operator=(LightService&&) noexcept;

        /**
         * @brief Adds a light. Returns a null handle when the pool is full.
         */
        LightHandle AddLight(const Light& light);
        /**
         * @brief Removes a light. Other LightHandles stay valid (free-list
         * slots); only @p handle becomes inert.
         */
        void RemoveLight(LightHandle handle);
        void UpdateLightPosition(LightHandle handle, const glm::vec3& position);
        void UpdateLight(LightHandle handle, const Light& light);
        const Light* GetLight(LightHandle handle) const;
        void         ClearLights();

        /**
         * @brief Per-frame cumulative light updates (thread-safe Upload).
         *
         * Begin → optional Reserve → Upload(span)* from any threads → End.
         * End applies to the host pool; GPU pack remains in Update().
         */
        void BeginLightUploads();
        void ReserveLightUploads(std::uint32_t count);
        void UploadLightUploads(std::span<const LightUpload> uploads);
        void EndLightUploads();

        void Update(std::uint32_t    frameIndex,
                    const glm::vec3& viewPosition,
                    const glm::vec3& cameraForward);

        [[nodiscard]] std::uint32_t GetLightCount() const;
        [[nodiscard]] std::uint32_t GetMaxLights() const;
        [[nodiscard]] bool          HasLights() const;

        void               SetIblIntensity(float intensity);
        float              GetIblIntensity() const;
        void               SetExposure(float exposure);
        float              GetExposure() const;
        void               SetShadowsEnabled(bool enabled);
        [[nodiscard]] bool GetShadowsEnabled() const;

        /**
         * @brief Mute/unmute all lights of @p type at GPU pack / shadow
         * gather time. Host Light records are unchanged (apps can still
         * UpdateLight while muted).
         */
        void               SetLightTypeEnabled(LightType type, bool enabled);
        [[nodiscard]] bool IsLightTypeEnabled(LightType type) const;

      private:
        friend struct LightServiceGpu;

        void applyLightUpdate(LightHandle handle, const Light& light);
        void enqueueOrApplyUpdate(LightHandle handle, const Light& light);

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
