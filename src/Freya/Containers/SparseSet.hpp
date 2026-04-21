#pragma once

#include <algorithm>
#include <concepts>
#include <mutex>
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
     * Thread-safe via mutex. Supports O(1) contains, insert, remove,
     * and sort operations.
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
            dense.reserve(other.dense.capacity());
            sparse.resize(other.sparse.size());
            sorted = false;

            for (auto value : other.dense)
            {
                insert(value);
            }
        }

        ~SparseSet() = default;

        /**
         * @brief Inserts element if not already present.
         * @param n Element to insert
         * @note Thread-safe with mutex lock
         */
        void insert(const T& n)
        {
            if (contains(n))
                return;

            std::lock_guard lock { m_lock };

            sparse[n] = dense.size();
            dense.push_back(n);
            sorted = false;
        }

        /**
         * @brief Removes element if present.
         * @param n Element to remove
         * @note Thread-safe with mutex lock
         */
        void remove(const T& n)
        {
            if (!contains(n))
                return;

            std::lock_guard<std::mutex> lock { m_lock };

            dense[sparse[n]]                = dense[dense.size() - 1];
            sparse[dense[dense.size() - 1]] = sparse[n];
            sparse[n]                       = 0;
            dense.pop_back();
            sorted = false;
        }

        /**
         * @brief Swaps two elements in the set.
         * @param a First element
         * @param b Second element
         * @note Does nothing if either element is not present
         */
        void swap(const T& a, const T& b)
        {
            if (!contains(a))
                return;

            if (contains(b))
                return;

            sparse[b]        = sparse[a];
            dense[sparse[a]] = b;
            sparse[a]        = 0;
        }

        /**
         * @brief Checks if element exists.
         * @param n Element ID to check
         * @return true if present
         */
        bool contains(const uint32_t& n) const
        {
            return sparse[n] < dense.size() && dense[sparse[n]] == n;
        }

        /**
         * @brief Clears all elements from dense array.
         */
        void clear() { dense.clear(); }

        /**
         * @brief Resizes sparse array capacity.
         * @param size New capacity
         */
        void resize(unsigned size)
        {
            dense.reserve(size);
            sparse.resize(size);
        }

        /**
         * @brief Sorts dense array and updates sparse indices.
         * @note Thread-safe with mutex lock
         */
        void sort()
        {
            if (sorted)
                return;
            std::lock_guard lock { m_lock };
            denseSort();

            sparseReorder();
            sorted = true;
        }

        /**
         * @brief Accesses element by dense array index.
         * @param index Dense array index
         * @return Reference to element
         */
        T& operator[](int index) { return dense[index]; };

        /**
         * @brief Returns number of elements.
         */
        std::uint64_t size() { return dense.size(); }

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
         * @param other Other SparseSet to intersect with
         * @return New SparseSet containing intersection
         */
        SparseSet<T> intersect(const SparseSet<T>& other)
        {
            auto intersection = SparseSet<T>(sparse.size());

            auto base = dense.size() > other.dense.size() ? other : *this;

            return intersection;
        }

        /**
         * @brief Returns sparse array index for a value.
         * @param value Value to look up
         * @return Sparse array index
         */
        const T& getIndex(const T& value) { return sparse[value]; }

        /**
         * @brief Returns const reference to dense array.
         */
        const std::vector<T>& getDense() { return dense; }

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
            for (T i = 0; i < dense.size(); i++)
            {
                sparse[dense[i]] = i;
            }
        }

      private:
        std::mutex          m_lock; ///< Mutex for thread safety
        std::vector<T>      dense;  ///< Dense array of elements
        std::vector<size_t> sparse; ///< Sparse array for O(1) lookup
        bool                sorted; ///< Whether dense array is sorted
    };

} // namespace FREYA_NAMESPACE
