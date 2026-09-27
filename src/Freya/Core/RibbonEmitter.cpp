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

        std::vector<StripPoint> strip;
        strip.reserve(mPoints.size());

        const auto numSegments = static_cast<float>(mPoints.size() - 1);

        for (std::size_t i = 0; i < mPoints.size(); ++i)
        {
            const Point& p = mPoints[i];
            const float  t = std::clamp(p.age / p.lifetime, 0.f, 1.f);

            StripPoint sp {};
            sp.pos   = p.pos;
            sp.width = width;
            sp.color = glm::mix(color0, color1, t);
            sp.u     = static_cast<float>(i) / numSegments;
            strip.push_back(sp);
        }

        // transposeUv keeps the legacy orientation (U across, V along).
        StripStyle style {};
        style.textureIndex = textureIndex;
        style.blend        = blend;
        style.transposeUv  = true;
        draw.Strip(strip, style, cameraRight, cameraUp);
    }

} // namespace FREYA_NAMESPACE
