#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Fluent builder for creating Buffer objects.
     *
     * Configures size, data, and usage type before building.
     * Handles memory allocation based on buffer type.
     *
     * @param device Device reference
     */
    class BufferBuilder
    {
      public:
        /**
         * @brief Constructs builder with device reference.
         * @param device Device reference
         */
        explicit BufferBuilder(const Ref<Device>& device) : mDevice(device) {}

        /**
         * @brief Sets buffer size in bytes.
         * @param size Buffer size
         * @return Reference to this for chaining
         */
        BufferBuilder& SetSize(const std::uint64_t size)
        {
            mSize = size;

            return *this;
        }

        /**
         * @brief Sets initial data to upload to buffer.
         * @tparam T Data type
         * @param data Pointer to data
         * @return Reference to this for chaining
         */
        template <typename T>
        BufferBuilder& SetData(T* data)
        {
            mData = static_cast<void*>(data);

            return *this;
        }

        /**
         * @brief Sets buffer usage type.
         * @param usage Buffer usage type
         * @return Reference to this for chaining
         */
        BufferBuilder& SetUsage(const BufferUsage usage)
        {
            mUsage = usage;
            return *this;
        }

        /**
         * @brief Builds and returns the Buffer object.
         * @return Shared pointer to created Buffer
         */
        Ref<Buffer> Build();

      private:
        Ref<Device> mDevice; ///< Device reference

        std::uint64_t mSize  = 0;                    ///< Buffer size in bytes
        void*         mData  = nullptr;              ///< Initial data to upload
        BufferUsage   mUsage = BufferUsage::Staging; ///< Buffer usage type
    };

} // namespace FREYA_NAMESPACE
