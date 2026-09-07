#pragma once

#include "Freya/Config.hpp"
#include "Freya/Core/Limits.hpp"
#include "Freya/Scene/AssetHandle.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Renderer;

    /**
     * @brief Upload frequency hint for retained Scene instances.
     *
     * Static props should not call SetTransform every frame. Dynamic actors
     * (characters, moving props) mark content dirty when updated. Topology
     * changes (Add/Remove/Clear) always force a full GPU rebuild.
     */
    enum class Mobility : std::uint8_t
    {
        Dynamic = 0,
        Static  = 1,
    };

    /**
     * @brief Retained instance list synced to the GPU via Scene::Upload.
     *
     * Upload is dirty-aware: unchanged scenes only refresh the current
     * frames-in-flight GPU slot when needed; transform-only edits patch
     * without re-sorting or material lookups.
     */
    class Scene
    {
      public:
        struct Instance
        {
            MeshHandle     mesh;
            MaterialHandle material;
            glm::mat4      model       = glm::mat4(1.0f);
            std::uint32_t  entityId    = 0;
            bool           castShadows = true;
            std::uint32_t  boneOffset  = kNoSkin;
            std::uint32_t  boneCount   = 0;
            Mobility       mobility    = Mobility::Dynamic;
        };

        using InstanceId = std::uint32_t;

        InstanceId Add(const Instance& instance);

        void Remove(InstanceId id);

        void SetTransform(InstanceId id, const glm::mat4& model);

        /**
         * @brief Mutable access. Marks content dirty (callers may edit mesh,
         * material, bones, etc.). Prefer SetTransform for motion-only updates.
         */
        [[nodiscard]] Instance*       Get(InstanceId id);
        [[nodiscard]] const Instance* Get(InstanceId id) const;

        void Clear();

        [[nodiscard]] std::size_t Size() const;

        template <typename Fn>
        void ForEach(Fn&& fn) const
        {
            for (std::uint32_t i = 0; i < mInstances.size(); ++i)
            {
                if (!mAlive[i])
                    continue;
                fn(i, mInstances[i]);
            }
        }

        /**
         * @brief Sync retained instances to @p renderer for this frame.
         *
         * No-op (aside from FiF slot commit) when nothing changed since the
         * last successful upload.
         */
        void Upload(Renderer& renderer);

      private:
        void markTopologyDirty();
        void markContentDirty();

        std::vector<Instance>     mInstances;
        std::vector<std::uint8_t> mAlive;
        bool                      mTopologyDirty = true;
        bool                      mContentDirty  = true;
    };

} // namespace FREYA_NAMESPACE
