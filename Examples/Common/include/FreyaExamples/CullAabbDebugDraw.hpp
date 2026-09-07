#pragma once

#include <Freya/Freya.hpp>

#include <cstdint>

#include <glm/glm.hpp>

namespace FreyaExamples
{
    /**
     * @brief Draws the wireframe of a GPU-cull AABB transformed by an
     * instance's world matrix.
     *
     * Mirrors exactly the box `Shaders/GpuDriven/CullFrustum.comp` tests
     * against (`computeClipCorners`): the 8 corners of `aabbMin`/`aabbMax`
     * in mesh-local space, transformed by `model`. Useful to visually
     * compare what the cull compute shader actually sees against the
     * rendered mesh, e.g. when chasing false-cull regressions such as
     * `cell_eyes_false_cull`.
     *
     * @param debugDraw Renderer's debug line queue (`Renderer::GetDebugDraw`),
     *                  gated behind `Renderer::SetDebugDrawEnabled`.
     * @param model     Instance world matrix (same one uploaded via
     *                  `SceneInstanceUpload::model`).
     * @param aabbMin   Mesh-local AABB min, as stored in `MeshPool`/`Mesh`.
     * @param aabbMax   Mesh-local AABB max, as stored in `MeshPool`/`Mesh`.
     * @param color     Line color (RGBA).
     */
    void DrawCullAabb(fra::DebugDraw& debugDraw, const glm::mat4& model,
                      const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                      const glm::vec4& color);

    /**
     * @brief Convenience overload: looks up the mesh's registered AABB from
     * `MeshPool` (the same one `IndirectDrawSystem` uploads into the GPU
     * `MeshInfoBuffer`) and draws it transformed by `model`.
     *
     * No-op if `meshId` isn't registered in `meshPool`.
     */
    void DrawCullAabb(fra::DebugDraw& debugDraw, fra::MeshPool& meshPool,
                      std::uint32_t meshId, const glm::mat4& model,
                      const glm::vec4& color);
} // namespace FreyaExamples
