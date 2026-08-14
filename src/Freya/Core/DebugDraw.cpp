#include "Freya/Core/DebugDraw.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        void TangentBasis(const glm::vec3& axis, glm::vec3& T, glm::vec3& B)
        {
            const auto len = glm::length(axis);
            const auto n  = len > 1e-6f ? axis / len : glm::vec3(0.f, 1.f, 0.f);
            const auto up = std::abs(n.y) < 0.99f ? glm::vec3(0.f, 1.f, 0.f)
                                                  : glm::vec3(1.f, 0.f, 0.f);
            T             = glm::normalize(glm::cross(up, n));
            B             = glm::cross(n, T);
        }
    } // namespace
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

    void DebugDraw::Circle(const glm::vec3& center, const glm::vec3& normal,
                           const float radius, const glm::vec4& color,
                           const std::uint32_t segments)
    {
        if (radius <= 0.f || segments < 3)
            return;

        glm::vec3 T, B;
        TangentBasis(normal, T, B);
        const auto count = std::max(3u, segments);
        glm::vec3  prev  = center + T * radius;
        for (std::uint32_t i = 1; i <= count; ++i)
        {
            const auto a = glm::two_pi<float>() * static_cast<float>(i) /
                           static_cast<float>(count);
            const auto p =
                center + (T * std::cos(a) + B * std::sin(a)) * radius;
            Line(prev, p, color);
            prev = p;
        }
    }

    void DebugDraw::Sphere(const glm::vec3& center, const float radius,
                           const glm::vec4& color, const std::uint32_t segments)
    {
        Circle(center, { 1.f, 0.f, 0.f }, radius, color, segments);
        Circle(center, { 0.f, 1.f, 0.f }, radius, color, segments);
        Circle(center, { 0.f, 0.f, 1.f }, radius, color, segments);
    }

    void DebugDraw::Cone(const glm::vec3& apex, const glm::vec3& direction,
                         const float length, const float halfAngleRad,
                         const glm::vec4& color, const std::uint32_t segments)
    {
        if (length <= 0.f || halfAngleRad <= 0.f)
            return;

        const auto dir  = glm::normalize(direction);
        const auto base = apex + dir * length;
        const auto r    = std::tan(halfAngleRad) * length;
        Circle(base, dir, r, color, segments);

        glm::vec3 T, B;
        TangentBasis(dir, T, B);
        const auto spokes = std::max(4u, segments / 4);
        for (std::uint32_t i = 0; i < spokes; ++i)
        {
            const auto a = glm::two_pi<float>() * static_cast<float>(i) /
                           static_cast<float>(spokes);
            Line(apex, base + (T * std::cos(a) + B * std::sin(a)) * r, color);
        }
    }

    void DebugDraw::Arrow(const glm::vec3& from, const glm::vec3& to,
                          const glm::vec4& color)
    {
        Line(from, to, color);
        const auto along = to - from;
        const auto len   = glm::length(along);
        if (len < 1e-4f)
            return;

        const auto dir = along / len;
        glm::vec3  T, B;
        TangentBasis(dir, T, B);
        const auto hs   = len * 0.12f;
        const auto neck = to - dir * hs;
        const auto w    = hs * 0.35f;
        Line(to, neck + T * w, color);
        Line(to, neck - T * w, color);
        Line(to, neck + B * w, color);
        Line(to, neck - B * w, color);
    }

    void DebugDraw::Rect(const glm::vec3& center, const glm::vec3& normal,
                         const glm::vec3& tangent, const float halfWidth,
                         const float halfHeight, const glm::vec4& color)
    {
        const auto N = glm::normalize(normal);
        auto       T = tangent - N * glm::dot(tangent, N);
        glm::vec3  B;
        if (glm::dot(T, T) < 1e-8f)
            TangentBasis(N, T, B);
        else
        {
            T = glm::normalize(T);
            B = glm::cross(N, T);
        }
        const auto hw = std::max(halfWidth, 1e-4f);
        const auto hh = std::max(halfHeight, 1e-4f);
        const auto c0 = center + T * hw + B * hh;
        const auto c1 = center - T * hw + B * hh;
        const auto c2 = center - T * hw - B * hh;
        const auto c3 = center + T * hw - B * hh;
        Line(c0, c1, color);
        Line(c1, c2, color);
        Line(c2, c3, color);
        Line(c3, c0, color);
        Arrow(center, center + N * std::min(hw, hh), color);
    }

    void DebugDraw::DrawSkeleton(const Skeleton&  skeleton,
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
