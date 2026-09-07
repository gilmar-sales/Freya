#include <FreyaExamples/CullAabbDebugDraw.hpp>

namespace FreyaExamples
{
    namespace
    {
        // Corner index pairs forming the 12 edges of a box. Same layout as
        // Shaders/GpuDriven/CullFrustum.comp's kAabbEdges / local[8] corner
        // ordering, kept 1:1 purely so the two are easy to cross-reference;
        // edge order doesn't matter for drawing.
        constexpr int kBoxEdges[24] = {
            0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3,
            2, 6, 3, 7, 4, 5, 4, 6, 5, 7, 6, 7,
        };
    } // namespace

    void DrawCullAabb(fra::DebugDraw& debugDraw, const glm::mat4& model,
                      const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                      const glm::vec4& color)
    {
        const glm::vec3 local[8] = {
            { aabbMin.x, aabbMin.y, aabbMin.z },
            { aabbMax.x, aabbMin.y, aabbMin.z },
            { aabbMin.x, aabbMax.y, aabbMin.z },
            { aabbMax.x, aabbMax.y, aabbMin.z },
            { aabbMin.x, aabbMin.y, aabbMax.z },
            { aabbMax.x, aabbMin.y, aabbMax.z },
            { aabbMin.x, aabbMax.y, aabbMax.z },
            { aabbMax.x, aabbMax.y, aabbMax.z },
        };

        glm::vec3 world[8];
        for (int i = 0; i < 8; ++i)
            world[i] = glm::vec3(model * glm::vec4(local[i], 1.0f));

        for (int edge = 0; edge < 12; ++edge)
        {
            const auto& a = world[kBoxEdges[edge * 2]];
            const auto& b = world[kBoxEdges[edge * 2 + 1]];
            debugDraw.Line(a, b, color);
        }
    }

    void DrawCullAabb(fra::DebugDraw& debugDraw, fra::MeshPool& meshPool,
                      const std::uint32_t meshId, const glm::mat4& model,
                      const glm::vec4& color)
    {
        if (!meshPool.Contains(meshId))
            return;
        const auto& mesh = meshPool.GetMesh(meshId);
        DrawCullAabb(debugDraw, model, mesh.aabbMin, mesh.aabbMax, color);
    }
} // namespace FreyaExamples
