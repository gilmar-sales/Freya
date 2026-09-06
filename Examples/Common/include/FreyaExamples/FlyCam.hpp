#pragma once

#include <Freya/Freya.hpp>

#include <cstdint>
#include <unordered_set>

namespace FreyaExamples
{
    /**
     * @brief Shared freecam for Freya examples (RMB look, WASD move).
     *
     * Defaults match IndustrialPipeLamp / CellBulbasaur / SsaoDebug
     * (flattened walk, vertical Space/Q and Ctrl/E). SkinnedFox uses
     * flattenForward=false, requireLookToMove=true, enableVerticalMove=false.
     */
    struct FlyCam
    {
        skr::Arc<fra::Window>             window;
        std::unordered_set<std::uint32_t> keysHeld;
        bool                              lookHeld  = false;
        glm::vec3                         cameraPos = { 0.0f, 4.0f, 18.0f };
        float                             yaw       = -90.0f;
        float                             pitch     = -12.0f;

        float moveSpeed          = 12.0f;
        float mouseSensitivity   = 0.12f;
        bool  flattenForward     = true;
        bool  requireLookToMove  = false;
        bool  enableVerticalMove = true;

        void BindInput(fra::EventManager& events);
        void Update(float dt);

        [[nodiscard]] bool      IsHeld(fra::KeyCode key) const;
        [[nodiscard]] glm::vec3 Forward() const;

        void Apply(fra::Renderer& renderer) const;

      private:
        void setLookHeld(bool held);
    };
} // namespace FreyaExamples
