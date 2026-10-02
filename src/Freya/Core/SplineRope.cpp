#include "Freya/Core/SplineRope.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{

    namespace
    {
        glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1,
                             const glm::vec3& p2, const glm::vec3& p3,
                             float t)
        {
            const float t2 = t * t;
            const float t3 = t2 * t;
            return 0.5f *
                   ((2.0f * p1) + (-p0 + p2) * t +
                    (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                    (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
        }

        glm::vec3 sampleSpline(const std::vector<glm::vec3>& cp, float t)
        {
            const int n = static_cast<int>(cp.size());
            t           = std::clamp(t, 0.0f, static_cast<float>(n - 1));
            const int   i  = std::min(static_cast<int>(t), n - 2);
            const float f  = t - static_cast<float>(i);
            const glm::vec3& p1 = cp[i];
            const glm::vec3& p2 = cp[i + 1];
            const glm::vec3  p0 = i > 0 ? cp[i - 1] : 2.0f * p1 - p2;
            const glm::vec3  p3 =
                i + 2 < n ? cp[i + 2] : 2.0f * p2 - p1;
            return catmullRom(p0, p1, p2, p3, f);
        }
    } // namespace

    void SplineRope::Submit(BillboardDraw&   draw,
                            const glm::vec3& camRight,
                            const glm::vec3& camUp) const
    {
        if (controlPoints.size() < 2 || growT <= 0.0f || segments == 0)
            return;

        const float tScale      = static_cast<float>(controlPoints.size() - 1);
        const float growClamped = std::clamp(growT, 0.0f, 1.0f);
        const float step        = 1.0f / static_cast<float>(segments);

        // Fixed world-space sample positions; the tip segment shortens as
        // growT sweeps so growth runs along the path at constant speed.
        std::vector<StripPoint> points;
        points.reserve(static_cast<std::size_t>(segments) + 1);
        for (std::uint32_t i = 0; i <= segments; ++i)
        {
            const float t =
                std::min(static_cast<float>(i) * step, growClamped);
            StripPoint p {};
            p.pos   = sampleSpline(controlPoints, t * tScale);
            p.width = 2.0f * glm::mix(baseRadius, tipRadius, t);
            p.color = glm::mix(color0, color1, t);
            p.u     = t * uvRepeat;
            points.push_back(p);
        }

        StripStyle style {};
        style.textureIndex = textureIndex;
        style.blend        = blend;
        style.layer        = layer;
        SetFlag(style.flags, BillboardFlags::DepthTest, depthTest);
        style.miterLimit   = miterLimit;
        draw.Strip(points, style, camRight, camUp);
    }

} // namespace FREYA_NAMESPACE
