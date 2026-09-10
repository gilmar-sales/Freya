#include "Freya/Asset/SceneTransform.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace FREYA_NAMESPACE
{
    glm::mat4 SceneTransform::ToMatrix() const
    {
        return glm::translate(glm::mat4(1.f), position) *
               glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.f), scale);
    }

    SceneTransform SceneTransform::FromMatrix(const glm::mat4& m)
    {
        SceneTransform t;
        t.position = glm::vec3(m[3]);
        t.scale    = glm::vec3(glm::length(glm::vec3(m[0])),
                               glm::length(glm::vec3(m[1])),
                               glm::length(glm::vec3(m[2])));
        const glm::mat3 rotMat(glm::vec3(m[0]) / std::max(t.scale.x, 1e-8f),
                               glm::vec3(m[1]) / std::max(t.scale.y, 1e-8f),
                               glm::vec3(m[2]) / std::max(t.scale.z, 1e-8f));
        t.rotation = glm::normalize(glm::quat_cast(rotMat));
        return t;
    }

} // namespace FREYA_NAMESPACE
