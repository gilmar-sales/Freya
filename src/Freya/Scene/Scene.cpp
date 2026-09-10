#include "Freya/Scene/Scene.hpp"

#include "Freya/Asset/SceneInstanceUpload.hpp"
#include "Freya/Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{
    void Scene::markTopologyDirty()
    {
        mTopologyDirty = true;
        mContentDirty  = true;
    }

    void Scene::markContentDirty()
    {
        mContentDirty = true;
    }

    Scene::InstanceId Scene::Add(const Instance& instance)
    {
        markTopologyDirty();
        for (std::uint32_t i = 0; i < mAlive.size(); ++i)
        {
            if (!mAlive[i])
            {
                mInstances[i] = instance;
                mAlive[i]     = 1;
                return i;
            }
        }
        mInstances.push_back(instance);
        mAlive.push_back(1);
        return static_cast<InstanceId>(mInstances.size() - 1);
    }

    void Scene::Remove(const InstanceId id)
    {
        if (id >= mAlive.size())
            return;
        if (!mAlive[id])
            return;
        mAlive[id] = 0;
        markTopologyDirty();
    }

    void Scene::SetTransform(const InstanceId      id,
                             const SceneTransform& transform)
    {
        if (id >= mAlive.size() || !mAlive[id])
            return;
        mInstances[id].transform = transform;
        markContentDirty();
    }

    void Scene::SetTransform(const InstanceId id, const glm::mat4& model)
    {
        SetTransform(id, SceneTransform::FromMatrix(model));
    }

    Scene::Instance* Scene::Get(const InstanceId id)
    {
        if (id >= mAlive.size() || !mAlive[id])
            return nullptr;
        markContentDirty();
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
        markTopologyDirty();
    }

    std::size_t Scene::Size() const
    {
        std::size_t n = 0;
        for (const auto alive : mAlive)
        {
            if (alive)
                ++n;
        }
        return n;
    }

    void Scene::Upload(Renderer& renderer)
    {
        if (!mTopologyDirty && !mContentDirty)
        {
            renderer.CommitSceneFrame();
            return;
        }

        renderer.BeginSceneInstances();
        renderer.ReserveSceneInstances(static_cast<std::uint32_t>(Size()));

        thread_local std::vector<SceneInstanceUpload> uploads;
        uploads.clear();
        uploads.reserve(Size());
        for (std::uint32_t i = 0; i < mInstances.size(); ++i)
        {
            if (!mAlive[i])
                continue;
            const auto& inst = mInstances[i];
            uploads.push_back(SceneInstanceUpload {
                .transform   = inst.transform,
                .mesh        = inst.mesh,
                .material    = inst.material,
                .entityId    = inst.entityId,
                .castShadows = inst.castShadows,
                .boneOffset  = inst.boneOffset,
                .boneCount   = inst.boneCount,
            });
        }

        if (!uploads.empty())
            renderer.UploadSceneInstances(uploads);

        renderer.EndSceneInstances();

        mTopologyDirty = false;
        mContentDirty  = false;
    }

} // namespace FREYA_NAMESPACE
