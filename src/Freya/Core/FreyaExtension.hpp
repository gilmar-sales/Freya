#include "Skirnir/ApplicationBuilder.hpp"

#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"

namespace FREYA_NAMESPACE
{
    class FreyaExtension : public skr::IExtension
    {
      public:
        void ConfigureServices(skr::ServiceCollection& services) override;

        FreyaExtension& WithOptions(
            std::function<void(FreyaOptionsBuilder&)> freyaOptionsBuilderFunc)
        {
            freyaOptionsBuilderFunc(mFreyaOptionsBuilder);

            return *this;
        }

      private:
        FreyaOptionsBuilder mFreyaOptionsBuilder;
    };
} // namespace FREYA_NAMESPACE
