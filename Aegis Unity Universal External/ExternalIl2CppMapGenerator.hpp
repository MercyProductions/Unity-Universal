#pragma once

#include "ExternalMethodResolver.hpp"
#include "ExternalProcess.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace Aegis::UnityExternal
{
    struct GeneratedIl2CppMethodMap
    {
        bool success = false;
        MethodMap methodMap;
        std::filesystem::path metadataPath;
        std::size_t imageCount = 0;
        std::size_t matchedModules = 0;
        std::size_t candidateMethods = 0;
        std::size_t resolvedMethods = 0;
        std::string message;
    };

    std::optional<std::filesystem::path> FindIl2CppMetadataPath(const UnityProcess& process);
    GeneratedIl2CppMethodMap GenerateIl2CppMethodMap(const UnityProcess& process);
}
