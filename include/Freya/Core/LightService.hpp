#pragma once

#include "Freya/Core/Limits.hpp"

#include <Skirnir/Skirnir.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Device;
    class Buffer;
    struct LightServiceGpu;

    /**
     * @brief Light source data structure for the lighting system.
     */
    struct Light
    {
        glm::vec3 position    = glm::vec3(0.0f);
        float     type        = 0.0f;
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

    inline Light MakePointLight(const glm::vec3& position,
                                const glm::vec3& color,
                                float            radius,
                                float            intensity = 1.0f)
    {
        Light light {};
        light.position  = position;
        light.type      = static_cast<float>(LightType::Point);
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
        light.type      = static_cast<float>(LightType::Directional);
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
        light.type        = static_cast<float>(LightType::Spot);
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
        light.type        = static_cast<float>(LightType::Area);
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

        LightService(const skr::Arc<Device>& device,
                     std::uint32_t           frameCount,
                     std::uint32_t           maxLights = kMaxLights);

        ~LightService();

        LightService(const LightService&)            = delete;
        LightService& operator=(const LightService&) = delete;
        LightService(LightService&&) noexcept;
        LightService& operator=(LightService&&) noexcept;

        std::int32_t AddLight(const Light& light);
        void         RemoveLight(std::uint32_t index);
        void         UpdateLightPosition(std::uint32_t    index,
                                         const glm::vec3& position);
        void         UpdateLight(std::uint32_t index, const Light& light);
        const Light* GetLight(std::uint32_t index) const;
        void         ClearLights();

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

      private:
        friend struct LightServiceGpu;

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
