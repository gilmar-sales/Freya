#pragma once

#include "Core/Window.hpp"
#include "Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{
    class AbstractApplication
    {
      public:
        virtual void Startup() = 0;
        virtual void Update() = 0;

        void Run ();
    
      protected:
        friend class ApplicationBuilder;

        std::shared_ptr<Window> mWindow;
        std::shared_ptr<Renderer> mRenderer;
    };
} // namespace FREYA_NAMESPACE
