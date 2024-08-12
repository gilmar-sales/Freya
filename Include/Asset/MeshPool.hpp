#pragma once

#include "Asset/Vertex.hpp"
#include "Containers/MeshSet.hpp"
#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Core/PhysicalDevice.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace FREYA_NAMESPACE
{
    class MeshPool
    {
      public:
        MeshPool(std::shared_ptr<Device> device,
                 std::shared_ptr<PhysicalDevice>
                     physicalDevice,
                 std::shared_ptr<CommandPool>
                     commandPool);

        void Draw(std::uint32_t meshId);
        void DrawInstanced(std::uint32_t meshId, size_t instanceCount, size_t firstInstance = 0);

        std::uint32_t              CreateMesh(std::vector<Vertex> vertices,
                                              std::vector<uint16_t>
                                                  indices);
        std::vector<std::uint32_t> CreateMeshFromFile(const std::string path);

      protected:
        std::uint32_t createVertexBuffer(std::uint32_t size);
        std::uint32_t queryVertexBuffer(std::uint32_t size);

        std::uint32_t createIndexBuffer(std::uint32_t size);
        std::uint32_t queryIndexBuffer(std::uint32_t size);

        std::uint32_t processMesh(aiMesh* mesh, const aiScene* scene);
        void          processNode(std::vector<std::uint32_t>& meshes,
                                  aiNode*                     node,
                                  const aiScene*              scene);

      private:
        std::shared_ptr<Device>         mDevice;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;
        std::shared_ptr<CommandPool>    mCommandPool;

        std::vector<std::shared_ptr<Buffer>> mVertexBuffers;
        std::vector<std::uint32_t>           mVertexBuffersOffsets;

        std::vector<std::shared_ptr<Buffer>> mIndexBuffers;
        std::vector<std::uint32_t>           mIndexBuffersOffsets;
        MeshSet                              mMeshes;
    };

} // namespace FREYA_NAMESPACE