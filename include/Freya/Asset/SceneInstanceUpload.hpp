#pragma once

#include "Freya/Config.hpp"
#include "Freya/Core/Limits.hpp"
#include "Freya/Scene/AssetHandle.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Host upload record (prevModel filled by Renderer).
     *
     * Prefer Scene::Upload for application code. This type is for Advanced /
     * tooling paths that build draws without a retained Scene.
     *
     * Contract: prefer sorting by `entityId` before upload so Freya keeps TAA
     * history stable (`prevModel` is looked up by `entityId`).
     */
    struct SceneInstanceUpload
    {
        glm::mat4      model = glm::mat4(1.0f);
        MeshHandle     mesh {};
        MaterialHandle material {};
        std::uint32_t  entityId    = 0;
        bool           castShadows = true;
        /// Offset into Renderer bone palette; `kNoSkin` = rigid.
        std::uint32_t boneOffset = kNoSkin;
        std::uint32_t boneCount  = 0;
    };

} // namespace FREYA_NAMESPACE
