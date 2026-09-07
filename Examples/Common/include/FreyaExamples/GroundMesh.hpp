#pragma once

#include <Freya/Freya.hpp>

#include <glm/glm.hpp>

namespace FreyaExamples
{
    /**
     * @brief Create a single-sided XZ ground quad centered at the origin.
     *
     * @param windingCCW true → indices {0,3,2, 0,2,1} (IPL/Cell/SSAO);
     *                   false → {0,1,2, 0,2,3} (SkinnedFox unit quad).
     */
    [[nodiscard]] fra::MeshHandle CreateGroundPlane(fra::MeshPool& meshPool,
                                                    float          halfExtent,
                                                    glm::vec3      tint,
                                                    bool windingCCW = true);
} // namespace FreyaExamples
