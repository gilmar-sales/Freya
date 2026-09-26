#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

// ── Church Shadows & Lights Showcase ─────────────────────────────────────────
//
// Gothic nave demonstrating opaque / transparent shadow interaction:
//   • Directional sun enters from the right wall (+x) at a steep angle
//   • 8 round pillars cast long shadow bars across the stone floor
//   • 4 stained glass windows (amber, blue, crimson, emerald) via WBOIT
//   • 4 candle point-lights with subtle flicker along the nave
//   • Altar spot light illuminates the chancel
//
// Layout (top-down):
//   x ∈ [-6, +6], z ∈ [0, 24], y ∈ [0, 10]
//   Pillars at x = ±3.5, z = {4, 9, 14, 19}
//   Windows on right wall (x = +6), z = {[1,6],[7,12],[13,18],[19,23]}
//
// Controls: RMB + WASD fly | F3 light gizmos | 1 sun / 2 altar / 3 all

// ── Geometry helpers ─────────────────────────────────────────────────────────
// All helpers use indices { 0,1,2, 0,2,3 }.
// Vertex ordering must make cross(v1-v0, v2-v0) point toward the intended n.
// Conventions (see comments beside each call site):
//   +Y  p0=(x0,y,z0) p1=(x0,y,z1) p2=(x1,y,z1) p3=(x1,y,z0)
//   -Y  p0=(x0,y,z1) p1=(x0,y,z0) p2=(x1,y,z0) p3=(x1,y,z1)
//   +X  p0=(x,y0,z0) p1=(x,y1,z0) p2=(x,y1,z1) p3=(x,y0,z1)
//   -X  p0=(x,y0,z1) p1=(x,y1,z1) p2=(x,y1,z0) p3=(x,y0,z0)
//   +Z  p0=(x0,y0,z) p1=(x1,y0,z) p2=(x1,y1,z) p3=(x0,y1,z)
//   -Z  p0=(x1,y0,z) p1=(x0,y0,z) p2=(x0,y1,z) p3=(x1,y1,z)

static void AppendFace(
    std::vector<fra::Vertex>& vs,
    std::vector<uint32_t>&    is,
    glm::vec3 p0, glm::vec3 p1,
    glm::vec3 p2, glm::vec3 p3,
    glm::vec3 n)
{
    const auto      base = static_cast<uint32_t>(vs.size());
    const glm::vec3 white {1.f};
    const glm::vec3 tang =
        glm::length(p1 - p0) > 1e-6f
            ? glm::normalize(p1 - p0)
            : glm::vec3(1.f, 0.f, 0.f);
    vs.push_back({p0, white, n, tang, {0.f, 0.f}});
    vs.push_back({p1, white, n, tang, {1.f, 0.f}});
    vs.push_back({p2, white, n, tang, {1.f, 1.f}});
    vs.push_back({p3, white, n, tang, {0.f, 1.f}});
    is.push_back(base);
    is.push_back(base + 1);
    is.push_back(base + 2);
    is.push_back(base);
    is.push_back(base + 2);
    is.push_back(base + 3);
}

static fra::MeshHandle BuildQuad(
    fra::MeshPool& pool,
    glm::vec3 p0, glm::vec3 p1,
    glm::vec3 p2, glm::vec3 p3,
    glm::vec3 n)
{
    std::vector<fra::Vertex> vs;
    std::vector<uint32_t>    is;
    AppendFace(vs, is, p0, p1, p2, p3, n);
    return pool.CreateMesh(vs, is);
}

static fra::MeshHandle BuildBox(
    fra::MeshPool& pool,
    glm::vec3 mn, glm::vec3 mx)
{
    std::vector<fra::Vertex> vs;
    std::vector<uint32_t>    is;

    auto f = [&](glm::vec3 a, glm::vec3 b,
                 glm::vec3 c, glm::vec3 d,
                 glm::vec3 n)
    {
        AppendFace(vs, is, a, b, c, d, n);
    };

    // +Y top
    f({mn.x, mx.y, mn.z}, {mn.x, mx.y, mx.z},
      {mx.x, mx.y, mx.z}, {mx.x, mx.y, mn.z},
      {0.f,  1.f, 0.f});
    // -Y bottom
    f({mn.x, mn.y, mx.z}, {mn.x, mn.y, mn.z},
      {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z},
      {0.f, -1.f, 0.f});
    // +Z front
    f({mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
      {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
      {0.f, 0.f,  1.f});
    // -Z back
    f({mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z},
      {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
      {0.f, 0.f, -1.f});
    // +X right
    f({mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z},
      {mx.x, mx.y, mx.z}, {mx.x, mn.y, mx.z},
      { 1.f, 0.f, 0.f});
    // -X left
    f({mn.x, mn.y, mx.z}, {mn.x, mx.y, mx.z},
      {mn.x, mx.y, mn.z}, {mn.x, mn.y, mn.z},
      {-1.f, 0.f, 0.f});

    return pool.CreateMesh(vs, is);
}

// N-sided vertical prism, centered on origin, y in [0, height].
// Face ordering: b_i, t_i, t_{i+1}, b_{i+1} gives outward normal.
static fra::MeshHandle BuildPillar(
    fra::MeshPool& pool,
    float radius, float height, int N = 8)
{
    std::vector<fra::Vertex> vs;
    std::vector<uint32_t>    is;

    const float k2Pi = 2.f * std::numbers::pi_v<float>;
    for (int i = 0; i < N; ++i)
    {
        const float a0 = k2Pi * i / N;
        const float a1 = k2Pi * (i + 1) / N;
        const float am = (a0 + a1) * 0.5f;

        const glm::vec3 b0 {
            radius * std::cos(a0), 0.f, radius * std::sin(a0)
        };
        const glm::vec3 b1 {
            radius * std::cos(a1), 0.f, radius * std::sin(a1)
        };
        const glm::vec3 t0 {b0.x, height, b0.z};
        const glm::vec3 t1 {b1.x, height, b1.z};
        const glm::vec3 n {std::cos(am), 0.f, std::sin(am)};

        AppendFace(vs, is, b0, t0, t1, b1, n);
    }
    return pool.CreateMesh(vs, is);
}

// ── Application ──────────────────────────────────────────────────────────────

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(
        const fra::Ref<fra::ServiceProvider>& sp) :
        AbstractApplication(sp)
    {
        auto ws       = GetMainServiceProvider();
        mMeshPool     = sp->GetService<fra::MeshPool>();
        mTexturePool  = sp->GetService<fra::TexturePool>();
        mMaterialPool = sp->GetService<fra::MaterialPool>();
        mLightService = ws->GetService<fra::LightService>();
        mFreyaOptions = ws->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mCam.window     = mWindow;
        mCam.cameraPos  = {0.f, 2.5f, 1.5f};
        mCam.yaw        = 90.f;
        mCam.pitch      = -5.f;
        mCam.moveSpeed  = 5.f;
        mCam.blockMouse = [this]
        {
            return mOverlay.WantsCaptureMouse();
        };
        mCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);
        mOverlay.SetCullDumpExampleName("ChurchShowcase");

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& ev)
            {
                if (ev.key == fra::KeyCode::F3)
                {
                    mShowGizmos = !mShowGizmos;
                    mRenderer->SetDebugDrawEnabled(mShowGizmos);
                }
                if (ev.key == fra::KeyCode::Num1 ||
                    ev.key == fra::KeyCode::Kp1)
                    setShadowMode(1);
                if (ev.key == fra::KeyCode::Num2 ||
                    ev.key == fra::KeyCode::Kp2)
                    setShadowMode(2);
                if (ev.key == fra::KeyCode::Num3 ||
                    ev.key == fra::KeyCode::Kp3)
                    setShadowMode(3);
            });

        mRenderer->ClearProjections();
        buildMaterials();
        buildChurch();
        buildLights();
        setShadowMode(3);
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mTime += dt;
        mCam.Update(dt);

        // Candle flicker — two beating sine waves
        for (std::size_t i = 0; i < mCandleHandles.size(); ++i)
        {
            const auto* c =
                mLightService->GetLight(mCandleHandles[i]);
            if (!c)
                continue;
            fra::Light l = *c;
            l.intensity =
                3.5f *
                (1.f + 0.12f *
                 std::sin(mTime * 7.3f +
                          static_cast<float>(i) * 2.1f) *
                 std::sin(mTime * 4.1f +
                          static_cast<float>(i) * 0.7f));
            mLightService->UpdateLight(mCandleHandles[i], l);
        }

        mRenderer->BeginFrame();
        if (mShowGizmos)
            drawGizmos();
        mCam.Apply(*mRenderer);
        mScene.Upload(*mRenderer);

        const float cpuMs = dt * 1000.f;
        mOverlay.Draw(
            *mRenderer, *mFreyaOptions,
            cpuMs, mOverlay.ElapsedUpdateMs(),
            mLightService.get());
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    // ── Materials ─────────────────────────────────────────────────────────
    void buildMaterials()
    {
        mStoneMat = mMaterialPool->Create({
            .albedoFactor    = {0.76f, 0.70f, 0.60f, 1.f},
            .roughnessFactor = 0.88f,
            .metalnessFactor = 0.f,
        });

        mDarkStoneMat = mMaterialPool->Create({
            .albedoFactor    = {0.40f, 0.35f, 0.28f, 1.f},
            .roughnessFactor = 0.82f,
            .metalnessFactor = 0.f,
        });

        // Amber / blue / crimson / emerald stained glass
        const std::array<glm::vec4, 4> tints {{
            {1.00f, 0.65f, 0.10f, 0.18f},
            {0.15f, 0.38f, 1.00f, 0.18f},
            {0.85f, 0.08f, 0.10f, 0.18f},
            {0.08f, 0.65f, 0.22f, 0.18f},
        }};
        for (std::size_t i = 0; i < 4; ++i)
        {
            mGlassMat[i] = mMaterialPool->Create({
                .albedoFactor    = tints[i],
                .roughnessFactor = 0.03f,
                .metalnessFactor = 0.f,
                .emissiveFactor  = glm::vec3(tints[i]) * 0.4f,
                .alphaMode       = fra::AlphaMode::Blend,
                .transmission    = 0.88f,
                .ior             = 1.52f,
                .doubleSided     = true,
            });
        }
    }

    // ── Scene instance helper ──────────────────────────────────────────────
    void addInst(
        fra::MeshHandle     mesh,
        fra::MaterialHandle mat,
        const glm::mat4&    xf,
        bool                castShadows = true,
        fra::Mobility       mob = fra::Mobility::Static)
    {
        fra::Scene::Instance inst {};
        inst.mesh      = mesh;
        inst.material  = mat;
        inst.transform = fra::SceneTransform::FromMatrix(xf);
        inst.flags     = castShadows
                             ? fra::kSceneInstanceFlagCastShadows
                             : 0u;
        inst.mobility  = mob;
        mScene.Add(inst);
    }

    // ── Church geometry ────────────────────────────────────────────────────
    void buildChurch()
    {
        // Floor — +Y: p0=(x0,y,z0) p1=(x0,y,z1) p2=(x1,y,z1) p3=(x1,y,z0)
        addInst(
            BuildQuad(*mMeshPool,
                      {-7.f, 0.f,  0.f}, {-7.f, 0.f, 24.f},
                      { 7.f, 0.f, 24.f}, { 7.f, 0.f,  0.f},
                      {0.f, 1.f, 0.f}),
            mStoneMat, glm::mat4(1.f));

        // Left solid wall (x=-6, normal +X toward nave)
        // +X: p0=(x,y0,z0) p1=(x,y1,z0) p2=(x,y1,z1) p3=(x,y0,z1)
        addInst(
            BuildQuad(*mMeshPool,
                      {-6.f, 0.f,  0.f}, {-6.f,10.f,  0.f},
                      {-6.f,10.f, 24.f}, {-6.f, 0.f, 24.f},
                      {1.f, 0.f, 0.f}),
            mStoneMat, glm::mat4(1.f));

        // Front wall (z=0, normal +Z)
        // +Z: p0=(x0,y0,z) p1=(x1,y0,z) p2=(x1,y1,z) p3=(x0,y1,z)
        addInst(
            BuildQuad(*mMeshPool,
                      {-6.f, 0.f, 0.f}, { 6.f, 0.f, 0.f},
                      { 6.f,10.f, 0.f}, {-6.f,10.f, 0.f},
                      {0.f, 0.f, 1.f}),
            mStoneMat, glm::mat4(1.f));

        // Back wall (z=24, normal -Z)
        // -Z: p0=(x1,y0,z) p1=(x0,y0,z) p2=(x0,y1,z) p3=(x1,y1,z)
        addInst(
            BuildQuad(*mMeshPool,
                      { 6.f, 0.f,24.f}, {-6.f, 0.f,24.f},
                      {-6.f,10.f,24.f}, { 6.f,10.f,24.f},
                      {0.f, 0.f,-1.f}),
            mStoneMat, glm::mat4(1.f));

        // Right wall (x=+6) — built in strips with window openings
        buildRightWall();

        // Arch beams spanning x = [-3.5, +3.5] at each pillar row
        {
            const auto beam = BuildBox(
                *mMeshPool,
                {-3.5f, 7.8f, -0.25f},
                { 3.5f, 8.3f,  0.25f});
            for (float bz : {4.f, 9.f, 14.f, 19.f})
            {
                addInst(beam, mStoneMat,
                        glm::translate(
                            glm::mat4(1.f), {0.f, 0.f, bz}));
            }
        }

        // Pillars — 4 pairs at x = ±3.5, z = {4, 9, 14, 19}
        {
            const auto pillar =
                BuildPillar(*mMeshPool, 0.45f, 7.8f, 8);
            for (float pz : {4.f, 9.f, 14.f, 19.f})
            {
                for (float px : {-3.5f, 3.5f})
                {
                    addInst(pillar, mStoneMat,
                            glm::translate(
                                glm::mat4(1.f), {px, 0.f, pz}));
                }
            }
        }

        // Altar platform and step
        addInst(
            BuildBox(*mMeshPool,
                     {-2.f, 0.f, 20.5f},
                     { 2.f, 0.7f, 23.f}),
            mDarkStoneMat, glm::mat4(1.f));
        addInst(
            BuildBox(*mMeshPool,
                     {-1.5f, 0.7f, 21.f},
                     { 1.5f, 0.90f, 22.5f}),
            mDarkStoneMat, glm::mat4(1.f));

        // Candlestick bases on altar (decorative boxes)
        for (float cx : {-1.f, 1.f})
        {
            addInst(
                BuildBox(*mMeshPool,
                         {cx - 0.06f, 0.90f, 21.8f},
                         {cx + 0.06f, 1.25f, 21.92f}),
                mDarkStoneMat, glm::mat4(1.f));
        }

        // Stained glass windows — right wall (x=+6, normal -X)
        // -X: p0=(x,y0,z1) p1=(x,y1,z1) p2=(x,y1,z0) p3=(x,y0,z0)
        const float winZ[4][2] = {
            { 1.f,  6.f},
            { 7.f, 12.f},
            {13.f, 18.f},
            {19.f, 23.f},
        };
        for (int i = 0; i < 4; ++i)
        {
            const float z0 = winZ[i][0], z1 = winZ[i][1];
            addInst(
                BuildQuad(*mMeshPool,
                          {6.f, 3.f, z1}, {6.f, 6.f, z1},
                          {6.f, 6.f, z0}, {6.f, 3.f, z0},
                          {-1.f, 0.f, 0.f}),
                mGlassMat[i], glm::mat4(1.f),
                /*castShadows=*/false);
        }
    }

    // Right wall: bottom strip, top strip, 5 pilasters around windows
    void buildRightWall()
    {
        // -X: p0=(x,y0,z1) p1=(x,y1,z1) p2=(x,y1,z0) p3=(x,y0,z0)
        auto addPanel = [&](float y0, float y1,
                            float z0, float z1)
        {
            addInst(
                BuildQuad(*mMeshPool,
                          {6.f, y0, z1}, {6.f, y1, z1},
                          {6.f, y1, z0}, {6.f, y0, z0},
                          {-1.f, 0.f, 0.f}),
                mStoneMat, glm::mat4(1.f));
        };

        addPanel(0.f,  3.f,  0.f, 24.f); // bottom strip
        addPanel(6.f, 10.f,  0.f, 24.f); // top strip
        addPanel(3.f,  6.f,  0.f,  1.f); // pilaster — nave entrance
        addPanel(3.f,  6.f,  6.f,  7.f); // pilaster between win 1-2
        addPanel(3.f,  6.f, 12.f, 13.f); // pilaster between win 2-3
        addPanel(3.f,  6.f, 18.f, 19.f); // pilaster between win 3-4
        addPanel(3.f,  6.f, 23.f, 24.f); // pilaster — altar end
    }

    // ── Lights ────────────────────────────────────────────────────────────
    void buildLights()
    {
        // Sun — enters from high angle on right (+x) side, downward
        {
            auto sun = fra::MakeDirectionalLight(
                glm::normalize(glm::vec3(-1.f, -1.f, 0.12f)),
                glm::vec3(1.f, 0.93f, 0.78f),
                1.8f);
            sun.castShadows = true;
            mSunHandle      = mLightService->AddLight(sun);
        }

        // Candles — warm orange point lights, one per pillar row
        for (float cz : {4.f, 9.f, 14.f, 19.f})
        {
            auto candle = fra::MakePointLight(
                glm::vec3(0.f, 1.8f, cz),
                glm::vec3(1.f, 0.55f, 0.15f),
                9.f, 3.5f);
            candle.castShadows = true;
            mCandleHandles.push_back(
                mLightService->AddLight(candle));
        }

        // Altar spot — golden light from the ceiling
        {
            auto spot = fra::MakeSpotLight(
                glm::vec3(0.f, 9.5f, 22.f),
                glm::normalize(glm::vec3(0.f, -1.f, -0.12f)),
                glm::vec3(1.f, 0.88f, 0.55f),
                14.f,
                glm::radians(18.f),
                glm::radians(30.f),
                5.5f);
            spot.castShadows = true;
            mAltarSpotHandle = mLightService->AddLight(spot);
        }

        // Cool skylight fill — very dim, no shadows
        {
            auto fill = fra::MakeDirectionalLight(
                glm::vec3(0.f, -1.f, 0.f),
                glm::vec3(0.50f, 0.60f, 0.80f),
                0.07f);
            fill.castShadows = false;
            mLightService->AddLight(fill);
        }
    }

    // ── Shadow mode toggle ────────────────────────────────────────────────
    void setShadowMode(int mode)
    {
        mShadowMode = mode;
        mLightService->SetLightCastShadows(
            mSunHandle, mode == 1 || mode == 3);
        mLightService->SetLightCastShadows(
            mAltarSpotHandle, mode == 2 || mode == 3);

        const char* label =
            mode == 1 ? "sun only"
            : mode == 2 ? "altar spot only"
                        : "sun + altar";
        mFreyaOptions->title =
            std::string("Church Showcase | shadows: ") +
            label + " | F3 gizmos | 1/2/3";
    }

    // ── Debug gizmos ──────────────────────────────────────────────────────
    void drawGizmos()
    {
        auto&       dd = mRenderer->GetDebugDraw();
        const auto  n  = mLightService->GetLightCount();

        for (std::uint32_t i = 0; i < n; ++i)
        {
            const auto* l =
                mLightService->GetLight(fra::LightHandle{i});
            if (!l)
                continue;

            const float pk = std::max(
                {l->color.r, l->color.g, l->color.b, 0.2f});
            const glm::vec4 col {
                l->color / pk,
                l->castShadows ? 1.f : 0.45f};

            if (l->type == fra::LightType::Directional)
            {
                constexpr glm::vec3 kAnchor {0.f, 14.f, 12.f};
                const auto dir = glm::normalize(l->direction);
                dd.Arrow(kAnchor - dir * 7.f,
                         kAnchor + dir * 3.f, col);
            }
            else if (l->type == fra::LightType::Point)
            {
                dd.Sphere(l->position, 0.3f, col, 8);
            }
            else if (l->type == fra::LightType::Spot)
            {
                const float half = std::acos(
                    std::clamp(l->outerCutoff, -1.f, 1.f));
                dd.Cone(l->position, l->direction,
                        std::min(l->radius * 0.4f, 10.f),
                        half, col);
                dd.Sphere(l->position, 0.25f, col, 8);
            }
        }
    }

    // ── Members ───────────────────────────────────────────────────────────
    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::TexturePool>  mTexturePool;
    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;

    fra::Scene mScene;
    FreyaExamples::FlyCam       mCam;
    FreyaExamples::DebugOverlay mOverlay;

    fra::MaterialHandle               mStoneMat {};
    fra::MaterialHandle               mDarkStoneMat {};
    std::array<fra::MaterialHandle,4> mGlassMat {};

    fra::LightHandle              mSunHandle {};
    fra::LightHandle              mAltarSpotHandle {};
    std::vector<fra::LightHandle> mCandleHandles;

    float mTime       = 0.f;
    int   mShadowMode = 3;
    bool  mShowGizmos = false;
};

// ── Entry point ──────────────────────────────────────────────────────────────

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& o)
        {
            o.SetTitle(
                 "Church Showcase"
                 " | shadows: sun + altar"
                 " | F3 gizmos | 1/2/3")
                .SetWidth(1920)
                .SetHeight(1080)
                .SetVSync(false)
                .SetSampleCount(8)
                .WithReverseZ()
                .SetIblIntensity(0.04f)
                .SetEnvironmentMapPath(
                    "./Resources/Environments/"
                    "horn-koppe_spring_4k.hdr")
                .SetShadowQuality(fra::ShadowQuality::High)
                .SetShadowBias(0.001f)
                .SetShadowLightSize(0.018f)
                .SetShadowMaxSoftness(5.f)
                .SetShadowMinVisibility(0.f);
        },
        [](skr::LoggingExtension& l)
        {
            FreyaExamples::ConfigureLogging(
                l, "ChurchShowcase.log");
        });
}
