#include "Skirnir/ApplicationBuilder.hpp"

#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/LODBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/LODPool.hpp"
#include "Freya/Asset/LODService.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Extension interface for Freya engine service registration.
     *
     * Implements skr::IExtension to register all Freya services
     * in the service provider. Use WithOptions() to configure
     * FreyaOptions before ConfigureServices() is called.
     */
    class FreyaExtension : public skr::IExtension
    {
      public:
        /**
         * @brief Fluent API to configure FreyaOptions.
         * @param freyaOptionsBuilderFunc Lambda receiving FreyaOptionsBuilder
         * @return Reference to this for chaining
         */
        FreyaExtension& WithOptions(
            const std::function<void(FreyaOptionsBuilder&)>&
                freyaOptionsBuilderFunc)
        {
            freyaOptionsBuilderFunc(mFreyaOptionsBuilder);

            return *this;
        }

      protected:
        void ConfigureServices(skr::ServiceCollection& services) override;

      private:
        FreyaOptionsBuilder
            mFreyaOptionsBuilder; ///< Builder for FreyaOptions configuration
    };
} // namespace FREYA_NAMESPACE
