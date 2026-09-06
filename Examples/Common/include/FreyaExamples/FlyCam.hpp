#pragma once

#include <Freya/Freya.hpp>

#include <cstdint>
#include <functional>
#include <unordered_set>

namespace FreyaExamples
{
    /**
     * @brief Shared freecam for Freya examples (RMB look, WASD move).
     *
     * WASD always moves (unless requireLookToMove and RMB is up). ImGui should
     * only gate mouse look via blockMouse — not keyboard — so the debug panel
     * does not steal freecam movement.
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

        /**
         * @brief When set and returns true, mouse look / grab are skipped
         * (e.g. ImGui WantCaptureMouse). Does not affect WASD.
         */
        std::function<bool()> blockMouse;

      private:
        void setLookHeld(bool held);
    };
} // namespace FreyaExamples
