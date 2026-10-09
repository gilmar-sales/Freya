#pragma once

#include <Freya/Asset/Pose.hpp>

#include <gtest/gtest.h>

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace fra::test
{
    inline Skeleton MakeSkeleton()
    {
        Skeleton skeleton;
        skeleton.names   = { "root", "child", "tip", "other" };
        skeleton.parents = { -1, 0, 1, 0 };
        skeleton.restLocal.assign(4, glm::mat4(1.f));
        skeleton.inverseBind.assign(4, glm::mat4(1.f));
        return skeleton;
    }

    inline AnimationClip MakeLinearClip()
    {
        AnimationClip clip;
        clip.name     = "Move";
        clip.duration = 2.f;
        AnimationChannel channel;
        channel.translations = { { 0.f, { 0.f, 0.f, 0.f } },
                                 { 2.f, { 8.f, 0.f, 0.f } } };
        clip.channels.push_back(channel);
        return clip;
    }

    inline AnimationClip MakeConstantClip(float x)
    {
        auto clip                     = MakeLinearClip();
        clip.channels[0].translations = { { 0.f, { x, 0.f, 0.f } } };
        return clip;
    }

    inline void ExpectVecNear(const glm::vec3& actual,
                              const glm::vec3& expected, float eps = 1e-5f)
    {
        for (int i = 0; i < 3; ++i)
            EXPECT_NEAR(actual[i], expected[i], eps) << "component " << i;
    }

    inline void ExpectMatrixNear(const glm::mat4& actual,
                                 const glm::mat4& expected, float eps = 1e-5f)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                EXPECT_NEAR(actual[c][r], expected[c][r], eps)
                    << "column " << c << ", row " << r;
    }

    inline void ExpectRotationNear(const glm::quat& actual,
                                   const glm::quat& expected)
    {
        // q and -q encode the same orientation.
        EXPECT_NEAR(std::abs(glm::dot(actual, expected)), 1.f, 1e-5f);
        EXPECT_NEAR(glm::length(actual), 1.f, 1e-5f);
    }
} // namespace fra::test
