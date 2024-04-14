#pragma once

#include <algorithm>
#include <iterator>
#include <mutex>
#include <vector>

#include "Asset/Mesh.hpp"

namespace FREYA_NAMESPACE
{
    class MeshSet
    {
      public:
        MeshSet(unsigned capacity = 512u)
        {
            dense.reserve(capacity);
            sparse.resize(capacity);
            sorted = false;
        }

        ~MeshSet() = default;

        void insert(Mesh n)
        {
            if (contains(n)) return;
            std::lock_guard<std::mutex> lock{m_lock};

            sparse[n] = dense.size();
            dense.push_back(n);
            sorted = false;
        }

        void remove(Mesh n)
        {
            if (!contains(n)) return;
            std::lock_guard<std::mutex> lock{m_lock};

            dense[sparse[n]]                = dense[dense.size() - 1];
            sparse[dense[dense.size() - 1]] = sparse[n];
            sparse[n]                       = 0;
            dense.pop_back();
            sorted = false;
        }

        bool contains(std::uint32_t n) const
        {
            return sparse[n] < dense.size() && dense[sparse[n]].id == n;
        }

        void clear() { dense.clear(); }

        void resize(unsigned size)
        {
            dense.reserve(size);
            sparse.resize(size);
        }

        void sort()
        {
            if (sorted) return;
            std::lock_guard<std::mutex> lock{m_lock};
            denseSort();

            sparseReorder();
            sorted = true;
        }

        Mesh &operator[](int index) { return dense[index]; };

        size_t size() { return dense.size(); }

        auto begin() const { return dense.rbegin(); }

        auto end() const { return dense.rend(); }

      protected:
        void denseSort() { std::sort(dense.begin(), dense.end()); }

        void sparseReorder()
        {
            for (auto i = 0; i < dense.size(); i++)
            {
                sparse[dense[i]] = i;
            }
        }

      private:
        std::mutex m_lock;
        std::vector<Mesh> dense;
        std::vector<std::uint32_t> sparse;
        bool sorted;
    };
} // namespace FREYA_NAMESPACE
