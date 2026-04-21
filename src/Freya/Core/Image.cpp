#include "Freya/Core/Image.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys image view, image, and frees device memory.
     */
    Image::~Image()
    {
        mDevice->Get().destroyImageView(mImageView);
        mDevice->Get().destroyImage(mImage);
        mDevice->Get().freeMemory(mMemory);
    }
} // namespace FREYA_NAMESPACE
