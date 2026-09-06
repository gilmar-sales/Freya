#pragma once

#include "Freya/Core/IPlatform.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <unordered_map>

struct SDL_Window;

namespace FREYA_NAMESPACE
{
    /**
     * @brief SDL3 implementation of IPlatform.
     *
     * Owns SDL_Init / SDL_Quit and the Vulkan loader for the process.
     */
    class SdlPlatform final : public IPlatform
    {
      public:
        explicit SdlPlatform(const skr::Arc<skr::Logger<SdlPlatform>>& logger);
        ~SdlPlatform() override;

        SdlPlatform(const SdlPlatform&)            = delete;
        SdlPlatform& operator=(const SdlPlatform&) = delete;

        void* CreateNativeWindow(const NativeWindowDesc& desc,
                                 FreyaOptions&           options) override;

        void DestroyNativeWindow(void* nativeWindow) override;

        void AttachWindow(void* nativeWindow, void* windowToken) override;

        void DetachWindow(void* nativeWindow) override;

        void PumpEvents() override;

        [[nodiscard]] float GetDisplayContentScale(
            void* nativeWindow) const override;

      private:
        skr::Arc<skr::Logger<SdlPlatform>>       mLogger;
        std::unordered_map<std::uint32_t, void*> mWindowsById;
    };

} // namespace FREYA_NAMESPACE
