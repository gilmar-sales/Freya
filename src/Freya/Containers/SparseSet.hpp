#pragma once

#include "Freya/Config.hpp"
#include "Freya/Core/SpinLock.hpp"

#include <algorithm>
#include <concepts>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Concept satisfied by types that can convert to size_t.
     */
    template <typename T>
    concept has_size_t_cast = requires(T value) {
        { value } -> std::convertible_to<std::size_t>;
    };

    /**
     * @brief Thread-safe sparse set container for ID-based storage.
     *
     * Implements a sparse set data structure with dense/sparse arrays.
     * Thread-safe via SpinLock. Supports O(1) contains, insert, remove,
     * and sort operations.
     *
     * IDs are stable: use contains()/atId()/find() by id. operator[]
     * indexes the dense array by position and must NOT be used for id
     * lookup.
     *
     * @tparam T Type convertible to size_t (provides id())
     */
    template <typename T>
        requires(has_size_t_cast<T>)
    class SparseSet
    {
      public:
        /**
         * @brief Constructs with optional capacity reservation.
         * @param capacity Initial capacity for sparse array
         */
        explicit SparseSet(unsigned capacity = 512u)
        {
            dense.reserve(capacity);
            sparse.resize(capacity);
            sorted = false;
        }

        /**
         * @brief Copy constructor.
         * @param other SparseSet to copy
         */
        SparseSet(const SparseSet& other)
        {
            SpinLockGuard lock { other.m_lock };
            dense  = other.dense;
            sparse = other.sparse;
            sorted = false;
        }

        SparseSet& operator=(const SparseSet& other)
        {
            if (this == &other)
                return *this;
            SpinLockGuard selfLock { m_lock };
            SpinLockGuard otherLock { other.m_lock };
            dense  = other.dense;
            sparse = other.sparse;
            sorted = false;
            return *this;
        }

        ~SparseSet() = default;

        /**
         * @brief Inserts element if not already present.
         * @param n Element to insert
         * @note Thread-safe with SpinLock; grows sparse array as needed.
         */
        void insert(const T& n)
        {
            const auto    id = static_cast<std::size_t>(n);
            SpinLockGuard lock { m_lock };
            ensureCapacityLocked(id);
            if (containsLocked(static_cast<std::uint32_t>(id)))
                return;

            sparse[id] = dense.size();
            dense.push_back(n);
            sorted = false;
        }

        /**
         * @brief Removes element if present.
         * @param n Element to remove
         * @note Thread-safe with SpinLock
         */
        void remove(const T& n)
        {
            const auto    id = static_cast<std::size_t>(n);
            SpinLockGuard lock { m_lock };
            if (!containsLocked(static_cast<std::uint32_t>(id)))
                return;

            dense[sparse[id]] = dense[dense.size() - 1];
            sparse[static_cast<std::size_t>(dense[dense.size() - 1])] =
                sparse[id];
            sparse[id] = 0;
            dense.pop_back();
            sorted = false;
        }

        /**
         * @brief Swaps positions of two present elements.
         * @note Does nothing if either element is not present.
         */
        void swap(const T& a, const T& b)
        {
            const auto    ida = static_cast<std::size_t>(a);
            const auto    idb = static_cast<std::size_t>(b);
            SpinLockGuard lock { m_lock };
            if (!containsLocked(static_cast<std::uint32_t>(ida)) ||
                !containsLocked(static_cast<std::uint32_t>(idb)))
                return;
            if (ida == idb)
                return;

            const auto posA = sparse[ida];
            const auto posB = sparse[idb];
            std::swap(dense[posA], dense[posB]);
            sparse[ida] = posB;
            sparse[idb] = posA;
            sorted      = false;
        }

        /**
         * @brief Renames a present id to an absent id, preserving position.
         * @note Does nothing unless a is present and b is absent.
         */
        void rename(const T& a, const T& b)
        {
            const auto    ida = static_cast<std::size_t>(a);
            const auto    idb = static_cast<std::size_t>(b);
            SpinLockGuard lock { m_lock };
            if (!containsLocked(static_cast<std::uint32_t>(ida)))
                return;
            ensureCapacityLocked(idb);
            if (containsLocked(static_cast<std::uint32_t>(idb)))
                return;

            sparse[idb]        = sparse[ida];
            dense[sparse[ida]] = b;
            sparse[ida]        = 0;
            sorted             = false;
        }

        [[nodiscard]] bool contains(const uint32_t& n) const
        {
            SpinLockGuard lock { m_lock };
            return containsLocked(n);
        }

        /**
         * @brief Stable lookup by id (not dense position).
         */
        T& atId(const uint32_t n)
        {
            SpinLockGuard lock { m_lock };
            return dense[sparse[n]];
        }

        const T& atId(const uint32_t n) const
        {
            SpinLockGuard lock { m_lock };
            return dense[sparse[n]];
        }

        /**
         * @brief Non-throwing stable lookup by id.
         * @return Pointer to element or nullptr when absent/out of range.
         */
        T* find(const uint32_t n)
        {
            SpinLockGuard lock { m_lock };
            if (!containsLocked(n))
                return nullptr;
            return &dense[sparse[n]];
        }

        const T* find(const uint32_t n) const
        {
            SpinLockGuard lock { m_lock };
            if (!containsLocked(n))
                return nullptr;
            return &dense[sparse[n]];
        }

        /**
         * @brief Clears all elements.
         */
        void clear()
        {
            SpinLockGuard lock { m_lock };
            dense.clear();
            std::fill(sparse.begin(), sparse.end(), 0);
            sorted = false;
        }

        /**
         * @brief Resizes sparse array capacity.
         * @param size New capacity
         */
        void resize(unsigned size)
        {
            SpinLockGuard lock { m_lock };
            dense.reserve(size);
            if (sparse.size() < size)
                sparse.resize(size);
        }

        /**
         * @brief Sorts dense array and updates sparse indices.
         * @note Thread-safe with SpinLock
         */
        void sort()
        {
            SpinLockGuard lock { m_lock };
            if (sorted)
                return;
            denseSort();

            sparseReorder();
            sorted = true;
        }

        /**
         * @brief Accesses element by dense array position.
         * @param index Dense array position (NOT a stable id)
         */
        T&       operator[](std::size_t index) { return dense[index]; }
        const T& operator[](std::size_t index) const { return dense[index]; }

        /**
         * @brief Returns number of live elements.
         */
        [[nodiscard]] std::uint64_t size() const
        {
            SpinLockGuard lock { m_lock };
            return dense.size();
        }

        /**
         * @brief Returns reverse iterator to beginning.
         */
        auto begin() const { return dense.rbegin(); }

        /**
         * @brief Returns reverse iterator to end.
         */
        auto end() const { return dense.rend(); }

        /**
         * @brief Computes intersection with another SparseSet.
         */
        SparseSet<T> intersect(const SparseSet<T>& other)
        {
            SpinLockGuard lock { m_lock };
            SparseSet<T>  intersection(static_cast<unsigned>(
                std::max(sparse.size(), other.sparse.size())));
            const auto&   smaller =
                (dense.size() <= other.dense.size()) ? dense : other.dense;
            for (const auto& value : smaller)
            {
                const auto id =
                    static_cast<std::uint32_t>(static_cast<std::size_t>(value));
                if (containsLocked(id) && other.contains(id))
                    intersection.insert(value);
            }
            return intersection;
        }

        /**
         * @brief Returns sparse array index for a value.
         */
        std::size_t getIndex(const T& value) const
        {
            SpinLockGuard lock { m_lock };
            return sparse[static_cast<std::size_t>(value)];
        }

        /**
         * @brief Returns const reference to dense array.
         */
        const std::vector<T>& getDense() const { return dense; }

      protected:
        /**
         * @brief Sorts the dense array in ascending order.
         */
        void denseSort() { std::sort(dense.begin(), dense.end()); }

        /**
         * @brief Reorders sparse array to match sorted dense array.
         */
        void sparseReorder()
        {
            for (std::size_t i = 0; i < dense.size(); i++)
            {
                sparse[static_cast<std::size_t>(dense[i])] = i;
            }
        }

        [[nodiscard]] bool containsLocked(const uint32_t n) const
        {
            const auto id = static_cast<std::size_t>(n);
            if (id >= sparse.size())
                return false;
            const auto pos = sparse[id];
            return pos < dense.size() &&
                   static_cast<std::size_t>(dense[pos]) == id;
        }

        void ensureCapacityLocked(const std::size_t id)
        {
            if (id < sparse.size())
                return;
            auto grown = sparse.size() == 0 ? 512u : sparse.size();
            while (grown <= id)
                grown *= 2;
            sparse.resize(grown);
            dense.reserve(grown);
        }

      private:
        mutable SpinLock    m_lock; ///< SpinLock for thread safety
        std::vector<T>      dense;  ///< Dense array of elements
        std::vector<size_t> sparse; ///< Sparse array for O(1) lookup
        bool                sorted; ///< Whether dense array is sorted
    };

} // namespace FREYA_NAMESPACE
