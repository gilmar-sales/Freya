#include <Builders/ApplicationBuilder.hpp>

#include <glm/ext/matrix_transform.hpp>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const Ref<ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();

        mMeshPool    = mRenderer->GetMeshPoolFactory()->CreateMeshPool();
        mTexturePool = mRenderer->GetTexturePoolFactory()->CreateTexturePool();

        mModelMatrix[0] = glm::translate(
            glm::scale(glm::mat4(1), glm::vec3(300)), glm::vec3(-1, 0, 0));
        mModelMatrix[1] = glm::translate(
            glm::scale(glm::mat4(1), glm::vec3(300)), glm::vec3(1, 0, 0));

        mTextureA = mTexturePool->CreateTextureFromFile(
            "D:/Models/textures/Office_sofa_DefaultMaterial_BaseColor.png");
        mTextureB = mTexturePool->CreateTextureFromFile(
            "D:/Models/textures/Office_sofa_DefaultMaterial_Normal.png");
        mModelA =
            mMeshPool->CreateMeshFromFile("D:/Models/source/Office sofa.fbx");
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();
        // mModelMatrix = glm::translate(mModelMatrix, glm::vec3(0.0,
        // glm::cos(mCurrentTime), 0.0) * mWindow->GetDeltaTime());
        mModelMatrix[0] = glm::rotate(
            mModelMatrix[0], glm::radians(15.0f * mWindow->GetDeltaTime()),
            glm::normalize(glm::vec3(0.0, 1.0, 0.0)));
        mModelMatrix[1] = glm::rotate(
            mModelMatrix[1], glm::radians(15.0f * mWindow->GetDeltaTime()),
            glm::normalize(glm::vec3(0.0, 1.0, 0.0)));

        mRenderer->BeginFrame();

        mWindow->Update();

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

    std::shared_ptr<fra::TexturePool> mTexturePool;
    std::shared_ptr<fra::MeshPool>    mMeshPool;
    glm::mat4                         mModelMatrix[2] {};
    float                             mCurrentTime {};

    Ref<fra::Buffer> mInstanceMatrixBuffers;
};

int main(int argc, const char** argv)
{
    const auto app =
        fra::ApplicationBuilder()
            .WithWindow([](fra::WindowBuilder& windowBuilder) {
                windowBuilder.SetTitle("Space")
                    .SetWidth(1920)
                    .SetHeight(1080)
                    .SetVSync(false);
            })
            .WithRenderer([](fra::RendererBuilder& rendererBuilder) {
                rendererBuilder.SetSamples(vk::SampleCountFlagBits::e8)
                    .SetDrawDistance(1000000.0f);
            })
            .Build<MainApp>();

    app->Run();

    return 0;
}