#pragma once

#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct DebugDrawVertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    /**
     * @brief CPU line queue for the DebugDrawPass overlay.
     *
     * Cleared each BeginFrame. Apps push world-space lines before EndFrame.
     */
    class DebugDraw
    {
      public:
        static constexpr std::uint32_t kDefaultMaxVertices = 1u << 18; // 256K

        explicit DebugDraw(std::uint32_t maxVertices = kDefaultMaxVertices);

        void Clear();

        [[nodiscard]] bool Empty() const { return mVerts.empty(); }
        [[nodiscard]] std::span<const DebugDrawVertex> Vertices() const
        {
            return mVerts;
        }
        [[nodiscard]] std::uint32_t MaxVertices() const { return mMaxVertices; }

        void Line(const glm::vec3& a, const glm::vec3& b,
                  const glm::vec4& color);
        void Cross(const glm::vec3& center, float size, const glm::vec4& color);
        void Axes(const glm::mat4& transform, float size);
        void Diamond(const glm::vec3& center, float size,
                     const glm::vec4& color);

        void Circle(const glm::vec3& center, const glm::vec3& normal,
                    float radius, const glm::vec4& color,
                    std::uint32_t segments = 28);
        void Sphere(const glm::vec3& center, float radius,
                    const glm::vec4& color, std::uint32_t segments = 24);
        void Cone(const glm::vec3& apex, const glm::vec3& direction,
                  float length, float halfAngleRad, const glm::vec4& color,
                  std::uint32_t segments = 20);
        void Arrow(const glm::vec3& from, const glm::vec3& to,
                   const glm::vec4& color);
        void Rect(const glm::vec3& center, const glm::vec3& normal,
                  const glm::vec3& tangent, float halfWidth, float halfHeight,
                  const glm::vec4& color);

        /**
         * @brief Draw skeleton bones from a local pose (parent→child segments).
         */
        void DrawSkeleton(const ::FREYA_NAMESPACE::Skeleton& skeleton,
                          const LocalPose& local, const glm::mat4& modelWorld,
                          const glm::vec4& color);

        /**
         * @brief Line from joint world position toward a look target.
         */
        void DrawLookRay(const ::FREYA_NAMESPACE::Skeleton& skeleton,
                         const LocalPose& local, const glm::mat4& modelWorld,
                         std::uint32_t joint, const glm::vec3& targetWorld,
                         const glm::vec4& color);

        /**
         * @brief Visualize a two-bone IK chain + tip target + pole.
         */
        void DrawTwoBoneIk(
            const ::FREYA_NAMESPACE::Skeleton& skeleton, const LocalPose& local,
            const glm::mat4& modelWorld, std::uint32_t root, std::uint32_t mid,
            std::uint32_t tip, const glm::vec3& targetWorld,
            const glm::vec3& poleWorld, const glm::vec4& chainColor,
            const glm::vec4& targetColor);

      private:
        bool pushVertex(const DebugDrawVertex& v);

        std::uint32_t                mMaxVertices = kDefaultMaxVertices;
        std::vector<DebugDrawVertex> mVerts;
    };

} // namespace FREYA_NAMESPACE
