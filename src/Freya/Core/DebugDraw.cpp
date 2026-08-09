#include "Freya/Core/DebugDraw.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    DebugDraw::DebugDraw(const std::uint32_t maxVertices) :
        mMaxVertices(std::max(2u, maxVertices))
    {
        mVerts.reserve(std::min(mMaxVertices, 4096u));
    }

    void DebugDraw::Clear()
    {
        mVerts.clear();
    }

    bool DebugDraw::pushVertex(const DebugDrawVertex& v)
    {
        if (mVerts.size() >= mMaxVertices)
            return false;
        mVerts.push_back(v);
        return true;
    }

    void DebugDraw::Line(const glm::vec3& a, const glm::vec3& b,
                         const glm::vec4& color)
    {
        if (mVerts.size() + 2 > mMaxVertices)
            return;
        mVerts.push_back({ a, color });
        mVerts.push_back({ b, color });
    }

    void DebugDraw::Cross(const glm::vec3& center, const float size,
                          const glm::vec4& color)
    {
        Line(center + glm::vec3(size, 0, 0), center - glm::vec3(size, 0, 0),
             color);
        Line(center + glm::vec3(0, size, 0), center - glm::vec3(0, size, 0),
             color);
        Line(center + glm::vec3(0, 0, size), center - glm::vec3(0, 0, size),
             color);
    }

    void DebugDraw::Axes(const glm::mat4& transform, const float size)
    {
        const glm::vec3 o(transform[3]);
        const glm::vec3 x = o + glm::vec3(transform[0]) * size;
        const glm::vec3 y = o + glm::vec3(transform[1]) * size;
        const glm::vec3 z = o + glm::vec3(transform[2]) * size;
        Line(o, x, { 1.f, 0.2f, 0.2f, 1.f });
        Line(o, y, { 0.2f, 1.f, 0.2f, 1.f });
        Line(o, z, { 0.3f, 0.5f, 1.f, 1.f });
    }

    void DebugDraw::Diamond(const glm::vec3& center, const float size,
                            const glm::vec4& color)
    {
        const glm::vec3 px = center + glm::vec3(size, 0, 0);
        const glm::vec3 nx = center - glm::vec3(size, 0, 0);
        const glm::vec3 py = center + glm::vec3(0, size, 0);
        const glm::vec3 ny = center - glm::vec3(0, size, 0);
        const glm::vec3 pz = center + glm::vec3(0, 0, size);
        const glm::vec3 nz = center - glm::vec3(0, 0, size);
        Line(px, py, color);
        Line(py, nx, color);
        Line(nx, ny, color);
        Line(ny, px, color);
        Line(pz, px, color);
        Line(pz, py, color);
        Line(pz, nx, color);
        Line(pz, ny, color);
        Line(nz, px, color);
        Line(nz, py, color);
        Line(nz, nx, color);
        Line(nz, ny, color);
    }

    void DebugDraw::DrawSkeleton(const Skeleton& skeleton,
                                 const LocalPose& local,
                                 const glm::mat4& modelWorld,
                                 const glm::vec4& color)
    {
        const auto global = LocalToGlobal(skeleton, local);
        const auto n      = skeleton.JointCount();
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const auto p = i < skeleton.parents.size() ? skeleton.parents[i]
                                                       : std::int32_t { -1 };
            if (p < 0)
                continue;
            const glm::vec3 a = glm::vec3(
                (modelWorld * global[static_cast<std::uint32_t>(p)])[3]);
            const glm::vec3 b = glm::vec3((modelWorld * global[i])[3]);
            Line(a, b, color);
        }
    }

    void DebugDraw::DrawLookRay(
        const Skeleton& skeleton, const LocalPose& local,
        const glm::mat4& modelWorld, const std::uint32_t joint,
        const glm::vec3& targetWorld, const glm::vec4& color)
    {
        if (joint >= skeleton.JointCount())
            return;
        const auto      global = LocalToGlobal(skeleton, local);
        const glm::vec3 eye    = glm::vec3((modelWorld * global[joint])[3]);
        Line(eye, targetWorld, color);
        Cross(targetWorld, 0.08f, color);
    }

    void DebugDraw::DrawTwoBoneIk(
        const Skeleton& skeleton, const LocalPose& local,
        const glm::mat4& modelWorld, const std::uint32_t root,
        const std::uint32_t mid, const std::uint32_t tip,
        const glm::vec3& targetWorld, const glm::vec3& poleWorld,
        const glm::vec4& chainColor, const glm::vec4& targetColor)
    {
        const auto n = skeleton.JointCount();
        if (root >= n || mid >= n || tip >= n)
            return;
        const auto      global = LocalToGlobal(skeleton, local);
        const glm::vec3 r      = glm::vec3((modelWorld * global[root])[3]);
        const glm::vec3 m      = glm::vec3((modelWorld * global[mid])[3]);
        const glm::vec3 t      = glm::vec3((modelWorld * global[tip])[3]);
        Line(r, m, chainColor);
        Line(m, t, chainColor);
        Line(r, poleWorld,
             { targetColor.r, targetColor.g, targetColor.b, 0.5f });
        Diamond(targetWorld, 0.06f, targetColor);
        Cross(poleWorld, 0.05f, { 1.f, 1.f, 0.2f, 1.f });
    }

} // namespace FREYA_NAMESPACE
