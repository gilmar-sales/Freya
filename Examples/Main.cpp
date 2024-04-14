#include "Builders/WindowBuilder.hpp"
#include "Builders/ApplicationBuilder.hpp"
#include <Core/UniformBuffer.hpp>

class MainApp : public fra::AbstractApplication
{
  public:

    void Startup() override
    {
        mMeshPool = mRenderer->GetMeshPoolFactory()->CreateMeshPool();

        red_ship_meshes = mMeshPool->CreateMeshFromFile("C:/Models/cartoon_spaceship_red.obj");
    }

    void Update() override
    {
        mRenderer->ClearProjection();

        for (const auto& mesh : red_ship_meshes)
        {
            mMeshPool->Draw(mesh);
        }
    }

  private:
    std::vector<unsigned> red_ship_meshes;
    std::shared_ptr<fra::MeshPool> mMeshPool;
};

int main(int argc, const char **argv)
{
    auto app = fra::ApplicationBuilder()
                .WithWindow([](fra::WindowBuilder& windowBuilder)
                {
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