#include "AbstractApplication.hpp"

void FREYA_NAMESPACE::AbstractApplication::Run()
{
    StartUp();

    while (mWindow->IsRunning())
    {
        mWindow->Update();
        Update();
    }

    ShutDown();
};
