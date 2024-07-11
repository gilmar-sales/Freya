#pragma once

#include "Core/Renderer.hpp"
#include "Core/Window.hpp"

namespace FREYA_NAMESPACE
{
    class AbstractApplication
    {
      public:
        virtual ~AbstractApplication() = default;
        virtual void Run()             = 0;

      protected:
        friend class ApplicationBuilder;

        float                     mDeltaTime;
        std::shared_ptr<Window>   mWindow;
        std::shared_ptr<Renderer> mRenderer;
    };
} // namespace FREYA_NAMESPACE
