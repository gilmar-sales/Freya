#include "Freya/Asset/SceneBvh.hpp"

#include <algorithm>
#include <limits>

namespace FREYA_NAMESPACE
{
    InstanceAabb TransformAabb(const glm::mat4& model,
                               const glm::vec3& localMin,
                               const glm::vec3& localMax)
    {
        const glm::vec3 corners[8] = {
            { localMin.x, localMin.y, localMin.z },
            { localMax.x, localMin.y, localMin.z },
            { localMin.x, localMax.y, localMin.z },
            { localMax.x, localMax.y, localMin.z },
            { localMin.x, localMin.y, localMax.z },
            { localMax.x, localMin.y, localMax.z },
            { localMin.x, localMax.y, localMax.z },
            { localMax.x, localMax.y, localMax.z },
        };

        InstanceAabb out;
        out.min = glm::vec3(std::numeric_limits<float>::max());
        out.max = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& c : corners)
        {
            const glm::vec3 w = glm::vec3(model * glm::vec4(c, 1.0f));
            out.min           = glm::min(out.min, w);
            out.max           = glm::max(out.max, w);
        }
        return out;
    }

    void SceneBvh::Clear()
    {
        mNodes.clear();
        mLeafInstances.clear();
        mMaxDepth     = 0;
        mTopologyHash = 0;
    }

    void SceneBvh::Build(const std::span<const InstanceAabb> aabbs,
                         const std::uint32_t                 leafSize)
    {
        Clear();
        if (aabbs.empty())
            return;

        const auto count = static_cast<std::uint32_t>(aabbs.size());
        const auto leaf =
            std::max(1u, leafSize == 0 ? kBvhDefaultLeafSize : leafSize);

        std::vector<BuildItem> items(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            items[i].instanceIndex = i;
            items[i].aabb          = aabbs[i];
            items[i].center        = 0.5f * (aabbs[i].min + aabbs[i].max);
        }

        mNodes.reserve(count * 2);
        mLeafInstances.reserve(count);
        mMaxDepth = 0;
        buildRecursive(items, 0, count, leaf, 1);
    }

    std::uint32_t SceneBvh::buildRecursive(std::vector<BuildItem>& items,
                                           const std::uint32_t     begin,
                                           const std::uint32_t     end,
                                           const std::uint32_t     leafSize,
                                           const std::uint32_t     depth)
    {
        mMaxDepth = std::max(mMaxDepth, depth);

        InstanceAabb bounds;
        bounds.min = glm::vec3(std::numeric_limits<float>::max());
        bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
        for (std::uint32_t i = begin; i < end; ++i)
        {
            bounds.min = glm::min(bounds.min, items[i].aabb.min);
            bounds.max = glm::max(bounds.max, items[i].aabb.max);
        }

        const auto nodeIndex = static_cast<std::uint32_t>(mNodes.size());
        mNodes.push_back(BvhNode {});

        const auto count = end - begin;
        if (count <= leafSize)
        {
            const auto first =
                static_cast<std::uint32_t>(mLeafInstances.size());
            for (std::uint32_t i = begin; i < end; ++i)
                mLeafInstances.push_back(items[i].instanceIndex);

            mNodes[nodeIndex].aabbMin      = bounds.min;
            mNodes[nodeIndex].aabbMax      = bounds.max;
            mNodes[nodeIndex].leftOrFirst  = first;
            mNodes[nodeIndex].countOrRight = count | kBvhLeafFlag;
            return nodeIndex;
        }

        const glm::vec3 extent = bounds.max - bounds.min;
        int             axis   = 0;
        if (extent.y > extent.x)
            axis = 1;
        if (extent.z > extent[axis])
            axis = 2;

        const auto mid = begin + count / 2;
        std::nth_element(items.begin() + static_cast<std::ptrdiff_t>(begin),
                         items.begin() + static_cast<std::ptrdiff_t>(mid),
                         items.begin() + static_cast<std::ptrdiff_t>(end),
                         [axis](const BuildItem& a, const BuildItem& b) {
                             return a.center[axis] < b.center[axis];
                         });

        // Ensure both sides non-empty even if centers coincide.
        auto split = mid;
        if (split == begin)
            ++split;
        if (split == end)
            --split;

        const auto left =
            buildRecursive(items, begin, split, leafSize, depth + 1);
        const auto right =
            buildRecursive(items, split, end, leafSize, depth + 1);

        mNodes[nodeIndex].aabbMin      = bounds.min;
        mNodes[nodeIndex].aabbMax      = bounds.max;
        mNodes[nodeIndex].leftOrFirst  = left;
        mNodes[nodeIndex].countOrRight = right;
        return nodeIndex;
    }

    void SceneBvh::refitNode(const std::uint32_t nodeIndex,
                             const std::span<const InstanceAabb>
                                 aabbs)
    {
        auto& node = mNodes[nodeIndex];
        if ((node.countOrRight & kBvhLeafFlag) != 0u)
        {
            const auto   count = node.countOrRight & ~kBvhLeafFlag;
            const auto   first = node.leftOrFirst;
            InstanceAabb bounds;
            bounds.min = glm::vec3(std::numeric_limits<float>::max());
            bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto idx = mLeafInstances[first + i];
                bounds.min     = glm::min(bounds.min, aabbs[idx].min);
                bounds.max     = glm::max(bounds.max, aabbs[idx].max);
            }
            node.aabbMin = bounds.min;
            node.aabbMax = bounds.max;
            return;
        }

        const auto left  = node.leftOrFirst;
        const auto right = node.countOrRight;
        refitNode(left, aabbs);
        refitNode(right, aabbs);
        node.aabbMin = glm::min(mNodes[left].aabbMin, mNodes[right].aabbMin);
        node.aabbMax = glm::max(mNodes[left].aabbMax, mNodes[right].aabbMax);
    }

    void SceneBvh::Refit(const std::span<const InstanceAabb> aabbs)
    {
        if (mNodes.empty() || aabbs.size() != mLeafInstances.size())
        {
            Build(aabbs);
            return;
        }
        refitNode(0, aabbs);
    }

} // namespace FREYA_NAMESPACE
