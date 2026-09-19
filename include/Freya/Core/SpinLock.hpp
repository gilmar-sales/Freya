#pragma once

#include "Freya/Config.hpp"

#include <atomic>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Exclusive spinlock for short critical sections (hotpaths).
     *
     * Satisfies BasicLockable / Lockable for use with SpinLockGuard.
     */
    class SpinLock
    {
      public:
        SpinLock() = default;

        SpinLock(const SpinLock&)            = delete;
        SpinLock& operator=(const SpinLock&) = delete;

        void lock()
        {
            while (mFlag.test_and_set(std::memory_order_acquire))
            {
                // Busy-wait; intended for brief host staging / container ops.
            }
        }

        [[nodiscard]] bool try_lock()
        {
            return !mFlag.test_and_set(std::memory_order_acquire);
        }

        void unlock() { mFlag.clear(std::memory_order_release); }

      private:
        std::atomic_flag mFlag = ATOMIC_FLAG_INIT;
    };

    /**
     * @brief RAII guard for SpinLock (lock_guard-style).
     */
    class SpinLockGuard
    {
      public:
        explicit SpinLockGuard(SpinLock& lock) : mLock(lock) { mLock.lock(); }

        ~SpinLockGuard() { mLock.unlock(); }

        SpinLockGuard(const SpinLockGuard&)            = delete;
        SpinLockGuard& operator=(const SpinLockGuard&) = delete;

      private:
        SpinLock& mLock;
    };

} // namespace FREYA_NAMESPACE
