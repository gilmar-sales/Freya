#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/Mesh.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/Vertex.hpp"

#include <Skirnir/Skirnir.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Device;
    class PhysicalDevice;
    class CommandPool;
    class Buffer;
    class Renderer;
    class IndirectDrawSystem;
    class MaterialPool;
    class TexturePool;

    class MeshPool
    {
      public:
        MeshPool(const skr::Arc<Device>&                device,
                 const skr::Arc<PhysicalDevice>&        physicalDevice,
                 const skr::Arc<CommandPool>&           commandPool,
                 const skr::Arc<skr::Logger<MeshPool>>& logger,
                 const skr::Arc<MaterialPool>&          materialPool,
                 const skr::Arc<TexturePool>&           texturePool);

        ~MeshPool();

        MeshPool(const MeshPool&)            = delete;
        MeshPool& operator=(const MeshPool&) = delete;
        MeshPool(MeshPool&&) noexcept;
        MeshPool& operator=(MeshPool&&) noexcept;

        std::uint32_t CreateMesh(const std::vector<Vertex>&        vertices,
                                 const std::vector<std::uint32_t>& indices);

        std::vector<std::uint32_t> CreateMeshFromFile(const std::string& path);

        std::vector<ModelSubmesh> CreateModelFromFile(const std::string& path);

        SkinnedModel CreateSkinnedModelFromFile(const std::string& path);

        [[nodiscard]] bool Contains(std::uint32_t meshId) const;

        [[nodiscard]] const Mesh& GetMesh(std::uint32_t meshId) const;

        [[nodiscard]] std::uint32_t GetMeshCount() const;

        void FillMeshInfos(std::vector<MeshInfo>& out) const;

        void FillMeshLods(std::vector<MeshLodInfo>& out) const;

      protected:
        friend class FREYA_NAMESPACE::IndirectDrawSystem;

        void BindGeometry() const;

        [[nodiscard]] const skr::Arc<Buffer>& GetVertexBuffer() const;
        [[nodiscard]] const skr::Arc<Buffer>& GetIndexBuffer() const;

        void Draw(std::uint32_t meshId);

        void DrawInstanced(std::uint32_t meshId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0);

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
