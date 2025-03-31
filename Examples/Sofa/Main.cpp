#include <Freya/Builders/ApplicationBuilder.hpp>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const Ref<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {

        mMeshPool    = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool = serviceProvider->GetService<fra::TexturePool>();
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();

        mModelMatrix[0] = glm::translate(
            glm::scale(glm::mat4(1), glm::vec3(3)), glm::vec3(-1, 0, 0));

        mModelMatrix[1] = glm::translate(
            glm::scale(glm::mat4(1), glm::vec3(3)), glm::vec3(1, 0, 0));

        mTextureA = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_BaseColor.png");
        mTextureB = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Normal.png");
        mModelA =
            mMeshPool->CreateMeshFromFile("./Resources/Models/OfficeSofa.fbx");
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

        mRenderer->BeginFrame();

        if (mInstanceMatrixBuffers == nullptr)
            mInstanceMatrixBuffers =
                mRenderer->GetBufferBuilder()
                    .SetData(&mModelMatrix[0][0])
                    .SetSize(sizeof(glm::mat4) * 2)
                    .SetUsage(fra::BufferUsage::Instance)
                    .Build();
        else
            mInstanceMatrixBuffers->Copy(
                &mModelMatrix[0][0], sizeof(glm::mat4) * 2);

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        for (const auto& mesh : mModelA)
        {
            mTexturePool->Bind(mTextureA);
            mMeshPool->DrawInstanced(mesh, 1);
            mTexturePool->Bind(mTextureB);
            mMeshPool->DrawInstanced(mesh, 1, 1);
        }

        mRenderer->EndFrame();
    }

  private:
    std::vector<unsigned> mModelA;
    std::uint32_t         mTextureA {};

    std::vector<unsigned> mModelB;
    std::uint32_t         mTextureB {};

    Ref<fra::TexturePool> mTexturePool;
    Ref<fra::MeshPool>    mMeshPool;
    glm::mat4             mModelMatrix[2] {};
    float                 mCurrentTime {};

    Ref<fra::Buffer> mInstanceMatrixBuffers;
};

int main(int argc, const char** argv)
{
    const auto app =
        fra::ApplicationBuilder()
            .WithOptions([](fra::FreyaOptions& freyaOptions) {
                freyaOptions.title       = "Sofa example";
                freyaOptions.width       = 1920;
                freyaOptions.height      = 1080;
                freyaOptions.vSync       = false;
                freyaOptions.sampleCount = vk::SampleCountFlagBits::e4;
            })
            .Build<MainApp>();

    app->Run();

    return 0;
}
