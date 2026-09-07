#pragma once

#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Asset/SceneInstanceUpload.hpp"
#include "Freya/Config.hpp"
#include "Freya/Scene/AssetHandle.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Renderer;

    /**
     * @brief Retained instance list synced to the GPU via UploadSceneInstances.
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
        };

        using InstanceId = std::uint32_t;

        InstanceId Add(const Instance& instance);

        void Remove(InstanceId id);

        void SetTransform(InstanceId id, const glm::mat4& model);

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
         * @brief Rebuild upload records and push to @p renderer for this frame.
         */
        void Upload(Renderer& renderer) const;

      private:
        std::vector<Instance> mInstances;
        std::vector<bool>     mAlive;
    };

} // namespace FREYA_NAMESPACE
