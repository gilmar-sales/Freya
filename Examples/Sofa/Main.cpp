#include <Freya/Freya.hpp>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const Ref<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
        mLODService   = serviceProvider->GetService<fra::LODService>();
        mLODPool      = serviceProvider->GetService<fra::LODPool>();
        mLogger       = serviceProvider->GetService<skr::Logger<MainApp>>();
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();

        // Two parallel queues extending into the distance, 10 objects each.
        // Each queue steps 10 units further back so the LOD system selects
        // progressively lower detail levels.
        constexpr std::uint32_t CountPerRow  = 10;
        constexpr std::uint32_t TotalObjects = CountPerRow * 2; // 20

        mModelMatrix.resize(TotalObjects);

        constexpr auto yPos     = -5.0f;
        constexpr auto initialX = 5.0f;
        constexpr auto xPosStep = 10.0f;
        for (std::uint32_t i = 0; i < CountPerRow; ++i)
        {
            // Spaceship queue (transforms 0..9): left side
            const float zPos = initialX + static_cast<float>(i) * xPosStep;
            mModelMatrix[i]  = glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(-3, yPos, zPos)),
                glm::vec3(0.3f));
        }
        for (std::uint32_t i = 0; i < CountPerRow; ++i)
        {
            // Sofa queue (transforms 10..19): right side
            const float zPos = initialX + static_cast<float>(i) * xPosStep;
            mModelMatrix[i + CountPerRow] = glm::scale(
                glm::translate(glm::mat4(1), glm::vec3(3, yPos, zPos)),
                glm::vec3(2.0f));
        }

        // Create textures
        mSofaAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_BaseColor.png");
        mSofaNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Normal.png");
        mSofaRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Roughness.png");
        mSofaMaterial =
            mMaterialPool->Create({ mSofaAlbedo, mSofaNormal, mSofaRoughness });

        mSpaceShipAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Base_color.jpg");
        mSpaceShipNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Normal.jpg");
        mSpaceShipRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Roughness.jpg");
        mSpaceShipMaterial = mMaterialPool->Create(
            { mSpaceShipAlbedo, mSpaceShipNormal, mSpaceShipRoughness });

        // Load meshes
        mSofaModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/OfficeSofa.fbx");
        mSpaceShipModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/SpaceShip.fbx");

        // Refresh mesh metadata after meshes are loaded
        if (mLODService)
            mLODService->RefreshMeshMetadata();

        // === LOD System Setup ===
        setupLODSystem();

        mLogger->LogInformation("Sofa example started with LOD system");
    }

    void setupLODSystem()
    {
        if (!mLODService || !mLODPool)
        {
            mLogger->LogWarning("LODService or LODPool not available");
            return;
        }

        // Create LOD groups for SpaceShip
        if (!mSpaceShipModel.empty())
        {
            std::vector<float> lodDistances = { 0.0f, 50.0f, 150.0f, 400.0f };

            for (std::uint32_t i = 0; i < mSpaceShipModel.size(); ++i)
            {
                const auto meshId = mSpaceShipModel[i];

                // Create progressively simplified versions via vertex
                // clustering decimation directly in MeshPool
                const auto lod0 = meshId;
                const auto lod1 =
                    mMeshPool->CreateSimplifiedMesh(meshId, 0.50f);
                const auto lod2 =
                    mMeshPool->CreateSimplifiedMesh(meshId, 0.75f);
                const auto lod3 =
                    mMeshPool->CreateSimplifiedMesh(meshId, 0.90f);

                std::vector<std::uint32_t> meshIds = { lod0, lod1, lod2, lod3 };
                std::uint32_t              groupId =
                    mLODPool->CreateLODGroup(meshIds, lodDistances);
                mSpaceShipLODGroups.push_back(groupId);
            }
            mLogger->LogInformation("Created {} LOD groups for SpaceShip",
                                    mSpaceShipLODGroups.size());
        }

        // Create LOD groups for Sofa
        if (!mSofaModel.empty())
        {
            std::vector<float> lodDistances = { 0.0f, 30.0f, 80.0f, 200.0f };

            for (std::uint32_t i = 0; i < mSofaModel.size(); ++i)
            {
                const auto meshId = mSofaModel[i];

                // Sofa has fewer triangles, so use gentler reductions
                const auto lod0 = meshId;
                const auto lod1 =
                    mMeshPool->CreateSimplifiedMesh(meshId, 0.30f);
                const auto lod2 =
                    mMeshPool->CreateSimplifiedMesh(meshId, 0.55f);
                const auto lod3 =
                    mMeshPool->CreateSimplifiedMesh(meshId, 0.75f);

                std::vector<std::uint32_t> meshIds = { lod0, lod1, lod2, lod3 };
                std::uint32_t              groupId =
                    mLODPool->CreateLODGroup(meshIds, lodDistances);
                mSofaLODGroups.push_back(groupId);
            }
            mLogger->LogInformation("Created {} LOD groups for Sofa",
                                    mSofaLODGroups.size());
        }

        // Simplified meshes were created after the initial RefreshMeshMetadata()
        // call, so re-populate the GPU metadata buffer so the compute shader
        // finds valid indexCount/firstIndex/vertexOffset for every mesh.
        mLODService->RefreshMeshMetadata();

        // Add LOD instances: 10 spaceships (transforms 0..9) and 10 sofas
        // (transforms 10..19).
        for (std::uint32_t idx = 0; idx < 10; ++idx)
        {
            for (const auto groupId : mSpaceShipLODGroups)
                mSpaceShipInstanceIds.push_back(
                    mLODService->AddInstance(groupId, idx, mSpaceShipMaterial));
            for (const auto groupId : mSofaLODGroups)
                mSofaInstanceIds.push_back(
                    mLODService->AddInstance(groupId, idx + 10, mSofaMaterial));
        }

        // Upload all initial transforms so the first Dispatch has valid data
        for (std::uint32_t i = 0;
             i < static_cast<std::uint32_t>(mModelMatrix.size());
             ++i)
            mLODService->UpdateTransform(i, mModelMatrix[i]);

        mLogger->LogInformation(
            "LOD system initialized with {} total instances",
            mLODService->GetInstanceCount());
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();

        // Rotate all model matrices uniformly
        const float angle = glm::radians(15.0f * dt);
        const auto  axis  = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
        for (auto& mat : mModelMatrix)
            mat = glm::rotate(mat, angle, axis);

        // Update LOD transform buffer and dispatch compute
        if (mLODService)
        {
            for (std::uint32_t i = 0;
                 i < static_cast<std::uint32_t>(mModelMatrix.size());
                 ++i)
                mLODService->UpdateTransform(i, mModelMatrix[i]);

            const glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, -10.1f);
            mLODService->SetCameraPosition(cameraPosition);
            mLODService->SetGlobalDrawDistance(mRenderer->GetDrawDistance());
            mLODService->Dispatch(mRenderer->GetCurrentFrameIndex());
        }

        mRenderer->BeginFrame();

        // Update instance matrix buffer (all transforms)
        const auto totalBytes =
            sizeof(glm::mat4) * static_cast<std::uint32_t>(mModelMatrix.size());
        if (mInstanceMatrixBuffers == nullptr)
            mInstanceMatrixBuffers =
                mRenderer->GetBufferBuilder()
                    .SetData(mModelMatrix.data())
                    .SetSize(totalBytes)
                    .SetUsage(fra::BufferUsage::Instance)
                    .Build();
        else
            mInstanceMatrixBuffers->Copy(mModelMatrix.data(), totalBytes);

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        // Draw using MDI (Multi-Draw Indirect) via LODService
        if (mLODService && mLODService->GetInstanceCount() > 0)
        {
            const auto drawCmdBuffer = mLODService->GetDrawCommandBufferRef();
            const auto drawCount     = mLODService->GetDrawCount();

            // Bind the global bindless sampler descriptor set (set 1)
            mMaterialPool->Bind(0);

            // Bind the draw metadata descriptor (set 2)
            {
                auto cmdBuffer =
                    mRenderer->GetCommandPool()->GetCommandBuffer();
                auto lodDescSet = mLODService->GetDrawMetaDescriptorSet();
                cmdBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mRenderer->GetRenderPass()->GetPipelineLayout(), 2, 1,
                    &lodDescSet, 0, nullptr);
            }

            // Single DrawIndirect call
            if (!mSpaceShipModel.empty())
                mMeshPool->DrawIndirect(
                    mSpaceShipModel[0], drawCmdBuffer, drawCount);
        }
        else
        {
            // Fallback
            mMaterialPool->Bind(0);
            for (const auto& mesh : mSpaceShipModel)
                mMeshPool->DrawInstanced(mesh, 10);
            mMaterialPool->Bind(0);
            for (const auto& mesh : mSofaModel)
                mMeshPool->DrawInstanced(mesh, 10, 10);
        }

        mRenderer->EndFrame();

        // Log LOD stats periodically
        static float logTimer = 0.0f;
        logTimer += dt;
        if (logTimer > 2.0f && mLODService)
        {
            mLogger->LogInformation("LOD Stats: Instances={}, MaxInstances={}",
                                    mLODService->GetInstanceCount(),
                                    mLODService->GetMaxInstances());
            logTimer = 0.0f;
        }
    }

  private:
    std::vector<unsigned> mSofaModel;
    std::uint32_t         mSofaAlbedo {};
    std::uint32_t         mSofaNormal {};
    std::uint32_t         mSofaRoughness {};
    std::uint32_t         mSofaMaterial {};

    std::vector<unsigned> mSpaceShipModel;
    std::uint32_t         mSpaceShipAlbedo {};
    std::uint32_t         mSpaceShipNormal {};
    std::uint32_t         mSpaceShipRoughness {};
    std::uint32_t         mSpaceShipMaterial {};

    Ref<fra::MaterialPool> mMaterialPool;
    Ref<fra::TexturePool>  mTexturePool;
    Ref<fra::MeshPool>     mMeshPool;
    Ref<fra::LODService>   mLODService;
    Ref<fra::LODPool>      mLODPool;
    std::vector<glm::mat4> mModelMatrix {};
    float                  mCurrentTime {};

    Ref<fra::Buffer>           mInstanceMatrixBuffers;
    std::vector<std::uint32_t> mSpaceShipLODGroups;
    std::vector<std::uint32_t> mSofaLODGroups;
    std::vector<std::uint32_t> mSpaceShipInstanceIds;
    std::vector<std::uint32_t> mSofaInstanceIds;

    Ref<skr::Logger<MainApp>> mLogger;
};

int main(int argc, const char** argv)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions.SetTitle("Sofa Example - LOD Demo")
                        .SetWidth(1920)
                        .SetHeight(1080)
                        .SetVSync(false)
                        .SetSampleCount(8)
                        .SetFullscreen(false)
                        .SetDrawDistance(1000.0f)
                        .SetLODDistances({ 0.0f, 50.0f, 150.0f, 400.0f })
                        .SetMaxLODLevels(4)
                        .SetUseDitherFade(true);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
