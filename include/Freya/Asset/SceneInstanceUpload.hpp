#pragma once

#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Config.hpp"

#include <cstdint>
#include <limits>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    constexpr std::uint32_t kPickMissId = 0xFFFFFFFFu;

    /**
     * @brief Host upload record (prevModel filled by Renderer).
     *
     * Contract: prefer sorting by `entityId` before upload so Freya keeps TAA
     * history stable (`prevModel` is looked up by `entityId`).
     */
    struct SceneInstanceUpload
    {
        glm::mat4     model       = glm::mat4(1.0f);
        std::uint32_t meshId      = 0;
        std::uint32_t materialId  = 0;
        std::uint32_t entityId    = 0;
        bool          castShadows = true;
        /// Offset into Renderer bone palette; `kNoSkin` = rigid.
        std::uint32_t boneOffset = kNoSkin;
        std::uint32_t boneCount  = 0;
    };

} // namespace FREYA_NAMESPACE
