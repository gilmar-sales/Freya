#include "Builders/ApplicationBuilder.hpp"
#include "Builders/WindowBuilder.hpp"
#include <Core/UniformBuffer.hpp>

class MainApp : public fra::AbstractApplication
{
  public:
    void Startup()
    {
        mRenderer->ClearProjections();

        mMeshPool = mRenderer->GetMeshPoolFactory()->CreateMeshPool();

        red_ship_meshes = mMeshPool->CreateMeshFromFile("C:/Models/cartoon_spaceship_red.obj");
        mModelMatrix    = glm::mat4(1);
    }

    void Run() override
    {
        Startup();

        mCurrentTime += mWindow->GetDeltaTime();
        mModelMatrix = glm::translate(mModelMatrix, glm::vec3(0.0, glm::cos(mCurrentTime), 0.0) * mWindow->GetDeltaTime());
        mModelMatrix = glm::rotate(mModelMatrix, glm::radians(15.0f * mWindow->GetDeltaTime()), glm::vec3(0.0, 1.0, 0.0));

        mRenderer->UpdateModel(mModelMatrix);

        for (const auto& mesh : red_ship_meshes)
        {
            mMeshPool->Draw(mesh);
        }
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
                           .SetWidth(1024)
                           .SetHeight(600)
                           .SetVSync(true);
                   })
                   .Build<MainApp>();

    app->Run();

    return 0;
}