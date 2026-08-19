#include "AbstractApplication.hpp"

FREYA_NAMESPACE::AbstractApplication::AbstractApplication(
    const skr::Arc<skr::ServiceProvider>& serviceProvider) :
    IApplication(serviceProvider), mDeltaTime(0)
{
    mEventManager = mRootServiceProvider->GetService<EventManager>();
    mWindow       = mRootServiceProvider->GetService<Window>();
    mRenderer     = mRootServiceProvider->GetService<Renderer>();
}

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
