#include "ExternalProcess.hpp"

#include <algorithm>
#include <cwctype>
#include <tlhelp32.h>
#include <utility>

namespace Aegis::UnityExternal
{
    std::wstring ToLower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        return value;
    }

    bool ContainsInsensitive(const std::wstring& haystack, const wchar_t* needle)
    {
        return needle && ToLower(haystack).find(ToLower(needle)) != std::wstring::npos;
    }

    std::wstring NormalizeExecutableName(std::wstring executableName)
    {
        std::replace(executableName.begin(), executableName.end(), L'/', L'\\');

        const std::size_t slash = executableName.find_last_of(L'\\');
        if (slash != std::wstring::npos)
        {
            executableName = executableName.substr(slash + 1);
        }

        executableName = ToLower(std::move(executableName));
        if (executableName.find(L'.') == std::wstring::npos)
        {
            executableName += L".exe";
        }

        return executableName;
    }

    bool ExecutableNameMatches(const std::wstring& processExecutable, const std::wstring& requestedExecutable)
    {
        if (requestedExecutable.empty())
        {
            return true;
        }

        return NormalizeExecutableName(processExecutable) == NormalizeExecutableName(requestedExecutable);
    }

    bool UnityRuntimeModules::HasUnityPlayer() const
    {
        return unityPlayer.has_value();
    }

    bool UnityRuntimeModules::HasGameAssembly() const
    {
        return gameAssembly.has_value();
    }

    bool UnityRuntimeModules::HasMono() const
    {
        return mono.has_value();
    }

    RuntimeBackend UnityRuntimeModules::Backend() const
    {
        if (HasGameAssembly())
        {
            return RuntimeBackend::IL2CPP;
        }

        if (HasMono())
        {
            return RuntimeBackend::Mono;
        }

        return RuntimeBackend::Unknown;
    }

    const wchar_t* RuntimeBackendName(RuntimeBackend backend)
    {
        switch (backend)
        {
        case RuntimeBackend::IL2CPP:
            return L"IL2CPP";
        case RuntimeBackend::Mono:
            return L"Mono";
        default:
            return L"Unknown";
        }
    }

    UnityRuntimeModules InspectRuntimeModules(DWORD pid)
    {
        UnityRuntimeModules modules;
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return modules;
        }

        MODULEENTRY32W entry = {};
        entry.dwSize = sizeof(entry);

        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                ModuleInfo module;
                module.name = entry.szModule;
                module.path = entry.szExePath;
                module.base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                module.size = entry.modBaseSize;

                const std::wstring moduleName = ToLower(module.name);
                if (moduleName == L"unityplayer.dll")
                {
                    modules.unityPlayer = module;
                }
                else if (moduleName == L"gameassembly.dll")
                {
                    modules.gameAssembly = module;
                }
                else if (moduleName == L"mono.dll" || moduleName == L"mono-2.0-bdwgc.dll")
                {
                    modules.mono = module;
                }
            }
            while (Module32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return modules;
    }

    static bool LooksLikeUnityProcess(const PROCESSENTRY32W& process, const UnityRuntimeModules& modules)
    {
        return modules.HasUnityPlayer() ||
            modules.HasGameAssembly() ||
            ContainsInsensitive(process.szExeFile, L"unity");
    }

    static std::wstring QueryProcessImagePath(DWORD pid)
    {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
        {
            return {};
        }

        std::wstring path(MAX_PATH, L'\0');
        DWORD size = static_cast<DWORD>(path.size());
        if (!QueryFullProcessImageNameW(process, 0, path.data(), &size))
        {
            CloseHandle(process);
            return {};
        }

        CloseHandle(process);
        path.resize(size);
        return path;
    }

    std::vector<UnityProcess> EnumerateUnityProcesses(std::optional<DWORD> requestedPid)
    {
        return EnumerateUnityProcesses(requestedPid, std::nullopt);
    }

    std::vector<UnityProcess> EnumerateUnityProcesses(
        std::optional<DWORD> requestedPid,
        const std::optional<std::wstring>& requestedExecutable)
    {
        std::vector<UnityProcess> processes;
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return processes;
        }

        const bool explicitTarget = requestedPid.has_value() ||
            (requestedExecutable && !requestedExecutable->empty());

        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (requestedPid && entry.th32ProcessID != *requestedPid)
                {
                    continue;
                }

                if (requestedExecutable && !ExecutableNameMatches(entry.szExeFile, *requestedExecutable))
                {
                    continue;
                }

                UnityRuntimeModules modules = InspectRuntimeModules(entry.th32ProcessID);
                if (!explicitTarget && !LooksLikeUnityProcess(entry, modules))
                {
                    continue;
                }

                processes.push_back(UnityProcess{
                    entry.th32ProcessID,
                    entry.szExeFile,
                    QueryProcessImagePath(entry.th32ProcessID),
                    modules
                });
            }
            while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return processes;
    }
}
