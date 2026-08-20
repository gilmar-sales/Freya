#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>

#include "Freya/Asset/InstanceTransform.hpp"

namespace FREYA_NAMESPACE
{
    constexpr std::uint32_t kMaxBindlessTextures          = 1024;
    constexpr std::uint32_t kMaxMeshInfos                 = 4096;
    constexpr std::uint32_t kMaxLodsPerMesh               = 4;
    constexpr std::uint32_t kSceneInstanceFlagCastShadows = 1u;
    constexpr std::uint32_t kSceneInstanceFlagTranslucent = 2u;
    constexpr std::uint32_t kSceneInstanceFlagSkinned     = 4u;

    constexpr std::uint32_t kMaterialFlagPackedMR      = 1u;
    constexpr std::uint32_t kMaterialFlagUnlit         = 2u;
    constexpr std::uint32_t kMaterialFlagDoubleSided   = 4u;
    constexpr std::uint32_t kMaterialFlagReceiveShadow = 8u;

    constexpr std::uint32_t kPickMissId = 0xFFFFFFFFu;

    constexpr std::uint32_t kBindlessWhiteTexture = 0;
    constexpr std::uint32_t kBindlessBlackTexture = 1;

    enum class CullMode : std::uint32_t
    {
        Camera      = 0,
        Shadow      = 1,
        Translucent = 2,
    };

    /**
     * @brief Per-LOD draw range (std430). Shared vertices via vertexOffset.
     */
    struct MeshLodInfo
    {
        std::uint32_t indexCount   = 0;
        std::uint32_t firstIndex   = 0;
        std::int32_t  vertexOffset = 0;
        std::uint32_t _pad         = 0;
    };

    static_assert(sizeof(MeshLodInfo) == 16, "MeshLodInfo must match GLSL");

    /**
     * @brief GPU mesh table entry (std430). Indexed by meshId.
     *
     * LOD ranges live in a side table at [lodBase, lodBase+lodCount).
     */
    struct MeshInfo
    {
        std::uint32_t lodCount = 0;
        std::uint32_t lodBase  = 0;
        std::uint32_t _pad0    = 0;
        std::uint32_t _pad1    = 0;
        std::uint32_t _pad2    = 0;
        std::uint32_t _pad3    = 0;
        std::uint32_t _pad4    = 0;
        std::uint32_t _pad5    = 0;
        glm::vec4     aabbMin  = glm::vec4(0.0f); ///< xyz = min
        glm::vec4     aabbMax  = glm::vec4(0.0f); ///< xyz = max
    };

    static_assert(sizeof(MeshInfo) == 64, "MeshInfo must match GLSL std430");
    static_assert(offsetof(MeshInfo, aabbMin) == 32,
                  "MeshInfo::aabbMin std430 offset");
    static_assert(offsetof(MeshInfo, aabbMax) == 48,
                  "MeshInfo::aabbMax std430 offset");

    /**
     * @brief Per-instance scene record for cull compute (std430).
     */
    struct SceneInstance
    {
        glm::mat4     model      = glm::mat4(1.0f);
        std::uint32_t meshId     = 0;
        std::uint32_t materialId = 0;
        std::uint32_t entityId   = 0;
        std::uint32_t flags      = kSceneInstanceFlagCastShadows;
    };

    static_assert(sizeof(SceneInstance) == 80,
                  "SceneInstance must match GLSL std430");

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

    /**
     * @brief Bindless material table entry (std430).
     */
    struct MaterialGPU
    {
        std::uint32_t albedoIndex        = kBindlessWhiteTexture;
        std::uint32_t normalIndex        = kBindlessWhiteTexture;
        std::uint32_t roughnessIndex     = kBindlessWhiteTexture;
        std::uint32_t emissiveIndex      = kBindlessBlackTexture;
        std::uint32_t metalnessIndex     = kBindlessBlackTexture;
        std::uint32_t alphaMode          = 0; ///< AlphaMode as uint
        float         clearcoat          = 0.f;
        float         clearcoatRoughness = 0.03f;
        glm::vec4     albedoFactor       = glm::vec4(1.0f);
        glm::vec4     emissiveFactor     = glm::vec4(1.0f); ///< w = ao
        glm::vec2     roughMetal         = glm::vec2(1.0f);
        float         materialId  = 0.0f; ///< CPU id; scene-color A is full id
        float         alphaCutoff = 0.0f;
        std::uint32_t occlusionIndex =
            kBindlessWhiteTexture; ///< AO (.r) or packed ORM .r
        std::uint32_t flags = kMaterialFlagReceiveShadow; ///< kMaterialFlag*
        std::uint32_t _pad0 = 0;
        std::uint32_t _pad1 = 0;
    };

    static_assert(sizeof(MaterialGPU) == 96, "MaterialGPU must match GLSL");

    /**
     * @brief Cull push constants (Approach B: per-instance compact + LOD).
     */
    struct CullPushConstants
    {
        glm::mat4     viewProj { 1.0f };
        glm::vec4     cameraPos     = glm::vec4(0.0f); ///< xyz
        glm::vec2     screenSize    = glm::vec2(1.0f);
        std::uint32_t instanceCount = 0;
        std::uint32_t cullMode      = 0;
        std::uint32_t reverseZ      = 0;
        std::uint32_t hizEnabled    = 0;
        float         lodPixelRef   = 256.0f; ///< ~pixels of diameter for LOD0
        float         lodStep       = 2.0f;   ///< diameter shrink per LOD
    };

    static_assert(sizeof(CullPushConstants) == 112, "CullPushConstants size");

} // namespace FREYA_NAMESPACE
