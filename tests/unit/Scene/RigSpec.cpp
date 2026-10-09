#include "../Support/AnimationFixtures.hpp"

#include <Freya/Asset/Rig.hpp>

using namespace fra::test;

TEST(Rig, RootLookupHandlesMissingParentsAndEmptySkeleton)
{
    auto skeleton = MakeSkeleton();
    EXPECT_EQ(fra::FindRootJoint(skeleton), 0);
    skeleton.parents = { 1, -1, 1, 1 };
    EXPECT_EQ(fra::FindRootJoint(skeleton), 1);
    skeleton.parents.clear();
    EXPECT_EQ(fra::FindRootJoint(skeleton), 0);
    EXPECT_EQ(fra::FindRootJoint({}), -1);
}

TEST(Rig, CancelRootTranslationPreservesHeightAndOtherJoints)
{
    const auto skeleton        = MakeSkeleton();
    auto       pose            = fra::RestLocalPose(skeleton);
    pose.joints[0].translation = { 3, 4, 5 };
    pose.joints[1].translation = { 6, 7, 8 };
    fra::CancelRootTranslationXZ(skeleton, pose);
    ExpectVecNear(pose.joints[0].translation, { 0, 4, 0 });
    ExpectVecNear(pose.joints[1].translation, { 6, 7, 8 });
    fra::LocalPose empty;
    fra::CancelRootTranslationXZ(skeleton, empty);
    EXPECT_EQ(empty.Size(), 0u);
}

TEST(Rig, PlanarRootMotionPreservesHeightAndRotatesAroundInstanceOrigin)
{
    const auto model = glm::translate(glm::mat4(1.f), glm::vec3(10, 5, 20));
    fra::RootMotionDelta delta;
    delta.translation = { 2, 7, 3 };
    delta.rotation    = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 1, 0));
    const auto result = fra::IntegrateRootMotion(model, delta);
    ExpectVecNear(glm::vec3(result[3]), { 12, 5, 23 });
    ExpectVecNear(glm::mat3(result) * glm::vec3(0, 0, 1), { 1, 0, 0 });
}

TEST(Rig, LocomotionUsesNormalizedWorldHeadingWithoutChangingHeight)
{
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3, 4, 5));
    model      = glm::rotate(model, glm::radians(90.f), glm::vec3(0, 1, 0));
    model      = glm::scale(model, glm::vec3(3.f));
    const auto result = fra::DrivePlanarLocomotion(model, 2.f, 0.5f);
    ExpectVecNear(glm::vec3(result[3]), { 2, 4, 5 });
    for (int c = 0; c < 3; ++c)
        ExpectVecNear(glm::vec3(result[c]), glm::vec3(model[c]));
    ExpectMatrixNear(fra::DrivePlanarLocomotion(model, 0.f, 1.f), model);
    ExpectMatrixNear(fra::DrivePlanarLocomotion(model, 2.f, -1.f), model);
}

TEST(Rig, LookAtAimsForwardAtTargetAndPreservesTrs)
{
    const auto skeleton = MakeSkeleton();
    auto       pose     = fra::RestLocalPose(skeleton);
    ASSERT_TRUE(fra::ApplyLookAt(
        skeleton, pose, glm::mat4(1.f), 0, { 1, 0, 0 }, 1.f, -1.f));
    ExpectVecNear(pose.joints[0].rotation * glm::vec3(0, 0, 1), { 1, 0, 0 });
    ExpectVecNear(pose.joints[0].translation, glm::vec3(0.f));
    ExpectVecNear(pose.joints[0].scale, glm::vec3(1.f));
}

TEST(Rig, LookAtInvalidInputAndZeroWeightLeavePoseUnchanged)
{
    const auto skeleton = MakeSkeleton();
    auto       pose     = fra::RestLocalPose(skeleton);
    EXPECT_FALSE(
        fra::ApplyLookAt(skeleton, pose, glm::mat4(1.f), 100, { 1, 0, 0 }));
    EXPECT_FALSE(
        fra::ApplyLookAt(skeleton, pose, glm::mat4(1.f), 0, { 1, 0, 0 }, 0.f));
    EXPECT_FALSE(fra::ApplyLookAt(skeleton, pose, glm::mat4(1.f), 0, {}));
    ExpectMatrixNear(pose.joints[0].ToMatrix(), glm::mat4(1.f));
}

TEST(Rig, TwoBoneIkReachesTargetAndPreservesBoneLengths)
{
    const auto skeleton        = MakeSkeleton();
    auto       pose            = fra::RestLocalPose(skeleton);
    pose.joints[1].translation = { 1, 0, 0 };
    pose.joints[2].translation = { 1, 0, 0 };
    const glm::vec3 target(1, 1, 0);
    ASSERT_TRUE(fra::SolveTwoBoneIK(
        skeleton, pose, glm::mat4(1.f), { 0, 1, 2 }, target, { 0, 0, 1 }));
    const auto global = fra::LocalToGlobal(skeleton, pose);
    ExpectVecNear(glm::vec3(global[2][3]), target, 1e-4f);
    EXPECT_NEAR(glm::length(glm::vec3(global[1][3] - global[0][3])), 1.f,
                1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(global[2][3] - global[1][3])), 1.f,
                1e-5f);
}

TEST(Rig, TwoBoneIkRejectsInvalidIndicesAndZeroLengthBones)
{
    const auto skeleton = MakeSkeleton();
    auto       pose     = fra::RestLocalPose(skeleton);
    EXPECT_FALSE(fra::SolveTwoBoneIK(skeleton, pose, glm::mat4(1.f),
                                     { 0, 1, 100 }, { 1, 1, 0 }, { 0, 0, 1 }));
    EXPECT_FALSE(fra::SolveTwoBoneIK(
        skeleton, pose, glm::mat4(1.f), { 0, 1, 2 }, { 1, 1, 0 }, { 0, 0, 1 }));
    ExpectMatrixNear(pose.joints[0].ToMatrix(), glm::mat4(1.f));
}
