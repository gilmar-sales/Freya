#include "../Support/AnimationFixtures.hpp"

#include <Freya/Asset/SceneTransform.hpp>

TEST(SceneTransform, DefaultAndIdentityMatrixAreIdentity)
{
    fra::test::ExpectMatrixNear(fra::SceneTransform {}.ToMatrix(),
                                glm::mat4(1.f));
    fra::test::ExpectMatrixNear(
        fra::SceneTransform::FromMatrix(glm::mat4(1.f)).ToMatrix(),
        glm::mat4(1.f));
}

TEST(SceneTransform, MatrixCompositionUsesTrsOrder)
{
    fra::SceneTransform transform;
    transform.position = { 3, 4, 5 };
    transform.scale    = { 2, 3, 4 };
    transform.rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 0, 1));
    fra::test::ExpectVecNear(
        glm::vec3(transform.ToMatrix() * glm::vec4(1, 0, 0, 1)),
        { 3, 6, 5 });
}

TEST(SceneTransform, PositiveNonuniformScaleRoundTripsAcrossRotations)
{
    for (float angle : { 0.f, 0.3f, 1.5f, 3.f })
    {
        SCOPED_TRACE(angle);
        fra::SceneTransform transform;
        transform.position = { -7, 2, 11 };
        transform.scale    = { 0.5f, 3.f, 7.f };
        transform.rotation =
            glm::angleAxis(angle, glm::normalize(glm::vec3(1, 2, 3)));
        const auto restored =
            fra::SceneTransform::FromMatrix(transform.ToMatrix());
        fra::test::ExpectVecNear(restored.position, transform.position);
        fra::test::ExpectVecNear(restored.scale, transform.scale);
        fra::test::ExpectRotationNear(restored.rotation, transform.rotation);
        fra::test::ExpectMatrixNear(restored.ToMatrix(), transform.ToMatrix());
    }
}
