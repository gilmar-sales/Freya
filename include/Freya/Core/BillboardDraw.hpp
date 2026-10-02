#pragma once

#include "Freya/Core/Flags.hpp"
#include "Freya/Core/SpinLock.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    enum class BillboardAlign : std::uint32_t
    {
        Screen      = 0,
        Cylindrical = 1,
        /// Per-instance point-toward-camera (correct at wide FOV).
        Spherical = 2,
        /// Cylindrical with a custom up axis (see Billboard::axisUp).
        FixedAxis = 3,
        /// Flat on a surface; axisUp is the surface normal.
        Planar = 4,
    };

    enum class BillboardBlend : std::uint32_t
    {
        Alpha    = 0,
        Additive = 1,
    };

    enum class BillboardLayer : std::uint32_t
    {
        Vfx = 0,
        Ui  = 1,
    };

    enum class BillboardFlags : std::uint32_t
    {
        None      = 0,
        AlignMask = 7u,
        DepthTest = 256u,
        Sdf       = 8u,
        /// Depth-based soft fade via subpass input attachment.
        Soft = 16u,
        /// Constant screen size; Billboard::size is in NDC half-extents.
        ScreenSize = 32u,
        /// Stretch along velocity; Billboard::velocity / velocityStretchScale.
        VelocityStretch = 64u,
        TransposeUv     = 128u
    };

    /**
     * @brief One camera-facing quad in world space.
     *
     * `textureIndex` is a bindless heap slot (0 = white). Convert TexturePool
     * handles with TexturePool::BindlessIndex.
     */
    struct Billboard
    {
        glm::vec3      worldPos { 0.f };
        glm::vec2      size { 1.f };
        glm::vec4      color { 1.f };
        glm::vec4      uvRect { 0.f, 0.f, 1.f, 1.f };
        std::uint32_t  textureIndex = 0;
        BillboardAlign align        = BillboardAlign::Screen;
        BillboardBlend blend        = BillboardBlend::Alpha;
        BillboardLayer layer        = BillboardLayer::Vfx;
        BillboardFlags flags        = BillboardFlags::DepthTest;
        float          clipMax      = 1.f;
        glm::vec2      localOffset { 0.f };
        float          outlineWidth = 0.f; ///< SDF units, 0 = no outline
        glm::vec4      outlineColor { 0.f, 0.f, 0.f, 1.f };
        float          rotation = 0.f;    ///< Screen-space rotation in radians
        float softFadeRange     = 0.002f; ///< NDC depth range for soft fade
        /// Custom up axis for FixedAxis / surface normal for Planar.
        glm::vec3 axisUp { 0.f, 1.f, 0.f };
        glm::vec3 velocity { 0.f };
        float     velocityStretchScale = 1.f;
    };

    /**
     * @brief One explicit-quad segment of a camera-facing strip.
     *
     * Unlike Billboard (a center + size + rotation rectangle), the four
     * corners are independent world positions, so consecutive segments can
     * share edge vertices exactly and connect without gaps or overlaps.
     * Corners c0/c1 form the edge at t0 (color0), c2/c3 the edge at t1
     * (color1). Triangles are (c0, c1, c2) and (c1, c3, c2); face culling
     * is disabled so winding does not matter.
     *
     * Produced by Strip() (miter joints computed on the CPU) or filled in
     * manually via ConnectedQuad()/ConnectedQuads().
     */
    struct ConnectedBillboard
    {
        glm::vec3      c0 { 0.f };
        glm::vec3      c1 { 0.f };
        glm::vec3      c2 { 0.f };
        glm::vec3      c3 { 0.f };
        glm::vec4      color0 { 1.f };
        glm::vec4      color1 { 1.f };
        glm::vec4      uvRect { 0.f, 0.f, 1.f, 1.f };
        std::uint32_t  textureIndex = 0;
        BillboardBlend blend        = BillboardBlend::Alpha;
        BillboardLayer layer        = BillboardLayer::Vfx;
        BillboardFlags flags        = BillboardFlags::DepthTest;
        float          clipMax      = 1.f;
    };

    /**
     * @brief One centerline sample of a Strip() ribbon.
     *
     * width is the full ribbon width at this point (2 * radius for tubes).
     * u is the texture coordinate along the strip.
     */
    struct StripPoint
    {
        glm::vec3 pos { 0.f };
        float     width = 1.f;
        glm::vec4 color { 1.f };
        float     u = 0.f;
    };

    /**
     * @brief Shared style for every quad of one Strip() call.
     *
     * miterLimit caps the joint extension as a multiple of the local
     * half-width (avoids spikes on hairpin turns; the excess is beveled).
     * transposeUv swaps the UV axes so U runs across and V along the
     * strip (matches the legacy RibbonEmitter orientation).
     */
    struct StripStyle
    {
        std::uint32_t  textureIndex = 0;
        BillboardBlend blend        = BillboardBlend::Alpha;
        BillboardLayer layer        = BillboardLayer::Vfx;
        BillboardFlags flags        = BillboardFlags::DepthTest;
        float          miterLimit   = 2.5f;
    };

    /**
     * @brief Per-frame CPU billboard queue (cleared each BeginFrame).
     *
     * Concurrent Quad/Quads/HealthBar/Text submits are safe (SpinLock).
     * Readers must use Snapshot — never iterate the live queue.
     */
    class BillboardDraw
    {
      public:
        static constexpr std::uint32_t kDefaultMaxQuads          = 1u << 16;
        static constexpr std::uint32_t kDefaultMaxConnectedQuads = 1u << 14;

        explicit BillboardDraw(std::uint32_t maxQuads = kDefaultMaxQuads);

        void Clear();

        [[nodiscard]] bool Empty() const;

        [[nodiscard]] bool ConnectedEmpty() const;

        /**
         * @brief Copy the current queue under lock into @p out.
         */
        void Snapshot(std::vector<Billboard>& out) const;

        /**
         * @brief Copy the current connected-quad queue under lock.
         */
        void SnapshotConnected(std::vector<ConnectedBillboard>& out) const;

        [[nodiscard]] std::uint32_t MaxQuads() const { return mMaxQuads; }

        void Quad(const Billboard& billboard);

        /**
         * @brief Append many quads under one lock (soft-capped at MaxQuads).
         */
        void Quads(std::span<const Billboard> billboards);

        /**
         * @brief Append one seamless strip segment (explicit corners).
         */
        void ConnectedQuad(const ConnectedBillboard& quad);

        /**
         * @brief Append many connected quads under one lock so a snapshot
         * never sees a partial strip.
         */
        void ConnectedQuads(std::span<const ConnectedBillboard> quads);

        /**
         * @brief Build a seamless camera-facing ribbon through @p points.
         *
         * Consecutive segments share mitered edge vertices exactly, so the
         * strip has no gaps or double-blended overlaps at joints (unlike
         * independent rotated quads). Offsets are computed in the screen
         * plane spanned by @p camRight / @p camUp, so all quads stay
         * coplanar for the current camera.
         *
         * Needs at least 2 points; zero-length runs are skipped. No-op
         * when fewer than one segment survives.
         */
        void Strip(std::span<const StripPoint> points, const StripStyle& style,
                   const glm::vec3& camRight, const glm::vec3& camUp);

        /**
         * @brief Nameplate: background + left-aligned fill.
         *
         * Default align is Cylindrical (yaw-only). Pass Screen for full
         * camera-facing.
         */
        void HealthBar(const glm::vec3& headPos, float width, float height,
                       float fill01, const glm::vec4& bg, const glm::vec4& fg,
                       BillboardAlign align = BillboardAlign::Cylindrical);

        /**
         * @brief Latin-1 LTR nameplate: one SDF quad per glyph, centered.
         *
         * @param outlineWidthPx Outline in atlas pixels (SDF padding range).
         *                       2 with the default 8px pad is a typical halo.
         */
        void Text(const glm::vec3& worldPos, std::string_view utf8,
                  const class FontAtlas& font, float heightMeters,
                  const glm::vec4& color, float outlineWidthPx = 0.f,
                  const glm::vec4& outlineColor = { 0.f, 0.f, 0.f, 1.f },
                  BillboardAlign   align        = BillboardAlign::Cylindrical,
                  BillboardLayer   layer        = BillboardLayer::Ui);

      private:
        void pushUnlocked(const Billboard& billboard);
        void pushConnectedUnlocked(const ConnectedBillboard& quad);

        std::uint32_t                   mMaxQuads = kDefaultMaxQuads;
        std::vector<Billboard>          mQuads;
        std::vector<ConnectedBillboard> mConnected;
        mutable SpinLock                mLock;
    };

} // namespace FREYA_NAMESPACE
