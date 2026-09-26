#include "Freya/Core/UiTypes.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    UiRect UiRect::Intersect(const UiRect& o) const
    {
        const float x0 = std::max(x, o.x);
        const float y0 = std::max(y, o.y);
        const float x1 = std::min(x + w, o.x + o.w);
        const float y1 = std::min(y + h, o.y + o.h);
        return { x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0) };
    }
} // namespace FREYA_NAMESPACE
