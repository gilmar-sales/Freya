#include "Freya/Scene/Camera.hpp"

#include "Freya/Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{
    void Camera::Apply(Renderer& renderer) const
    {
        if (useFov)
        {
            renderer.UpdateCamera(position, target, up, fovRadians, nearPlane,
                                  farPlane);
        }
        else
        {
            renderer.UpdateCamera(position, target, up);
        }
    }

} // namespace FREYA_NAMESPACE
