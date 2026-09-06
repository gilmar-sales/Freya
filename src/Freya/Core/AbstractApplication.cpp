#include "AbstractApplication.hpp"

#include "Freya/Core/IPlatform.hpp"
#include "Freya/Core/WindowConfigContext.hpp"
#include "Freya/Internal/WindowSlot.hpp"

namespace FREYA_NAMESPACE
{
    struct AbstractApplication::MultiWindowState
    {
        std::vector<WindowSlot>       slots;
        std::vector<skr::Arc<Window>> windows;
    };

    namespace
    {
        WindowSlot CreateWindowSlot(const skr::Arc<skr::ServiceProvider>& root,
                                    const skr::Arc<FreyaOptions>& options)
        {
            auto scope = root->CreateServiceScope();
            auto sp    = scope->GetServiceProvider();

            auto ctx     = sp->GetService<WindowConfigContext>();
            ctx->options = options;

            auto window   = sp->GetService<Window>();
            auto renderer = sp->GetService<Renderer>();

            return WindowSlot { .scope    = std::move(scope),
                                .window   = std::move(window),
                                .renderer = std::move(renderer) };
        }
    } // namespace

    AbstractApplication::AbstractApplication(
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        IApplication(serviceProvider), mDeltaTime(0),
        mMultiWindow(std::make_unique<MultiWindowState>())
    {
        mPlatform = mRootServiceProvider->GetService<IPlatform>();

        const auto defaults =
            mRootServiceProvider->GetService<FreyaOptionsTemplate>();

        // Main window seeds the shared template Arc so PhysicalDevice /
        // Surface clamps (sampleCount / frameCount) update the template for
        // later CreateWindow clones.
        auto mainSlot =
            CreateWindowSlot(mRootServiceProvider, defaults->options);
        mMainScope = mainSlot.scope;
        mWindow    = mainSlot.window;
        mRenderer  = mainSlot.renderer;
        mEventManager =
            mMainScope->GetServiceProvider()->GetService<EventManager>();
    }

    AbstractApplication::~AbstractApplication() = default;

    skr::Arc<skr::ServiceProvider> AbstractApplication::GetMainServiceProvider()
        const
    {
        return mMainScope->GetServiceProvider();
    }

    const std::vector<skr::Arc<Window>>& AbstractApplication::SecondaryWindows()
        const
    {
        return mMultiWindow->windows;
    }

    skr::Arc<skr::ServiceProvider> AbstractApplication::GetWindowServices(
        const Window& window) const
    {
        if (mWindow.get() == &window)
            return GetMainServiceProvider();

        for (const auto& slot : mMultiWindow->slots)
        {
            if (slot.window.get() == &window)
                return slot.scope->GetServiceProvider();
        }

        return nullptr;
    }

    skr::Arc<Renderer> AbstractApplication::GetRenderer(
        const Window& window) const
    {
        if (mWindow.get() == &window)
            return mRenderer;

        for (const auto& slot : mMultiWindow->slots)
        {
            if (slot.window.get() == &window)
                return slot.renderer;
        }

        return nullptr;
    }

    skr::Arc<Window> AbstractApplication::CreateWindow(
        const std::function<void(FreyaOptionsBuilder&)>& configure)
    {
        const auto defaults =
            mRootServiceProvider->GetService<FreyaOptionsTemplate>();

        FreyaOptionsBuilder builder;
        *builder.Build() = *defaults->options;
        if (configure)
            configure(builder);

        auto slot   = CreateWindowSlot(mRootServiceProvider, builder.Build());
        auto window = slot.window;
        mMultiWindow->windows.push_back(window);
        mMultiWindow->slots.push_back(std::move(slot));
        return window;
    }

    void AbstractApplication::Run()
    {
        StartUp();

        while (mWindow->IsRunning())
        {
            mPlatform->PumpEvents();
            if (!mWindow->IsRunning())
                break;

            mWindow->Update();
            Update();

            auto& slots   = mMultiWindow->slots;
            auto& windows = mMultiWindow->windows;

            for (std::size_t i = 0; i < slots.size();)
            {
                if (!slots[i].window->IsRunning())
                {
                    slots.erase(slots.begin() + static_cast<std::ptrdiff_t>(i));
                    windows.erase(
                        windows.begin() + static_cast<std::ptrdiff_t>(i));
                    continue;
                }

                slots[i].window->Update();
                UpdateSecondaryWindow(slots[i].window);
                ++i;
            }
        }

        mMultiWindow->slots.clear();
        mMultiWindow->windows.clear();
        ShutDown();
    }

} // namespace FREYA_NAMESPACE
