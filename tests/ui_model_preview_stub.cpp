#include <Freya/Core/UiModelPreview.hpp>

// FreyaTests does not link the full UiModelPreview GPU stack. Stub only the
// symbols referenced by UiContext::ModelPreview so UI unit tests stay
// device-free.

namespace FREYA_NAMESPACE
{
    UiModelPreviewOrbit& UiModelPreview::Orbit()
    {
        static UiModelPreviewOrbit sOrbit;
        return sOrbit;
    }

    void UiModelPreview::FeedMouseMove(float, float) {}

    void UiModelPreview::FeedMouseButton(MouseButton, bool) {}

    void UiModelPreview::SetFrameDelta(float) {}

    void UiModelPreview::SetActive(bool) {}

    bool UiModelPreview::IsActive() const { return false; }
} // namespace FREYA_NAMESPACE
