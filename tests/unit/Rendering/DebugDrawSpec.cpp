#include "../Support/AnimationFixtures.hpp"

#include <Freya/Core/DebugDraw.hpp>

TEST(DebugDraw, LinePreservesEndpointsColorAndClearAllowsReuse)
{
    fra::DebugDraw draw;
    EXPECT_TRUE(draw.Empty());
    const glm::vec3 a(1, 2, 3), b(4, 5, 6);
    const glm::vec4 color(0.1f, 0.2f, 0.3f, 1.f);
    draw.Line(a, b, color);
    ASSERT_EQ(draw.Vertices().size(), 2u);
    EXPECT_EQ(draw.Vertices()[0].position, a);
    EXPECT_EQ(draw.Vertices()[1].position, b);
    EXPECT_EQ(draw.Vertices()[0].color, color);
    EXPECT_EQ(draw.Vertices()[1].color, color);
    draw.Clear();
    EXPECT_TRUE(draw.Empty());
    draw.Line(a, b, color);
    EXPECT_EQ(draw.Vertices().size(), 2u);
}

TEST(DebugDraw, CapacityRejectsWholeLinesAndClampsMinimum)
{
    fra::DebugDraw draw(3);
    draw.Line(glm::vec3(0.f), glm::vec3(1.f), glm::vec4(1.f));
    draw.Line(glm::vec3(2.f), glm::vec3(3.f), glm::vec4(1.f));
    EXPECT_EQ(draw.Vertices().size(), 2u);
    EXPECT_EQ(fra::DebugDraw(0).MaxVertices(), 2u);
}

TEST(DebugDraw, CircleIsClosedAndStaysOnRequestedPlane)
{
    fra::DebugDraw  draw;
    const glm::vec3 center(2, 3, 4);
    draw.Circle(center, { 0, 0, 1 }, 2.f, glm::vec4(1.f), 8);
    const auto vertices = draw.Vertices();
    ASSERT_EQ(vertices.size(), 16u);
    for (const auto& vertex : vertices)
    {
        EXPECT_NEAR(glm::length(vertex.position - center), 2.f, 1e-5f);
        EXPECT_NEAR(vertex.position.z, center.z, 1e-5f);
    }
    fra::test::ExpectVecNear(vertices.front().position,
                             vertices.back().position);
}

TEST(DebugDraw, InvalidCircleParametersDoNotEmitGeometry)
{
    fra::DebugDraw draw;
    draw.Circle({}, { 0, 1, 0 }, 0.f, glm::vec4(1.f));
    draw.Circle({}, { 0, 1, 0 }, -1.f, glm::vec4(1.f));
    draw.Circle({}, { 0, 1, 0 }, 1.f, glm::vec4(1.f), 2);
    EXPECT_TRUE(draw.Empty());
}

TEST(DebugDraw, SphereEmitsThreeGreatCircles)
{
    fra::DebugDraw draw;
    draw.Sphere({}, 2.f, glm::vec4(1.f), 8);
    ASSERT_EQ(draw.Vertices().size(), 48u);
    for (const auto& vertex : draw.Vertices())
        EXPECT_NEAR(glm::length(vertex.position), 2.f, 1e-5f);
}

TEST(DebugDraw, SkeletonLinesIncludeHierarchyAndWorldTransform)
{
    const auto skeleton        = fra::test::MakeSkeleton();
    auto       pose            = fra::RestLocalPose(skeleton);
    pose.joints[1].translation = { 1, 0, 0 };
    pose.joints[2].translation = { 0, 2, 0 };
    pose.joints[3].translation = { 0, 0, 3 };
    fra::DebugDraw draw;
    draw.DrawSkeleton(skeleton, pose,
                      glm::translate(glm::mat4(1.f), glm::vec3(10, 0, 0)),
                      glm::vec4(1.f));
    ASSERT_EQ(draw.Vertices().size(), 6u);
    fra::test::ExpectVecNear(draw.Vertices()[0].position, { 10, 0, 0 });
    fra::test::ExpectVecNear(draw.Vertices()[1].position, { 11, 0, 0 });
    fra::test::ExpectVecNear(draw.Vertices()[3].position, { 11, 2, 0 });
}
