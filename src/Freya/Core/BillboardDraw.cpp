#include "Freya/Core/BillboardDraw.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    BillboardDraw::BillboardDraw(const std::uint32_t maxQuads) :
        mMaxQuads(std::max(1u, maxQuads))
    {
        mQuads.reserve(std::min(mMaxQuads, 256u));
    }

    void BillboardDraw::Clear()
    {
        SpinLockGuard lock(mLock);
        mQuads.clear();
        mConnected.clear();
    }

    bool BillboardDraw::Empty() const
    {
        SpinLockGuard lock(mLock);
        return mQuads.empty() && mConnected.empty();
    }

    bool BillboardDraw::ConnectedEmpty() const
    {
        SpinLockGuard lock(mLock);
        return mConnected.empty();
    }

    void BillboardDraw::Snapshot(std::vector<Billboard>& out) const
    {
        SpinLockGuard lock(mLock);
        out = mQuads;
    }

    void BillboardDraw::SnapshotConnected(
        std::vector<ConnectedBillboard>& out) const
    {
        SpinLockGuard lock(mLock);
        out = mConnected;
    }

    void BillboardDraw::pushUnlocked(const Billboard& billboard)
    {
        if (mQuads.size() >= mMaxQuads)
            return;
        mQuads.push_back(billboard);
    }

    void BillboardDraw::pushConnectedUnlocked(
        const ConnectedBillboard& quad)
    {
        if (mConnected.size() >= mMaxQuads)
            return;
        mConnected.push_back(quad);
    }

    void BillboardDraw::Quad(const Billboard& billboard)
    {
        SpinLockGuard lock(mLock);
        pushUnlocked(billboard);
    }

    void BillboardDraw::Quads(const std::span<const Billboard> billboards)
    {
        SpinLockGuard lock(mLock);
        for (const auto& b : billboards)
            pushUnlocked(b);
    }

    void BillboardDraw::ConnectedQuad(const ConnectedBillboard& quad)
    {
        SpinLockGuard lock(mLock);
        pushConnectedUnlocked(quad);
    }

    void BillboardDraw::ConnectedQuads(
        const std::span<const ConnectedBillboard> quads)
    {
        SpinLockGuard lock(mLock);
        for (const auto& q : quads)
            pushConnectedUnlocked(q);
    }

    namespace
    {
        /// 2D screen-space normal of a world delta, or zero when the
        /// projection degenerates (segment pointing at the camera).
        glm::vec2 screenNormal(const glm::vec3& delta,
                               const glm::vec3& camRight,
                               const glm::vec3& camUp)
        {
            const glm::vec2 t(glm::dot(delta, camRight),
                              glm::dot(delta, camUp));
            const float     len = glm::length(t);
            if (!(len > 1e-9f))
                return glm::vec2(0.f);
            const glm::vec2 dir = t / len;
            return glm::vec2(-dir.y, dir.x);
        }
    } // namespace

    void BillboardDraw::Strip(const std::span<const StripPoint> points,
                              const StripStyle& style,
                              const glm::vec3&  camRight,
                              const glm::vec3&  camUp)
    {
        const std::size_t count = points.size();
        if (count < 2)
            return;

        // Per-segment screen-space normals; degenerate segments borrow
        // the nearest valid neighbour so joints stay well-defined.
        std::vector<glm::vec2> segNormals(count - 1, glm::vec2(0.f));
        std::vector<bool>      segValid(count - 1, false);
        for (std::size_t i = 0; i + 1 < count; ++i)
        {
            const glm::vec2 n = screenNormal(
                points[i + 1].pos - points[i].pos, camRight, camUp);
            if (glm::dot(n, n) > 0.5f)
            {
                segNormals[i] = n;
                segValid[i]   = true;
            }
        }
        // Forward fill, then backward fill for leading runs.
        glm::vec2 fallback(0.f);
        bool      haveFallback = false;
        for (std::size_t i = 0; i + 1 < count; ++i)
        {
            if (segValid[i])
            {
                fallback     = segNormals[i];
                haveFallback = true;
            }
            else if (haveFallback)
            {
                segNormals[i] = fallback;
                segValid[i]   = true;
            }
        }
        for (std::size_t i = count - 1; i-- > 0;)
        {
            if (segValid[i])
            {
                fallback     = segNormals[i];
                haveFallback = true;
            }
            else if (haveFallback)
            {
                segNormals[i] = fallback;
                segValid[i]   = true;
            }
        }
        if (!haveFallback)
            return; // Every point coincides; nothing to draw.

        const float miterLimit = std::max(style.miterLimit, 1.0f);

        // Miter direction + extension scale per point.
        std::vector<glm::vec2> miters(count);
        std::vector<float>     scales(count, 1.0f);
        for (std::size_t i = 0; i < count; ++i)
        {
            const glm::vec2 n0 =
                segNormals[i > 0 ? i - 1 : 0];
            const glm::vec2 n1 =
                segNormals[i + 1 < count ? i : count - 2];
            const glm::vec2 sum = n0 + n1;
            if (glm::dot(sum, sum) < 1e-8f)
            {
                // Hairpin reversal: bevel instead of an infinite miter.
                miters[i] = n1;
                scales[i] = 1.0f;
                continue;
            }
            const glm::vec2 miter = sum * (1.0f / glm::length(sum));
            const float cosHalf = glm::dot(miter, n1);
            miters[i] = miter;
            scales[i] =
                cosHalf > 1e-4f
                    ? std::min(1.0f / cosHalf, miterLimit)
                    : miterLimit;
        }

        std::vector<ConnectedBillboard> batch;
        batch.reserve(count - 1);
        for (std::size_t i = 0; i + 1 < count; ++i)
        {
            const StripPoint& p0 = points[i];
            const StripPoint& p1 = points[i + 1];
            if (glm::distance(p0.pos, p1.pos) < 1e-9f)
                continue;

            const float half0 =
                std::max(p0.width, 0.0f) * 0.5f * scales[i];
            const float half1 =
                std::max(p1.width, 0.0f) * 0.5f * scales[i + 1];
            const glm::vec3 side0 =
                (camRight * miters[i].x + camUp * miters[i].y) * half0;
            const glm::vec3 side1 =
                (camRight * miters[i + 1].x + camUp * miters[i + 1].y) *
                half1;

            ConnectedBillboard q {};
            q.c0            = p0.pos + side0;
            q.c1            = p0.pos - side0;
            q.c2            = p1.pos + side1;
            q.c3            = p1.pos - side1;
            q.color0        = p0.color;
            q.color1        = p1.color;
            const bool transpose =
                HasFlag(style.flags, BillboardFlags::TransposeUv);
            q.uvRect = transpose ? glm::vec4(0.f, p0.u, 1.f, p1.u)
                                 : glm::vec4(p0.u, 0.f, p1.u, 1.f);
            q.textureIndex  = style.textureIndex;
            q.blend         = style.blend;
            q.layer         = style.layer;
            q.flags         = style.flags;
            q.clipMax       = 1.f;
            batch.push_back(q);
        }

        if (batch.empty())
            return;
        SpinLockGuard lock(mLock);
        for (const auto& q : batch)
            pushConnectedUnlocked(q);
    }

    void BillboardDraw::HealthBar(const glm::vec3& headPos, const float width,
                                  const float height, const float fill01,
                                  const glm::vec4& bg, const glm::vec4& fg,
                                  const BillboardAlign align)
    {
        const float fill = std::clamp(fill01, 0.f, 1.f);
        Billboard   plate {};
        plate.worldPos     = headPos;
        plate.size         = { width, height };
        plate.align        = align;
        plate.blend        = BillboardBlend::Alpha;
        plate.layer        = BillboardLayer::Ui;
        SetFlag(plate.flags, BillboardFlags::DepthTest, true);
        plate.textureIndex = 0;

        SpinLockGuard lock(mLock);
        plate.color   = bg;
        plate.clipMax = 1.f;
        pushUnlocked(plate);

        plate.color   = fg;
        plate.clipMax = fill;
        pushUnlocked(plate);
    }

} // namespace FREYA_NAMESPACE
