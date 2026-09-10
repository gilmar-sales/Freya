#include <Freya/Asset/MeshLod.hpp>
#include <Freya/Asset/Vertex.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
    fra::Vertex V(float x, float y, float z, float u, float v)
    {
        return fra::Vertex {
            .position = { x, y, z },
            .color    = { 1.f, 1.f, 1.f },
            .normal   = { 0.f, 1.f, 0.f },
            .tangent  = { 1.f, 0.f, 0.f },
            .texCoord = { u, v },
        };
    }

    /// Grid of quads in the XZ plane (enough indices to pass minSourceIndices).
    void MakeGrid(std::vector<fra::Vertex>&   verts,
                  std::vector<std::uint32_t>& indices, int n)
    {
        verts.clear();
        indices.clear();
        verts.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));
        for (int z = 0; z <= n; ++z)
        {
            for (int x = 0; x <= n; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(n);
                const float v = static_cast<float>(z) / static_cast<float>(n);
                verts.push_back(
                    V(static_cast<float>(x), 0.f, static_cast<float>(z), u, v));
            }
        }
        for (int z = 0; z < n; ++z)
        {
            for (int x = 0; x < n; ++x)
            {
                const auto i0 = static_cast<std::uint32_t>(z * (n + 1) + x);
                const auto i1 = i0 + 1;
                const auto i2 = i0 + static_cast<std::uint32_t>(n + 1);
                const auto i3 = i2 + 1;
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }
    }
} // namespace

TEST_CASE("BuildMeshLodIndexSets keeps LOD0 and disables cleanly", "[mesh-lod]")
{
    std::vector<fra::Vertex>   verts;
    std::vector<std::uint32_t> indices;
    MakeGrid(verts, indices, 16);

    fra::MeshLodBuildOptions off { .enabled = false };
    const auto only = fra::BuildMeshLodIndexSets(verts, indices, off);
    REQUIRE(only.size() == 1);
    REQUIRE(only[0].size() == indices.size());

    fra::MeshLodBuildOptions tiny { .minSourceIndices = 1'000'000 };
    const auto skipped = fra::BuildMeshLodIndexSets(verts, indices, tiny);
    REQUIRE(skipped.size() == 1);
}

TEST_CASE("BuildMeshLodIndexSets decimates with shared vertices", "[mesh-lod]")
{
    std::vector<fra::Vertex>   verts;
    std::vector<std::uint32_t> indices;
    MakeGrid(verts, indices, 32);
    REQUIRE(indices.size() >= 768);

    const auto lods = fra::BuildMeshLodIndexSets(verts, indices, {});
    REQUIRE(lods.size() >= 2);
    REQUIRE(lods.size() <= fra::kMaxLodsPerMesh);
    REQUIRE(lods[0].size() == indices.size());

    for (std::size_t i = 1; i < lods.size(); ++i)
    {
        REQUIRE(lods[i].size() % 3 == 0);
        REQUIRE(lods[i].size() < lods[i - 1].size());
        for (const auto idx : lods[i])
            REQUIRE(idx < verts.size());
    }
}
