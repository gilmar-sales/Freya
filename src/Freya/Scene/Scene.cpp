#include "Freya/Scene/Scene.hpp"

#include "Freya/Core/Renderer.hpp"

#include <utility>

namespace FREYA_NAMESPACE
{
    Scene::InstanceId Scene::Add(const Instance& instance)
    {
        for (std::uint32_t i = 0; i < mAlive.size(); ++i)
        {
            if (!mAlive[i])
            {
                mInstances[i] = instance;
                mAlive[i]     = true;
                return i;
            }
        }
        mInstances.push_back(instance);
        mAlive.push_back(true);
        return static_cast<InstanceId>(mInstances.size() - 1);
    }

    void Scene::Remove(const InstanceId id)
    {
        if (id >= mAlive.size())
            return;
        mAlive[id] = false;
    }

    void Scene::SetTransform(const InstanceId id, const glm::mat4& model)
    {
        if (id >= mAlive.size() || !mAlive[id])
            return;
        mInstances[id].model = model;
    }

    Scene::Instance* Scene::Get(const InstanceId id)
    {
        if (id >= mAlive.size() || !mAlive[id])
            return nullptr;
        return &mInstances[id];
    }

    const Scene::Instance* Scene::Get(const InstanceId id) const
    {
        if (id >= mAlive.size() || !mAlive[id])
            return nullptr;
        return &mInstances[id];
    }

    void Scene::Clear()
    {
        mInstances.clear();
        mAlive.clear();
    }

    std::size_t Scene::Size() const
    {
        std::size_t n = 0;
        for (bool alive : mAlive)
        {
            if (alive)
                ++n;
        }
        return n;
    }

    void Scene::Upload(Renderer& renderer) const
    {
        std::vector<SceneInstanceUpload> uploads;
        uploads.reserve(Size());
        for (std::uint32_t i = 0; i < mInstances.size(); ++i)
        {
            if (!mAlive[i])
                continue;
            const auto& inst = mInstances[i];
            uploads.push_back(SceneInstanceUpload {
                .model       = inst.model,
                .meshId      = inst.mesh.Id(),
                .materialId  = inst.material.Id(),
                .entityId    = inst.entityId,
                .castShadows = inst.castShadows,
                .boneOffset  = inst.boneOffset,
                .boneCount   = inst.boneCount,
            });
        }
        renderer.UploadSceneInstances(uploads);
    }

} // namespace FREYA_NAMESPACE
