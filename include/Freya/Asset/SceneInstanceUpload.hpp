#pragma once

#include "Freya/Asset/SceneTransform.hpp"
#include "Freya/Config.hpp"
#include "Freya/Core/Limits.hpp"
#include "Freya/Scene/AssetHandle.hpp"

#include <cstdint>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Host upload record (model mats filled on GPU by ExpandTransforms).
     *
     * Prefer Scene::Upload or Renderer Begin/Reserve/Upload/End for app code.
     * This type is for packing ECS chunks / Advanced tooling.
     *
     * Contract: prefer sorting by `entityId` before End so Freya keeps TAA
     * history stable (`prevModel` is matched by entityId on the GPU).
     */
    struct SceneInstanceUpload
    {
        SceneTransform transform {};
        MeshHandle     mesh {};
        MaterialHandle material {};
        std::uint32_t  entityId    = 0;
        bool           castShadows = true;
        /// Offset into Renderer bone palette; `kNoSkin` = rigid.
        std::uint32_t boneOffset = kNoSkin;
        std::uint32_t boneCount  = 0;
    };

} // namespace FREYA_NAMESPACE
