#pragma once

#include "ExternalMethodResolver.hpp"
#include "ExternalProcess.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace Aegis::UnityExternal
{
    struct GeneratedMonoMethodMap
    {
        bool success = false;
        std::string message;
        MethodMap methodMap;
        std::filesystem::path managedDirectory;
        std::size_t assemblyCount = 0;
        std::size_t typeCount = 0;
        std::size_t methodCount = 0;
    };

    std::optional<std::filesystem::path> FindMonoManagedDirectory(const UnityProcess& process);
    GeneratedMonoMethodMap GenerateMonoMethodMap(const UnityProcess& process);
}
