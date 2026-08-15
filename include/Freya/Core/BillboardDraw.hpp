#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    enum class BillboardAlign : std::uint32_t
    {
        Screen      = 0,
        Cylindrical = 1,
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

    constexpr std::uint32_t kBillboardFlagCylindrical = 1u;

    /**
     * @brief GPU instance (std430). Keep in sync with billboard.vert.
     */
    struct BillboardGpuInstance
    {
        glm::vec3     worldPos { 0.f };
        float         clipMax = 1.f;
        glm::vec2     size { 1.f };
        std::uint32_t textureIndex = 0;
        std::uint32_t flags        = 0;
        glm::vec4     color { 1.f };
        glm::vec4     uvRect { 0.f, 0.f, 1.f, 1.f };
    };

    static_assert(sizeof(BillboardGpuInstance) == 64,
                  "BillboardGpuInstance must match GLSL std430");

    /**
     * @brief One camera-facing quad in world space.
     *
     * `textureIndex` is a bindless heap slot (0 = white). Convert TexturePool
     * ids with MaterialDescriptorResources::TextureHeapIndex.
     */
    struct Billboard
    {
        glm::vec3        worldPos { 0.f };
        glm::vec2        size { 1.f };
        glm::vec4        color { 1.f };
        glm::vec4        uvRect { 0.f, 0.f, 1.f, 1.f };
        std::uint32_t    textureIndex = 0;
        BillboardAlign   align        = BillboardAlign::Screen;
        BillboardBlend   blend        = BillboardBlend::Alpha;
        BillboardLayer   layer        = BillboardLayer::Vfx;
        bool             depthTest    = true;
        float            clipMax      = 1.f;
    };

    /**
     * @brief CPU billboard queue. Cleared each BeginFrame.
     */
    class BillboardDraw
    {
      public:
        static constexpr std::uint32_t kDefaultMaxQuads = 1u << 14;

        explicit BillboardDraw(std::uint32_t maxQuads = kDefaultMaxQuads);

        void Clear();

        [[nodiscard]] bool Empty() const { return mQuads.empty(); }

        [[nodiscard]] std::span<const Billboard> Quads() const
        {
            return mQuads;
        }

        [[nodiscard]] std::uint32_t MaxQuads() const { return mMaxQuads; }

        void Quad(const Billboard& billboard);

        /**
         * @brief Cylindrical nameplate: background + left-aligned fill.
         */
        void HealthBar(const glm::vec3& headPos, float width, float height,
                       float fill01, const glm::vec4& bg, const glm::vec4& fg);

      private:
        std::uint32_t         mMaxQuads = kDefaultMaxQuads;
        std::vector<Billboard> mQuads;
    };

} // namespace FREYA_NAMESPACE
