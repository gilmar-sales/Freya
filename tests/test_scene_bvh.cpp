#include <Freya/Asset/SceneBvh.hpp>

#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <unordered_set>
#include <vector>

TEST_CASE("BvhNode keeps GLSL std430 layout", "[scene-bvh]")
{
    STATIC_REQUIRE(sizeof(fra::BvhNode) == 32);
    STATIC_REQUIRE(sizeof(fra::BvhCullPushConstants) == 96);
    STATIC_REQUIRE(sizeof(fra::PrepareBvhDispatchPushConstants) == 16);
    STATIC_REQUIRE(fra::kBvhLeafFlag == 0x80000000u);
}

TEST_CASE("SceneBvh empty build clears the tree", "[scene-bvh]")
{
    fra::SceneBvh bvh;
    bvh.Build({});
    REQUIRE(bvh.Empty());
    REQUIRE(bvh.NodeCount() == 0);
    REQUIRE(bvh.MaxDepth() == 0);
}

TEST_CASE("SceneBvh median-split covers every instance once", "[scene-bvh]")
{
    constexpr std::uint32_t        kCount = 64;
    std::vector<fra::InstanceAabb> aabbs(kCount);
    for (std::uint32_t i = 0; i < kCount; ++i)
    {
        const float x = static_cast<float>(i % 8);
        const float y = static_cast<float>(i / 8);
        aabbs[i].min  = { x, y, 0.0f };
        aabbs[i].max  = { x + 0.8f, y + 0.8f, 0.8f };
    }

    fra::SceneBvh bvh;
    bvh.Build(aabbs, 8);

    REQUIRE_FALSE(bvh.Empty());
    REQUIRE(bvh.LeafInstanceCount() == kCount);
    REQUIRE(bvh.MaxDepth() >= 1u);

    std::unordered_set<std::uint32_t> seen;
    for (const auto idx : bvh.LeafInstances())
    {
        REQUIRE(idx < kCount);
        REQUIRE(seen.insert(idx).second);
    }
    REQUIRE(seen.size() == kCount);

    // Every internal node bounds must contain its children.
    const auto& nodes = bvh.Nodes();
    for (std::uint32_t i = 0; i < nodes.size(); ++i)
    {
        const auto& n = nodes[i];
        if ((n.countOrRight & fra::kBvhLeafFlag) != 0u)
        {
            const auto count = n.countOrRight & ~fra::kBvhLeafFlag;
            REQUIRE(count > 0u);
            REQUIRE(count <= 8u);
            continue;
        }
        const auto& left  = nodes[n.leftOrFirst];
        const auto& right = nodes[n.countOrRight];
        REQUIRE(n.aabbMin.x <= left.aabbMin.x);
        REQUIRE(n.aabbMin.y <= left.aabbMin.y);
        REQUIRE(n.aabbMin.z <= left.aabbMin.z);
        REQUIRE(n.aabbMax.x >= left.aabbMax.x);
        REQUIRE(n.aabbMax.y >= left.aabbMax.y);
        REQUIRE(n.aabbMax.z >= left.aabbMax.z);
        REQUIRE(n.aabbMin.x <= right.aabbMin.x);
        REQUIRE(n.aabbMin.y <= right.aabbMin.y);
        REQUIRE(n.aabbMin.z <= right.aabbMin.z);
        REQUIRE(n.aabbMax.x >= right.aabbMax.x);
        REQUIRE(n.aabbMax.y >= right.aabbMax.y);
        REQUIRE(n.aabbMax.z >= right.aabbMax.z);
    }
}

TEST_CASE("SceneBvh refit updates leaf bounds without rebuild", "[scene-bvh]")
{
    std::vector<fra::InstanceAabb> aabbs = {
        { { 0, 0, 0 }, { 1, 1, 1 } },
        { { 2, 0, 0 }, { 3, 1, 1 } },
        { { 0, 2, 0 }, { 1, 3, 1 } },
        { { 2, 2, 0 }, { 3, 3, 1 } },
    };

    fra::SceneBvh bvh;
    bvh.Build(aabbs, 2);
    const auto nodeCount = bvh.NodeCount();
    const auto depth     = bvh.MaxDepth();

    aabbs[0] = { { 10, 10, 10 }, { 11, 11, 11 } };
    bvh.Refit(aabbs);

    REQUIRE(bvh.NodeCount() == nodeCount);
    REQUIRE(bvh.MaxDepth() == depth);

    // Root must expand to cover the moved leaf.
    const auto& root = bvh.Nodes()[bvh.RootIndex()];
    REQUIRE(root.aabbMin.x <= 10.0f);
    REQUIRE(root.aabbMax.x >= 11.0f);
}

TEST_CASE("TransformAabb expands under non-uniform scale", "[scene-bvh]")
{
    glm::mat4 model(1.0f);
    model = glm::scale(model, glm::vec3(2.0f, 0.5f, 1.0f));
    model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));

    const auto box =
        fra::TransformAabb(model, glm::vec3(-1.0f), glm::vec3(1.0f));
    REQUIRE(box.min.x < box.max.x);
    REQUIRE(box.min.y < box.max.y);
    REQUIRE(box.min.z < box.max.z);
}
