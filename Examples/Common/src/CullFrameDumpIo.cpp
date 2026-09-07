#include <FreyaExamples/CullFrameDumpIo.hpp>

#include <nlohmann/json.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace FreyaExamples
{
    namespace
    {
        constexpr std::uint32_t kHizMagic = 0x315A4948u; // 'HIZ1' LE

        std::uint32_t HizPackedCount(const std::uint32_t width,
                                     const std::uint32_t height,
                                     const std::uint32_t mipCount)
        {
            std::uint32_t total = 0;
            for (std::uint32_t mip = 0; mip < mipCount; ++mip)
                total +=
                    std::max(1u, width >> mip) * std::max(1u, height >> mip);
            return total;
        }

        nlohmann::json Mat4ToJson(const glm::mat4& m)
        {
            nlohmann::json a = nlohmann::json::array();
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    a.push_back(m[col][row]);
            return a;
        }

        glm::mat4 Mat4FromJson(const nlohmann::json& a)
        {
            glm::mat4 m(1.0f);
            if (!a.is_array() || a.size() < 16)
                return m;
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    m[col][row] = a[col * 4 + row].get<float>();
            return m;
        }

        nlohmann::json Vec4ToJson(const glm::vec4& v)
        {
            return nlohmann::json::array({ v.x, v.y, v.z, v.w });
        }

        glm::vec4 Vec4FromJson(const nlohmann::json& a)
        {
            if (!a.is_array() || a.size() < 4)
                return {};
            return { a[0].get<float>(), a[1].get<float>(), a[2].get<float>(),
                     a[3].get<float>() };
        }

        nlohmann::json Vec2ToJson(const glm::vec2& v)
        {
            return nlohmann::json::array({ v.x, v.y });
        }

        glm::vec2 Vec2FromJson(const nlohmann::json& a)
        {
            if (!a.is_array() || a.size() < 2)
                return {};
            return { a[0].get<float>(), a[1].get<float>() };
        }

        bool WriteHizFile(const std::filesystem::path& path,
                          const fra::CullHiZDump&      hiz)
        {
            std::ofstream out(path, std::ios::binary);
            if (!out)
                return false;
            const std::uint32_t magic = kHizMagic;
            out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
            out.write(reinterpret_cast<const char*>(&hiz.width),
                      sizeof(hiz.width));
            out.write(reinterpret_cast<const char*>(&hiz.height),
                      sizeof(hiz.height));
            out.write(reinterpret_cast<const char*>(&hiz.mipCount),
                      sizeof(hiz.mipCount));
            if (!hiz.pixels.empty())
            {
                out.write(reinterpret_cast<const char*>(hiz.pixels.data()),
                          static_cast<std::streamsize>(
                              hiz.pixels.size() * sizeof(float)));
            }
            return static_cast<bool>(out);
        }

        bool ReadHizFile(const std::filesystem::path& path,
                         fra::CullHiZDump&            hiz)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return false;
            std::uint32_t magic = 0;
            in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            if (magic != kHizMagic)
                return false;
            in.read(reinterpret_cast<char*>(&hiz.width), sizeof(hiz.width));
            in.read(reinterpret_cast<char*>(&hiz.height), sizeof(hiz.height));
            in.read(reinterpret_cast<char*>(&hiz.mipCount),
                    sizeof(hiz.mipCount));
            const auto total =
                HizPackedCount(hiz.width, hiz.height, hiz.mipCount);
            hiz.pixels.resize(total);
            in.read(reinterpret_cast<char*>(hiz.pixels.data()),
                    static_cast<std::streamsize>(total * sizeof(float)));
            hiz.present = true;
            hiz.ready   = true;
            hiz.file    = path.filename().string();
            return static_cast<bool>(in) || in.eof();
        }

        nlohmann::json SnapshotToJson(const fra::CullFrameSnapshot& snap)
        {
            nlohmann::json j;
            j["version"] = snap.version;
            j["meta"]    = {
                { "example", snap.example },
                { "label", snap.label },
                { "notes", snap.notes },
            };

            const auto& pc     = snap.pushConstants;
            j["pushConstants"] = {
                { "viewProj", Mat4ToJson(pc.viewProj) },
                { "cameraPos", Vec4ToJson(pc.cameraPos) },
                { "screenSize", Vec2ToJson(pc.screenSize) },
                { "instanceCount", pc.instanceCount },
                { "cullMode", pc.cullMode },
                { "reverseZ", pc.reverseZ },
                { "hizEnabled", pc.hizEnabled },
                { "lodPixelRef", pc.lodPixelRef },
                { "lodStep", pc.lodStep },
                { "techniqueFilter", pc.techniqueFilter },
                { "maxDraws", pc.maxDraws },
                { "hizDepthBias", pc.hizDepthBias },
            };

            j["meshes"] = nlohmann::json::array();
            for (const auto& m : snap.meshes)
            {
                j["meshes"].push_back({
                    { "lodCount", m.lodCount },
                    { "lodBase", m.lodBase },
                    { "aabbMin", Vec4ToJson(m.aabbMin) },
                    { "aabbMax", Vec4ToJson(m.aabbMax) },
                });
            }

            j["lods"] = nlohmann::json::array();
            for (const auto& lod : snap.lods)
            {
                j["lods"].push_back({
                    { "indexCount", lod.indexCount },
                    { "firstIndex", lod.firstIndex },
                    { "vertexOffset", lod.vertexOffset },
                });
            }

            j["instances"] = nlohmann::json::array();
            for (const auto& inst : snap.instances)
            {
                j["instances"].push_back({
                    { "model", Mat4ToJson(inst.model) },
                    { "meshId", inst.meshId },
                    { "materialId", inst.materialId },
                    { "entityId", inst.entityId },
                    { "flags", inst.flags },
                    { "techniqueId", inst.techniqueId },
                });
            }

            if (snap.hiz.present && !snap.hiz.pixels.empty())
            {
                j["hiz"] = {
                    { "file",
                      snap.hiz.file.empty() ? "hiz.r32f" : snap.hiz.file },
                    { "width", snap.hiz.width },
                    { "height", snap.hiz.height },
                    { "mipCount", snap.hiz.mipCount },
                    { "enabled", snap.hiz.enabled },
                    { "ready", snap.hiz.ready },
                };
            }
            else
            {
                j["hiz"] = nullptr;
            }

            nlohmann::json survivors = nlohmann::json::array();
            for (const auto& s : snap.survivors)
            {
                survivors.push_back({
                    { "entityId", s.entityId },
                    { "meshId", s.meshId },
                    { "slot", s.slot },
                });
            }
            j["observed"] = {
                { "drawCount", snap.observedDrawCount },
                { "survivors", survivors },
            };

            j["expected"] = {
                { "mustSurviveEntityIds", snap.expected.mustSurviveEntityIds },
                { "mustDieEntityIds", snap.expected.mustDieEntityIds },
                { "drawCount", snap.expected.drawCount },
            };
            return j;
        }

        bool SnapshotFromJson(const nlohmann::json&   j,
                              fra::CullFrameSnapshot& out)
        {
            out         = {};
            out.version = j.value("version", 1u);
            if (j.contains("meta"))
            {
                out.example = j["meta"].value("example", "");
                out.label   = j["meta"].value("label", "");
                out.notes   = j["meta"].value("notes", "");
            }

            if (!j.contains("pushConstants"))
                return false;
            const auto& pcj  = j["pushConstants"];
            auto&       pc   = out.pushConstants;
            pc.viewProj      = Mat4FromJson(pcj.at("viewProj"));
            pc.cameraPos     = Vec4FromJson(pcj.at("cameraPos"));
            pc.screenSize    = Vec2FromJson(pcj.at("screenSize"));
            pc.instanceCount = pcj.value("instanceCount", 0u);
            pc.cullMode      = pcj.value("cullMode", 0u);
            pc.reverseZ      = pcj.value("reverseZ", 0u);
            pc.hizEnabled    = pcj.value("hizEnabled", 0u);
            pc.lodPixelRef   = pcj.value("lodPixelRef", 256.f);
            pc.lodStep       = pcj.value("lodStep", 2.f);
            pc.techniqueFilter =
                pcj.value("techniqueFilter", fra::kTechniqueFilterAll);
            pc.maxDraws     = pcj.value("maxDraws", 0u);
            pc.hizDepthBias = pcj.value("hizDepthBias", 1e-4f);

            for (const auto& m : j.value("meshes", nlohmann::json::array()))
            {
                fra::MeshInfo info {};
                info.lodCount = m.value("lodCount", 0u);
                info.lodBase  = m.value("lodBase", 0u);
                info.aabbMin  = Vec4FromJson(m.at("aabbMin"));
                info.aabbMax  = Vec4FromJson(m.at("aabbMax"));
                out.meshes.push_back(info);
            }
            for (const auto& lod : j.value("lods", nlohmann::json::array()))
            {
                fra::MeshLodInfo info {};
                info.indexCount   = lod.value("indexCount", 0u);
                info.firstIndex   = lod.value("firstIndex", 0u);
                info.vertexOffset = lod.value("vertexOffset", 0);
                out.lods.push_back(info);
            }
            for (const auto& inst :
                 j.value("instances", nlohmann::json::array()))
            {
                fra::SceneInstance s {};
                s.model       = Mat4FromJson(inst.at("model"));
                s.meshId      = inst.value("meshId", 0u);
                s.materialId  = inst.value("materialId", 0u);
                s.entityId    = inst.value("entityId", 0u);
                s.flags       = inst.value("flags", 0u);
                s.techniqueId = inst.value("techniqueId", 0u);
                out.instances.push_back(s);
            }

            if (j.contains("hiz") && !j["hiz"].is_null())
            {
                const auto& h    = j["hiz"];
                out.hiz.present  = true;
                out.hiz.file     = h.value("file", "hiz.r32f");
                out.hiz.width    = h.value("width", 0u);
                out.hiz.height   = h.value("height", 0u);
                out.hiz.mipCount = h.value("mipCount", 0u);
                out.hiz.enabled  = h.value("enabled", false);
                out.hiz.ready    = h.value("ready", false);
            }

            if (j.contains("observed"))
            {
                out.observedDrawCount = j["observed"].value("drawCount", 0u);
                for (const auto& s :
                     j["observed"].value("survivors", nlohmann::json::array()))
                {
                    fra::CullSurvivor surv {};
                    surv.entityId = s.value("entityId", 0u);
                    surv.meshId   = s.value("meshId", 0u);
                    surv.slot     = s.value("slot", 0u);
                    out.survivors.push_back(surv);
                }
            }

            if (j.contains("expected"))
            {
                const auto& e          = j["expected"];
                out.expected.drawCount = e.value("drawCount", -1);
                if (e.contains("mustSurviveEntityIds"))
                    out.expected.mustSurviveEntityIds =
                        e["mustSurviveEntityIds"]
                            .get<std::vector<std::uint32_t>>();
                if (e.contains("mustDieEntityIds"))
                    out.expected.mustDieEntityIds =
                        e["mustDieEntityIds"].get<std::vector<std::uint32_t>>();
            }
            return true;
        }
    } // namespace

    std::filesystem::path MakeCullDumpDirectory(
        const std::filesystem::path& root)
    {
        using clock = std::chrono::system_clock;
        const auto now =
            std::chrono::time_point_cast<std::chrono::seconds>(clock::now());
        const std::time_t t = clock::to_time_t(now);
        std::tm           tm {};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        auto            dir = root / oss.str();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    std::string WriteCullFrameDump(const fra::CullFrameSnapshot& snap,
                                   const std::filesystem::path&  directory)
    {
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec)
            return {};

        auto toWrite = snap;
        if (toWrite.hiz.present && !toWrite.hiz.pixels.empty())
        {
            if (toWrite.hiz.file.empty())
                toWrite.hiz.file = "hiz.r32f";
            if (!WriteHizFile(directory / toWrite.hiz.file, toWrite.hiz))
                std::fprintf(stderr, "CullFrameDump: failed to write %s\n",
                             toWrite.hiz.file.c_str());
        }

        const auto    jsonPath = directory / "frame.json";
        std::ofstream out(jsonPath);
        if (!out)
            return {};
        out << SnapshotToJson(toWrite).dump(2) << '\n';
        return std::filesystem::absolute(directory).string();
    }

    bool LoadCullFrameDump(const std::filesystem::path& directory,
                           fra::CullFrameSnapshot&      out)
    {
        const auto    jsonPath = directory / "frame.json";
        std::ifstream in(jsonPath);
        if (!in)
            return false;
        nlohmann::json j;
        try
        {
            in >> j;
        }
        catch (...)
        {
            return false;
        }
        if (!SnapshotFromJson(j, out))
            return false;

        if (out.hiz.present && !out.hiz.file.empty())
        {
            fra::CullHiZDump hiz = out.hiz;
            if (!ReadHizFile(directory / out.hiz.file, hiz))
                return false;
            hiz.enabled = out.hiz.enabled;
            out.hiz     = std::move(hiz);
        }
        return true;
    }

} // namespace FreyaExamples
