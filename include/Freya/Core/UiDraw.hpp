#pragma once

#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Core/SpinLock.hpp"
#include "Freya/Core/UiTypes.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    constexpr std::uint32_t kUiFlagSdfGlyph   = 1u;
    constexpr std::uint32_t kUiFlagSdfRounded = 2u;
    constexpr std::uint32_t kUiFlagClipU      = 4u;
    /** Radial wipe: `clipMax` = remaining fraction (1=full, 0=none). */
    constexpr std::uint32_t kUiFlagClipRadial = 8u;

    /**
     * @brief One screen-space UI quad.
     *
     * `rect` is logical pixels (xywh, top-left origin). The pass resolves
     * logical → framebuffer using the reference/viewport scale.
     *
     * `textureIndex` is a bindless heap slot (0 = white). Convert
     * TexturePool handles with TexturePool::BindlessIndex.
     */
    struct UiQuad
    {
        glm::vec4     rect { 0.f }; ///< xywh logical pixels
        glm::vec4     uvRect { 0.f, 0.f, 1.f, 1.f };
        glm::vec4     color { 1.f };
        std::uint32_t textureIndex = 0;
        std::uint32_t flags        = 0;
        float         rounding     = 0.f;
        float         borderWidth  = 0.f;
        float         clipMax      = 1.f;
        float         outlineWidth = 0.f; ///< SDF units, 0 = no outline
        glm::vec4     outlineColor { 0.f, 0.f, 0.f, 1.f };
        float         z = 0.f;
    };

    /**
     * @brief Per-frame CPU UI quad queue (cleared each BeginFrame).
     *
     * Concurrent Rect/Image/Text/ProgressBar/Quads submits are safe
     * (SpinLock). Readers must use Snapshot — never iterate the live queue.
     */
    class UiDraw
    {
      public:
        static constexpr std::uint32_t kDefaultMaxQuads = 1u << 14; // 16384

        explicit UiDraw(std::uint32_t maxQuads = kDefaultMaxQuads);

        void Clear();

        /**
         * @brief Route subsequent quads to the overlay list (drawn last).
         * Main-thread / matching Begin/End pairs. Nested depth supported.
         */
        void BeginOverlay();
        void EndOverlay();

        [[nodiscard]] bool Empty() const;

        /**
         * @brief Copy the current queue under lock into @p out
         * (base quads then overlays).
         */
        void Snapshot(std::vector<UiQuad>& out) const;

        [[nodiscard]] std::uint32_t MaxQuads() const { return mMaxQuads; }

        void Quad(const UiQuad& quad);

        /**
         * @brief Append many quads under one lock (soft-capped at MaxQuads).
         */
        void Quads(std::span<const UiQuad> quads);

        /**
         * @brief Solid / rounded / bordered rectangle (white texture).
         */
        void Rect(const UiRect& rect, const glm::vec4& color,
                  float            rounding    = 0.f,
                  float            borderWidth = 0.f,
                  const glm::vec4& borderColor = { 0.f, 0.f, 0.f, 1.f });

        /**
         * @brief Textured rect with fit mode (Stretch/Contain/Cover/Slice/Tile).
         */
        void Image(const UiRect& rect, std::uint32_t textureIndex,
                   const UiImageOpts& opts = {});

        void Image(const UiRect& rect, TextureHandle texture,
                   const UiImageOpts& opts = {});

        /**
         * @brief Background + left-aligned fill (uses ClipU on the fill).
         */
        void ProgressBar(const UiRect& rect, float fill01, const glm::vec4& bg,
                         const glm::vec4& fg, float rounding = 0.f);

        /**
         * @brief Square/rounded cooldown sweep over @p rect (clockwise from
         * top). @p remaining01: 1 = fully covered, 0 = invisible.
         */
        void CooldownRadial(const UiRect& rect, float remaining01,
                            const glm::vec4& color = { 0.f, 0.f, 0.f, 0.65f },
                            float            rounding = 0.f);

        /**
         * @brief Screen-space LTR SDF text from @p rect top-left.
         *
         * @param heightPx     Glyph height in logical pixels.
         * @param maxWidth     Wrap width in logical px; 0 = no wrap.
         * @param outlineWidthPx Outline in atlas pixels (SDF padding range).
         */
        void Text(const UiRect& rect, std::string_view utf8,
                  const class FontAtlas& font, float heightPx,
                  const glm::vec4& color, float maxWidth = 0.f,
                  float            outlineWidthPx = 0.f,
                  const glm::vec4& outlineColor   = { 0.f, 0.f, 0.f, 1.f });

      private:
        void pushUnlocked(const UiQuad& quad);
        void pushImageUnlocked(const UiRect& rect, std::uint32_t textureIndex,
                               const UiImageOpts& opts);

        std::uint32_t       mMaxQuads = kDefaultMaxQuads;
        std::vector<UiQuad> mQuads;
        std::vector<UiQuad> mOverlayQuads;
        int                 mOverlayDepth = 0;
        mutable SpinLock    mLock;
    };

} // namespace FREYA_NAMESPACE
