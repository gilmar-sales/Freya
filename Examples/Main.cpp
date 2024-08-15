#include <iostream>

#include <Builders/ApplicationBuilder.hpp>

#include <glm/ext/matrix_transform.hpp>

class MainApp final : public fra::AbstractApplication
{
  public:
    void StartUp() override
    {
        mRenderer->ClearProjections();

        mMeshPool    = mRenderer->GetMeshPoolFactory()->CreateMeshPool();
        mTexturePool = mRenderer->GetTexturePoolFactory()->CreateTexturePool();

        mModelMatrix = glm::scale(glm::mat4(1), glm::vec3(10000));

        mTextureA = mTexturePool->CreateTextureFromFile("D:/Models/Emergency Backup Generator_Default_color.jpg");
        mModelA   = mMeshPool->CreateMeshFromFile("C:/Models/moon.fbx");

        mEventManager->Subscribe<fra::KeyPressedEvent>([](fra::KeyPressedEvent event) {
            std::println("Key pressed");
        });

        mEventManager->Subscribe<fra::KeyReleasedEvent>([](fra::KeyReleasedEvent event) {
            std::println("Key released");
        });

        mEventManager->Subscribe<fra::MouseMoveEvent>([](fra::MouseMoveEvent event) {
            std::println("Mouse position: {}, {}", event.x, event.y);
            std::println("Mouse delta: {}, {}", event.deltaX, event.deltaY);
        });

        mEventManager->Subscribe<fra::MouseButtonPressedEvent>([](fra::MouseButtonPressedEvent event) {
            std::println("Mouse button pressed: {}", static_cast<int>(event.button));
        });
        mEventManager->Subscribe<fra::MouseButtonReleasedEvent>([](fra::MouseButtonReleasedEvent event) {
            std::println("Mouse button released: {}", static_cast<int>(event.button));
        });
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();
        // mModelMatrix = glm::translate(mModelMatrix, glm::vec3(0.0, glm::cos(mCurrentTime), 0.0) * mWindow->GetDeltaTime());
        mModelMatrix = glm::rotate(mModelMatrix, glm::radians(15.0f * mWindow->GetDeltaTime()), glm::normalize(glm::vec3(1.0, 1.0, 0.0)));

        mRenderer->BeginFrame();

        mWindow->Update();

        auto mInstanceMatrixBuffers =
            mRenderer->GetBufferBuilder()
                .SetData(&mModelMatrix[0][0])
                .SetSize(sizeof(glm::mat4))
                .SetUsage(fra::BufferUsage::Instance)
                .Build();

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        for (const auto& mesh : mModelA)
        {
            mTexturePool->Bind(mTextureA);
            mMeshPool->Draw(mesh);
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
    glm::mat4                         mModelMatrix {};
    float                             mCurrentTime {};
};

int main(int argc, const char** argv)
{
    const auto app = fra::ApplicationBuilder()
                         .WithWindow([](fra::WindowBuilder& windowBuilder) {
                             windowBuilder
                                 .SetTitle("Space")
                                 .SetWidth(1920)
                                 .SetHeight(1080)
                                 .SetVSync(false);
                         })
                         .WithRenderer([](fra::RendererBuilder& rendererBuilder) {
                             rendererBuilder.SetSamples(vk::SampleCountFlagBits::e8).SetDrawDistance(1000000.0f);
                         })
                         .Build<MainApp>();

    app->Run();

    return 0;
}