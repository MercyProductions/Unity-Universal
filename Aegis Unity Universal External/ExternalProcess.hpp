#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Aegis::UnityExternal
{
    enum class RuntimeBackend
    {
        Unknown,
        IL2CPP,
        Mono
    };

    struct ModuleInfo
    {
        std::wstring name;
        std::wstring path;
        uintptr_t base = 0;
        std::uint32_t size = 0;
    };

    struct UnityRuntimeModules
    {
        std::optional<ModuleInfo> unityPlayer;
        std::optional<ModuleInfo> gameAssembly;
        std::optional<ModuleInfo> mono;

        bool HasUnityPlayer() const;
        bool HasGameAssembly() const;
        bool HasMono() const;
        RuntimeBackend Backend() const;
    };

    struct UnityProcess
    {
        DWORD pid = 0;
        std::wstring executable;
        std::wstring executablePath;
        UnityRuntimeModules modules;
    };

    std::wstring ToLower(std::wstring value);
    bool ContainsInsensitive(const std::wstring& haystack, const wchar_t* needle);
    std::wstring NormalizeExecutableName(std::wstring executableName);
    bool ExecutableNameMatches(const std::wstring& processExecutable, const std::wstring& requestedExecutable);
    const wchar_t* RuntimeBackendName(RuntimeBackend backend);

    UnityRuntimeModules InspectRuntimeModules(DWORD pid);
    std::vector<UnityProcess> EnumerateUnityProcesses(std::optional<DWORD> requestedPid);
    std::vector<UnityProcess> EnumerateUnityProcesses(
        std::optional<DWORD> requestedPid,
        const std::optional<std::wstring>& requestedExecutable);
}
