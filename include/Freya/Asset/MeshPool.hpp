#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/Mesh.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/Vertex.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Buffer;
    class Renderer;
    class IndirectDrawSystem;

    /**
     * @brief Manages mega vertex/index buffers and GPU mesh/LOD tables.
     *
     * All geometries live in a single uint32-indexed VBO/IBO pair. Mesh files
     * are loaded through Assimp; LODs are generated with meshoptimizer.
     */
    class MeshPool
    {
      public:
        MeshPool(const skr::Arc<Device>&                device,
                 const skr::Arc<PhysicalDevice>&        physicalDevice,
                 const skr::Arc<CommandPool>&           commandPool,
                 const skr::Arc<skr::Logger<MeshPool>>& logger);

        ~MeshPool();

        MeshPool(const MeshPool&)            = delete;
        MeshPool& operator=(const MeshPool&) = delete;
        MeshPool(MeshPool&&) noexcept;
        MeshPool& operator=(MeshPool&&) noexcept;

        /**
         * @brief Creates a mesh from CPU vertex/index data (builds LODs).
         * @return Mesh ID for later drawing/binding
         */
        std::uint32_t CreateMesh(const std::vector<Vertex>&        vertices,
                                 const std::vector<std::uint32_t>& indices);

        /**
         * @brief Creates meshes from a model file (GLTF, OBJ, FBX, etc.).
         * @return Vector of mesh IDs created from the file
         */
        std::vector<std::uint32_t> CreateMeshFromFile(const std::string& path);

        /**
         * @brief Load a skinned model (no PreTransformVertices).
         *
         * Builds a shared Skeleton, AnimationClips, and mesh IDs with
         * joints/weights. Cull AABBs are inflated for posed conservatism.
         */
        SkinnedModel CreateSkinnedModelFromFile(const std::string& path);

        [[nodiscard]] bool Contains(std::uint32_t meshId) const;

        [[nodiscard]] const Mesh& GetMesh(std::uint32_t meshId) const;

        [[nodiscard]] std::uint32_t GetMeshCount() const;

        /**
         * @brief Fill a MeshInfo table (index = meshId). Grows to mesh count.
         */
        void FillMeshInfos(std::vector<MeshInfo>& out) const;

        /**
         * @brief Flat LOD table referenced by MeshInfo::lodBase.
         */
        void FillMeshLods(std::vector<MeshLodInfo>& out) const;

        /**
         * @brief Bind the global mega vertex + uint32 index buffers.
         */
        void BindGeometry() const;

        [[nodiscard]] const skr::Arc<Buffer>& GetVertexBuffer() const;

        [[nodiscard]] const skr::Arc<Buffer>& GetIndexBuffer() const;

      protected:
        friend class FREYA_NAMESPACE::Renderer;
        friend class FREYA_NAMESPACE::IndirectDrawSystem;

        void Draw(std::uint32_t meshId);

        void DrawInstanced(std::uint32_t meshId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0);

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
