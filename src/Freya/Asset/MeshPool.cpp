#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes           = 1024 * 1024;
    constexpr auto MinVertexBufferSize = 1 * MegaBytes;
    constexpr auto MinIndexBufferSize  = 2 * MegaBytes;

    MeshPool::MeshPool(const Ref<Device>&                device,
                       const Ref<PhysicalDevice>&        physicalDevice,
                       const Ref<CommandPool>&           commandPool,
                       const Ref<skr::Logger<MeshPool>>& logger) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mCommandPool(commandPool), mLogger(logger), mMeshes(4096)
    {
        mVertexBuffers.reserve(1024);
        mIndexBuffers.reserve(1024);
        mStagingBuffers.reserve(1024);

        createVertexBuffer(MinVertexBufferSize);
    }

    void MeshPool::Draw(const std::uint32_t meshId)
    {
        if (mMeshes.contains(meshId))
        {
            const auto& mesh = mMeshes[meshId];

            const auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            const auto  vertexOffset =
                static_cast<vk::DeviceSize>(mesh.vertexBufferOffset);

            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &vertexOffset);

            const auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            const auto  indexOffset =
                static_cast<vk::DeviceSize>(mesh.indexBufferOffset);
            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                indexOffset,
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer()
                .drawIndexed(mesh.indexCount, 1, 0, 0, 0);
        }
    }

    void MeshPool::DrawInstanced(std::uint32_t meshId,
                                 size_t        instanceCount,
                                 size_t        firstInstance)
    {
        if (mMeshes.contains(meshId))
        {
            auto& mesh = mMeshes[meshId];

            const auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            const auto  vertexOffset =
                static_cast<vk::DeviceSize>(mesh.vertexBufferOffset);

            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &vertexOffset);

            const auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            const auto  indexOffset =
                static_cast<vk::DeviceSize>(mesh.indexBufferOffset);

            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                indexOffset,
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer().drawIndexed(
                mesh.indexCount,
                instanceCount,
                0,
                0,
                firstInstance);
        }
    }

    std::uint32_t MeshPool::CreateMesh(const std::vector<Vertex>&   vertices,
                                       const std::vector<uint16_t>& indices)
    {
        mLogger->LogTrace("Creating mesh with {} vertices and {} indices.",
                          vertices.size(),
                          indices.size());

        constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        const auto commandBuffer = mCommandPool->CreateCommandBuffer();

        commandBuffer.begin(beginInfo);

        const auto vertexMemorySize = vertices.size() * sizeof(vertices[0]);
        const auto indexMemorySize  = indices.size() * sizeof(indices[0]);

        const auto stagingBuffer =
            queryStagingBuffer(vertexMemorySize + indexMemorySize);

        stagingBuffer->Copy(vertices.data(), vertexMemorySize);

        stagingBuffer->Copy(indices.data(), indexMemorySize, vertexMemorySize);

        // copy vertices
        const auto vertexBufferIndex = queryVertexBuffer(vertexMemorySize);
        auto& vertexBufferOffset     = mVertexBuffersOffsets[vertexBufferIndex];

        const auto vertexCopyRegion =
            vk::BufferCopy()
                .setSrcOffset(0)
                .setSize(vertexMemorySize)
                .setDstOffset(vertexBufferOffset);

        commandBuffer.copyBuffer(stagingBuffer->Get(),
                                 mVertexBuffers[vertexBufferIndex]->Get(),
                                 1,
                                 &vertexCopyRegion);

        // copy indices
        const auto indexBufferIndex  = queryIndexBuffer(indexMemorySize);
        auto&      indexBufferOffset = mIndexBuffersOffsets[indexBufferIndex];

        const auto indexCopyRegion =
            vk::BufferCopy()
                .setSrcOffset(vertexMemorySize)
                .setSize(indexMemorySize)
                .setDstOffset(indexBufferOffset);

        commandBuffer.copyBuffer(stagingBuffer->Get(),
                                 mIndexBuffers[indexBufferIndex]->Get(),
                                 1,
                                 &indexCopyRegion);

        commandBuffer.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                &commandBuffer);

        mDevice->GetTransferQueue().submit(submitInfo);
        mDevice->GetTransferQueue().waitIdle();

        mCommandPool->FreeCommandBuffer(commandBuffer);

        const auto mesh =
            Mesh { .vertexBufferIndex  = vertexBufferIndex,
                   .vertexBufferOffset = vertexBufferOffset,
                   .indexBufferIndex   = indexBufferIndex,
                   .indexBufferOffset  = indexBufferOffset,
                   .indexCount = static_cast<std::uint32_t>(indices.size()),
                   .id         = static_cast<std::uint32_t>(mMeshes.size()) };

        mMeshes.insert(mesh);

        vertexBufferOffset += vertexMemorySize;
        indexBufferOffset += indexMemorySize;

        return mesh.id;
    }

    std::vector<std::uint32_t> MeshPool::CreateMeshFromFile(
        const std::string& path)
    {
        mLogger->LogTrace("Creating mesh from file: {}", path.data());

        auto meshes = std::vector<std::uint32_t>();

        Assimp::Importer import;
        const aiScene*   scene = import.ReadFile(
            path,
            aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                aiProcess_SortByPType | aiProcess_GenNormals |
                aiProcess_GenUVCoords | aiProcess_OptimizeMeshes |
                aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale |
                aiProcess_ValidateDataStructure);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
            !scene->mRootNode)
        {
            mLogger->LogError("Failed loading mesh: {}",
                              import.GetErrorString());

            return meshes;
        }

        processNode(meshes, scene->mRootNode, scene);

        return meshes;
    }

    std::uint32_t MeshPool::createVertexBuffer(const std::uint32_t size)
    {
        const auto vertexBuffer =
            BufferBuilder(mDevice)
                .SetSize(size)
                .SetUsage(BufferUsage::Vertex)
                .Build();

        mVertexBuffers.push_back(vertexBuffer);
        mVertexBuffersOffsets.push_back(0);

        return mVertexBuffers.size() - 1;
    }

    std::uint32_t MeshPool::queryVertexBuffer(const std::uint32_t size)
    {
        for (auto index = 0; index < mVertexBuffers.size(); index++)
        {
            if (mVertexBuffers[index]->GetSize() -
                    mVertexBuffersOffsets[index] >=
                size)
            {
                return index;
            }
        }

        return createVertexBuffer(
            size > MinVertexBufferSize ? size : MinVertexBufferSize);
    }

    std::uint32_t MeshPool::createIndexBuffer(const std::uint32_t size)
    {
        const auto indexBuffer =
            BufferBuilder(mDevice)
                .SetSize(size)
                .SetUsage(BufferUsage::Index)
                .Build();

        mIndexBuffers.push_back(indexBuffer);
        mIndexBuffersOffsets.push_back(0);

        return mIndexBuffers.size() - 1;
    }

    std::uint32_t MeshPool::queryIndexBuffer(const std::uint32_t size)
    {
        for (auto index = 0; index < mIndexBuffers.size(); index++)
        {
            if (mVertexBuffers[index]->GetSize() -
                    mVertexBuffersOffsets[index] >=
                size)
            {
                return index;
            }
        }

        return createIndexBuffer(
            size > MinIndexBufferSize ? size : MinIndexBufferSize);
    }

    std::uint32_t MeshPool::processMesh(const aiMesh*  mesh,
                                        const aiScene* scene)
    {
        std::vector<Vertex>        vertices;
        std::vector<std::uint16_t> indices;

        for (auto i = 0; i < mesh->mNumVertices; i++)
        {
            const auto& aVertex    = mesh->mVertices[i];
            const auto& aNormal    = mesh->mNormals[i];
            const auto& aTangent   = mesh->mTangents[i];
            const auto& aTextCoord = mesh->mTextureCoords[0][i];

            const auto material = scene->mMaterials[mesh->mMaterialIndex];

            aiColor3D aColor(1.0, 1.0, 1.0);
            material->Get(AI_MATKEY_COLOR_DIFFUSE, aColor);

            auto vertex = Vertex {
                .position = glm::vec3(aVertex.x, aVertex.y, aVertex.z),
                .color    = glm::vec3(aColor.r, aColor.g, aColor.b),
                .normal   = glm::vec3(aNormal.x, aNormal.y, aNormal.z),
                .tangent  = glm::vec3(aTangent.x, aTangent.y, aTangent.z),
                .texCoord = glm::vec2(aTextCoord.x, aTextCoord.y)
            };

            // process vertex positions, normals and texture coordinates
            vertices.push_back(vertex);
        }

        for (auto i = 0; i < mesh->mNumFaces; i++)
        {
            const auto& face = mesh->mFaces[i];

            for (auto j = 0; j < face.mNumIndices; j++)
            {
                indices.push_back(face.mIndices[j]);
            }
        }

        return CreateMesh(vertices, indices);
    }

    void MeshPool::processNode(std::vector<std::uint32_t>& meshes,
                               const aiNode*               node,
                               const aiScene*              scene)
    {
        // process all the node's meshes (if any)
        if (node->mNumMeshes > 0)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(processMesh(mesh, scene));
            }
        }

        // then do the same for each of its children
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(meshes, node->mChildren[i], scene);
        }
    }

    Ref<Buffer> MeshPool::queryStagingBuffer(const std::uint32_t size)
    {
        for (auto stagingBuffer : mStagingBuffers)
        {
            if (stagingBuffer->GetSize() >= size)
            {
                return stagingBuffer;
            }
        }

        return createStagingBuffer(size);
    }

    Ref<Buffer> MeshPool::createStagingBuffer(const std::uint32_t size)
    {
        const auto bufferSize = (size / MegaBytes + 4) * MegaBytes;

        auto stagingBuffer =
            BufferBuilder(mDevice)
                .SetSize(bufferSize)
                .SetUsage(BufferUsage::Staging)
                .Build();

        mStagingBuffers.push_back(stagingBuffer);

        return stagingBuffer;
    }
} // namespace FREYA_NAMESPACE
