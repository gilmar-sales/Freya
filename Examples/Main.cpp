#include <iostream>

#include <Builders/ApplicationBuilder.hpp>

class MainApp : public fra::AbstractApplication
{
  public:
    void StartUp() override
    {
        mRenderer->ClearProjections();

        mMeshPool = mRenderer->GetMeshPoolFactory()->CreateMeshPool();

        red_ship_meshes = mMeshPool->CreateMeshFromFile("D:/Models/x-wing.obj");
        mModelMatrix    = glm::mat4(1);

        mEventManager->Subscribe<fra::KeyPressedEvent>([](fra::KeyPressedEvent event) {
            std::cout << ("Key pressed\n");
        });

        mEventManager->Subscribe<fra::KeyReleasedEvent>([](fra::KeyReleasedEvent event) {
            std::cout << ("Key released\n");
        });
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();
        mModelMatrix = glm::translate(mModelMatrix, glm::vec3(0.0, glm::cos(mCurrentTime), 0.0) * mWindow->GetDeltaTime());
        mModelMatrix = glm::rotate(mModelMatrix, glm::radians(15.0f * mWindow->GetDeltaTime()), glm::vec3(0.0, 1.0, 0.0));

        mRenderer->BeginFrame();

        mWindow->Update();

        auto mInstanceMatrixBuffers =
            mRenderer->GetBufferBuilder()
                .SetData(&mModelMatrix[0][0])
                .SetSize(sizeof(glm::mat4))
                .SetUsage(fra::BufferUsage::Instance)
                .Build();

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        for (const auto& mesh : red_ship_meshes)
        {
            mMeshPool->Draw(mesh);
        }

        mRenderer->EndFrame();
    }

  private:
    std::vector<unsigned>          red_ship_meshes;
    std::shared_ptr<fra::MeshPool> mMeshPool;
    glm::mat4                      mModelMatrix;
    float                          mCurrentTime;
};

int main(int argc, const char** argv)
{
    auto app = fra::ApplicationBuilder()
                   .WithWindow([](fra::WindowBuilder& windowBuilder) {
                       windowBuilder
                           .SetTitle("Space")
                           .SetWidth(1920)
                           .SetHeight(1080)
                           .SetVSync(false);
                   })
                   .WithRenderer([](fra::RendererBuilder& rendererBuilder) {
                       rendererBuilder.SetSamples(vk::SampleCountFlagBits::e8);
                   })
                   .Build<MainApp>();

    app->Run();

    return 0;
}