#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /// Leaf flag in `BvhNode::countOrRight` (high bit).
    constexpr std::uint32_t kBvhLeafFlag = 0x80000000u;

    /// Default instances per leaf before a split is forced.
    constexpr std::uint32_t kBvhDefaultLeafSize = 8;

    /// Min instance count before hierarchical GPU traversal is used.
    constexpr std::uint32_t kHierarchicalCullMinInstances = 512;

    /**
     * @brief GPU BVH node (std430, 32 B).
     *
     * Internal: leftOrFirst = left child, countOrRight = right child.
     * Leaf: leftOrFirst = first index into leaf-instance table,
     *       countOrRight = (instance count | kBvhLeafFlag).
     */
    struct BvhNode
    {
        glm::vec3     aabbMin { 0.0f };
        std::uint32_t leftOrFirst = 0;
        glm::vec3     aabbMax { 0.0f };
        std::uint32_t countOrRight = 0;
    };

    static_assert(sizeof(BvhNode) == 32, "BvhNode must match GLSL std430");
    static_assert(offsetof(BvhNode, leftOrFirst) == 12,
                  "BvhNode::leftOrFirst std430 offset");
    static_assert(offsetof(BvhNode, aabbMax) == 16,
                  "BvhNode::aabbMax std430 offset");
    static_assert(offsetof(BvhNode, countOrRight) == 28,
                  "BvhNode::countOrRight std430 offset");

    /**
     * @brief World-space AABB for one scene instance (CPU build/refit input).
     */
    struct InstanceAabb
    {
        glm::vec3 min { 0.0f };
        glm::vec3 max { 0.0f };
    };

    /**
     * @brief CPU median-split BVH over scene-instance AABBs.
     *
     * Rebuild when instance topology changes; Refit when only transforms move.
     */
    class SceneBvh
    {
      public:
        void Clear();

        /// Full rebuild (median split). Empty input clears the tree.
        void Build(std::span<const InstanceAabb> aabbs,
                   std::uint32_t leafSize = kBvhDefaultLeafSize);

        /// Bottom-up bound update; requires same instance count as last Build.
        void Refit(std::span<const InstanceAabb> aabbs);

        [[nodiscard]] bool Empty() const { return mNodes.empty(); }

        [[nodiscard]] std::uint32_t RootIndex() const { return 0; }

        [[nodiscard]] std::uint32_t MaxDepth() const { return mMaxDepth; }

        [[nodiscard]] std::uint32_t NodeCount() const
        {
            return static_cast<std::uint32_t>(mNodes.size());
        }

        [[nodiscard]] std::uint32_t LeafInstanceCount() const
        {
            return static_cast<std::uint32_t>(mLeafInstances.size());
        }

        [[nodiscard]] const std::vector<BvhNode>& Nodes() const
        {
            return mNodes;
        }

        [[nodiscard]] const std::vector<std::uint32_t>& LeafInstances() const
        {
            return mLeafInstances;
        }

        /// Topology fingerprint from last Build (entity/mesh sequence).
        [[nodiscard]] std::uint64_t TopologyHash() const
        {
            return mTopologyHash;
        }

        void SetTopologyHash(std::uint64_t hash) { mTopologyHash = hash; }

      private:
        struct BuildItem
        {
            std::uint32_t instanceIndex = 0;
            glm::vec3     center { 0.0f };
            InstanceAabb  aabb {};
        };

        std::uint32_t buildRecursive(std::vector<BuildItem>& items,
                                     std::uint32_t           begin,
                                     std::uint32_t           end,
                                     std::uint32_t           leafSize,
                                     std::uint32_t           depth);

        void refitNode(
            std::uint32_t nodeIndex, std::span<const InstanceAabb> aabbs);

        std::vector<BvhNode>       mNodes;
        std::vector<std::uint32_t> mLeafInstances;
        std::uint32_t              mMaxDepth     = 0;
        std::uint64_t              mTopologyHash = 0;
    };

    /// Transform local mesh AABB by model matrix → world AABB.
    [[nodiscard]] InstanceAabb TransformAabb(const glm::mat4& model,
                                             const glm::vec3& localMin,
                                             const glm::vec3& localMax);

    /**
     * @brief Push constants for BvhCullLevel.comp (96 B).
     */
    struct BvhCullPushConstants
    {
        glm::mat4     viewProj { 1.0f };
        glm::vec2     screenSize { 1.0f };
        std::uint32_t reverseZ      = 0;
        std::uint32_t hizEnabled    = 0;
        std::uint32_t queueCount    = 0;
        std::uint32_t maxCandidates = 0;
        std::uint32_t maxQueue      = 0;
        float         hizDepthBias  = 1e-4f;
    };

    static_assert(sizeof(BvhCullPushConstants) == 96,
                  "BvhCullPushConstants size");

    /**
     * @brief Push constants for PrepareBvhDispatch.comp (16 B).
     */
    struct PrepareBvhDispatchPushConstants
    {
        std::uint32_t localSizeX = 64;
        std::uint32_t _pad0      = 0;
        std::uint32_t _pad1      = 0;
        std::uint32_t _pad2      = 0;
    };

    static_assert(sizeof(PrepareBvhDispatchPushConstants) == 16,
                  "PrepareBvhDispatchPushConstants size");

} // namespace FREYA_NAMESPACE
