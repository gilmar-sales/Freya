#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/Mesh.hpp"
#include "Freya/Asset/Vertex.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <memory>
#include <span>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Buffer;
    class Renderer;
    class IndirectDrawSystem;

    /**
     * @brief Manages mesh buffers and GPU-side mesh storage.
     *
     * Handles vertex/index buffer allocation and uploads. Mesh files are
     * loaded through a private implementation (Assimp is not part of the
     * public API surface).
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
         * @brief Creates a mesh from CPU vertex/index data.
         * @return Mesh ID for later drawing/binding
         */
        std::uint32_t CreateMesh(const std::vector<Vertex>&   vertices,
                                 const std::vector<uint16_t>& indices);

        /**
         * @brief Creates meshes from a model file (GLTF, OBJ, FBX, etc.).
         * @return Vector of mesh IDs created from the file
         */
        std::vector<std::uint32_t> CreateMeshFromFile(const std::string& path);

        [[nodiscard]] bool Contains(std::uint32_t meshId) const;

        [[nodiscard]] const Mesh& GetMesh(std::uint32_t meshId) const;

        [[nodiscard]] std::uint32_t GetMeshCount() const;

        /**
         * @brief Fill a MeshInfo table (index = meshId). Grows to mesh count.
         */
        void FillMeshInfos(std::vector<MeshInfo>& out) const;

        /**
         * @brief Bind vertex/index buffers for a chunk at byte offset 0.
         */
        void BindChunk(std::uint32_t vertexBufferIndex,
                       std::uint32_t indexBufferIndex) const;

        [[nodiscard]] const skr::Arc<Buffer>& GetVertexBuffer(
            std::uint32_t index) const;

        [[nodiscard]] const skr::Arc<Buffer>& GetIndexBuffer(
            std::uint32_t index) const;

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
