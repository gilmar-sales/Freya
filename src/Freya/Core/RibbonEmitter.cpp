#include "Freya/Core/RibbonEmitter.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace FREYA_NAMESPACE
{
    void RibbonEmitter::Reset()
    {
        mPoints.clear();
        mLastPos = glm::vec3(std::numeric_limits<float>::max());
    }

    void RibbonEmitter::Tick(const float       dt,
                             BillboardDraw&    draw,
                             const glm::vec3&  cameraRight,
                             const glm::vec3&  cameraUp)
    {
        if (dt <= 0.f)
            return;

        // Age and remove expired points.
        for (auto& p : mPoints)
            p.age += dt;
        std::erase_if(mPoints, [](const Point& p) { return p.age >= p.lifetime; });

        // Record a new point if origin moved far enough.
        const glm::vec3 delta   = origin - mLastPos;
        const float     distSq  = glm::dot(delta, delta);
        const float     minDistSq = minDistance * minDistance;
        const bool      firstPoint =
            mLastPos.x == std::numeric_limits<float>::max();

        if (firstPoint || distSq >= minDistSq)
        {
            if (mPoints.size() >= maxPoints)
                mPoints.erase(mPoints.begin());

            Point pt {};
            pt.pos      = origin;
            pt.age      = 0.f;
            pt.lifetime = pointLifetime;
            mPoints.push_back(pt);
            mLastPos = origin;
        }

        if (mPoints.size() < 2)
            return;

        std::vector<Billboard> batch;
        batch.reserve(mPoints.size() - 1);

        const auto numSegments = static_cast<float>(mPoints.size() - 1);

        for (std::size_t i = 0; i + 1 < mPoints.size(); ++i)
        {
            const Point& a = mPoints[i];
            const Point& b = mPoints[i + 1];

            const float tA = std::clamp(a.age / a.lifetime, 0.f, 1.f);
            const float tB = std::clamp(b.age / b.lifetime, 0.f, 1.f);
            const float t  = (tA + tB) * 0.5f;

            const float segLen = glm::distance(a.pos, b.pos);
            if (segLen < 1e-5f)
                continue;

            const glm::vec3 segDir = (b.pos - a.pos) / segLen;

            // Compute screen-space rotation to align billboard Y axis with
            // the projected segment direction.
            const float screenX = glm::dot(segDir, cameraRight);
            const float screenY = glm::dot(segDir, cameraUp);
            const float angle   = std::atan2(screenX, screenY);

            const float segT = static_cast<float>(i) / numSegments;

            Billboard bb {};
            bb.worldPos     = (a.pos + b.pos) * 0.5f;
            bb.size         = { width, segLen };
            bb.color        = glm::mix(color0, color1, t);
            bb.uvRect       = { 0.f, segT, 1.f, segT + 1.f / numSegments };
            bb.textureIndex = textureIndex;
            bb.align        = BillboardAlign::Screen;
            bb.blend        = blend;
            bb.layer        = BillboardLayer::Vfx;
            bb.depthTest    = true;
            bb.rotation     = angle;
            batch.push_back(bb);
        }

        if (!batch.empty())
            draw.Quads(batch);
    }

} // namespace FREYA_NAMESPACE
