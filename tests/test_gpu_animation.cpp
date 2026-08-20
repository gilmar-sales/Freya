#include <Freya/Asset/GpuAnimation.hpp>
#include <Freya/Asset/Pose.hpp>
#include <Freya/Asset/Skeleton.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/quaternion.hpp>

TEST_CASE("GpuClipKey is stable FNV-1a and never zero", "[gpu-anim]")
{
    REQUIRE(fra::GpuClipKey("Idle") == fra::GpuClipKey("Idle"));
    REQUIRE(fra::GpuClipKey("Idle") != fra::GpuClipKey("Walk"));
    REQUIRE(fra::GpuClipKey("") != 0ull);
}

TEST_CASE("identity quaternion omits W in smallest-three pack", "[gpu-anim]")
{
    const auto bits = fra::PackQuatSmallestThree(glm::quat(1.f, 0.f, 0.f, 0.f));
    REQUIRE((bits >> 30) == 3u);
}

TEST_CASE("float joint pack copies TRS", "[gpu-anim]")
{
    fra::JointTRS j;
    j.translation = { 1.f, 2.f, 3.f };
    j.rotation    = glm::quat(1.f, 0.f, 0.f, 0.f);
    j.scale       = { 2.f, 2.f, 2.f };

    const auto g = fra::ToGpuFloatJoint(j);
    REQUIRE(g.t == j.translation);
    REQUIRE(g.s == j.scale);
    REQUIRE(g.q.w == Catch::Approx(1.f));
}

TEST_CASE("PackSkeleton fills missing parents and IBM", "[gpu-anim]")
{
    fra::Skeleton sk;
    sk.names = { "root", "child" };

    const auto pack = fra::PackSkeleton(sk);
    REQUIRE(pack.jointCount == 2u);
    REQUIRE(pack.parents.size() == 2);
    REQUIRE(pack.inverseBind.size() == 2);
    REQUIRE(pack.parents[0] == -1);
    REQUIRE(pack.inverseBind[1] == glm::mat4(1.f));
}

TEST_CASE("PackBoneMask clamps and pads", "[gpu-anim]")
{
    fra::BoneMask mask;
    mask.weights = { 1.5f, -0.2f };

    const auto packed = fra::PackBoneMask(mask, 4);
    REQUIRE(packed.size() == 4);
    REQUIRE(packed[0] == Catch::Approx(1.f));
    REQUIRE(packed[1] == Catch::Approx(0.f));
    REQUIRE(packed[2] == Catch::Approx(0.f));
    REQUIRE(packed[3] == Catch::Approx(0.f));
}
