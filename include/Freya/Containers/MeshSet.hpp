#pragma once

#include <algorithm>
#include <iterator>
#include <mutex>
#include <vector>

#include "Freya/Asset/Mesh.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Specialized SparseSet for Mesh objects.
     *
     * Mesh-specific sparse set with Mesh::id comparison instead of
     * implicit size_t conversion. Same thread-safe behavior as SparseSet.
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

        /**
         * @brief Inserts mesh if not already present.
         * @param n Mesh to insert
         */
        void insert(Mesh n)
        {
            if (contains(n))
                return;
            std::lock_guard<std::mutex> lock { m_lock };

            sparse[n] = dense.size();
            dense.push_back(n);
            sorted = false;
        }

        /**
         * @brief Removes mesh if present.
         * @param n Mesh to remove
         */
        void remove(Mesh n)
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
         * @brief Checks if mesh ID exists.
         * @param n Mesh ID to check
         * @return true if present
         */
        [[nodiscard]] bool contains(const size_t n) const
        {
            return sparse[n] < dense.size() && dense[sparse[n]].id == n;
        }

        /**
         * @brief Clears all meshes.
         */
        void clear() { dense.clear(); }

        /**
         * @brief Resizes sparse array capacity.
         * @param size New capacity
         */
        void resize(const unsigned size)
        {
            dense.reserve(size);
            sparse.resize(size);
        }

        /**
         * @brief Sorts dense array and reorders sparse indices.
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
         * @brief Accesses mesh by dense array index.
         * @param index Dense array index
         * @return Reference to mesh
         */
        Mesh& operator[](const size_t index) { return dense[index]; };

        /**
         * @brief Returns number of meshes.
         */
        [[nodiscard]] size_t size() const { return dense.size(); }

        /**
         * @brief Returns reverse iterator to beginning.
         */
        [[nodiscard]] auto begin() const { return dense.rbegin(); }

        /**
         * @brief Returns reverse iterator to end.
         */
        [[nodiscard]] auto end() const { return dense.rend(); }

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
            for (auto i = 0; i < dense.size(); i++)
            {
                sparse[dense[i]] = i;
            }
        }

      private:
        std::mutex          m_lock; ///< Mutex for thread safety
        std::vector<Mesh>   dense;  ///< Dense array of meshes
        std::vector<size_t> sparse; ///< Sparse array for O(1) lookup
        bool                sorted; ///< Whether dense array is sorted
    };
} // namespace FREYA_NAMESPACE
