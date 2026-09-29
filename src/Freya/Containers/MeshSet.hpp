#pragma once

#include "Freya/Asset/Mesh.hpp"
#include "Freya/Core/SpinLock.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Specialized SparseSet for Mesh objects.
     *
     * Mesh-specific sparse set with Mesh::id comparison instead of
     * implicit size_t conversion. Same thread-safe behavior as SparseSet.
     * IDs are stable: use contains()/atId()/find() by id. operator[]
     * indexes the dense array by position.
     *
     * @param capacity Initial capacity (default 512)
     */
    class MeshSet
    {
      public:
        /**
         * @brief Constructs with optional capacity reservation.
         * @param capacity Initial capacity (default 512)
         */
        explicit MeshSet(const unsigned capacity = 512u)
        {
            dense.reserve(capacity);
            sparse.resize(capacity);
            sorted = false;
        }

        ~MeshSet() = default;

        MeshSet(const MeshSet& other)
        {
            SpinLockGuard lock { other.m_lock };
            dense  = other.dense;
            sparse = other.sparse;
            sorted = false;
        }

        MeshSet& operator=(const MeshSet& other)
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

        /**
         * @brief Inserts mesh if not already present.
         * @param n Mesh to insert
         */
        void insert(Mesh n)
        {
            const auto id = static_cast<std::size_t>(n.id);
            SpinLockGuard lock { m_lock };
            ensureCapacityLocked(id);
            if (containsLocked(id))
                return;

            sparse[id] = dense.size();
            dense.push_back(n);
            sorted = false;
        }

        /**
         * @brief Removes mesh if present.
         * @param n Mesh to remove
         */
        void remove(Mesh n)
        {
            const auto id = static_cast<std::size_t>(n.id);
            SpinLockGuard lock { m_lock };
            if (!containsLocked(id))
                return;

            dense[sparse[id]]                = dense[dense.size() - 1];
            sparse[dense[dense.size() - 1]] = sparse[id];
            sparse[id]                       = 0;
            dense.pop_back();
            sorted = false;
        }

        /**
         * @brief Checks if mesh ID exists.
         * @return true if present
         */
        [[nodiscard]] bool contains(const size_t n) const
        {
            SpinLockGuard lock { m_lock };
            return containsLocked(n);
        }

        /**
         * @brief Stable lookup by mesh id (not dense position).
         */
        Mesh& atId(const size_t id)
        {
            SpinLockGuard lock { m_lock };
            return dense[sparse[id]];
        }

        const Mesh& atId(const size_t id) const
        {
            SpinLockGuard lock { m_lock };
            return dense[sparse[id]];
        }

        Mesh* find(const size_t id)
        {
            SpinLockGuard lock { m_lock };
            if (!containsLocked(id))
                return nullptr;
            return &dense[sparse[id]];
        }

        const Mesh* find(const size_t id) const
        {
            SpinLockGuard lock { m_lock };
            if (!containsLocked(id))
                return nullptr;
            return &dense[sparse[id]];
        }

        /**
         * @brief Clears all meshes.
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
        void resize(const unsigned size)
        {
            SpinLockGuard lock { m_lock };
            dense.reserve(size);
            if (sparse.size() < size)
                sparse.resize(size);
        }

        /**
         * @brief Sorts dense array and reorders sparse indices.
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
         * @brief Accesses mesh by dense array position (NOT a stable id).
         */
        Mesh& operator[](const size_t index) { return dense[index]; };

        const Mesh& operator[](const size_t index) const
        {
            return dense[index];
        };

        /**
         * @brief Returns number of meshes.
         */
        [[nodiscard]] size_t size() const
        {
            SpinLockGuard lock { m_lock };
            return dense.size();
        }

        /**
         * @brief Returns reverse iterator to beginning.
         */
        [[nodiscard]] auto begin() const { return dense.rbegin(); }

        /**
         * @brief Returns reverse iterator to end.
         */
        [[nodiscard]] auto end() const { return dense.rend(); }

        /**
         * @brief Read-only access to dense storage for iteration.
         */
        [[nodiscard]] const std::vector<Mesh>& getDense() const
        {
            return dense;
        }

      protected:
        /**
         * @brief Sorts dense array by mesh ID.
         */
        void denseSort() { std::sort(dense.begin(), dense.end()); }

        /**
         * @brief Reorders sparse array to match sorted dense array.
         */
        void sparseReorder()
        {
            for (std::size_t i = 0; i < dense.size(); i++)
            {
                sparse[dense[i]] = i;
            }
        }

        [[nodiscard]] bool containsLocked(const size_t n) const
        {
            if (n >= sparse.size())
                return false;
            const auto pos = sparse[n];
            return pos < dense.size() && dense[pos].id == n;
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
        std::vector<Mesh>   dense;  ///< Dense array of meshes
        std::vector<size_t> sparse; ///< Sparse array for O(1) lookup
        bool                sorted; ///< Whether dense array is sorted
    };
} // namespace FREYA_NAMESPACE
