#include <Freya/Builders/ApplicationBuilder.hpp>

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

        mRenderer->BeginFrame();

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

        mMaterialPool->Bind(mSpaceShipMaterial);

        for (const auto& mesh : mSpaceShipModel)
        {
            mMeshPool->DrawInstanced(mesh, 2);
        }

        mMaterialPool->Bind(mSofaMaterial);

        for (const auto& mesh : mSofaModel)
        {
            mMeshPool->DrawInstanced(mesh, 2, 2);
        }

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
    const auto app = fra::ApplicationBuilder()
                         .WithOptions([](fra::FreyaOptions& freyaOptions) {
                             freyaOptions.title       = "Sofa example";
                             freyaOptions.width       = 1920;
                             freyaOptions.height      = 1080;
                             freyaOptions.vSync       = false;
                             freyaOptions.sampleCount = 8;
                         })
                         .Build<MainApp>();

    app->Run();

    return 0;
}
