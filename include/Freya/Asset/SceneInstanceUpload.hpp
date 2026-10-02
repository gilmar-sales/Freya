#pragma once

#include "Freya/Asset/GpuScene.hpp"
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
     * App/ECS resolves `techniqueId` and `flags` (no MaterialPool lookup in
     * End). Contract: prefer ascending `entityId` before End so TAA
     * `prevModel` stays stable on the GPU.
     */
    struct SceneInstanceUpload
    {
        SceneTransform     transform {};
        MeshHandle         mesh {};
        MaterialHandle     material {};
        std::uint32_t      entityId    = 0;
        std::uint32_t      techniqueId = 0;
        SceneInstanceFlags flags       = SceneInstanceFlags::CastShadows;
        std::uint32_t      boneOffset  = kNoSkin;
        std::uint32_t      boneCount   = 0;
    };

    [[nodiscard]] SceneInstanceFlags MakeSceneInstanceFlags(
        SceneInstanceFlags flags);

} // namespace FREYA_NAMESPACE
