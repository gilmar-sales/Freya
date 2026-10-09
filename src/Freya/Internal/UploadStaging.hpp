#pragma once

#include "Freya/Config.hpp"

#include <mutex>
#include <span>
#include <stdexcept>
#include <vector>

namespace FREYA_NAMESPACE
{
    // Begin/Reserve on the coordinator, Upload on workers, join, then End.
    // End's view stays valid until the next window. Count is published only
    // after the complete chunk, under the same lock as vector growth.
    template <typename T>
    class UploadStaging
    {
      public:
        void Begin()
        {
            std::lock_guard lock(mMutex);
            if (mOpen)
                throw std::logic_error("Upload window already open");
            mData.clear();
            mOpen = true;
        }

        void Reserve(std::size_t count)
        {
            std::lock_guard lock(mMutex);
            mData.reserve(count);
        }

        void Upload(std::span<const T> values)
        {
            std::lock_guard lock(mMutex);
            if (!mOpen)
                throw std::logic_error("Upload outside staging window");
            mData.insert(mData.end(), values.begin(), values.end());
        }

        [[nodiscard]] std::span<const T> End()
        {
            std::lock_guard lock(mMutex);
            if (!mOpen)
                throw std::logic_error("Upload window not open");
            mOpen = false;
            return mData;
        }

      private:
        std::mutex     mMutex;
        std::vector<T> mData;
        bool           mOpen = false;
    };
} // namespace FREYA_NAMESPACE
