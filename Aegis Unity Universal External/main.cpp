#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ExternalMemory.hpp"
#include "ExternalGui.hpp"
#include "ExternalIl2CppMapGenerator.hpp"
#include "ExternalMethodResolver.hpp"
#include "ExternalMonoMetadataGenerator.hpp"
#include "ExternalProcess.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Aegis::UnityExternal;

namespace
{
    struct TargetSelection
    {
        std::optional<DWORD> pid;
        std::optional<std::wstring> executable;
    };

    struct SessionConfig
    {
        std::wstring lastTarget;
        std::wstring lastMethodMap;
    };

    struct KnownMethod
    {
        const wchar_t* label = L"";
        const char* imageName = "";
        const char* className = "";
        const char* methodName = "";
        int argumentCount = -1;
    };

    enum class SessionAction
    {
        Exit,
        ChangeTarget
    };

    bool gInputClosed = false;

    bool IsFlag(const wchar_t* value)
    {
        return value && value[0] == L'-';
    }

    std::wstring Trim(std::wstring value)
    {
        const auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) {
            return std::iswspace(ch) != 0;
        });
        const auto last = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) {
            return std::iswspace(ch) != 0;
        }).base();

        if (first >= last)
        {
            return {};
        }

        return std::wstring(first, last);
    }

    bool HasFlag(int argc, wchar_t* argv[], const wchar_t* flag)
    {
        for (int index = 1; index < argc; ++index)
        {
            if (_wcsicmp(argv[index], flag) == 0)
            {
                return true;
            }
        }

        return false;
    }

    std::optional<std::wstring> GetOptionValue(int argc, wchar_t* argv[], const wchar_t* flag)
    {
        for (int index = 1; index + 1 < argc; ++index)
        {
            if (_wcsicmp(argv[index], flag) == 0 && !IsFlag(argv[index + 1]))
            {
                return argv[index + 1];
            }
        }

        return std::nullopt;
    }

    bool TryParsePidText(const std::wstring& text, DWORD* pid)
    {
        if (!pid || text.empty())
        {
            return false;
        }

        const bool allDigits = std::all_of(text.begin(), text.end(), [](wchar_t ch) {
            return std::iswdigit(ch) != 0;
        });
        if (!allDigits)
        {
            return false;
        }

        wchar_t* end = nullptr;
        const unsigned long value = std::wcstoul(text.c_str(), &end, 10);
        if (!end || *end != L'\0' || value == 0)
        {
            return false;
        }

        *pid = static_cast<DWORD>(value);
        return true;
    }

    std::optional<std::uint64_t> ParseUnsignedInteger(const std::wstring& text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        wchar_t* end = nullptr;
        const unsigned long long value = std::wcstoull(text.c_str(), &end, 0);
        if (!end || *end != L'\0')
        {
            return std::nullopt;
        }

        return static_cast<std::uint64_t>(value);
    }

    std::optional<DWORD> ParsePid(int argc, wchar_t* argv[])
    {
        const std::optional<std::wstring> text = GetOptionValue(argc, argv, L"--pid");
        if (!text)
        {
            return std::nullopt;
        }

        DWORD pid = 0;
        return TryParsePidText(Trim(*text), &pid) ? std::optional<DWORD>(pid) : std::nullopt;
    }

    std::optional<std::wstring> ParseExecutableName(int argc, wchar_t* argv[])
    {
        if (const std::optional<std::wstring> executable = GetOptionValue(argc, argv, L"--exe"))
        {
            return Trim(*executable);
        }

        return std::nullopt;
    }

    TargetSelection ParseTargetSelection(int argc, wchar_t* argv[])
    {
        TargetSelection selection;
        selection.pid = ParsePid(argc, argv);
        selection.executable = ParseExecutableName(argc, argv);
        return selection;
    }

    std::filesystem::path ExternalExecutableDirectory()
    {
        std::wstring path(MAX_PATH, L'\0');
        DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        while (size == path.size())
        {
            path.resize(path.size() * 2);
            size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        }

        if (size == 0)
        {
            return std::filesystem::current_path();
        }

        path.resize(size);
        return std::filesystem::path(path).parent_path();
    }

    std::filesystem::path ConfigPath()
    {
        return ExternalExecutableDirectory() / L"AegisUnityUniversalExternal.ini";
    }

    std::wstring ConfigValue(const std::wstring& line, const wchar_t* key)
    {
        const std::wstring prefix = std::wstring(key) + L"=";
        if (line.rfind(prefix, 0) != 0)
        {
            return {};
        }

        return Trim(line.substr(prefix.size()));
    }

    SessionConfig LoadSessionConfig()
    {
        SessionConfig config;
        std::wifstream file{ ConfigPath() };
        if (!file)
        {
            return config;
        }

        std::wstring line;
        while (std::getline(file, line))
        {
            if (std::wstring value = ConfigValue(Trim(line), L"last_target"); !value.empty())
            {
                config.lastTarget = value;
            }
            else if (value = ConfigValue(Trim(line), L"last_method_map"); !value.empty())
            {
                config.lastMethodMap = value;
            }
        }

        return config;
    }

    void SaveSessionConfig(const SessionConfig& config)
    {
        std::wofstream file{ ConfigPath() };
        if (!file)
        {
            return;
        }

        file << L"last_target=" << config.lastTarget << L'\n';
        file << L"last_method_map=" << config.lastMethodMap << L'\n';
    }

    std::wstring ReadLine(const std::wstring& prompt)
    {
        std::wcout << prompt;
        std::wstring input;
        if (!std::getline(std::wcin, input))
        {
            gInputClosed = true;
            return {};
        }

        return Trim(std::move(input));
    }

    TargetSelection ReadTargetFromConsole(const SessionConfig* config = nullptr)
    {
        std::wstring prompt = L"Target exe or pid";
        if (config && !config->lastTarget.empty())
        {
            prompt += L" [" + config->lastTarget + L"]";
        }
        prompt += L": ";

        std::wstring input = ReadLine(prompt);
        if (input.empty() && config && !config->lastTarget.empty())
        {
            input = config->lastTarget;
        }

        TargetSelection selection;
        DWORD pid = 0;
        if (TryParsePidText(input, &pid))
        {
            selection.pid = pid;
        }
        else if (!input.empty())
        {
            selection.executable = input;
        }

        return selection;
    }

    std::string NarrowUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
        {
            return {};
        }

        std::string result(static_cast<std::size_t>(size - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring WidenUtf8(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 1)
        {
            return {};
        }

        std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
        return result;
    }

    std::optional<MethodQuery> ParseMethodQuery(int argc, wchar_t* argv[])
    {
        for (int index = 1; index + 2 < argc; ++index)
        {
            if (_wcsicmp(argv[index], L"--resolve") != 0)
            {
                continue;
            }

            MethodQuery query;
            query.className = NarrowUtf8(argv[index + 1]);
            query.methodName = NarrowUtf8(argv[index + 2]);

            const int argsIndex = index + 3;
            if (argsIndex < argc && !IsFlag(argv[argsIndex]))
            {
                wchar_t* end = nullptr;
                const long value = std::wcstol(argv[argsIndex], &end, 10);
                if (end && *end == L'\0')
                {
                    query.argumentCount = static_cast<int>(value);
                }
            }

            if (const std::optional<std::wstring> imageName = GetOptionValue(argc, argv, L"--image"))
            {
                query.imageName = NarrowUtf8(*imageName);
            }

            return query;
        }

        return std::nullopt;
    }

    std::wstring ModuleSummary(const std::optional<ModuleInfo>& module)
    {
        if (!module)
        {
            return L"no";
        }

        std::wstringstream stream;
        stream << L"yes @ 0x"
            << std::hex << std::uppercase << module->base
            << std::dec << L" (" << module->size << L" bytes)";
        return stream.str();
    }

    void PrintProcess(const UnityProcess& process)
    {
        std::wcout << L"[pid " << process.pid << L"] "
            << process.executable
            << L" | runtime: " << RuntimeBackendName(process.modules.Backend())
            << L" | UnityPlayer: " << ModuleSummary(process.modules.unityPlayer)
            << L" | GameAssembly: " << ModuleSummary(process.modules.gameAssembly)
            << L" | Mono: " << ModuleSummary(process.modules.mono)
            << L'\n';
    }

    void PrintUsage()
    {
        std::wcout
            << L"Aegis Unity Universal External\n"
            << L"Usage:\n"
            << L"  AegisUnityUniversalExternal.exe\n"
            << L"  AegisUnityUniversalExternal.exe --gui\n"
            << L"  AegisUnityUniversalExternal.exe --console\n"
            << L"  AegisUnityUniversalExternal.exe --pid <process id>\n"
            << L"  AegisUnityUniversalExternal.exe --exe <game.exe>\n"
            << L"  AegisUnityUniversalExternal.exe --watch\n"
            << L"  AegisUnityUniversalExternal.exe --exe <game.exe> --api il2cpp_class_get_method_from_name\n"
            << L"  AegisUnityUniversalExternal.exe --exe <game.exe> --map methods.txt --resolve UnityEngine.Time get_timeScale 0\n"
            << L"  AegisUnityUniversalExternal.exe --exe <game.exe> --object-cache UnityEngine.Rigidbody\n\n"
            << L"No-args launch opens the external GUI. Use --console for the interactive console flow.\n\n"
            << L"Method map entries can use any of these forms:\n"
            << L"  image|Namespace.Type|Method|argc|0xRVA\n"
            << L"  Namespace.Type|Method|argc|0xRVA\n"
            << L"  Namespace.Type::Method|argc|0xRVA\n";
    }

    int PrintSnapshot(const TargetSelection& target)
    {
        const std::vector<UnityProcess> processes = EnumerateUnityProcesses(target.pid, target.executable);
        if (processes.empty())
        {
            if (target.pid)
            {
                std::wcout << L"No process was found for pid " << *target.pid << L".\n";
            }
            else if (target.executable)
            {
                std::wcout << L"No process was found for exe " << *target.executable << L".\n";
            }
            else
            {
                std::wcout << L"No Unity processes were found.\n";
            }
            return 1;
        }

        for (const UnityProcess& process : processes)
        {
            PrintProcess(process);
        }

        return 0;
    }

    std::optional<UnityProcess> SelectProcess(const TargetSelection& target, bool allowPromptForPid)
    {
        const std::vector<UnityProcess> processes = EnumerateUnityProcesses(target.pid, target.executable);
        if (processes.empty())
        {
            if (target.executable)
            {
                std::wcout << L"No process was found for exe " << *target.executable << L".\n";
            }
            else if (target.pid)
            {
                std::wcout << L"No process was found for pid " << *target.pid << L".\n";
            }
            return std::nullopt;
        }

        if (!target.pid && processes.size() > 1)
        {
            std::wcout << L"Multiple matching processes were found:\n";
            for (const UnityProcess& process : processes)
            {
                PrintProcess(process);
            }

            if (!allowPromptForPid)
            {
                std::wcout << L"Pass --pid to choose one.\n";
                return std::nullopt;
            }

            while (true)
            {
                const std::wstring pidText = ReadLine(L"Choose pid: ");
                if (gInputClosed)
                {
                    return std::nullopt;
                }

                DWORD selectedPid = 0;
                if (!TryParsePidText(pidText, &selectedPid))
                {
                    std::wcout << L"Please type a numeric pid.\n";
                    continue;
                }

                const auto found = std::find_if(processes.begin(), processes.end(), [selectedPid](const UnityProcess& process) {
                    return process.pid == selectedPid;
                });
                if (found != processes.end())
                {
                    return *found;
                }

                std::wcout << L"That pid was not in the matching list.\n";
            }
        }

        return processes.front();
    }

    bool IsYes(const std::wstring& value, bool defaultValue)
    {
        if (value.empty())
        {
            return defaultValue;
        }

        return _wcsicmp(value.c_str(), L"y") == 0 ||
            _wcsicmp(value.c_str(), L"yes") == 0 ||
            _wcsicmp(value.c_str(), L"1") == 0 ||
            _wcsicmp(value.c_str(), L"true") == 0;
    }

    std::optional<UnityProcess> WaitForProcess(const TargetSelection& target, DWORD timeoutSeconds)
    {
        if (!target.pid && !target.executable)
        {
            return std::nullopt;
        }

        const ULONGLONG start = GetTickCount64();
        const ULONGLONG timeoutMs = static_cast<ULONGLONG>(timeoutSeconds) * 1000ULL;
        std::wcout << L"Waiting for target";
        if (target.executable)
        {
            std::wcout << L" " << *target.executable;
        }
        else if (target.pid)
        {
            std::wcout << L" pid " << *target.pid;
        }
        std::wcout << L" ...\n";

        while (!gInputClosed)
        {
            const std::vector<UnityProcess> processes = EnumerateUnityProcesses(target.pid, target.executable);
            if (!processes.empty())
            {
                if (processes.size() == 1 || target.pid)
                {
                    return processes.front();
                }

                std::wcout << L"Multiple matching processes were found:\n";
                for (const UnityProcess& process : processes)
                {
                    PrintProcess(process);
                }
                return SelectProcess(target, true);
            }

            if (GetTickCount64() - start >= timeoutMs)
            {
                break;
            }

            Sleep(1000);
        }

        std::wcout << L"Target wait timed out.\n";
        return std::nullopt;
    }

    std::optional<UnityProcess> WaitForRuntimeModules(UnityProcess process, DWORD timeoutSeconds)
    {
        if (process.modules.Backend() != RuntimeBackend::Unknown || process.pid == 0)
        {
            return process;
        }

        const std::wstring answer = ReadLine(L"Runtime modules are not loaded yet. Wait for Unity modules? [Y/n]: ");
        if (!IsYes(answer, true))
        {
            return process;
        }

        const ULONGLONG start = GetTickCount64();
        const ULONGLONG timeoutMs = static_cast<ULONGLONG>(timeoutSeconds) * 1000ULL;
        while (!gInputClosed)
        {
            process.modules = InspectRuntimeModules(process.pid);
            if (process.modules.Backend() != RuntimeBackend::Unknown || process.modules.HasUnityPlayer())
            {
                std::wcout << L"Unity runtime modules detected.\n";
                return process;
            }

            if (GetTickCount64() - start >= timeoutMs)
            {
                break;
            }

            std::wcout << L".";
            Sleep(1000);
        }

        std::wcout << L"\nRuntime wait timed out; continuing with current snapshot.\n";
        return process;
    }

    std::optional<UnityProcess> SelectProcessForResolver(const TargetSelection& target)
    {
        return SelectProcess(target, false);
    }

    std::optional<UnityProcess> SelectProcessInteractive(const SessionConfig& config)
    {
        while (true)
        {
            TargetSelection target = ReadTargetFromConsole(&config);
            if (!target.pid && !target.executable)
            {
                if (gInputClosed)
                {
                    return std::nullopt;
                }

                std::wcout << L"Type a process id or executable name, for example MyUnityGame.exe.\n";
                continue;
            }

            std::optional<UnityProcess> process = SelectProcess(target, true);
            if (process)
            {
                return WaitForRuntimeModules(*process, 60);
            }

            if (target.executable)
            {
                const std::wstring answer = ReadLine(L"Target is not running. Wait for it? [Y/n]: ");
                if (gInputClosed)
                {
                    return std::nullopt;
                }

                if (IsYes(answer, true))
                {
                    if (std::optional<UnityProcess> waited = WaitForProcess(target, 120))
                    {
                        return WaitForRuntimeModules(*waited, 60);
                    }
                }
            }
        }
    }

    void PrintResolvedAddress(const std::wstring& label, const ResolvedAddress& resolved)
    {
        std::wcout << label << L'\n'
            << L"  module : " << resolved.moduleName << L'\n'
            << L"  source : " << WidenUtf8(resolved.source) << L'\n';
        if (resolved.hasMetadataToken)
        {
            std::wcout << L"  token  : 0x" << std::hex << std::uppercase << resolved.metadataToken << std::dec << L'\n';
        }
        if (resolved.hasRva)
        {
            std::wcout << L"  rva    : 0x" << std::hex << std::uppercase << resolved.rva << std::dec << L'\n';
        }
        if (resolved.hasAddress)
        {
            std::wcout << L"  address: 0x" << std::hex << std::uppercase << resolved.address << std::dec << L'\n';
        }
        else
        {
            std::wcout << L"  address: metadata-only; no target-process call was made\n";
        }
        if (!resolved.detail.empty())
        {
            std::wcout << L"  detail : " << WidenUtf8(resolved.detail) << L'\n';
        }
    }

    int PrintResolveResult(const std::wstring& label, const ResolveResult& result)
    {
        if (result.value)
        {
            PrintResolvedAddress(label, *result.value);
            return 0;
        }

        std::wcout << L"Resolve failed: "
            << (result.error ? WidenUtf8(result.error->message) : L"unknown error")
            << L'\n';
        return 1;
    }

    void AddUniquePath(std::vector<std::filesystem::path>* paths, const std::filesystem::path& path)
    {
        if (!paths || path.empty())
        {
            return;
        }

        const std::wstring normalized = ToLower(path.lexically_normal().wstring());
        const auto found = std::find_if(paths->begin(), paths->end(), [&normalized](const std::filesystem::path& candidate) {
            return ToLower(candidate.lexically_normal().wstring()) == normalized;
        });

        if (found == paths->end())
        {
            paths->push_back(path);
        }
    }

    bool FileExists(const std::filesystem::path& path)
    {
        std::error_code error;
        return std::filesystem::exists(path, error) && std::filesystem::is_regular_file(path, error);
    }

    void AddParentDirectory(std::vector<std::filesystem::path>* directories, const std::wstring& path)
    {
        if (path.empty())
        {
            return;
        }

        std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
        {
            AddUniquePath(directories, parent);
        }
    }

    std::vector<std::filesystem::path> CandidateMethodMapDirectories(const UnityProcess& process)
    {
        std::vector<std::filesystem::path> directories;
        AddUniquePath(&directories, std::filesystem::current_path());
        AddUniquePath(&directories, ExternalExecutableDirectory());
        AddParentDirectory(&directories, process.executablePath);

        if (process.modules.unityPlayer)
        {
            AddParentDirectory(&directories, process.modules.unityPlayer->path);
        }
        if (process.modules.gameAssembly)
        {
            AddParentDirectory(&directories, process.modules.gameAssembly->path);
        }
        if (process.modules.mono)
        {
            AddParentDirectory(&directories, process.modules.mono->path);
        }

        return directories;
    }

    std::vector<std::wstring> CandidateMethodMapNames()
    {
        return {
            L"methods.txt",
            L"method_map.txt",
            L"il2cpp_methods.txt",
            L"aegis_methods.txt",
            L"aegis_unity_methods.txt",
            L"script.json",
            L"dump.cs"
        };
    }

    std::vector<std::filesystem::path> CandidateMethodMaps(const UnityProcess& process, const SessionConfig& config)
    {
        std::vector<std::filesystem::path> candidates;

        if (!config.lastMethodMap.empty())
        {
            AddUniquePath(&candidates, config.lastMethodMap);
        }

        const std::vector<std::filesystem::path> directories = CandidateMethodMapDirectories(process);
        const std::vector<std::wstring> names = CandidateMethodMapNames();
        for (const std::filesystem::path& directory : directories)
        {
            for (const std::wstring& name : names)
            {
                AddUniquePath(&candidates, directory / name);
            }
        }

        return candidates;
    }

    std::vector<std::string> StandardExports(RuntimeBackend backend)
    {
        if (backend == RuntimeBackend::IL2CPP)
        {
            return {
                "il2cpp_init",
                "il2cpp_domain_get",
                "il2cpp_domain_get_assemblies",
                "il2cpp_class_from_name",
                "il2cpp_class_get_method_from_name",
                "il2cpp_class_get_methods",
                "il2cpp_class_get_fields",
                "il2cpp_class_get_field_from_name",
                "il2cpp_class_get_property_from_name",
                "il2cpp_image_get_class_count",
                "il2cpp_image_get_class",
                "il2cpp_resolve_icall",
                "il2cpp_string_new",
                "il2cpp_thread_attach",
                "il2cpp_thread_detach",
                "il2cpp_type_get_object"
            };
        }

        if (backend == RuntimeBackend::Mono)
        {
            return {
                "mono_get_root_domain",
                "mono_thread_attach",
                "mono_thread_detach",
                "mono_assembly_foreach",
                "mono_assembly_get_image",
                "mono_image_get_name",
                "mono_image_get_filename",
                "mono_class_from_name",
                "mono_class_get_method_from_name",
                "mono_method_desc_new",
                "mono_method_desc_search_in_image",
                "mono_compile_method",
                "mono_runtime_invoke"
            };
        }

        return {};
    }

    std::vector<KnownMethod> KnownUnityMethods(RuntimeBackend backend)
    {
        if (backend == RuntimeBackend::Unknown)
        {
            return {};
        }

        return {
            { L"UnityEngine.Shader::Find", "UnityEngine.CoreModule", "UnityEngine.Shader", "Find", 1 },
            { L"UnityEngine.Time::get_timeScale", "UnityEngine.CoreModule", "UnityEngine.Time", "get_timeScale", 0 },
            { L"UnityEngine.Time::set_timeScale", "UnityEngine.CoreModule", "UnityEngine.Time", "set_timeScale", 1 },
            { L"UnityEngine.Camera::get_main", "UnityEngine.CoreModule", "UnityEngine.Camera", "get_main", 0 },
            { L"UnityEngine.Camera::set_fieldOfView", "UnityEngine.CoreModule", "UnityEngine.Camera", "set_fieldOfView", 1 },
            { L"UnityEngine.Camera::WorldToScreenPoint", "UnityEngine.CoreModule", "UnityEngine.Camera", "WorldToScreenPoint", 2 },
            { L"UnityEngine.GameObject::Find", "UnityEngine.CoreModule", "UnityEngine.GameObject", "Find", 1 },
            { L"UnityEngine.GameObject::GetComponent", "UnityEngine.CoreModule", "UnityEngine.GameObject", "GetComponent", 1 },
            { L"UnityEngine.Component::get_transform", "UnityEngine.CoreModule", "UnityEngine.Component", "get_transform", 0 },
            { L"UnityEngine.Transform::get_position", "UnityEngine.CoreModule", "UnityEngine.Transform", "get_position", 0 }
        };
    }

    void PrintStandardExportResolution(const ExternalMethodResolver& resolver)
    {
        const std::vector<std::string> exports = StandardExports(resolver.Backend());
        if (exports.empty())
        {
            std::wcout << L"No standard runtime export set for this backend.\n";
            return;
        }

        std::wcout << L"\nRuntime exports:\n";
        for (const std::string& exportName : exports)
        {
            const ResolveResult result = resolver.ResolveRuntimeExport(exportName);
            std::wcout << L"  " << WidenUtf8(exportName) << L" -> ";
            if (result.value)
            {
                if (result.value->hasAddress)
                {
                    std::wcout << L"0x" << std::hex << std::uppercase << result.value->address << std::dec;
                }
                else if (result.value->hasMetadataToken)
                {
                    std::wcout << L"token 0x" << std::hex << std::uppercase << result.value->metadataToken << std::dec;
                }
                else
                {
                    std::wcout << L"metadata-only";
                }
            }
            else
            {
                std::wcout << L"missing";
            }
            std::wcout << L'\n';
        }
    }

    void PrintKnownMethodResolution(const ExternalMethodResolver& resolver)
    {
        const std::vector<KnownMethod> methods = KnownUnityMethods(resolver.Backend());
        if (methods.empty())
        {
            std::wcout << L"No known managed method preset set for this backend.\n";
            return;
        }

        if (resolver.Backend() == RuntimeBackend::IL2CPP && !resolver.HasMethodMap())
        {
            std::wcout
                << L"\nKnown Unity method presets skipped: no method map/dump is loaded.\n"
                << L"Runtime exports can still resolve directly from GameAssembly.dll.\n";
            return;
        }

        if (resolver.Backend() == RuntimeBackend::Mono && !resolver.HasMethodMap())
        {
            std::wcout
                << L"\nKnown Unity method presets skipped: no Mono metadata map is loaded.\n"
                << L"Runtime exports can still resolve directly from the Mono module.\n";
            return;
        }

        std::wcout << L"\nKnown Unity method map resolutions:\n";
        for (const KnownMethod& method : methods)
        {
            MethodQuery query;
            query.imageName = method.imageName;
            query.className = method.className;
            query.methodName = method.methodName;
            query.argumentCount = method.argumentCount;

            const ResolveResult result = resolver.ResolveMethod(query);
            std::wcout << L"  " << method.label << L" -> ";
            if (result.value)
            {
                if (result.value->hasAddress)
                {
                    std::wcout << L"0x" << std::hex << std::uppercase << result.value->address << std::dec;
                }
                else if (result.value->hasMetadataToken)
                {
                    std::wcout << L"token 0x" << std::hex << std::uppercase << result.value->metadataToken << std::dec;
                }
                else
                {
                    std::wcout << L"metadata-only";
                }
            }
            else
            {
                std::wcout << L"missing";
            }
            std::wcout << L'\n';
        }
    }

    bool LoadMethodMapFromPath(
        const std::wstring& path,
        ExternalMethodResolver* resolver,
        std::optional<MethodMap>* loadedMap,
        SessionConfig* config = nullptr)
    {
        if (!resolver || !loadedMap || path.empty())
        {
            return false;
        }

        MethodMap map;
        std::string errorMessage;
        if (!map.Load(path, &errorMessage))
        {
            std::wcout << L"Method map load failed: " << WidenUtf8(errorMessage) << L'\n';
            return false;
        }

        *loadedMap = map;
        const std::size_t entryCount = loadedMap->has_value() ? loadedMap->value().Count() : 0;
        resolver->SetMethodMap(std::move(map));
        if (config)
        {
            config->lastMethodMap = path;
            SaveSessionConfig(*config);
        }

        std::wcout << L"Method map loaded: " << path << L" (" << entryCount << L" entries)\n";
        PrintKnownMethodResolution(*resolver);
        return true;
    }

    bool GenerateMethodMapFromTarget(
        const UnityProcess& process,
        ExternalMethodResolver* resolver,
        std::optional<MethodMap>* loadedMap)
    {
        if (!resolver || !loadedMap)
        {
            return false;
        }

        if (process.modules.Backend() == RuntimeBackend::IL2CPP)
        {
            std::wcout << L"\nNo compatible method map file found; attempting IL2CPP metadata auto-generation...\n";
            GeneratedIl2CppMethodMap generated = GenerateIl2CppMethodMap(process);
            if (!generated.success)
            {
                std::wcout << L"IL2CPP metadata auto-generation failed: " << WidenUtf8(generated.message) << L'\n';
                return false;
            }

            *loadedMap = generated.methodMap;
            resolver->SetMethodMap(generated.methodMap);
            std::wcout
                << L"Generated IL2CPP method map: " << generated.resolvedMethods << L" entries, "
                << generated.matchedModules << L"/" << generated.imageCount << L" modules matched\n"
                << L"Metadata: " << generated.metadataPath.wstring() << L'\n';
            PrintKnownMethodResolution(*resolver);
            return true;
        }

        if (process.modules.Backend() == RuntimeBackend::Mono)
        {
            std::wcout << L"\nNo compatible method map file found; attempting Mono assembly metadata generation...\n";
            GeneratedMonoMethodMap generated = GenerateMonoMethodMap(process);
            if (!generated.success)
            {
                std::wcout << L"Mono metadata generation failed: " << WidenUtf8(generated.message) << L'\n';
                return false;
            }

            *loadedMap = generated.methodMap;
            resolver->SetMethodMap(generated.methodMap);
            std::wcout
                << L"Generated Mono metadata map: " << generated.methodCount << L" methods, "
                << generated.typeCount << L" types, " << generated.assemblyCount << L" assemblies\n"
                << L"Managed folder: " << generated.managedDirectory.wstring() << L'\n';
            PrintKnownMethodResolution(*resolver);
            return true;
        }

        return false;
    }

    bool AutoLoadMethodMap(
        const UnityProcess& process,
        ExternalMethodResolver* resolver,
        std::optional<MethodMap>* loadedMap,
        SessionConfig* config)
    {
        if (!resolver || !loadedMap || !config)
        {
            return false;
        }

        for (const std::filesystem::path& candidate : CandidateMethodMaps(process, *config))
        {
            if (!FileExists(candidate))
            {
                continue;
            }

            std::wcout << L"\nFound method map candidate: " << candidate.wstring() << L'\n';
            if (LoadMethodMapFromPath(candidate.wstring(), resolver, loadedMap, config))
            {
                return true;
            }
        }

        if (GenerateMethodMapFromTarget(process, resolver, loadedMap))
        {
            return true;
        }

        return false;
    }

    void PromptForOptionalMethodMap(
        ExternalMethodResolver* resolver,
        std::optional<MethodMap>* loadedMap,
        SessionConfig* config = nullptr)
    {
        const std::wstring path = ReadLine(L"\nMethod map path (optional, Enter to skip): ");
        if (!path.empty() &&
            _wcsicmp(path.c_str(), L"0") != 0 &&
            _wcsicmp(path.c_str(), L"n") != 0 &&
            _wcsicmp(path.c_str(), L"no") != 0 &&
            _wcsicmp(path.c_str(), L"skip") != 0)
        {
            LoadMethodMapFromPath(path, resolver, loadedMap, config);
        }
    }

    void ResolveRuntimeExportInteractive(const ExternalMethodResolver& resolver)
    {
        const std::wstring exportName = ReadLine(L"Export name: ");
        if (exportName.empty())
        {
            return;
        }

        PrintResolveResult(
            L"Resolved runtime export " + exportName,
            resolver.ResolveRuntimeExport(NarrowUtf8(exportName)));
    }

    void ResolveManagedMethodInteractive(const ExternalMethodResolver& resolver)
    {
        MethodQuery query;
        const std::wstring imageName = ReadLine(L"Image/module name (optional): ");
        const std::wstring className = ReadLine(L"Class name (Namespace.Type): ");
        const std::wstring methodName = ReadLine(L"Method name: ");
        const std::wstring argumentCount = ReadLine(L"Argument count (* or blank for any): ");

        query.imageName = NarrowUtf8(imageName);
        query.className = NarrowUtf8(className);
        query.methodName = NarrowUtf8(methodName);

        if (!argumentCount.empty() && argumentCount != L"*")
        {
            const std::optional<std::uint64_t> parsed = ParseUnsignedInteger(argumentCount);
            if (!parsed || *parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            {
                std::wcout << L"Invalid argument count.\n";
                return;
            }
            query.argumentCount = static_cast<int>(*parsed);
        }

        std::wstring label = L"Resolved method " + className + L"::" + methodName;
        PrintResolveResult(label, resolver.ResolveMethod(query));
    }

    void PrintHexDump(uintptr_t address, const std::vector<std::uint8_t>& bytes)
    {
        constexpr std::size_t bytesPerLine = 16;
        for (std::size_t offset = 0; offset < bytes.size(); offset += bytesPerLine)
        {
            std::wcout << L"  0x"
                << std::hex << std::uppercase << (address + offset)
                << L": ";

            for (std::size_t index = 0; index < bytesPerLine; ++index)
            {
                if (offset + index < bytes.size())
                {
                    std::wcout << std::setw(2) << std::setfill(L'0')
                        << static_cast<unsigned int>(bytes[offset + index])
                        << L' ';
                }
                else
                {
                    std::wcout << L"   ";
                }
            }

            std::wcout << L" ";
            for (std::size_t index = 0; index < bytesPerLine && offset + index < bytes.size(); ++index)
            {
                const wchar_t ch = static_cast<wchar_t>(bytes[offset + index]);
                std::wcout << (std::iswprint(ch) ? ch : L'.');
            }

            std::wcout << std::dec << std::setfill(L' ') << L'\n';
        }
    }

    std::wstring FormatHex(std::uint64_t value)
    {
        std::wstringstream stream;
        stream << L"0x" << std::hex << std::uppercase << value;
        return stream.str();
    }

    std::optional<std::size_t> ReadBoundedSize(
        const std::wstring& prompt,
        std::size_t defaultValue,
        std::size_t maxValue)
    {
        const std::wstring text = ReadLine(prompt);
        if (text.empty())
        {
            return defaultValue;
        }

        const std::optional<std::uint64_t> parsed = ParseUnsignedInteger(text);
        if (!parsed || *parsed == 0)
        {
            std::wcout << L"Invalid size.\n";
            return std::nullopt;
        }

        return static_cast<std::size_t>(std::min<std::uint64_t>(*parsed, maxValue));
    }

    void PrintReadFailure(const ExternalMemoryReader& reader)
    {
        std::wcout << L"Read failed. GetLastError=" << reader.LastErrorCode() << L'\n';
    }

    template <typename T>
    void ReadIntegerValue(const ExternalMemoryReader& reader, uintptr_t address, const wchar_t* typeName)
    {
        const std::optional<T> value = reader.Read<T>(address);
        if (!value)
        {
            PrintReadFailure(reader);
            return;
        }

        using UnsignedT = std::make_unsigned_t<T>;
        const auto raw = static_cast<unsigned long long>(static_cast<UnsignedT>(*value));

        std::wstringstream stream;
        stream << typeName << L" @ " << FormatHex(address) << L" = ";
        if constexpr (std::is_signed_v<T>)
        {
            stream << static_cast<long long>(*value);
        }
        else
        {
            stream << static_cast<unsigned long long>(*value);
        }
        stream << L" (" << FormatHex(raw) << L")";
        std::wcout << stream.str() << L'\n';
    }

    template <typename T>
    void ReadFloatingValue(const ExternalMemoryReader& reader, uintptr_t address, const wchar_t* typeName)
    {
        const std::optional<T> value = reader.Read<T>(address);
        if (!value)
        {
            PrintReadFailure(reader);
            return;
        }

        std::wstringstream stream;
        stream << typeName << L" @ " << FormatHex(address) << L" = "
            << std::setprecision(std::numeric_limits<T>::max_digits10)
            << *value;
        std::wcout << stream.str() << L'\n';
    }

    void ReadPointerValue(const ExternalMemoryReader& reader, uintptr_t address)
    {
        const std::optional<uintptr_t> value = reader.Read<uintptr_t>(address);
        if (!value)
        {
            PrintReadFailure(reader);
            return;
        }

        std::wcout << L"ptr @ " << FormatHex(address) << L" = " << FormatHex(*value) << L'\n';
    }

    void ReadAsciiStringValue(const ExternalMemoryReader& reader, uintptr_t address)
    {
        const std::optional<std::size_t> maxLength =
            ReadBoundedSize(L"Max characters (default 128, max 4096): ", 128, 4096);
        if (!maxLength)
        {
            return;
        }

        const std::vector<std::uint8_t> bytes = reader.ReadBytes(address, *maxLength);
        if (bytes.empty())
        {
            PrintReadFailure(reader);
            return;
        }

        std::wstring text;
        text.reserve(bytes.size());
        for (std::uint8_t byte : bytes)
        {
            if (byte == 0)
            {
                break;
            }

            text.push_back(byte >= 0x20 && byte < 0x7F ? static_cast<wchar_t>(byte) : L'.');
        }

        std::wcout << L"ascii @ " << FormatHex(address) << L" = \"" << text << L"\"\n";
        if (bytes.size() != *maxLength)
        {
            std::wcout << L"Partial read: " << bytes.size() << L" of " << *maxLength << L" bytes.\n";
        }
    }

    void ReadUtf16StringValue(const ExternalMemoryReader& reader, uintptr_t address)
    {
        const std::optional<std::size_t> maxChars =
            ReadBoundedSize(L"Max characters (default 128, max 4096): ", 128, 4096);
        if (!maxChars)
        {
            return;
        }

        const std::size_t byteCount = *maxChars * sizeof(std::uint16_t);
        const std::vector<std::uint8_t> bytes = reader.ReadBytes(address, byteCount);
        if (bytes.empty())
        {
            PrintReadFailure(reader);
            return;
        }

        std::wstring text;
        text.reserve(bytes.size() / sizeof(std::uint16_t));
        for (std::size_t offset = 0; offset + 1 < bytes.size(); offset += sizeof(std::uint16_t))
        {
            const std::uint16_t codeUnit =
                static_cast<std::uint16_t>(bytes[offset])
                | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
            if (codeUnit == 0)
            {
                break;
            }

            const bool printable = codeUnit >= 0x20 && (codeUnit < 0xD800 || codeUnit > 0xDFFF);
            text.push_back(printable ? static_cast<wchar_t>(codeUnit) : L'.');
        }

        std::wcout << L"utf16 @ " << FormatHex(address) << L" = \"" << text << L"\"\n";
        if (bytes.size() != byteCount)
        {
            std::wcout << L"Partial read: " << bytes.size() << L" of " << byteCount << L" bytes.\n";
        }
    }

    void ReadMemoryInteractive(const ExternalMemoryReader& reader)
    {
        if (!reader.IsOpen())
        {
            std::wcout << L"Process memory is not open for reading.\n";
            return;
        }

        const std::wstring addressText = ReadLine(L"Address (hex or decimal): ");
        const std::optional<std::uint64_t> address = ParseUnsignedInteger(addressText);
        if (!address)
        {
            std::wcout << L"Invalid address.\n";
            return;
        }

        const std::wstring sizeText = ReadLine(L"Bytes to read (default 64, max 4096): ");
        std::size_t size = 64;
        if (!sizeText.empty())
        {
            const std::optional<std::uint64_t> parsedSize = ParseUnsignedInteger(sizeText);
            if (!parsedSize || *parsedSize == 0)
            {
                std::wcout << L"Invalid size.\n";
                return;
            }

            size = static_cast<std::size_t>(std::min<std::uint64_t>(*parsedSize, 4096));
        }

        const std::vector<std::uint8_t> bytes = reader.ReadBytes(static_cast<uintptr_t>(*address), size);
        if (bytes.empty())
        {
            std::wcout << L"Read failed. GetLastError=" << reader.LastErrorCode() << L'\n';
            return;
        }

        PrintHexDump(static_cast<uintptr_t>(*address), bytes);
        if (bytes.size() != size)
        {
            std::wcout << L"Partial read: " << bytes.size() << L" of " << size << L" bytes.\n";
        }
    }

    void ReadTypedMemoryInteractive(const ExternalMemoryReader& reader)
    {
        if (!reader.IsOpen())
        {
            std::wcout << L"Process memory is not open for reading.\n";
            return;
        }

        const std::wstring addressText = ReadLine(L"Address (hex or decimal): ");
        const std::optional<std::uint64_t> address = ParseUnsignedInteger(addressText);
        if (!address)
        {
            std::wcout << L"Invalid address.\n";
            return;
        }

        std::wcout
            << L"Types: u8 i8 u16 i16 u32 i32 u64 i64 float double ptr ascii utf16\n";
        const std::wstring type = ReadLine(L"Type: ");
        if (type.empty())
        {
            return;
        }

        const uintptr_t targetAddress = static_cast<uintptr_t>(*address);
        if (_wcsicmp(type.c_str(), L"u8") == 0)
        {
            ReadIntegerValue<std::uint8_t>(reader, targetAddress, L"u8");
        }
        else if (_wcsicmp(type.c_str(), L"i8") == 0)
        {
            ReadIntegerValue<std::int8_t>(reader, targetAddress, L"i8");
        }
        else if (_wcsicmp(type.c_str(), L"u16") == 0)
        {
            ReadIntegerValue<std::uint16_t>(reader, targetAddress, L"u16");
        }
        else if (_wcsicmp(type.c_str(), L"i16") == 0)
        {
            ReadIntegerValue<std::int16_t>(reader, targetAddress, L"i16");
        }
        else if (_wcsicmp(type.c_str(), L"u32") == 0)
        {
            ReadIntegerValue<std::uint32_t>(reader, targetAddress, L"u32");
        }
        else if (_wcsicmp(type.c_str(), L"i32") == 0 || _wcsicmp(type.c_str(), L"int") == 0)
        {
            ReadIntegerValue<std::int32_t>(reader, targetAddress, L"i32");
        }
        else if (_wcsicmp(type.c_str(), L"u64") == 0)
        {
            ReadIntegerValue<std::uint64_t>(reader, targetAddress, L"u64");
        }
        else if (_wcsicmp(type.c_str(), L"i64") == 0)
        {
            ReadIntegerValue<std::int64_t>(reader, targetAddress, L"i64");
        }
        else if (_wcsicmp(type.c_str(), L"float") == 0 || _wcsicmp(type.c_str(), L"f32") == 0)
        {
            ReadFloatingValue<float>(reader, targetAddress, L"float");
        }
        else if (_wcsicmp(type.c_str(), L"double") == 0 || _wcsicmp(type.c_str(), L"f64") == 0)
        {
            ReadFloatingValue<double>(reader, targetAddress, L"double");
        }
        else if (_wcsicmp(type.c_str(), L"ptr") == 0 || _wcsicmp(type.c_str(), L"pointer") == 0)
        {
            ReadPointerValue(reader, targetAddress);
        }
        else if (_wcsicmp(type.c_str(), L"ascii") == 0 || _wcsicmp(type.c_str(), L"string") == 0)
        {
            ReadAsciiStringValue(reader, targetAddress);
        }
        else if (_wcsicmp(type.c_str(), L"utf16") == 0 || _wcsicmp(type.c_str(), L"wstring") == 0)
        {
            ReadUtf16StringValue(reader, targetAddress);
        }
        else
        {
            std::wcout << L"Unknown type.\n";
        }
    }

    void PrintSessionHeader(const UnityProcess& process, const ExternalMemoryReader& reader)
    {
        std::wcout << L"\nTarget: " << process.executable << L" [pid " << process.pid << L"]\n"
            << L"Runtime: " << RuntimeBackendName(process.modules.Backend()) << L'\n'
            << L"UnityPlayer: " << ModuleSummary(process.modules.unityPlayer) << L'\n'
            << L"GameAssembly: " << ModuleSummary(process.modules.gameAssembly) << L'\n'
            << L"Mono: " << ModuleSummary(process.modules.mono) << L'\n'
            << L"Read access: " << (reader.IsOpen() ? L"yes" : L"no") << L'\n';

        if (!process.executablePath.empty())
        {
            std::wcout << L"Process path: " << process.executablePath << L'\n';
        }

        if (!reader.IsOpen())
        {
            std::wcout << L"Warning: could not open target for read/query access. "
                << L"Try running as administrator if needed. GetLastError="
                << reader.LastErrorCode() << L'\n';
        }
    }

    void PrintMenu()
    {
        std::wcout
            << L"\nActions\n"
            << L"  1. Resolve runtime export\n"
            << L"  2. Load/reload method map\n"
            << L"  3. Resolve managed method from map\n"
            << L"  4. Read memory bytes\n"
            << L"  5. Read typed memory value\n"
            << L"  6. Show standard runtime exports\n"
            << L"  7. Resolve known Unity methods\n"
            << L"  8. Show session diagnostics\n"
            << L"  9. Refresh target/runtime modules\n"
            << L"  10. Change target\n"
            << L"  0. Exit\n";
    }

    void PrintSessionDiagnostics(
        const UnityProcess& process,
        const ExternalMemoryReader& reader,
        const std::optional<MethodMap>& loadedMap,
        const SessionConfig* config)
    {
        PrintSessionHeader(process, reader);
        std::wcout << L"Config path: " << ConfigPath().wstring() << L'\n';
        if (config)
        {
            std::wcout << L"Saved target: " << (config->lastTarget.empty() ? L"(none)" : config->lastTarget) << L'\n';
            std::wcout << L"Saved method map: " << (config->lastMethodMap.empty() ? L"(none)" : config->lastMethodMap) << L'\n';
        }
        std::wcout << L"Loaded method map entries: " << (loadedMap ? loadedMap->Count() : 0) << L'\n';
    }

    void RefreshTargetSnapshot(
        UnityProcess* process,
        ExternalMethodResolver* resolver,
        const std::optional<MethodMap>& loadedMap)
    {
        if (!process || !resolver)
        {
            return;
        }

        process->modules = InspectRuntimeModules(process->pid);
        *resolver = ExternalMethodResolver(process->modules);
        if (loadedMap)
        {
            resolver->SetMethodMap(*loadedMap);
        }

        std::wcout << L"Target snapshot refreshed.\n";
        ExternalMemoryReader probe;
        probe.Open(process->pid);
        PrintSessionHeader(*process, probe);
        PrintStandardExportResolution(*resolver);
    }

    SessionAction RunTargetSession(UnityProcess process, SessionConfig* config)
    {
        ExternalMemoryReader reader;
        reader.Open(process.pid);

        ExternalMethodResolver resolver(process.modules);
        std::optional<MethodMap> loadedMap;

        PrintSessionHeader(process, reader);
        PrintStandardExportResolution(resolver);
        if (!config || !AutoLoadMethodMap(process, &resolver, &loadedMap, config))
        {
            PromptForOptionalMethodMap(&resolver, &loadedMap, config);
        }

        while (true)
        {
            PrintMenu();
            const std::wstring choice = ReadLine(L"> ");
            if (gInputClosed)
            {
                return SessionAction::Exit;
            }

            if (choice == L"0" || _wcsicmp(choice.c_str(), L"exit") == 0 || _wcsicmp(choice.c_str(), L"quit") == 0)
            {
                return SessionAction::Exit;
            }

            if (choice == L"1")
            {
                ResolveRuntimeExportInteractive(resolver);
            }
            else if (choice == L"2")
            {
                const std::wstring path = ReadLine(L"Method map path: ");
                LoadMethodMapFromPath(path, &resolver, &loadedMap, config);
            }
            else if (choice == L"3")
            {
                ResolveManagedMethodInteractive(resolver);
            }
            else if (choice == L"4")
            {
                ReadMemoryInteractive(reader);
            }
            else if (choice == L"5")
            {
                ReadTypedMemoryInteractive(reader);
            }
            else if (choice == L"6")
            {
                PrintStandardExportResolution(resolver);
            }
            else if (choice == L"7")
            {
                PrintKnownMethodResolution(resolver);
            }
            else if (choice == L"8")
            {
                PrintSessionDiagnostics(process, reader, loadedMap, config);
            }
            else if (choice == L"9")
            {
                RefreshTargetSnapshot(&process, &resolver, loadedMap);
            }
            else if (choice == L"10")
            {
                return SessionAction::ChangeTarget;
            }
            else
            {
                std::wcout << L"Unknown action.\n";
            }
        }
    }

    int RunInteractiveConsole()
    {
        std::wcout
            << L"Aegis Unity Universal External\n"
            << L"Interactive external resolver\n\n";

        SessionConfig config = LoadSessionConfig();

        while (true)
        {
            std::optional<UnityProcess> process = SelectProcessInteractive(config);
            if (!process)
            {
                return 1;
            }

            config.lastTarget = process->executable;
            SaveSessionConfig(config);

            if (RunTargetSession(*process, &config) == SessionAction::Exit)
            {
                break;
            }
        }

        ReadLine(L"\nPress Enter to close...");
        return 0;
    }

    int RunArgumentMode(int argc, wchar_t* argv[])
    {
        const std::optional<std::wstring> apiExport = GetOptionValue(argc, argv, L"--api");
        const std::optional<MethodQuery> methodQuery = ParseMethodQuery(argc, argv);
        const std::optional<std::wstring> objectCacheComponent = GetOptionValue(argc, argv, L"--object-cache");
        const std::optional<std::wstring> objectCacheFallback = GetOptionValue(argc, argv, L"--fallback");
        TargetSelection target = ParseTargetSelection(argc, argv);

        if ((apiExport || methodQuery || objectCacheComponent) && !target.pid && !target.executable)
        {
            target = ReadTargetFromConsole();
            if (!target.pid && !target.executable)
            {
                std::wcout << L"No target was provided.\n";
                return 1;
            }
        }

        if (objectCacheComponent)
        {
            std::wstring diagnosticTarget;
            if (target.pid)
            {
                diagnosticTarget = std::to_wstring(*target.pid);
            }
            else if (target.executable)
            {
                diagnosticTarget = *target.executable;
            }

            if (diagnosticTarget.empty())
            {
                std::wcout << L"No target was provided.\n";
                return 1;
            }

            return RunExternalObjectCacheDiagnostic(
                diagnosticTarget,
                NarrowUtf8(*objectCacheComponent),
                objectCacheFallback ? NarrowUtf8(*objectCacheFallback) : std::string{});
        }

        if (HasFlag(argc, argv, L"--watch") && !apiExport && !methodQuery)
        {
            for (;;)
            {
                system("cls");
                PrintSnapshot(target);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }

        if (!apiExport && !methodQuery)
        {
            return PrintSnapshot(target);
        }

        const std::optional<UnityProcess> process = SelectProcessForResolver(target);
        if (!process)
        {
            return 1;
        }

        ExternalMemoryReader reader;
        reader.Open(process->pid);
        PrintSessionHeader(*process, reader);

        ExternalMethodResolver resolver(process->modules);
        std::optional<MethodMap> loadedMap;

        if (const std::optional<std::wstring> mapPath = GetOptionValue(argc, argv, L"--map"))
        {
            if (!LoadMethodMapFromPath(*mapPath, &resolver, &loadedMap))
            {
                return 1;
            }
        }
        else if (methodQuery)
        {
            GenerateMethodMapFromTarget(*process, &resolver, &loadedMap);
        }

        if (apiExport)
        {
            const std::string exportName = NarrowUtf8(*apiExport);
            return PrintResolveResult(
                L"Resolved runtime export " + *apiExport,
                resolver.ResolveRuntimeExport(exportName));
        }

        if (methodQuery)
        {
            std::wstring label = L"Resolved method "
                + WidenUtf8(methodQuery->className)
                + L"::"
                + WidenUtf8(methodQuery->methodName);
            return PrintResolveResult(label, resolver.ResolveMethod(*methodQuery));
        }

        return 0;
    }

    std::wstring InitialGuiTarget(int argc, wchar_t* argv[])
    {
        if (const std::optional<std::wstring> pid = GetOptionValue(argc, argv, L"--pid"))
        {
            return Trim(*pid);
        }

        if (const std::optional<std::wstring> executable = GetOptionValue(argc, argv, L"--exe"))
        {
            return Trim(*executable);
        }

        return {};
    }
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    if (HasFlag(argc, argv, L"--help") || HasFlag(argc, argv, L"-h") || HasFlag(argc, argv, L"/?"))
    {
        PrintUsage();
        return 0;
    }

    if (argc <= 1 || HasFlag(argc, argv, L"--gui"))
    {
        return RunExternalGui(InitialGuiTarget(argc, argv));
    }

    if (HasFlag(argc, argv, L"--console"))
    {
        return RunInteractiveConsole();
    }

    return RunArgumentMode(argc, argv);
}
