#include "Core/AbstractApplication.hpp"

namespace FREYA_NAMESPACE
{
    void AbstractApplication::Run()
    {
        Startup();

        while(mWindow->IsRunning())
        {
            mRenderer->BeginFrame();

            mWindow->Update();
            Update();

            mRenderer->EndFrame();
        }
    }
} // namespace FREYA_NAMESPACE
