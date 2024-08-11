#include "Asset/MeshPool.hpp"
#include "Builders/BufferBuilder.hpp"

namespace FREYA_NAMESPACE
{
    constexpr auto MinVertexBufferSize = 1'048'576;
    constexpr auto MinIndexBufferSize  = 2'097'152;

    MeshPool::MeshPool(std::shared_ptr<Device> device,
                       std::shared_ptr<PhysicalDevice>
                           physicalDevice,
                       std::shared_ptr<CommandPool>
                           commandPool) :
        mDevice(device),
        mPhysicalDevice(physicalDevice), mCommandPool(commandPool), mMeshes(4096)
    {
        mVertexBuffers.reserve(1024);
        mIndexBuffers.reserve(1024);

        createVertexBuffer(MinVertexBufferSize);
    }

    void MeshPool::Draw(std::uint32_t meshId)
    {
        if (mMeshes.contains(meshId))
        {
            auto& mesh = mMeshes[meshId];

            auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            auto  vertexOffset = vk::DeviceSize(mesh.vertexBufferOffset);

            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &vertexOffset);

            auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            auto  indexOffset = vk::DeviceSize(mesh.indexBufferOffset);
            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                indexOffset,
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer().drawIndexed(mesh.indexCount, 1, 0, 0, 0);
        }
    }

    void MeshPool::DrawInstanced(std::uint32_t meshId, size_t instanceCount, size_t firstInstance)
    {
        if (mMeshes.contains(meshId))
        {
            auto& mesh = mMeshes[meshId];

            auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            auto  vertexOffset = vk::DeviceSize(mesh.vertexBufferOffset);

            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &vertexOffset);

            auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            auto  indexOffset = vk::DeviceSize(mesh.indexBufferOffset);
            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                indexOffset,
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer().drawIndexed(mesh.indexCount, instanceCount, 0, 0, firstInstance);
        }
    }

    std::uint32_t MeshPool::CreateMesh(
        std::vector<Vertex> vertices, std::vector<uint16_t> indices)
    {
        auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        auto commandBuffer = mCommandPool->CreateCommandBuffer();

        commandBuffer.begin(beginInfo);

        auto vertexMemorySize = vertices.size() * sizeof(vertices[0]);
        auto vertexStaging =
            BufferBuilder(mDevice)
                .SetData(vertices.data())
                .SetSize(vertexMemorySize)
                .SetUsage(BufferUsage::Staging)
                .Build();

        // copy vertices
        auto  vertexBufferIndex  = queryVertexBuffer(vertexMemorySize);
        auto& vertexBufferOffset = mVertexBuffersOffsets[vertexBufferIndex];

        auto vertexCopyRegion =
            vk::BufferCopy()
                .setSrcOffset(0)
                .setSize(vertexMemorySize)
                .setDstOffset(vertexBufferOffset);

        commandBuffer.copyBuffer(vertexStaging->Get(),
                                 mVertexBuffers[vertexBufferIndex]->Get(),
                                 1,
                                 &vertexCopyRegion);

        auto indexMemorySize = indices.size() * sizeof(indices[0]);
        auto indexStaging =
            BufferBuilder(mDevice)
                .SetData(indices.data())
                .SetSize(indexMemorySize)
                .SetUsage(BufferUsage::Staging)
                .Build();

        // copy indices
        auto  indexBufferIndex  = queryIndexBuffer(indexMemorySize);
        auto& indexBufferOffset = mIndexBuffersOffsets[indexBufferIndex];

        auto indexCopyRegion =
            vk::BufferCopy()
                .setSrcOffset(0)
                .setSize(indexMemorySize)
                .setDstOffset(indexBufferOffset);

        commandBuffer.copyBuffer(indexStaging->Get(),
                                 mIndexBuffers[indexBufferIndex]->Get(),
                                 1,
                                 &indexCopyRegion);

        commandBuffer.end();

        auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&commandBuffer);

        mDevice->GetTransferQueue().submit(submitInfo);
        mDevice->GetTransferQueue().waitIdle();

        mCommandPool->FreeCommandBuffer(commandBuffer);

        auto mesh = Mesh { .vertexBufferIndex  = vertexBufferIndex,
                           .vertexBufferOffset = vertexBufferOffset,
                           .indexBufferIndex   = indexBufferIndex,
                           .indexBufferOffset  = indexBufferOffset,
                           .indexCount         = (std::uint32_t) indices.size(),
                           .id                 = (std::uint32_t) mMeshes.size() };

        mMeshes.insert(mesh);

        vertexBufferOffset += vertexMemorySize;
        indexBufferOffset += indexMemorySize;

        return mesh.id;
    }

    std::vector<std::uint32_t> MeshPool::CreateMeshFromFile(const std::string path)
    {
        auto meshes = std::vector<std::uint32_t>();

        Assimp::Importer import;
        const aiScene*   scene = import.ReadFile(
            path,
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_ImproveCacheLocality |
                aiProcess_FixInfacingNormals | aiProcess_JoinIdenticalVertices);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::println("Assimp Error: {}", import.GetErrorString());
            return meshes;
        }

        processNode(meshes, scene->mRootNode, scene);

        return meshes;
    }

    std::uint32_t MeshPool::createVertexBuffer(std::uint32_t size)
    {
        auto vertexBuffer =
            BufferBuilder(mDevice)
                .SetSize(sizeof(Vertex) * size)
                .SetUsage(BufferUsage::Vertex)
                .Build();

        mVertexBuffers.push_back(vertexBuffer);
        mVertexBuffersOffsets.push_back(0);

        return mVertexBuffers.size() - 1;
    }

    std::uint32_t MeshPool::queryVertexBuffer(std::uint32_t size)
    {
        for (auto index = 0; index < mVertexBuffers.size(); index++)
        {
            if (mVertexBuffers[index]->GetSize() - mVertexBuffersOffsets[index] >= size)
            {
                return index;
            }
        }

        return createVertexBuffer(
            size > MinVertexBufferSize ? size : MinVertexBufferSize);
    }

    std::uint32_t MeshPool::createIndexBuffer(std::uint32_t size)
    {
        auto indexBuffer =
            BufferBuilder(mDevice)
                .SetSize(sizeof(uint16_t) * size)
                .SetUsage(BufferUsage::Index)
                .Build();

        mIndexBuffers.push_back(indexBuffer);
        mIndexBuffersOffsets.push_back(0);

        return mIndexBuffers.size() - 1;
    }

    std::uint32_t MeshPool::queryIndexBuffer(std::uint32_t size)
    {
        for (auto index = 0; index < mIndexBuffers.size(); index++)
        {
            if (mVertexBuffers[index]->GetSize() - mVertexBuffersOffsets[index] >= size)
            {
                return index;
            }
        }

        return createIndexBuffer(size > MinIndexBufferSize ? size : MinIndexBufferSize);
    }

    std::uint32_t MeshPool::processMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex>        vertices;
        std::vector<std::uint16_t> indices;

        for (auto i = 0; i < mesh->mNumVertices; i++)
        {
            auto& aVertex = mesh->mVertices[i];

            auto material = scene->mMaterials[mesh->mMaterialIndex];

            aiColor3D aColor(1.0, 1.0, 1.0);
            material->Get(AI_MATKEY_COLOR_DIFFUSE, aColor);

            auto vertex = Vertex { .position = glm::vec3(aVertex.x, aVertex.y, aVertex.z),
                                   .color    = glm::vec3(aColor.r, aColor.g, aColor.b) };

            // process vertex positions, normals and texture coordinates
            vertices.push_back(vertex);
        }

        for (auto i = 0; i < mesh->mNumFaces; i++)
        {
            auto& face = mesh->mFaces[i];

            for (auto j = 0; j < face.mNumIndices; j++)
            {
                indices.push_back(face.mIndices[j]);
            }
        }

        return CreateMesh(vertices, indices);
    }

    void MeshPool::processNode(std::vector<std::uint32_t>& meshes,
                               aiNode*                     node,
                               const aiScene*              scene)
    {
        // process all the node's meshes (if any)
        if (node->mNumMeshes > 0)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(processMesh(mesh, scene));
            }
        }

        // then do the same for each of its children
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(meshes, node->mChildren[i], scene);
        }
    }

} // namespace FREYA_NAMESPACE
