#include <FreyaExamples/FlyCam.hpp>

#include <algorithm>
#include <cmath>

namespace FreyaExamples
{
    void FlyCam::setLookHeld(const bool held)
    {
        lookHeld = held;
        if (window)
            window->SetMouseGrab(held);
    }

    bool FlyCam::IsHeld(const fra::KeyCode key) const
    {
        return keysHeld.contains(static_cast<std::uint32_t>(key));
    }

    glm::vec3 FlyCam::Forward() const
    {
        const float yawRad   = glm::radians(yaw);
        const float pitchRad = glm::radians(pitch);
        return glm::normalize(glm::vec3 {
            std::cos(pitchRad) * std::cos(yawRad),
            std::sin(pitchRad),
            std::cos(pitchRad) * std::sin(yawRad),
        });
    }

    void FlyCam::Update(const float dt)
    {
        if (blockKeyboard && blockKeyboard())
            return;
        if (requireLookToMove && !lookHeld)
            return;

        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 look = Forward();
        const glm::vec3 forward =
            flattenForward ? glm::normalize(glm::vec3(look.x, 0.0f, look.z))
                           : look;
        const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3       move(0.0f);

        if (IsHeld(fra::KeyCode::W))
            move += forward;
        if (IsHeld(fra::KeyCode::S))
            move -= forward;
        if (IsHeld(fra::KeyCode::D))
            move += right;
        if (IsHeld(fra::KeyCode::A))
            move -= right;

        if (enableVerticalMove)
        {
            if (IsHeld(fra::KeyCode::Space) || IsHeld(fra::KeyCode::Q))
                move += worldUp;
            if (IsHeld(fra::KeyCode::LCtrl) || IsHeld(fra::KeyCode::RCtrl) ||
                IsHeld(fra::KeyCode::E))
                move -= worldUp;
        }

        if (glm::length(move) > 1e-4f)
            cameraPos += glm::normalize(move) * moveSpeed * dt;
    }

    void FlyCam::BindInput(fra::EventManager& events)
    {
        events.Subscribe<fra::KeyPressedEvent>(
            [this](const fra::KeyPressedEvent& event) {
                keysHeld.insert(static_cast<std::uint32_t>(event.key));
            });

        events.Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                keysHeld.erase(static_cast<std::uint32_t>(event.key));
                if (event.key == fra::KeyCode::Escape && lookHeld)
                    setLookHeld(false);
            });

        events.Subscribe<fra::MouseButtonPressedEvent>(
            [this](const fra::MouseButtonPressedEvent& event) {
                if (blockMouse && blockMouse())
                    return;
                if (event.button == fra::MouseButton::Right)
                    setLookHeld(true);
            });

        events.Subscribe<fra::MouseButtonReleasedEvent>(
            [this](const fra::MouseButtonReleasedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                    setLookHeld(false);
            });

        events.Subscribe<fra::MouseMoveEvent>(
            [this](const fra::MouseMoveEvent& event) {
                if (blockMouse && blockMouse())
                    return;
                if (!lookHeld)
                    return;
                yaw += event.deltaX * mouseSensitivity;
                pitch -= event.deltaY * mouseSensitivity;
                pitch = std::clamp(pitch, -89.0f, 89.0f);
            });
    }

    void FlyCam::Apply(fra::Renderer& renderer) const
    {
        const glm::vec3 forward = Forward();
        renderer.UpdateCamera(cameraPos, cameraPos + forward,
                              glm::vec3(0.0f, 1.0f, 0.0f));
    }
} // namespace FreyaExamples
