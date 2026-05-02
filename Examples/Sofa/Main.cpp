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

        // Initialize model matrices
        mModelMatrix[0] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(-3, 2, 0)), glm::vec3(0.3));
        mModelMatrix[1] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(3, 2, 0)), glm::vec3(0.3));
        mModelMatrix[2] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(-3, -2, 0)), glm::vec3(2));
        mModelMatrix[3] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(3, -2, 0)), glm::vec3(2));

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
        // (LODService constructor ran before StartUp, when mesh pool was empty)
        if (mLODService)
        {
            mLODService->RefreshMeshMetadata();
        }

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
        // For demo: simulate 3 LOD levels using the same meshes at different
        // distances In production, you'd have actual simplified mesh variants
        if (!mSpaceShipModel.empty())
        {
            // Simulated LOD distances (in world units)
            // LOD0: 0-50 units, LOD1: 50-150 units, LOD2: 150-400 units, LOD3:
            // 400+ (culled)
            std::vector<float> lodDistances = { 0.0f, 50.0f, 150.0f, 400.0f };

            // Create multiple LOD groups to simulate different detail levels
            // Using the same mesh ID for all levels as placeholder
            // In reality, you'd have: spaceship_lod0, spaceship_lod1,
            // spaceship_lod2, etc.
            for (std::uint32_t i = 0; i < mSpaceShipModel.size(); ++i)
            {
                auto                       meshId  = mSpaceShipModel[i];
                std::vector<std::uint32_t> meshIds = { meshId, meshId, meshId,
                                                       meshId };

                std::uint32_t groupId =
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
                auto                       meshId  = mSofaModel[i];
                std::vector<std::uint32_t> meshIds = { meshId, meshId, meshId,
                                                       meshId };

                std::uint32_t groupId =
                    mLODPool->CreateLODGroup(meshIds, lodDistances);
                mSofaLODGroups.push_back(groupId);
            }

            mLogger->LogInformation("Created {} LOD groups for Sofa",
                                    mSofaLODGroups.size());
        }

        // Add LOD instances for ALL sub-meshes of each model.
        // Each instance references a LOD group (one per sub-mesh) and a
        // transform index. If a model has multiple sub-meshes, all must be
        // instantiated so the compute shader emits draw commands for each.
        //
        // First spaceship at transform 0: each sub-mesh gets an instance
        for (const auto groupId : mSpaceShipLODGroups)
        {
            mSpaceShipInstanceIds.push_back(
                mLODService->AddInstance(groupId, 0));
        }
        // Second spaceship at transform 1
        for (const auto groupId : mSpaceShipLODGroups)
        {
            mSpaceShipInstanceIds.push_back(
                mLODService->AddInstance(groupId, 1));
        }

        // Sofa at transform 2: each sub-mesh gets an instance
        for (const auto groupId : mSofaLODGroups)
        {
            mSofaInstanceIds.push_back(
                mLODService->AddInstance(groupId, 2));
        }

        // Initialize transform buffer for LOD system
        // Upload initial transforms so the first Dispatch has valid data
        for (std::uint32_t i = 0; i < 4; ++i)
        {
            mLODService->UpdateTransform(i, mModelMatrix[i]);
        }

        mLogger->LogInformation(
            "LOD system initialized with {} total instances",
            mLODService->GetInstanceCount());
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();

        // Rotate model matrices
        mModelMatrix[0] = glm::rotate(
            mModelMatrix[0], glm::radians(15.0f * mWindow->GetDeltaTime()),
            glm::normalize(glm::vec3(0.0, 1.0, 0.0)));
        mModelMatrix[1] = glm::rotate(
            mModelMatrix[1], glm::radians(15.0f * mWindow->GetDeltaTime()),
            glm::normalize(glm::vec3(0.0, 1.0, 0.0)));
        mModelMatrix[2] = glm::rotate(
            mModelMatrix[2], glm::radians(15.0f * mWindow->GetDeltaTime()),
            glm::normalize(glm::vec3(0.0, 1.0, 0.0)));
        mModelMatrix[3] = glm::rotate(
            mModelMatrix[3], glm::radians(15.0f * mWindow->GetDeltaTime()),
            glm::normalize(glm::vec3(0.0, 1.0, 0.0)));

        // Update transform buffer for LOD system
        if (mLODService)
        {
            for (std::uint32_t i = 0; i < 4; ++i)
            {
                mLODService->UpdateTransform(i, mModelMatrix[i]);
            }

            // Camera position (fixed as per renderer)
            const glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, -10.1f);
            mLODService->SetCameraPosition(cameraPosition);
            mLODService->SetGlobalDrawDistance(mRenderer->GetDrawDistance());

            // Dispatch LOD compute shader OUTSIDE render pass
            // This must happen BEFORE BeginFrame() which starts the render pass
            mLODService->Dispatch(mRenderer->GetCurrentFrameIndex());
        }

        mRenderer->BeginFrame();

        // Update instance matrix buffer
        if (mInstanceMatrixBuffers == nullptr)
            mInstanceMatrixBuffers =
                mRenderer->GetBufferBuilder()
                    .SetData(&mModelMatrix[0][0])
                    .SetSize(sizeof(glm::mat4) * 4)
                    .SetUsage(fra::BufferUsage::Instance)
                    .Build();
        else
            mInstanceMatrixBuffers->Copy(
                &mModelMatrix[0][0], sizeof(glm::mat4) * 4);

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        // Draw using MDI (Multi-Draw Indirect) via LODService
        // The compute shader generates draw commands, we execute them here.
        //
        // The draw commands contain absolute buffer offsets (vertexOffset in
        // vertex-index units, firstIndex in index units), so we bind the
        // shared vertex/index buffer at offset 0. A single DrawIndirect call
        // executes all LOD-selected draws for all instances.
        //
        // Note: When instances use different materials, we need to issue
        // separate DrawIndirect calls per material (each re-executes the
        // same draw commands with the active material). A production system
        // would encode material in the draw command or sort by material.
        if (mLODService && mLODService->GetInstanceCount() > 0)
        {
            const auto drawCmdBuffer = mLODService->GetDrawCommandBufferRef();
            const auto drawCount     = mLODService->GetDrawCount();

            // Draw with spaceship material
            mMaterialPool->Bind(mSpaceShipMaterial);
            if (!mSpaceShipModel.empty())
            {
                mMeshPool->DrawIndirect(
                    mSpaceShipModel[0], drawCmdBuffer, drawCount);
            }

            // Draw with sofa material
            mMaterialPool->Bind(mSofaMaterial);
            if (!mSofaModel.empty())
            {
                mMeshPool->DrawIndirect(
                    mSofaModel[0], drawCmdBuffer, drawCount);
            }
        }
        else
        {
            // Fallback to traditional instanced rendering
            // Draw SpaceShips (2 instances)
            mMaterialPool->Bind(mSpaceShipMaterial);
            for (const auto& mesh : mSpaceShipModel)
            {
                mMeshPool->DrawInstanced(mesh, 2);
            }

            // Draw Sofas (2 instances)
            mMaterialPool->Bind(mSofaMaterial);
            for (const auto& mesh : mSofaModel)
            {
                mMeshPool->DrawInstanced(mesh, 2, 2);
            }
        }

        mRenderer->EndFrame();

        // Log LOD stats periodically
        static float logTimer = 0.0f;
        logTimer += mWindow->GetDeltaTime();
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
    glm::mat4              mModelMatrix[4] {};
    float                  mCurrentTime {};

    Ref<fra::Buffer> mInstanceMatrixBuffers;

    // LOD system data
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
                    freyaOptions.SetTitle("Sofa example - LOD Demo")
                        .SetWidth(1920)
                        .SetHeight(1080)
                        .SetVSync(false)
                        .SetSampleCount(8)
                        .SetFullscreen(false)
                        .SetDrawDistance(500.0f)
                        .SetLODDistances({ 0.0f, 50.0f, 150.0f, 400.0f })
                        .SetMaxLODLevels(4)
                        .SetUseDitherFade(true);
                });
            })
            .Build<MainApp>();

    app->Run();

    return 0;
}
