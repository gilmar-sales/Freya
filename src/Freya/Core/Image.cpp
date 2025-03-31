#include "Freya/Core/Image.hpp"

namespace FREYA_NAMESPACE
{
    Image::~Image()
    {
        mDevice->Get().destroyImageView(mImageView);
        mDevice->Get().destroyImage(mImage);
        mDevice->Get().freeMemory(mMemory);
    }
} // namespace FREYA_NAMESPACE
