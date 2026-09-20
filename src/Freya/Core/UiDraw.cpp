#include "Freya/Core/UiDraw.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace FREYA_NAMESPACE
{
    namespace
    {
        UiQuad MakeBase(const UiRect& r, const glm::vec4& color,
                        const std::uint32_t textureIndex)
        {
            UiQuad q {};
            q.rect         = { r.x, r.y, r.w, r.h };
            q.color        = color;
            q.textureIndex = textureIndex;
            q.clipMax      = 1.f;
            return q;
        }

        void EmitStretch(const UiRect& r, const std::uint32_t textureIndex,
                         const UiImageOpts& opts, auto&& push)
        {
            UiQuad q = MakeBase(r, opts.tint, textureIndex);
            q.uvRect = opts.uvRect;
            if (opts.rounding > 0.f)
            {
                q.rounding = opts.rounding;
                q.flags |= kUiFlagSdfRounded;
            }
            push(q);
        }

        void FitContainCover(UiRect& dest, glm::vec4& uv, const UiRect& r,
                             const UiImageOpts& opts, const bool cover)
        {
            dest = r;
            uv   = opts.uvRect;
            float srcW =
                opts.sourceSize.x > 0.f
                    ? opts.sourceSize.x
                    : std::max(1e-5f, opts.uvRect.z - opts.uvRect.x);
            float srcH =
                opts.sourceSize.y > 0.f
                    ? opts.sourceSize.y
                    : std::max(1e-5f, opts.uvRect.w - opts.uvRect.y);
            const float srcAspect = srcW / srcH;
            const float dstAspect = r.w / std::max(r.h, 1e-5f);

            if (!cover)
            {
                // Contain: letterbox dest.
                if (srcAspect > dstAspect)
                {
                    const float h = r.w / srcAspect;
                    dest.y        = r.y + 0.5f * (r.h - h);
                    dest.h        = h;
                }
                else
                {
                    const float w = r.h * srcAspect;
                    dest.x        = r.x + 0.5f * (r.w - w);
                    dest.w        = w;
                }
                return;
            }

            // Cover: crop UV.
            const float u0 = opts.uvRect.x;
            const float v0 = opts.uvRect.y;
            const float u1 = opts.uvRect.z;
            const float v1 = opts.uvRect.w;
            const float uS = u1 - u0;
            const float vS = v1 - v0;
            if (srcAspect > dstAspect)
            {
                const float keep = dstAspect / srcAspect;
                const float pad  = 0.5f * (1.f - keep);
                uv               = { u0 + pad * uS, v0, u1 - pad * uS, v1 };
            }
            else
            {
                const float keep = srcAspect / dstAspect;
                const float pad  = 0.5f * (1.f - keep);
                uv               = { u0, v0 + pad * vS, u1, v1 - pad * vS };
            }
        }

        void EmitSlice(const UiRect& r, const std::uint32_t textureIndex,
                       const UiImageOpts& opts, auto&& push)
        {
            const float u0 = opts.uvRect.x;
            const float v0 = opts.uvRect.y;
            const float u1 = opts.uvRect.z;
            const float v1 = opts.uvRect.w;
            const float uS = u1 - u0;
            const float vS = v1 - v0;

            const float mlPx =
                std::clamp(opts.sliceMargins.x, 0.f, r.w * 0.5f);
            const float mrPx =
                std::clamp(opts.sliceMargins.y, 0.f, r.w * 0.5f);
            const float mtPx =
                std::clamp(opts.sliceMargins.z, 0.f, r.h * 0.5f);
            const float mbPx =
                std::clamp(opts.sliceMargins.w, 0.f, r.h * 0.5f);

            float uMl = 0.f;
            float uMr = 0.f;
            float vMt = 0.f;
            float vMb = 0.f;
            if (opts.sourceSize.x > 0.f && opts.sourceSize.y > 0.f)
            {
                uMl = opts.sliceMargins.x / opts.sourceSize.x * uS;
                uMr = opts.sliceMargins.y / opts.sourceSize.x * uS;
                vMt = opts.sliceMargins.z / opts.sourceSize.y * vS;
                vMb = opts.sliceMargins.w / opts.sourceSize.y * vS;
            }
            else
            {
                // Symmetric mapping when atlas pixel size is unknown.
                uMl = (r.w > 1e-5f) ? mlPx / r.w * uS : 0.f;
                uMr = (r.w > 1e-5f) ? mrPx / r.w * uS : 0.f;
                vMt = (r.h > 1e-5f) ? mtPx / r.h * vS : 0.f;
                vMb = (r.h > 1e-5f) ? mbPx / r.h * vS : 0.f;
            }

            const float xs[4] = { r.x, r.x + mlPx, r.x + r.w - mrPx,
                                  r.x + r.w };
            const float ys[4] = { r.y, r.y + mtPx, r.y + r.h - mbPx,
                                  r.y + r.h };
            const float us[4] = { u0, u0 + uMl, u1 - uMr, u1 };
            const float vs[4] = { v0, v0 + vMt, v1 - vMb, v1 };

            for (int row = 0; row < 3; ++row)
            {
                const float h = ys[row + 1] - ys[row];
                if (h <= 1e-5f)
                    continue;
                for (int col = 0; col < 3; ++col)
                {
                    const float w = xs[col + 1] - xs[col];
                    if (w <= 1e-5f)
                        continue;
                    UiQuad q = MakeBase({ xs[col], ys[row], w, h }, opts.tint,
                                        textureIndex);
                    q.uvRect = { us[col], vs[row], us[col + 1], vs[row + 1] };
                    push(q);
                }
            }
        }

        void EmitTile(const UiRect& r, const std::uint32_t textureIndex,
                      const UiImageOpts& opts, auto&& push)
        {
            float tileW = opts.sourceSize.x > 0.f ? opts.sourceSize.x : r.w;
            float tileH = opts.sourceSize.y > 0.f ? opts.sourceSize.y : r.h;
            tileW       = std::max(tileW, 1.f);
            tileH       = std::max(tileH, 1.f);

            for (float y = r.y; y < r.y + r.h - 1e-4f; y += tileH)
            {
                const float h = std::min(tileH, r.y + r.h - y);
                for (float x = r.x; x < r.x + r.w - 1e-4f; x += tileW)
                {
                    const float w  = std::min(tileW, r.x + r.w - x);
                    const float u1 = opts.uvRect.x +
                                     (opts.uvRect.z - opts.uvRect.x) *
                                         (w / tileW);
                    const float v1 = opts.uvRect.y +
                                     (opts.uvRect.w - opts.uvRect.y) *
                                         (h / tileH);
                    UiQuad q =
                        MakeBase({ x, y, w, h }, opts.tint, textureIndex);
                    q.uvRect = { opts.uvRect.x, opts.uvRect.y, u1, v1 };
                    if (opts.rounding > 0.f)
                    {
                        q.rounding = opts.rounding;
                        q.flags |= kUiFlagSdfRounded;
                    }
                    push(q);
                }
            }
        }
    } // namespace

    UiDraw::UiDraw(const std::uint32_t maxQuads) :
        mMaxQuads(std::max(1u, maxQuads))
    {
        mQuads.reserve(std::min(mMaxQuads, 256u));
        mOverlayQuads.reserve(64);
    }

    void UiDraw::Clear()
    {
        SpinLockGuard lock(mLock);
        mQuads.clear();
        mOverlayQuads.clear();
        mOverlayDepth = 0;
    }

    void UiDraw::BeginOverlay()
    {
        SpinLockGuard lock(mLock);
        ++mOverlayDepth;
    }

    void UiDraw::EndOverlay()
    {
        SpinLockGuard lock(mLock);
        if (mOverlayDepth > 0)
            --mOverlayDepth;
    }

    bool UiDraw::Empty() const
    {
        SpinLockGuard lock(mLock);
        return mQuads.empty() && mOverlayQuads.empty();
    }

    void UiDraw::Snapshot(std::vector<UiQuad>& out) const
    {
        SpinLockGuard lock(mLock);
        out = mQuads;
        out.insert(out.end(), mOverlayQuads.begin(), mOverlayQuads.end());
    }

    void UiDraw::pushUnlocked(const UiQuad& quad)
    {
        auto& dst = mOverlayDepth > 0 ? mOverlayQuads : mQuads;
        if (dst.size() >= mMaxQuads)
            return;
        dst.push_back(quad);
    }

    void UiDraw::Quad(const UiQuad& quad)
    {
        SpinLockGuard lock(mLock);
        pushUnlocked(quad);
    }

    void UiDraw::Quads(const std::span<const UiQuad> quads)
    {
        SpinLockGuard lock(mLock);
        for (const auto& q : quads)
            pushUnlocked(q);
    }

    void UiDraw::Rect(const UiRect& rect, const glm::vec4& color,
                      const float rounding, const float borderWidth,
                      const glm::vec4& borderColor)
    {
        if (rect.w <= 0.f || rect.h <= 0.f)
            return;

        UiQuad q = MakeBase(rect, color, 0);
        if (rounding > 0.f || borderWidth > 0.f)
        {
            q.rounding     = rounding;
            q.borderWidth  = borderWidth;
            q.outlineColor = borderColor;
            q.flags |= kUiFlagSdfRounded;
        }

        SpinLockGuard lock(mLock);
        pushUnlocked(q);
    }

    void UiDraw::pushImageUnlocked(const UiRect&        rect,
                                   const std::uint32_t  textureIndex,
                                   const UiImageOpts&   opts)
    {
        if (rect.w <= 0.f || rect.h <= 0.f)
            return;

        auto push = [this](const UiQuad& q) { pushUnlocked(q); };

        switch (opts.fit)
        {
            case UiImageFit::Contain:
            {
                UiRect    dest {};
                glm::vec4 uv {};
                FitContainCover(dest, uv, rect, opts, false);
                UiImageOpts local = opts;
                local.uvRect      = uv;
                local.fit         = UiImageFit::Stretch;
                EmitStretch(dest, textureIndex, local, push);
                break;
            }
            case UiImageFit::Cover:
            {
                UiRect    dest {};
                glm::vec4 uv {};
                FitContainCover(dest, uv, rect, opts, true);
                UiImageOpts local = opts;
                local.uvRect      = uv;
                local.fit         = UiImageFit::Stretch;
                EmitStretch(dest, textureIndex, local, push);
                break;
            }
            case UiImageFit::Slice:
                EmitSlice(rect, textureIndex, opts, push);
                break;
            case UiImageFit::Tile:
                EmitTile(rect, textureIndex, opts, push);
                break;
            case UiImageFit::Stretch:
            default:
                EmitStretch(rect, textureIndex, opts, push);
                break;
        }
    }

    void UiDraw::Image(const UiRect& rect, const std::uint32_t textureIndex,
                       const UiImageOpts& opts)
    {
        SpinLockGuard lock(mLock);
        pushImageUnlocked(rect, textureIndex, opts);
    }

    void UiDraw::Image(const UiRect& rect, const TextureHandle texture,
                       const UiImageOpts& opts)
    {
        Image(rect, TexturePool::BindlessIndex(texture), opts);
    }

    void UiDraw::ProgressBar(const UiRect& rect, const float fill01,
                             const glm::vec4& bg, const glm::vec4& fg,
                             const float rounding)
    {
        if (rect.w <= 0.f || rect.h <= 0.f)
            return;

        const float fill = std::clamp(fill01, 0.f, 1.f);
        UiQuad      plate = MakeBase(rect, bg, 0);
        if (rounding > 0.f)
        {
            plate.rounding = rounding;
            plate.flags |= kUiFlagSdfRounded;
        }

        SpinLockGuard lock(mLock);
        plate.color   = bg;
        plate.clipMax = 1.f;
        pushUnlocked(plate);

        plate.color   = fg;
        plate.clipMax = fill;
        plate.flags |= kUiFlagClipU;
        pushUnlocked(plate);
    }

    void UiDraw::CooldownRadial(const UiRect& rect, const float remaining01,
                                const glm::vec4& color, const float rounding)
    {
        if (rect.w <= 0.f || rect.h <= 0.f)
            return;
        const float rem = std::clamp(remaining01, 0.f, 1.f);
        if (rem <= 1e-4f)
            return;

        UiQuad q = MakeBase(rect, color, 0);
        q.flags |= kUiFlagClipRadial;
        q.clipMax = rem;
        if (rounding > 0.f)
        {
            q.rounding = rounding;
            q.flags |= kUiFlagSdfRounded;
        }

        SpinLockGuard lock(mLock);
        pushUnlocked(q);
    }

} // namespace FREYA_NAMESPACE
