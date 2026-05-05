#include <Freya/Freya.hpp>

#include <cmath>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const Ref<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {

        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();

        mModelMatrix[0] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(-3, 2, 0)), glm::vec3(0.3));

        mModelMatrix[1] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(3, 2, 0)), glm::vec3(0.3));

        mModelMatrix[2] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(-3, -2, 0)), glm::vec3(2));

        mModelMatrix[3] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(3, -2, 0)), glm::vec3(2));

        mSofaAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_BaseColor.png");
        mSofaNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Normal.png");
        mSofaRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Roughness.png");

        mSofaMaterial =
            mMaterialPool->Create({ mSofaAlbedo, mSofaNormal, mSofaRoughness });

        mSofaModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/OfficeSofa.fbx");

        mSpaceShipAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Base_color.jpg");

        mSpaceShipNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Normal.jpg");

        mSpaceShipRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Roughness.jpg");

        mSpaceShipMaterial = mMaterialPool->Create(
            { mSpaceShipAlbedo, mSpaceShipNormal, mSpaceShipRoughness });

        mSpaceShipModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/SpaceShip.fbx");
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();

        mRenderer->BeginFrame(); // subpass 0 (depth pre-pass)

        // Orbit camera around the origin
        constexpr float radius = 15.0f;
        constexpr float speed  = 0.3f;
        const glm::vec3 position(radius * std::cos(speed * mCurrentTime), 2.0f,
                                 radius * std::sin(speed * mCurrentTime));
        mRenderer->UpdateCamera(
            position, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });

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

        // Subpass 0: depth pre-pass (only writes depth, no material needed)
        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        for (const auto& mesh : mSpaceShipModel)
            mMeshPool->DrawInstanced(mesh, 2);
        for (const auto& mesh : mSofaModel)
            mMeshPool->DrawInstanced(mesh, 2, 2);

        // Subpass 1: G-buffer (writes position, normal, albedo)
        mRenderer->AdvanceSubpass(fra::DeferredGBufferPass);

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        mMaterialPool->Bind(mSpaceShipMaterial);
        for (const auto& mesh : mSpaceShipModel)
            mMeshPool->DrawInstanced(mesh, 2);

        mMaterialPool->Bind(mSofaMaterial);
        for (const auto& mesh : mSofaModel)
            mMeshPool->DrawInstanced(mesh, 2, 2);

        // EndFrame advances to lighting → translucent → composite
        // and draws the fullscreen triangles for lighting + composite.
        mRenderer->EndFrame();
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
    glm::mat4              mModelMatrix[4] {};
    float                  mCurrentTime {};

    Ref<fra::Buffer> mInstanceMatrixBuffers;
};

int main(int argc, const char** argv)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions.SetTitle("Sofa example")
                        .SetWidth(1920)
                        .SetHeight(1080)
                        .SetVSync(false)
                        .SetSampleCount(8)
                        .WithReverseZ()
                        .SetFullscreen(false)
                        .SetRenderingStrategy(fra::RenderingStrategy::Deferred);
                });
            })
            .Build<MainApp>();

    app->Run();

    return 0;
}
