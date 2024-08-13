#include <iostream>

#include <Builders/ApplicationBuilder.hpp>

#include <glm/ext/matrix_transform.hpp>

class MainApp : public fra::AbstractApplication
{
  public:
    void StartUp() override
    {
        mRenderer->ClearProjections();

        mMeshPool    = mRenderer->GetMeshPoolFactory()->CreateMeshPool();
        mTexturePool = mRenderer->GetTexturePoolFactory()->CreateTexturePool();

        red_ship_meshes = mMeshPool->CreateMeshFromFile("D:/Models/Civic.obj");
        mModelMatrix    = glm::mat4(1);

        auto texture = mTexturePool->CreateTextureFromFile("./Shaders/papel-parede.jpg");

        mEventManager->Subscribe<fra::KeyPressedEvent>([](fra::KeyPressedEvent event) {
            std::println("Key pressed");
        });

        mEventManager->Subscribe<fra::KeyReleasedEvent>([](fra::KeyReleasedEvent event) {
            std::println("Key released");
        });

        // mEventManager->Subscribe<fra::MouseMoveEvent>([](fra::MouseMoveEvent event) {
        //     std::println("Mouse position: {}, {}", event.x, event.y);
        //     std::println("Mouse delta: {}, {}", event.deltaX, event.deltaY);
        // });

        mEventManager->Subscribe<fra::MouseButtonPressedEvent>([](fra::MouseButtonPressedEvent event) {
            std::println("Mouse button pressed: {}", (int) event.button);
        });
        mEventManager->Subscribe<fra::MouseButtonReleasedEvent>([](fra::MouseButtonReleasedEvent event) {
            std::println("Mouse button released: {}", (int) event.button);
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
    std::vector<unsigned>             red_ship_meshes;
    std::shared_ptr<fra::TexturePool> mTexturePool;
    std::shared_ptr<fra::MeshPool>    mMeshPool;
    glm::mat4                         mModelMatrix;
    float                             mCurrentTime;
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