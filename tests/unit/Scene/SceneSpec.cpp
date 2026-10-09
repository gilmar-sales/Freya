#include "../Support/AnimationFixtures.hpp"

#include <Freya/Scene/Scene.hpp>

#include <limits>
#include <utility>

TEST(Scene, AddPreservesInstanceMetadataAndCountsLiveEntries)
{
    fra::Scene scene;
    EXPECT_EQ(scene.Size(), 0u);
    fra::Scene::Instance instance;
    instance.mesh        = fra::MeshHandle(0);
    instance.material    = fra::MaterialHandle(3);
    instance.entityId    = 42;
    instance.techniqueId = 2;
    instance.boneOffset  = 10;
    instance.boneCount   = 4;
    instance.mobility    = fra::Mobility::Static;
    const auto  id       = scene.Add(instance);
    const auto* stored   = std::as_const(scene).Get(id);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(scene.Size(), 1u);
    EXPECT_EQ(stored->mesh, instance.mesh);
    EXPECT_EQ(stored->material, instance.material);
    EXPECT_EQ(stored->entityId, 42u);
    EXPECT_EQ(stored->techniqueId, 2u);
    EXPECT_EQ(stored->boneOffset, 10u);
    EXPECT_EQ(stored->boneCount, 4u);
    EXPECT_EQ(stored->mobility, fra::Mobility::Static);
    EXPECT_EQ(stored->flags, instance.flags);
}

TEST(Scene, RemoveIsIdempotentAndInvalidIdsAreHarmless)
{
    fra::Scene scene;
    const auto a = scene.Add({});
    const auto b = scene.Add({});
    scene.Remove(a);
    scene.Remove(a);
    const auto invalid = std::numeric_limits<fra::Scene::InstanceId>::max();
    scene.Remove(invalid);
    scene.SetTransform(invalid, fra::SceneTransform {});
    scene.SetTransform(a, fra::SceneTransform {});
    EXPECT_EQ(scene.Size(), 1u);
    EXPECT_EQ(scene.Get(a), nullptr);
    EXPECT_EQ(std::as_const(scene).Get(a), nullptr);
    EXPECT_EQ(scene.Get(invalid), nullptr);
    EXPECT_NE(scene.Get(b), nullptr);
}

TEST(Scene, AddReusesRemovedSlotWithoutChangingOtherInstances)
{
    fra::Scene scene;
    const auto a = scene.Add({ .entityId = 1 });
    const auto b = scene.Add({ .entityId = 2 });
    scene.Remove(a);
    const auto reused = scene.Add({ .entityId = 3 });
    EXPECT_EQ(reused, a);
    ASSERT_NE(scene.Get(reused), nullptr);
    ASSERT_NE(scene.Get(b), nullptr);
    EXPECT_EQ(scene.Get(reused)->entityId, 3u);
    EXPECT_EQ(scene.Get(b)->entityId, 2u);
    EXPECT_EQ(scene.Size(), 2u);
}

TEST(Scene, TransformSettersAndMutableGetUpdateStoredInstance)
{
    fra::Scene          scene;
    const auto          id = scene.Add({});
    fra::SceneTransform transform;
    transform.position = { 2, 3, 4 };
    transform.scale    = { 2, 3, 4 };
    transform.rotation = glm::angleAxis(0.5f, glm::vec3(0, 1, 0));
    scene.SetTransform(id, transform);
    ASSERT_NE(scene.Get(id), nullptr);
    fra::test::ExpectMatrixNear(scene.Get(id)->transform.ToMatrix(),
                                transform.ToMatrix());
    transform.position.x = 20.f;
    scene.SetTransform(id, transform.ToMatrix());
    fra::test::ExpectMatrixNear(scene.Get(id)->transform.ToMatrix(),
                                transform.ToMatrix());
    scene.Get(id)->entityId = 99;
    EXPECT_EQ(std::as_const(scene).Get(id)->entityId, 99u);
}

TEST(Scene, ForEachSkipsRemovedEntriesAndPreservesIds)
{
    fra::Scene scene;
    const auto a = scene.Add({ .entityId = 10 });
    const auto b = scene.Add({ .entityId = 20 });
    const auto c = scene.Add({ .entityId = 30 });
    scene.Remove(b);
    std::vector<fra::Scene::InstanceId> ids;
    std::vector<std::uint32_t>          entities;
    std::as_const(scene).ForEach([&](auto id, const auto& instance) {
        ids.push_back(id);
        entities.push_back(instance.entityId);
    });
    EXPECT_EQ(ids, (std::vector<fra::Scene::InstanceId> { a, c }));
    EXPECT_EQ(entities, (std::vector<std::uint32_t> { 10, 30 }));
}

TEST(Scene, ClearInvalidatesEntriesAndAllowsReuse)
{
    fra::Scene scene;
    const auto id = scene.Add({});
    scene.Clear();
    scene.Clear();
    EXPECT_EQ(scene.Size(), 0u);
    EXPECT_EQ(scene.Get(id), nullptr);
    unsigned visits = 0;
    scene.ForEach([&](auto, const auto&) { ++visits; });
    EXPECT_EQ(visits, 0u);
    EXPECT_EQ(scene.Add({}), 0u);
    EXPECT_EQ(scene.Size(), 1u);
}
