#include "../Support/AnimationFixtures.hpp"

using namespace fra::test;

TEST(Pose, JointMatrixAppliesScaleThenRotationThenTranslation)
{
    fra::JointTRS joint;
    joint.translation = { 3.f, 4.f, 5.f };
    joint.scale       = { 2.f, 3.f, 4.f };
    joint.rotation    = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 0, 1));
    ExpectVecNear(glm::vec3(joint.ToMatrix() * glm::vec4(1, 0, 0, 1)),
                  { 3.f, 6.f, 5.f });
}

TEST(Pose, ResizeResetsExistingJointsToIdentity)
{
    fra::LocalPose pose;
    pose.Resize(3);
    pose.joints[0].translation.x = 12.f;
    pose.Resize(1);
    ASSERT_EQ(pose.Size(), 1u);
    ExpectMatrixNear(pose.joints[0].ToMatrix(), glm::mat4(1.f));
    pose.Resize(0);
    EXPECT_EQ(pose.Size(), 0u);
}

TEST(Pose, RestPoseDecomposesTrsAndDefaultsMissingMatrices)
{
    auto          skeleton = MakeSkeleton();
    fra::JointTRS joint;
    joint.translation  = { 2.f, -3.f, 4.f };
    joint.scale        = { 2.f, 3.f, 4.f };
    joint.rotation     = glm::angleAxis(0.7f, glm::vec3(0, 1, 0));
    skeleton.restLocal = { joint.ToMatrix() };
    const auto pose    = fra::RestLocalPose(skeleton);
    ASSERT_EQ(pose.Size(), 4u);
    ExpectMatrixNear(pose.joints[0].ToMatrix(), joint.ToMatrix());
    ExpectMatrixNear(pose.joints[3].ToMatrix(), glm::mat4(1.f));
}

TEST(Pose, SamplingInterpolatesAllTrsComponents)
{
    auto       clip     = MakeLinearClip();
    auto&      channel  = clip.channels[0];
    const auto rotation = glm::angleAxis(1.f, glm::vec3(0, 1, 0));
    channel.rotations = { { 0.f, { 1.f, 0.f, 0.f, 0.f } }, { 2.f, rotation } };
    channel.scales    = { { 0.f, glm::vec3(1.f) }, { 2.f, glm::vec3(3.f) } };
    const auto pose   = fra::SampleClip(MakeSkeleton(), clip, 1.f);
    ASSERT_EQ(pose.Size(), 4u);
    ExpectVecNear(pose.joints[0].translation, { 4.f, 0.f, 0.f });
    ExpectVecNear(pose.joints[0].scale, glm::vec3(2.f));
    ExpectRotationNear(pose.joints[0].rotation,
                       glm::angleAxis(0.5f, glm::vec3(0, 1, 0)));
}

TEST(Pose, SamplingWrapsPositiveAndNegativeLoopTimes)
{
    const auto skeleton = MakeSkeleton();
    const auto clip     = MakeLinearClip();
    for (float time : { 0.5f, 2.5f, -1.5f })
    {
        SCOPED_TRACE(time);
        EXPECT_FLOAT_EQ(
            fra::SampleClip(skeleton, clip, time).joints[0].translation.x, 2.f);
    }
    EXPECT_FLOAT_EQ(
        fra::SampleClip(skeleton, clip, 2.f).joints[0].translation.x, 0.f);
}

TEST(Pose, NonLoopSamplingClampsToKeyEndpoints)
{
    const auto skeleton = MakeSkeleton();
    const auto clip     = MakeLinearClip();
    EXPECT_FLOAT_EQ(
        fra::SampleClip(skeleton, clip, -2.f, false).joints[0].translation.x,
        0.f);
    EXPECT_FLOAT_EQ(
        fra::SampleClip(skeleton, clip, 20.f, false).joints[0].translation.x,
        8.f);
}

TEST(Pose, MissingTracksKeepRestComponentsAndInvalidChannelsAreIgnored)
{
    auto skeleton         = MakeSkeleton();
    skeleton.restLocal[0] = glm::scale(glm::mat4(1.f), glm::vec3(3.f));
    auto clip             = MakeLinearClip();
    auto invalid          = clip.channels[0];
    invalid.jointIndex    = 100;
    clip.channels.push_back(invalid);
    const auto pose = fra::SampleClip(skeleton, clip, 1.f);
    ASSERT_EQ(pose.Size(), 4u);
    ExpectVecNear(pose.joints[0].scale, glm::vec3(3.f));
    ExpectMatrixNear(pose.joints[1].ToMatrix(), glm::mat4(1.f));
}

TEST(Pose, IntoSamplingReplacesPreviouslyLargerOutput)
{
    fra::LocalPose out;
    out.Resize(10);
    fra::SampleClipInto(MakeSkeleton(), MakeLinearClip(), 1.f, out);
    ASSERT_EQ(out.Size(), 4u);
    EXPECT_FLOAT_EQ(out.joints[0].translation.x, 4.f);
    fra::RestLocalPoseInto({}, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(Pose, BlendClampsWeightAndInterpolates)
{
    auto a                    = fra::RestLocalPose(MakeSkeleton());
    auto b                    = a;
    b.joints[0].translation.x = 8.f;
    b.joints[0].scale         = glm::vec3(3.f);
    b.joints[0].rotation      = glm::angleAxis(1.f, glm::vec3(0, 1, 0));
    EXPECT_FLOAT_EQ(fra::BlendLocalPoses(a, b, -1.f).joints[0].translation.x,
                    0.f);
    EXPECT_FLOAT_EQ(fra::BlendLocalPoses(a, b, 2.f).joints[0].translation.x,
                    8.f);
    const auto mid = fra::BlendLocalPoses(a, b, 0.5f);
    ExpectVecNear(mid.joints[0].scale, glm::vec3(2.f));
    EXPECT_FLOAT_EQ(mid.joints[0].translation.x, 4.f);
    ExpectRotationNear(mid.joints[0].rotation,
                       glm::angleAxis(0.5f, glm::vec3(0, 1, 0)));
}

TEST(Pose, BoneMaskClampsWeightsAndSelectsOnlyDescendants)
{
    const auto skeleton = MakeSkeleton();
    auto       mask     = fra::BoneMask::Filled(4, -1.f);
    mask.SetSubtree(skeleton, 1, 2.f);
    ASSERT_EQ(mask.Size(), 4u);
    EXPECT_EQ(mask.weights, (std::vector<float> { 0.f, 1.f, 1.f, 0.f }));
    mask.SetJoint(3, 0.25f);
    mask.SetJoint(100, 1.f);
    mask.SetSubtree(skeleton, 100, 1.f);
    EXPECT_FLOAT_EQ(mask.weights[3], 0.25f);
    mask.SetJoint(1, -2.f);
    EXPECT_FLOAT_EQ(mask.weights[1], 0.f);
}

TEST(Pose, MaskedBlendMultipliesMaskByLayerWeight)
{
    auto base    = fra::RestLocalPose(MakeSkeleton());
    auto overlay = base;
    for (auto& joint : overlay.joints)
        joint.translation.x = 8.f;
    fra::BoneMask mask;
    mask.weights      = { 0.f, 0.5f, 1.f, 2.f };
    const auto result = fra::BlendMasked(base, overlay, mask, 0.5f);
    ASSERT_EQ(result.Size(), 4u);
    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(result.joints[i].translation.x,
                        (i == 0   ? 0.f
                         : i == 1 ? 2.f
                                  : 4.f));
}

TEST(Pose, AdditiveBlendAppliesDifferenceFromReference)
{
    auto reference                    = fra::RestLocalPose(MakeSkeleton());
    auto base                         = reference;
    auto additive                     = reference;
    base.joints[0].translation.x      = 10.f;
    reference.joints[0].translation.x = 2.f;
    additive.joints[0].translation.x  = 6.f;
    additive.joints[0].scale          = glm::vec3(3.f);
    additive.joints[0].rotation       = glm::angleAxis(1.f, glm::vec3(0, 1, 0));
    const auto result = fra::BlendAdditive(base, additive, reference, 0.5f);
    EXPECT_FLOAT_EQ(result.joints[0].translation.x, 12.f);
    ExpectVecNear(result.joints[0].scale, glm::vec3(2.f));
    ExpectRotationNear(result.joints[0].rotation,
                       glm::angleAxis(0.5f, glm::vec3(0, 1, 0)));
    const auto masked = fra::BlendAdditive(
        base, additive, reference, fra::BoneMask::Filled(4, 0.f));
    EXPECT_FLOAT_EQ(masked.joints[0].translation.x, 10.f);
}

TEST(Pose, JointSearchUsesFirstSubstringMatch)
{
    const auto skeleton = MakeSkeleton();
    EXPECT_EQ(fra::FindJointIndex(skeleton, "oot"), 0);
    EXPECT_EQ(fra::FindJointIndex(skeleton, "tip"), 2);
    EXPECT_EQ(fra::FindJointIndex(skeleton, "missing"), -1);
    EXPECT_EQ(fra::FindJointIndex(skeleton, ""), -1);
}

TEST(Pose, GlobalMatricesHandleParentsAfterChildrenAndNonBoneBridges)
{
    auto skeleton    = MakeSkeleton();
    skeleton.parents = { 1, -1, 0, -1 };
    skeleton.nonBoneParent.assign(4, glm::mat4(1.f));
    skeleton.nonBoneParent[0] =
        glm::translate(glm::mat4(1.f), glm::vec3(0, 3, 0));
    auto pose                    = fra::RestLocalPose(skeleton);
    pose.joints[0].translation.x = 2.f;
    pose.joints[1].translation.x = 10.f;
    pose.joints[2].translation.z = 4.f;
    const auto global            = fra::LocalToGlobal(skeleton, pose);
    ASSERT_EQ(global.size(), 4u);
    ExpectVecNear(glm::vec3(global[0][3]), { 12.f, 3.f, 0.f });
    ExpectVecNear(glm::vec3(global[2][3]), { 12.f, 3.f, 4.f });
    ExpectMatrixNear(global[3], glm::mat4(1.f));
}

TEST(Pose, SkinMatricesApplyInverseBindAndDefaultMissingData)
{
    auto skeleton                = MakeSkeleton();
    auto pose                    = fra::RestLocalPose(skeleton);
    pose.joints[0].translation.x = 5.f;
    skeleton.inverseBind         = { glm::translate(
        glm::mat4(1.f), glm::vec3(-5, 0, 0)) };
    const auto skin              = fra::PoseToSkinMatrices(skeleton, pose);
    ASSERT_EQ(skin.size(), 4u);
    ExpectMatrixNear(skin[0], glm::mat4(1.f));
    ExpectVecNear(glm::vec3(skin[1][3]), { 5.f, 0.f, 0.f });
    skeleton.parents.clear();
    const auto global = fra::LocalToGlobal(skeleton, {});
    for (const auto& matrix : global)
        ExpectMatrixNear(matrix, glm::mat4(1.f));
}

TEST(Pose, RootMotionExtractsTranslationAndUnwrapsBoundary)
{
    const auto skeleton = MakeSkeleton();
    const auto clip     = MakeLinearClip();
    ExpectVecNear(
        fra::ExtractRootDelta(skeleton, clip, 0.25f, 0.75f).translation,
        { 2.f, 0.f, 0.f });
    ExpectVecNear(
        fra::ExtractRootDelta(skeleton, clip, 1.75f, 0.25f).translation,
        { 2.f, 0.f, 0.f }, 1e-3f);
    ExpectVecNear(fra::ExtractRootDelta({}, clip, 0.f, 1.f).translation,
                  glm::vec3(0.f));
}
