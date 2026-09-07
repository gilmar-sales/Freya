#ifndef FREYA_SHADER_ROOT
#define FREYA_SHADER_ROOT "./Resources/Shaders"
#endif
#ifndef FREYA_GPU_FIXTURE_ROOT
#define FREYA_GPU_FIXTURE_ROOT "./tests/fixtures/gpu_cull"
#endif

#include <vulkan/vulkan.hpp>

#include <Freya/Freya.hpp>
#include <FreyaExamples/CullFrameDumpIo.hpp>

#include "Freya/Core/IndirectDrawSystem.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <unordered_set>
#include <vector>

namespace
{
    struct FixtureCase
    {
        const char* relativePath;
    };

    fra::CullFrameSnapshot gCullSnap {};
    std::vector<FixtureCase> gCases;
    std::string              gRunError;
    bool                     gAllOk = false;

    std::filesystem::path FixtureRoot()
    {
        const char* env = std::getenv("FREYA_GPU_FIXTURE_ROOT");
        return env ? env : FREYA_GPU_FIXTURE_ROOT;
    }

    std::uint32_t CreateAabbBoxMesh(fra::MeshPool& pool, const glm::vec3& bmin,
                                    const glm::vec3& bmax)
    {
        const glm::vec3 corners[8] = {
            { bmin.x, bmin.y, bmin.z }, { bmax.x, bmin.y, bmin.z },
            { bmin.x, bmax.y, bmin.z }, { bmax.x, bmax.y, bmin.z },
            { bmin.x, bmin.y, bmax.z }, { bmax.x, bmin.y, bmax.z },
            { bmin.x, bmax.y, bmax.z }, { bmax.x, bmax.y, bmax.z },
        };
        std::vector<fra::Vertex> verts(8);
        for (int i = 0; i < 8; ++i)
        {
            verts[i].position = corners[i];
            verts[i].normal   = { 0.f, 1.f, 0.f };
            verts[i].color    = { 1.f, 1.f, 1.f };
            verts[i].tangent  = { 1.f, 0.f, 0.f };
            verts[i].texCoord = { 0.f, 0.f };
        }
        const std::vector<std::uint32_t> indices = {
            0, 1, 2, 1, 3, 2, 4, 6, 5, 5, 6, 7, 0, 2, 4, 2, 6, 4,
            1, 5, 3, 3, 5, 7, 2, 3, 6, 3, 7, 6, 0, 4, 1, 1, 4, 5,
        };
        return pool.CreateMesh(verts, indices);
    }

    void AssertExpected(const fra::CullFrameExpected&          expected,
                        const std::vector<fra::CullSurvivor>& survivors,
                        const std::uint32_t                   drawCount)
    {
        std::unordered_set<std::uint32_t> live;
        for (const auto& s : survivors)
            live.insert(s.entityId);

        for (const auto id : expected.mustSurviveEntityIds)
        {
            INFO("missing mustSurvive entityId=" << id
                 << " (survivors=" << survivors.size() << ")");
            REQUIRE(live.contains(id));
        }
        for (const auto id : expected.mustDieEntityIds)
        {
            INFO("unexpected survivor entityId=" << id);
            REQUIRE_FALSE(live.contains(id));
        }
        if (expected.drawCount >= 0)
            REQUIRE(drawCount ==
                    static_cast<std::uint32_t>(expected.drawCount));
    }

    bool ReplayOnDevice(fra::MeshPool& meshPool, fra::MaterialPool& materialPool,
                        fra::IndirectDrawSystem&  indirect,
                        fra::CommandPool&         commandPool,
                        fra::Device&              device,
                        const fra::CullFrameSnapshot& snap,
                        std::uint32_t&                outDrawCount,
                        std::vector<fra::CullSurvivor>& outSurvivors,
                        std::string&                    error)
    {
        outDrawCount = 0;
        outSurvivors.clear();

        const auto materialId = materialPool.Create({
            .albedoFactor    = { 1.f, 1.f, 1.f, 1.f },
            .roughnessFactor = 1.f,
            .metalnessFactor = 0.f,
        });

        std::vector<std::uint32_t> meshRemap(snap.meshes.size());
        for (std::size_t i = 0; i < snap.meshes.size(); ++i)
        {
            const auto& m = snap.meshes[i];
            meshRemap[i]  = CreateAabbBoxMesh(meshPool, glm::vec3(m.aabbMin),
                                              glm::vec3(m.aabbMax));
        }

        std::vector<fra::SceneInstanceUpload> uploads;
        uploads.reserve(snap.instances.size());
        for (const auto& inst : snap.instances)
        {
            fra::SceneInstanceUpload u {};
            u.model      = inst.model;
            u.meshId     = inst.meshId < meshRemap.size()
                               ? meshRemap[inst.meshId]
                               : meshRemap.front();
            u.materialId = materialId;
            u.entityId   = inst.entityId;
            u.castShadows =
                (inst.flags & fra::kSceneInstanceFlagCastShadows) != 0;
            if (inst.flags & fra::kSceneInstanceFlagSkinned)
            {
                // Non-kNoSkin so CullFrustum skips Hi-Z (matches live dump).
                u.boneOffset = 0;
                u.boneCount  = 1;
            }
            uploads.push_back(u);
        }

        if (snap.pushConstants.hizEnabled != 0 && snap.hiz.present &&
            !snap.hiz.pixels.empty())
        {
            if (!indirect.UploadHiZFromDump(snap.hiz))
            {
                error = "Hi-Z upload failed";
                return false;
            }
        }

        // Record on FiF slot 0 without BeginFrame/Present — avoids swapchain
        // waits when replaying multiple fixtures in one process.
        constexpr std::uint32_t kFrame = 0;
        commandPool.SetCommandBufferIndex(kFrame);
        auto& cb = commandPool.GetCommandBuffer();
        cb.reset();
        cb.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        indirect.UploadSceneInstances(uploads, kFrame);

        indirect.SetCullView(
            glm::vec3(snap.pushConstants.cameraPos),
            vk::Extent2D {
                static_cast<std::uint32_t>(
                    std::max(snap.pushConstants.screenSize.x, 1.f)),
                static_cast<std::uint32_t>(
                    std::max(snap.pushConstants.screenSize.y, 1.f)) });

        auto pc          = snap.pushConstants;
        pc.instanceCount = static_cast<std::uint32_t>(uploads.size());
        if (pc.techniqueFilter == 0)
            pc.techniqueFilter = fra::kTechniqueFilterAll;
        indirect.DispatchCullExact(pc);

        cb.end();
        const auto submit =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cb);
        device.GetGraphicsQueue().submit(submit);
        device.GetGraphicsQueue().waitIdle();

        if (!indirect.ReadbackCullOutputs(kFrame, pc.techniqueFilter,
                                          outDrawCount, outSurvivors))
        {
            error = "readback failed";
            return false;
        }
        return true;
    }

    class CullReplayApp final : public fra::AbstractApplication
    {
      public:
        explicit CullReplayApp(
            const skr::Arc<skr::ServiceProvider>& serviceProvider) :
            AbstractApplication(serviceProvider)
        {
        }

        void StartUp() override
        {
            try
            {
                auto sp = GetMainServiceProvider();
                auto meshPool =
                    GetRootServiceProvider()->GetService<fra::MeshPool>();
                auto materialPool =
                    GetRootServiceProvider()->GetService<fra::MaterialPool>();
                auto indirect = sp->GetService<fra::IndirectDrawSystem>();
                auto commandPool = sp->GetService<fra::CommandPool>();
                auto device      = sp->GetService<fra::Device>();
                if (!meshPool || !materialPool || !indirect || !commandPool ||
                    !device || !mRenderer)
                {
                    gRunError = "missing Freya services";
                    mDone     = true;
                    return;
                }

                gAllOk = true;
                for (const auto& cse : gCases)
                {
                    const auto dir = FixtureRoot() / cse.relativePath;
                    fra::CullFrameSnapshot snap {};
                    if (!FreyaExamples::LoadCullFrameDump(dir, snap))
                    {
                        gRunError = "failed to load " + dir.string();
                        gAllOk    = false;
                        break;
                    }

                    std::uint32_t                  drawCount = 0;
                    std::vector<fra::CullSurvivor> survivors;
                    std::string                    error;
                    if (!ReplayOnDevice(*meshPool, *materialPool, *indirect,
                                        *commandPool, *device, snap, drawCount,
                                        survivors, error))
                    {
                        gRunError = cse.relativePath + std::string(": ") +
                                    error;
                        gAllOk = false;
                        break;
                    }

                    AssertExpected(snap.expected, survivors, drawCount);
                }
            }
            catch (const std::exception& ex)
            {
                gRunError = ex.what();
                gAllOk    = false;
            }
            mDone = true;
        }

        void Update() override
        {
            if (mDone && mWindow)
                mWindow->Close();
        }

      private:
        bool mDone = false;
    };
} // namespace

namespace
{
    void RunGpuCullFixtures(std::vector<FixtureCase> cases)
    {
        gCases = std::move(cases);
        gRunError.clear();
        gAllOk = false;

        for (const auto& cse : gCases)
        {
            if (!std::filesystem::exists(FixtureRoot() / cse.relativePath /
                                         "frame.json"))
                SKIP("fixture missing: " << cse.relativePath);
        }

        try
        {
            const auto app =
                skr::ApplicationBuilder()
                    .WithExtension<fra::FreyaExtension>(
                        [](fra::FreyaExtension freya) {
                            freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
                                o.SetTitle("FreyaGpuTests")
                                    .SetWidth(64)
                                    .SetHeight(64)
                                    .SetVSync(false)
                                    .SetSampleCount(1)
                                    .SetFullscreen(false)
                                    .WithReverseZ(true)
                                    .SetShaderRoot(FREYA_SHADER_ROOT);
                            });
                        })
                    .Build<CullReplayApp>();

            app->Run();
        }
        catch (const std::exception& ex)
        {
            SKIP("Vulkan/Freya init failed: " << ex.what());
        }

        if (!gRunError.empty() && !gAllOk)
        {
            if (gRunError.find("SDL") != std::string::npos ||
                gRunError.find("Vulkan") != std::string::npos ||
                gRunError.find("Failed") != std::string::npos)
                SKIP("Vulkan/Freya init failed: " << gRunError);
            FAIL(gRunError);
        }
        REQUIRE(gAllOk);
    }
} // namespace

TEST_CASE("GPU cull fixtures from JSON", "[gpu-cull]")
{
    RunGpuCullFixtures({
        { "frustum_visible" },
        { "frustum_culled" },
        { "ipl_ground_center_offscreen" },
        { "ipl_camera_motion_pair/frame0" },
        { "ipl_camera_motion_pair/frame1" },
    });
}

TEST_CASE("CellBulbasaur eyes must survive at dump camera (Hi-Z false cull)",
          "[gpu-cull][cell-eyes]")
{
    // Inputs: cull_dumps/20260907_130957 (eyes entityId 2/5 missing).
    // Expected: survivors from 20260907_131005 (eyes visible).
    RunGpuCullFixtures({ { "cell_eyes_false_cull" } });
}
