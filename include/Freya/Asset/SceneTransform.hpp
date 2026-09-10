#pragma once

#include "Freya/Config.hpp"

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Packed world-space TRS (40 B) for Scene upload / ECS memcpy.
     *
     * Layout matches common TransformComponent (position, scale, rotation).
     * GPU SSBO uses the same stride via scalar block layout.
     */
    struct SceneTransform
    {
        glm::vec3 position {};
        glm::vec3 scale { 1.f, 1.f, 1.f };
        glm::quat rotation { 1.f, 0.f, 0.f, 0.f };

        [[nodiscard]] glm::mat4 ToMatrix() const;
        [[nodiscard]] static SceneTransform FromMatrix(const glm::mat4& m);
    };

    static_assert(sizeof(SceneTransform) == 40,
                  "SceneTransform must stay packed at 40 bytes");
    static_assert(offsetof(SceneTransform, scale) == 12,
                  "SceneTransform::scale offset");
    static_assert(offsetof(SceneTransform, rotation) == 24,
                  "SceneTransform::rotation offset");

    /// Alias for the GPU TRS SSBO (scalar layout, same bytes as SceneTransform).
    using GpuSceneTransform = SceneTransform;

} // namespace FREYA_NAMESPACE
