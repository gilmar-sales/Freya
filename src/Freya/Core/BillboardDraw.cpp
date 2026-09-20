#include "Freya/Core/BillboardDraw.hpp"

#include <algorithm>
#include <vector>

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
    }

    bool BillboardDraw::Empty() const
    {
        SpinLockGuard lock(mLock);
        return mQuads.empty();
    }

    void BillboardDraw::Snapshot(std::vector<Billboard>& out) const
    {
        SpinLockGuard lock(mLock);
        out = mQuads;
    }

    void BillboardDraw::pushUnlocked(const Billboard& billboard)
    {
        if (mQuads.size() >= mMaxQuads)
            return;
        mQuads.push_back(billboard);
    }

    void BillboardDraw::Quad(const Billboard& billboard)
    {
        SpinLockGuard lock(mLock);
        pushUnlocked(billboard);
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
        plate.depthTest    = true;
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
