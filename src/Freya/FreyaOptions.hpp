#pragma once

namespace FREYA_NAMESPACE
{
    struct FreyaOptions
    {
        std::string         title        = "Freya Window";
        std::uint32_t       width        = 800;
        std::uint32_t       height       = 600;
        bool                vSync        = true;
        std::uint32_t       sampleCount  = 1;
        std::uint32_t       frameCount   = 4;
        vk::ClearColorValue clearColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
        float               drawDistance = 1000.0f;
    };

} // namespace FREYA_NAMESPACE
