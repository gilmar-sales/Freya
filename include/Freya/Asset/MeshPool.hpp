#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/Mesh.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/Vertex.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Buffer;
    class CommandPool;
    class IndirectDrawSystem;

    class MeshPool
    {
      public:
        MeshPool(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~MeshPool();

        MeshPool(const MeshPool&)            = delete;
        MeshPool& operator=(const MeshPool&) = delete;
        MeshPool(MeshPool&&) noexcept;
        MeshPool& operator=(MeshPool&&) noexcept;

        std::uint32_t CreateMesh(const std::vector<Vertex>&        vertices,
                                 const std::vector<std::uint32_t>& indices);

        std::vector<ModelSubmesh> CreateModelFromFile(const std::string& path);

        SkinnedModel CreateSkinnedModelFromFile(const std::string& path);

        [[nodiscard]] bool Contains(std::uint32_t meshId) const;

        [[nodiscard]] const Mesh& GetMesh(std::uint32_t meshId) const;

        [[nodiscard]] std::uint32_t GetMeshCount() const;

        void FillMeshInfos(std::vector<MeshInfo>& out) const;

        void FillMeshLods(std::vector<MeshLodInfo>& out) const;

        void Destroy(std::uint32_t meshId);

      private:
        friend class IndirectDrawSystem;

        void BindGeometry(const skr::Arc<CommandPool>& commandPool) const;

        [[nodiscard]] const skr::Arc<Buffer>& GetVertexBuffer() const;
        [[nodiscard]] const skr::Arc<Buffer>& GetIndexBuffer() const;

        void Draw(const skr::Arc<CommandPool>& commandPool,
                  std::uint32_t                meshId);

        void DrawInstanced(const skr::Arc<CommandPool>& commandPool,
                           std::uint32_t                meshId,
                           size_t                       instanceCount,
                           size_t                       firstInstance = 0);

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
