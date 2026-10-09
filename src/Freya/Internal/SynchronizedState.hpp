#pragma once

#include "Freya/Config.hpp"

#include <algorithm>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace FREYA_NAMESPACE
{
    // Shared by all queues of a device, so aliased handles cannot bypass it.
    class HostSynchronization
    {
      public:
        template <typename F>
        decltype(auto) WithLock(F&& callback) const
        {
            std::lock_guard lock(mMutex);
            return std::invoke(std::forward<F>(callback));
        }

      private:
        mutable std::mutex mMutex;
    };

    template <typename T>
    class SynchronizedState
    {
      public:
        SynchronizedState() = default;
        explicit SynchronizedState(T value) : mValue(std::move(value)) {}

        // Callbacks must not return references or re-enter this state.
        template <typename F>
        decltype(auto) WithLock(F&& callback) const
        {
            return mAccess.WithLock([&]() -> decltype(auto) {
                return std::invoke(std::forward<F>(callback), mValue);
            });
        }

        [[nodiscard]] T Snapshot() const
        {
            return WithLock([](const T& value) { return value; });
        }

      private:
        HostSynchronization mAccess;
        mutable T           mValue {};
    };

    // A visitor pins membership until it returns. Remove is a CPU lifetime
    // barrier: the caller may destroy a removed object after Remove returns.
    // Callbacks must not Add/Remove from this same registry.
    template <typename T>
    class SynchronizedRegistry
    {
      public:
        void Add(T value)
        {
            mValues.WithLock([&](auto& values) {
                if (std::find(values.begin(), values.end(), value) ==
                    values.end())
                    values.push_back(value);
            });
        }

        void Remove(T value)
        {
            mValues.WithLock([&](auto& values) { std::erase(values, value); });
        }

        template <typename F>
        void ForEach(F&& callback) const
        {
            mValues.WithLock([&](const auto& values) {
                for (const auto& value : values)
                    std::invoke(callback, value);
            });
        }

      private:
        SynchronizedState<std::vector<T>> mValues;
    };
} // namespace FREYA_NAMESPACE
