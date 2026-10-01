#include "Freya/Core/Image.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys image view and VMA image allocation.
     */
    Image::~Image()
    {
        mDevice->Get().waitIdle();
        mDevice->Get().destroyImageView(mImageView);
        mDevice->GetAllocator()->DestroyImage(
            static_cast<VkImage>(mImage), mAllocation);
        mAllocation = {};
        mImage      = vk::Image {};
    }
} // namespace FREYA_NAMESPACE
