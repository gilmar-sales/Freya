#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    constexpr std::uint32_t kMaxBindlessTextures          = 1024;
    constexpr std::uint32_t kMaxMeshInfos                 = 4096;
    constexpr std::uint32_t kSceneInstanceFlagCastShadows = 1u;

    constexpr std::uint32_t kBindlessWhiteTexture = 0;
    constexpr std::uint32_t kBindlessBlackTexture = 1;

    enum class CullMode : std::uint32_t
    {
        Camera = 0,
        Shadow = 1,
    };

    /**
     * @brief GPU mesh table entry (std430). Indexed by meshId.
     *
     * Layout matches GLSL std430: six uint/int fields, two pad uints to
     * reach offset 32, then vec4 AABBs (base alignment 16).
     */
    struct MeshInfo
    {
        std::uint32_t indexCount        = 0;
        std::uint32_t firstIndex        = 0;
        std::int32_t  vertexOffset      = 0;
        std::uint32_t vertexBufferIndex = 0;
        std::uint32_t indexBufferIndex  = 0;
        std::uint32_t _pad0             = 0;
        std::uint32_t _pad1             = 0;
        std::uint32_t _pad2             = 0;
        glm::vec4     aabbMin           = glm::vec4(0.0f); ///< xyz = min
        glm::vec4     aabbMax           = glm::vec4(0.0f); ///< xyz = max
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
     * Contract: prefer sorting by `(meshId, entityId)` before upload so
     * Freya can form contiguous mesh batches and keep TAA history stable
     * (prevModel is looked up by `entityId`). If already sorted that way
     * (and by vertex/index chunk), Freya skips the internal sort.
     */
    struct SceneInstanceUpload
    {
        glm::mat4     model       = glm::mat4(1.0f);
        std::uint32_t meshId      = 0;
        std::uint32_t materialId  = 0;
        std::uint32_t entityId    = 0;
        bool          castShadows = true;
    };

    /**
     * @brief Bindless material table entry (std430).
     */
    struct MaterialGPU
    {
        std::uint32_t albedoIndex    = kBindlessWhiteTexture;
        std::uint32_t normalIndex    = kBindlessWhiteTexture;
        std::uint32_t roughnessIndex = kBindlessWhiteTexture;
        std::uint32_t emissiveIndex  = kBindlessBlackTexture;
        std::uint32_t metalnessIndex = kBindlessBlackTexture;
        std::uint32_t _pad0          = 0;
        std::uint32_t _pad1          = 0;
        std::uint32_t _pad2          = 0;
        glm::vec4     albedoFactor   = glm::vec4(1.0f);
        glm::vec4     emissiveFactor = glm::vec4(1.0f); ///< w = ao
        glm::vec2     roughMetal     = glm::vec2(1.0f);
        float         materialId     = 0.0f;
        float         alphaCutoff    = 0.0f;
    };

    /**
     * @brief Cull push constants.
     *
     * Approach A: per-instance frustum test + atomic compaction into
     * batched `VkDrawIndexedIndirectCommand`s (one draw per mesh run).
     */
    struct CullPushConstants
    {
        glm::mat4     viewProj { 1.0f };
        std::uint32_t instanceCount = 0;
        std::uint32_t cullMode      = 0;
        std::uint32_t reverseZ      = 0;
        std::uint32_t _pad0         = 0;
    };

    static_assert(sizeof(CullPushConstants) == 80, "CullPushConstants size");

} // namespace FREYA_NAMESPACE
