#pragma once

#include <Freya/Asset/CullFrameDump.hpp>

#include <filesystem>
#include <string>

namespace FreyaExamples
{
    /**
     * @brief Write frame.json (+ optional hiz.r32f) for FreyaGpuTests.
     * @return Absolute path to the dump directory, or empty on failure.
     */
    std::string WriteCullFrameDump(const fra::CullFrameSnapshot& snap,
                                   const std::filesystem::path&  directory);

    /**
     * @brief Load a fixture directory containing frame.json.
     */
    bool LoadCullFrameDump(const std::filesystem::path& directory,
                           fra::CullFrameSnapshot&      out);

    /**
     * @brief Default dump root: ./cull_dumps/<timestamp>/
     */
    std::filesystem::path MakeCullDumpDirectory(
        const std::filesystem::path& root = "cull_dumps");

} // namespace FreyaExamples
