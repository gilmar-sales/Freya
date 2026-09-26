#include "Freya/Core/LightService.hpp"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    Light MakePointLight(const glm::vec3& position, const glm::vec3& color,
                         float radius, float intensity)
    {
        Light light {};
        light.position  = position;
        light.type      = LightType::Point;
        light.color     = color;
        light.radius    = radius;
        light.intensity = intensity;
        return light;
    }

    Light MakeDirectionalLight(const glm::vec3& direction,
                               const glm::vec3& color, float intensity)
    {
        Light light {};
        light.type      = LightType::Directional;
        light.color     = color;
        light.direction = glm::normalize(direction);
        light.intensity = intensity;
        return light;
    }

    Light MakeSpotLight(const glm::vec3& position, const glm::vec3& direction,
                        const glm::vec3& color, float radius,
                        float innerAngleRad, float outerAngleRad,
                        float intensity)
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

    Light MakeAreaLight(const glm::vec3& center, const glm::vec3& normal,
                        const glm::vec3& tangent, float halfWidth,
                        float halfHeight, const glm::vec3& color,
                        float intensity)
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
} // namespace FREYA_NAMESPACE
