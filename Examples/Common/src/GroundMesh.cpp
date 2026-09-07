#include <FreyaExamples/GroundMesh.hpp>

#include <vector>

namespace FreyaExamples
{
    fra::MeshHandle CreateGroundPlane(fra::MeshPool&  meshPool,
                                      const float     halfExtent,
                                      const glm::vec3 tint,
                                      const bool      windingCCW)
    {
        const auto up  = glm::vec3(0.0f, 1.0f, 0.0f);
        const auto tan = glm::vec3(1.0f, 0.0f, 0.0f);

        const std::vector<fra::Vertex> vertices = {
            { { -halfExtent, 0.0f, -halfExtent },
              tint,
              up,
              tan,
              { 0.0f, 0.0f } },
            { { halfExtent, 0.0f, -halfExtent },
              tint,
              up,
              tan,
              { 1.0f, 0.0f } },
            { { halfExtent, 0.0f, halfExtent }, tint, up, tan, { 1.0f, 1.0f } },
            { { -halfExtent, 0.0f, halfExtent },
              tint,
              up,
              tan,
              { 0.0f, 1.0f } },
        };

        // Single-sided (+Y). Two-sided coplanar indices z-fight in the CSM
        // depth map under CullBack + lightProj Y-flip.
        const std::vector<std::uint32_t> indices =
            windingCCW ? std::vector<std::uint32_t> { 0, 3, 2, 0, 2, 1 }
                       : std::vector<std::uint32_t> { 0, 1, 2, 0, 2, 3 };

        return meshPool.CreateMesh(vertices, indices);
    }
} // namespace FreyaExamples
