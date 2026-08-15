#include "Freya/Core/BillboardDraw.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    BillboardDraw::BillboardDraw(const std::uint32_t maxQuads) :
        mMaxQuads(std::max(1u, maxQuads))
    {
        mQuads.reserve(std::min(mMaxQuads, 256u));
    }

    void BillboardDraw::Clear() { mQuads.clear(); }

    void BillboardDraw::Quad(const Billboard& billboard)
    {
        if (mQuads.size() >= mMaxQuads)
            return;
        mQuads.push_back(billboard);
    }

    void BillboardDraw::HealthBar(const glm::vec3& headPos, const float width,
                                  const float height, const float fill01,
                                  const glm::vec4& bg, const glm::vec4& fg)
    {
        const float fill = std::clamp(fill01, 0.f, 1.f);
        Billboard   plate {};
        plate.worldPos = headPos;
        plate.size     = { width, height };
        plate.align    = BillboardAlign::Cylindrical;
        plate.blend    = BillboardBlend::Alpha;
        plate.layer    = BillboardLayer::Ui;
        plate.depthTest    = true;
        plate.textureIndex = 0;

        plate.color   = bg;
        plate.clipMax = 1.f;
        Quad(plate);

        plate.color   = fg;
        plate.clipMax = fill;
        Quad(plate);
    }

} // namespace FREYA_NAMESPACE
