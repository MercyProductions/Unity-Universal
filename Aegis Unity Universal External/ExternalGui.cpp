#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ExternalGui.hpp"

#include "ExternalIl2CppMapGenerator.hpp"
#include "ExternalMemory.hpp"
#include "ExternalMethodResolver.hpp"
#include "ExternalMonoMetadataGenerator.hpp"
#include "ExternalProcess.hpp"

#include "../Aegis Unity Universal/Libraries/imgui/imgui.h"
#include "../Aegis Unity Universal/Libraries/imgui/imgui_impl_dx11.h"
#include "../Aegis Unity Universal/Libraries/imgui/imgui_impl_win32.h"

#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Aegis::UnityExternal
{
    namespace
    {
        enum class GuiTab
        {
            Visual,
            Aim,
            Exploits,
            Misc,
            Universal,
            Developer
        };

        struct KnownMethod
        {
            const char* label = "";
            const char* imageName = "";
            const char* className = "";
            const char* methodName = "";
            int argumentCount = -1;
        };

        struct WatchEntry
        {
            std::string label;
            std::string addressText;
            int type = 4;
            int size = 64;
            std::string value;
        };

        struct ScanResult
        {
            uintptr_t address = 0;
            std::uint32_t rva = 0;
            std::string moduleName;
        };

        struct PatternByte
        {
            std::uint8_t value = 0;
            bool wildcard = false;
        };

        struct Vec3
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        struct Vec4
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float w = 0.0f;
        };

        struct EspEntity
        {
            uintptr_t address = 0;
            Vec3 position;
            bool onScreen = false;
            ImVec2 screen;
            ImVec2 head;
        };

        struct ObjectCacheEntry
        {
            uintptr_t address = 0;
            uintptr_t matchedPointer = 0;
            uintptr_t positionAddress = 0;
            uintptr_t transformBase = 0;
            Vec3 position;
            bool hasPosition = false;
            bool positionFromTransform = false;
            std::string label;
            std::string source;
            std::string positionRoute;
        };

        struct ObjectCacheTarget
        {
            uintptr_t pointer = 0;
            std::string label;
            std::string metadata;
            std::string source;
        };

        struct ManagedNativeObject
        {
            uintptr_t managedAddress = 0;
            uintptr_t nativePointer = 0;
            uintptr_t matchedPointer = 0;
            std::string label;
            std::string source;
        };

        struct FastTrackedTarget
        {
            uintptr_t objectAddress = 0;
            uintptr_t matchedPointer = 0;
            uintptr_t positionAddress = 0;
            uintptr_t transformBase = 0;
            uintptr_t cachedPtr = 0;
            std::int64_t objectToPositionOffset = 0;
            std::int64_t cachedPtrToPositionOffset = 0;
            std::size_t positionShareCount = 1;
            Vec3 position;
            Vec3 lastGoodPosition;
            bool hasPosition = false;
            bool hasLastGoodPosition = false;
            bool positionFromTransform = false;
            bool likelyPlayer = false;
            int score = 0;
            int staleReads = 0;
            ULONGLONG lastGoodReadTick = 0;
            ULONGLONG lastFallbackProbeTick = 0;
            std::string label;
            std::string source;
            std::string positionRoute;
            std::string offsetSummary;
        };

        struct MatrixCandidate
        {
            uintptr_t address = 0;
            int layout = 0;
            int score = 0;
            int structuralScore = 0;
            int validPoints = 0;
            int onScreenPoints = 0;
            int goodHeightPoints = 0;
            std::array<float, 16> matrix{};
        };

        struct GuiState
        {
            GuiTab tab = GuiTab::Universal;
            char target[260] = "";
            char methodMapPath[520] = "";
            char exportName[160] = "il2cpp_class_get_method_from_name";
            char imageName[160] = "UnityEngine.CoreModule";
            char className[160] = "UnityEngine.Time";
            char methodName[160] = "get_timeScale";
            int argumentCount = 0;
            bool anyArgumentCount = false;
            char readAddress[80] = "";
            int readSize = 64;
            int readType = 4;
            char watchLabel[80] = "";
            char watchAddress[80] = "";
            int watchType = 4;
            int watchSize = 64;
            char scanPattern[260] = "";
            int scanModule = 0;
            char methodFilter[160] = "";
            char objectCacheComponentName[160] = "PlayerController";
            char objectCacheFallbackComponentName[160] = "UnityEngine.Rigidbody";
            char objectCachePointer[80] = "";
            char objectCacheFallbackPointer[80] = "";
            int objectCacheMaxResults = 32;
            bool objectCacheUseFallback = true;
            bool objectCacheAutoResolveMono = true;
            bool objectCacheAutoResolveIl2Cpp = true;
            bool objectCacheRequireCachedPtr = true;
            bool objectCacheUseMetadataFallbacks = false;
            int classScanMaxMs = 12000;
            int objectCacheScanMaxMs = 20000;
            bool autoBuildUnityObjectIndex = true;
            int unityObjectIndexMaxResults = 4096;
            int nativeCompanionScanBytes = 0x800;
            int monoClassNameOffset = 0x30;
            int monoClassNamespaceOffset = 0x38;
            int monoVTableClassOffset = 0;
            int il2cppClassNameOffset = 0x10;
            int il2cppClassNamespaceOffset = 0x18;
            int entitySource = 0;
            bool autoBuildFastTargets = true;
            bool fastTargetsFallbackToCache = true;
            bool fastTargetsUseLastGood = true;
            bool allowWeakAutoProbeVisuals = false;
            int maxFastTargets = 64;
            int fastTargetLastGoodMs = 350;
            int fastTargetFallbackProbeMs = 250;
            int objectPositionMode = 5;
            int objectPointerOffset = 0;
            int objectCachedPtrOffset = 0x10;
            int objectTransformPointerOffset = 0;
            float objectPositionMinMagnitude = 0.001f;
            float objectPositionMaxAbs = 100000.0f;
            bool overlayMode = false;
            bool alignToTargetWindow = true;
            bool clickThroughOverlay = false;
            bool espEnabled = false;
            bool espBoxes = true;
            bool espSnaplines = true;
            bool radarEnabled = false;
            bool objectCacheAutoRebuild = false;
            int objectCacheAutoRebuildMs = 30000;
            char entityListAddress[80] = "";
            char entityCountAddress[80] = "";
            int entityCount = 32;
            int entityStride = 0x8;
            int entityLayout = 0;
            int positionOffset = 0;
            char viewProjectionAddress[80] = "";
            bool autoResolveViewProjection = true;
            bool allowFewSampleViewProjectionGuess = true;
            int viewProjectionScanMaxMs = 2500;
            int matrixLayout = 0;
            int upAxis = 0;
            float entityHeight = 1.8f;
            int entityPositionAnchor = 0;
            float entityHeadOffset = 1.0f;
            float entityFeetOffset = 1.0f;
            char localPositionAddress[80] = "";
            float radarRange = 100.0f;
            float radarSize = 160.0f;
            float radarPosX = 24.0f;
            float radarPosY = 72.0f;
            bool autoRefresh = false;
            bool keepOnTop = false;

            std::optional<UnityProcess> process;
            std::optional<MethodMap> methodMap;
            std::optional<ExternalMethodResolver> resolver;
            ExternalMemoryReader reader;
            std::vector<UnityProcess> scannedProcesses;
            std::vector<WatchEntry> watches;
            std::vector<ScanResult> scanResults;
            std::vector<EspEntity> espEntities;
            std::vector<ObjectCacheEntry> objectCache;
            std::vector<ManagedNativeObject> transformIndex;
            std::vector<ManagedNativeObject> gameObjectIndex;
            std::vector<ManagedNativeObject> cameraIndex;
            std::vector<FastTrackedTarget> fastTargets;
            std::string objectCacheStatus;
            std::string unityObjectIndexStatus;
            std::size_t objectCachePositionsReadThisFrame = 0;
            std::size_t objectCachePositionFailuresThisFrame = 0;
            std::size_t fastTargetsReadThisFrame = 0;
            std::size_t fastTargetsFallbacksThisFrame = 0;
            std::size_t fastTargetsHeldThisFrame = 0;
            std::size_t fastTargetsFailedThisFrame = 0;
            ULONGLONG objectCacheLastLiveReadTick = 0;
            ULONGLONG objectCacheLastFullScanTick = 0;
            std::string viewProjectionAutoStatus;
            ULONGLONG viewProjectionLastAutoScanTick = 0;
            std::vector<std::string> log;
        };

        ID3D11Device* gDevice = nullptr;
        ID3D11DeviceContext* gDeviceContext = nullptr;
        IDXGISwapChain* gSwapChain = nullptr;
        ID3D11RenderTargetView* gRenderTargetView = nullptr;
        HWND gWindow = nullptr;
        constexpr COLORREF kTransparentWindowColorKey = RGB(0, 0, 0);

        bool IsAllowedMemoryType(DWORD type, bool includeImages);
        bool IsReadableMemoryProtection(DWORD protect);
        bool IsReadableProcessRange(HANDLE process, uintptr_t address, std::size_t size);
        uintptr_t OffsetAddress(uintptr_t base, int offset);
        void AddUniqueLabel(std::vector<std::string>* labels, std::string label);
        bool ReadMatrix4x4(const ExternalMemoryReader& reader, const std::string& addressText, std::array<float, 16>* matrix);
        bool WorldToScreen(const Vec3& position, const std::array<float, 16>& matrix, int layout, const ImVec2& screenSize, ImVec2* out);
        Vec3 EntityFeetFromPosition(const GuiState& state, const Vec3& position);
        Vec3 EntityHeadFromPosition(const GuiState& state, const Vec3& position);
        bool IsNearScreen(const ImVec2& point, const ImVec2& screenSize, float marginScale);
        bool IsOnScreen(const ImVec2& point, const ImVec2& screenSize);
        bool ReadTransformAccessWorldPosition(const GuiState& state, uintptr_t transformBase, Vec3* position, uintptr_t* positionAddress);
        void RebuildFastTargetsFromObjectCache(GuiState* state, bool logResult);
        void RefreshFastTargetLivePositions(GuiState* state);
        void RescoreFastTargets(GuiState* state);

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

        std::string WideToUtf8(const std::wstring& value)
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

        std::wstring Utf8ToWide(const std::string& value)
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

        void CopyToBuffer(char* destination, std::size_t destinationSize, const std::string& value)
        {
            if (!destination || destinationSize == 0)
            {
                return;
            }

            const std::size_t count = std::min(destinationSize - 1, value.size());
            std::copy_n(value.data(), count, destination);
            destination[count] = '\0';
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

        std::filesystem::path SharedExternalProfilePath()
        {
            std::wstring appData(MAX_PATH, L'\0');
            const DWORD size = GetEnvironmentVariableW(L"APPDATA", appData.data(), static_cast<DWORD>(appData.size()));
            std::filesystem::path directory = size > 0
                ? std::filesystem::path(appData.c_str()) / L"AegisUnityUniversal"
                : ExternalExecutableDirectory();
            return directory / L"external_profile.ini";
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

        void LoadGuiConfig(GuiState* state, const std::wstring& initialTarget)
        {
            if (!state)
            {
                return;
            }

            std::wifstream file{ ConfigPath() };
            std::wstring lastTarget;
            std::wstring lastMethodMap;
            std::wstring line;
            const auto parseBool = [](const std::wstring& value) {
                return value == L"1" || value == L"true" || value == L"True";
            };
            const auto parseInt = [](const std::wstring& value, int fallback) {
                wchar_t* end = nullptr;
                const long parsed = std::wcstol(value.c_str(), &end, 0);
                return end && *end == L'\0' ? static_cast<int>(parsed) : fallback;
            };
            const auto parseFloat = [](const std::wstring& value, float fallback) {
                wchar_t* end = nullptr;
                const float parsed = std::wcstof(value.c_str(), &end);
                return end && *end == L'\0' ? parsed : fallback;
            };

            while (std::getline(file, line))
            {
                const std::wstring trimmed = Trim(line);
                if (std::wstring value = ConfigValue(trimmed, L"last_target"); !value.empty())
                {
                    lastTarget = value;
                }
                else if (value = ConfigValue(trimmed, L"last_method_map"); !value.empty())
                {
                    lastMethodMap = value;
                }
                else if (value = ConfigValue(trimmed, L"object_cache_component"); !value.empty())
                {
                    CopyToBuffer(state->objectCacheComponentName, sizeof(state->objectCacheComponentName), WideToUtf8(value));
                }
                else if (value = ConfigValue(trimmed, L"object_cache_fallback"); !value.empty())
                {
                    CopyToBuffer(state->objectCacheFallbackComponentName, sizeof(state->objectCacheFallbackComponentName), WideToUtf8(value));
                }
                else if (value = ConfigValue(trimmed, L"object_cache_use_fallback"); !value.empty())
                {
                    state->objectCacheUseFallback = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"object_cache_require_cached_ptr"); !value.empty())
                {
                    state->objectCacheRequireCachedPtr = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"metadata_fallback_labels"); !value.empty())
                {
                    state->objectCacheUseMetadataFallbacks = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"auto_resolve_mono_vtables"); !value.empty())
                {
                    state->objectCacheAutoResolveMono = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"auto_resolve_il2cpp_classes"); !value.empty())
                {
                    state->objectCacheAutoResolveIl2Cpp = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"class_scan_ms"); !value.empty())
                {
                    state->classScanMaxMs = parseInt(value, state->classScanMaxMs);
                }
                else if (value = ConfigValue(trimmed, L"object_cache_scan_ms"); !value.empty())
                {
                    state->objectCacheScanMaxMs = parseInt(value, state->objectCacheScanMaxMs);
                }
                else if (value = ConfigValue(trimmed, L"unity_object_index"); !value.empty())
                {
                    state->autoBuildUnityObjectIndex = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"unity_object_index_max"); !value.empty())
                {
                    state->unityObjectIndexMaxResults = parseInt(value, state->unityObjectIndexMaxResults);
                }
                else if (value = ConfigValue(trimmed, L"companion_scan_bytes"); !value.empty())
                {
                    state->nativeCompanionScanBytes = parseInt(value, state->nativeCompanionScanBytes);
                }
                else if (value = ConfigValue(trimmed, L"object_position_mode"); !value.empty())
                {
                    state->objectPositionMode = parseInt(value, state->objectPositionMode);
                }
                else if (value = ConfigValue(trimmed, L"object_pointer_offset"); !value.empty())
                {
                    state->objectPointerOffset = parseInt(value, state->objectPointerOffset);
                }
                else if (value = ConfigValue(trimmed, L"cached_ptr_offset"); !value.empty())
                {
                    state->objectCachedPtrOffset = parseInt(value, state->objectCachedPtrOffset);
                }
                else if (value = ConfigValue(trimmed, L"transform_pointer_offset"); !value.empty())
                {
                    state->objectTransformPointerOffset = parseInt(value, state->objectTransformPointerOffset);
                }
                else if (value = ConfigValue(trimmed, L"view_projection"); !value.empty())
                {
                    CopyToBuffer(state->viewProjectionAddress, sizeof(state->viewProjectionAddress), WideToUtf8(value));
                }
                else if (value = ConfigValue(trimmed, L"matrix_layout"); !value.empty())
                {
                    state->matrixLayout = parseInt(value, state->matrixLayout);
                }
                else if (value = ConfigValue(trimmed, L"up_axis"); !value.empty())
                {
                    state->upAxis = parseInt(value, state->upAxis);
                }
                else if (value = ConfigValue(trimmed, L"entity_position_anchor"); !value.empty())
                {
                    state->entityPositionAnchor = parseInt(value, state->entityPositionAnchor);
                }
                else if (value = ConfigValue(trimmed, L"entity_height"); !value.empty())
                {
                    state->entityHeight = parseFloat(value, state->entityHeight);
                }
                else if (value = ConfigValue(trimmed, L"entity_head_offset"); !value.empty())
                {
                    state->entityHeadOffset = parseFloat(value, state->entityHeadOffset);
                }
                else if (value = ConfigValue(trimmed, L"entity_feet_offset"); !value.empty())
                {
                    state->entityFeetOffset = parseFloat(value, state->entityFeetOffset);
                }
                else if (value = ConfigValue(trimmed, L"auto_build_fast_targets"); !value.empty())
                {
                    state->autoBuildFastTargets = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"fast_targets_cache_fallback"); !value.empty())
                {
                    state->fastTargetsFallbackToCache = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"smooth_fast_targets"); !value.empty())
                {
                    state->fastTargetsUseLastGood = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"allow_weak_auto_probe_visuals"); !value.empty())
                {
                    state->allowWeakAutoProbeVisuals = parseBool(value);
                }
                else if (value = ConfigValue(trimmed, L"fast_hold_last_good_ms"); !value.empty())
                {
                    state->fastTargetLastGoodMs = parseInt(value, state->fastTargetLastGoodMs);
                }
                else if (value = ConfigValue(trimmed, L"fast_fallback_probe_ms"); !value.empty())
                {
                    state->fastTargetFallbackProbeMs = parseInt(value, state->fastTargetFallbackProbeMs);
                }
                else if (value = ConfigValue(trimmed, L"max_fast_targets"); !value.empty())
                {
                    state->maxFastTargets = parseInt(value, state->maxFastTargets);
                }
            }

            if (!initialTarget.empty())
            {
                lastTarget = initialTarget;
            }

            CopyToBuffer(state->target, sizeof(state->target), WideToUtf8(lastTarget));
            CopyToBuffer(state->methodMapPath, sizeof(state->methodMapPath), WideToUtf8(lastMethodMap));
        }

        void SaveGuiConfig(const GuiState& state)
        {
            std::wofstream file{ ConfigPath() };
            if (!file)
            {
                return;
            }

            file << L"last_target=" << Utf8ToWide(state.target) << L'\n';
            file << L"last_method_map=" << Utf8ToWide(state.methodMapPath) << L'\n';
            file << L"object_cache_component=" << Utf8ToWide(state.objectCacheComponentName) << L'\n';
            file << L"object_cache_fallback=" << Utf8ToWide(state.objectCacheFallbackComponentName) << L'\n';
            file << L"object_cache_use_fallback=" << (state.objectCacheUseFallback ? 1 : 0) << L'\n';
            file << L"object_cache_require_cached_ptr=" << (state.objectCacheRequireCachedPtr ? 1 : 0) << L'\n';
            file << L"metadata_fallback_labels=" << (state.objectCacheUseMetadataFallbacks ? 1 : 0) << L'\n';
            file << L"auto_resolve_mono_vtables=" << (state.objectCacheAutoResolveMono ? 1 : 0) << L'\n';
            file << L"auto_resolve_il2cpp_classes=" << (state.objectCacheAutoResolveIl2Cpp ? 1 : 0) << L'\n';
            file << L"class_scan_ms=" << state.classScanMaxMs << L'\n';
            file << L"object_cache_scan_ms=" << state.objectCacheScanMaxMs << L'\n';
            file << L"unity_object_index=" << (state.autoBuildUnityObjectIndex ? 1 : 0) << L'\n';
            file << L"unity_object_index_max=" << state.unityObjectIndexMaxResults << L'\n';
            file << L"companion_scan_bytes=" << state.nativeCompanionScanBytes << L'\n';
            file << L"object_position_mode=" << state.objectPositionMode << L'\n';
            file << L"object_pointer_offset=" << state.objectPointerOffset << L'\n';
            file << L"cached_ptr_offset=" << state.objectCachedPtrOffset << L'\n';
            file << L"transform_pointer_offset=" << state.objectTransformPointerOffset << L'\n';
            file << L"view_projection=" << Utf8ToWide(state.viewProjectionAddress) << L'\n';
            file << L"matrix_layout=" << state.matrixLayout << L'\n';
            file << L"up_axis=" << state.upAxis << L'\n';
            file << L"entity_position_anchor=" << state.entityPositionAnchor << L'\n';
            file << L"entity_height=" << state.entityHeight << L'\n';
            file << L"entity_head_offset=" << state.entityHeadOffset << L'\n';
            file << L"entity_feet_offset=" << state.entityFeetOffset << L'\n';
            file << L"auto_build_fast_targets=" << (state.autoBuildFastTargets ? 1 : 0) << L'\n';
            file << L"fast_targets_cache_fallback=" << (state.fastTargetsFallbackToCache ? 1 : 0) << L'\n';
            file << L"smooth_fast_targets=" << (state.fastTargetsUseLastGood ? 1 : 0) << L'\n';
            file << L"allow_weak_auto_probe_visuals=" << (state.allowWeakAutoProbeVisuals ? 1 : 0) << L'\n';
            file << L"fast_hold_last_good_ms=" << state.fastTargetLastGoodMs << L'\n';
            file << L"fast_fallback_probe_ms=" << state.fastTargetFallbackProbeMs << L'\n';
            file << L"max_fast_targets=" << state.maxFastTargets << L'\n';
        }

        std::optional<std::uint64_t> ParseUnsignedInteger(const std::string& text)
        {
            if (text.empty())
            {
                return std::nullopt;
            }

            char* end = nullptr;
            const unsigned long long value = std::strtoull(text.c_str(), &end, 0);
            if (!end || *end != '\0')
            {
                return std::nullopt;
            }

            return static_cast<std::uint64_t>(value);
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

        std::string FormatHex(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase << value;
            return stream.str();
        }

        std::string FormatSignedHexOffset(std::int64_t value)
        {
            if (value < 0)
            {
                return "-" + FormatHex(static_cast<std::uint64_t>(-value));
            }

            return "+" + FormatHex(static_cast<std::uint64_t>(value));
        }

        std::int64_t SignedAddressDelta(uintptr_t to, uintptr_t from)
        {
            return static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from);
        }

        std::string FormatResolvedValue(const ResolvedAddress& resolved)
        {
            if (resolved.hasAddress)
            {
                return FormatHex(resolved.address);
            }

            if (resolved.hasMetadataToken)
            {
                std::string value = "metadata token " + FormatHex(resolved.metadataToken);
                if (resolved.hasRva && resolved.rva != 0)
                {
                    value += ", IL RVA " + FormatHex(resolved.rva);
                }
                return value;
            }

            return "metadata-only";
        }

        const char* MethodEntryKindName(MethodMapEntryKind kind)
        {
            switch (kind)
            {
            case MethodMapEntryKind::NativeRva:
                return "native-rva";
            case MethodMapEntryKind::MonoMetadataToken:
                return "mono-token";
            default:
                return "unknown";
            }
        }

        std::string ModuleSummary(const std::optional<ModuleInfo>& module)
        {
            if (!module)
            {
                return "no";
            }

            std::ostringstream stream;
            stream << "yes @ " << FormatHex(module->base) << " (" << module->size << " bytes)";
            return stream.str();
        }

        void AddLog(GuiState* state, const char* format, ...)
        {
            if (!state || !format)
            {
                return;
            }

            char buffer[1024] = {};
            va_list args;
            va_start(args, format);
            std::vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

            state->log.emplace_back(buffer);
            std::cout << "[Aegis External] " << buffer << std::endl;
            if (state->log.size() > 300)
            {
                state->log.erase(state->log.begin(), state->log.begin() + (state->log.size() - 300));
            }
        }

        bool LoadInternalExternalProfile(GuiState* state)
        {
            if (!state)
            {
                return false;
            }

            const std::filesystem::path path = SharedExternalProfilePath();
            std::wifstream file{ path };
            if (!file)
            {
                AddLog(state, "No internal external-profile file found at %s.", WideToUtf8(path.wstring()).c_str());
                return false;
            }

            const auto parseBool = [](const std::wstring& value) {
                return value == L"1" || value == L"true" || value == L"True";
            };
            const auto parseInt = [](const std::wstring& value, int fallback) {
                wchar_t* end = nullptr;
                const long parsed = std::wcstol(value.c_str(), &end, 0);
                return end && *end == L'\0' ? static_cast<int>(parsed) : fallback;
            };
            const auto parseFloat = [](const std::wstring& value, float fallback) {
                wchar_t* end = nullptr;
                const float parsed = std::wcstof(value.c_str(), &end);
                return end && *end == L'\0' ? parsed : fallback;
            };

            std::wstring line;
            std::vector<std::wstring> profileLines;
            int applied = 0;
            while (std::getline(file, line))
            {
                const std::wstring trimmed = Trim(line);
                if (trimmed.empty() || trimmed[0] == L'#')
                {
                    continue;
                }
                profileLines.push_back(trimmed);

                std::wstring value;
                if (value = ConfigValue(trimmed, L"object_cache_component"); !value.empty())
                {
                    CopyToBuffer(state->objectCacheComponentName, sizeof(state->objectCacheComponentName), WideToUtf8(value));
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"object_cache_fallback"); !value.empty())
                {
                    CopyToBuffer(state->objectCacheFallbackComponentName, sizeof(state->objectCacheFallbackComponentName), WideToUtf8(value));
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"object_cache_use_fallback"); !value.empty())
                {
                    state->objectCacheUseFallback = parseBool(value);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"object_position_mode"); !value.empty())
                {
                    state->objectPositionMode = parseInt(value, state->objectPositionMode);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"cached_ptr_offset"); !value.empty())
                {
                    state->objectCachedPtrOffset = parseInt(value, state->objectCachedPtrOffset);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"unity_object_index"); !value.empty())
                {
                    state->autoBuildUnityObjectIndex = parseBool(value);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"auto_build_fast_targets"); !value.empty())
                {
                    state->autoBuildFastTargets = parseBool(value);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"fast_targets_cache_fallback"); !value.empty())
                {
                    state->fastTargetsFallbackToCache = parseBool(value);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"entity_position_anchor"); !value.empty())
                {
                    state->entityPositionAnchor = parseInt(value, state->entityPositionAnchor);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"up_axis"); !value.empty())
                {
                    state->upAxis = parseInt(value, state->upAxis);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"entity_height"); !value.empty())
                {
                    state->entityHeight = parseFloat(value, state->entityHeight);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"entity_head_offset"); !value.empty())
                {
                    state->entityHeadOffset = parseFloat(value, state->entityHeadOffset);
                    ++applied;
                }
                else if (value = ConfigValue(trimmed, L"entity_feet_offset"); !value.empty())
                {
                    state->entityFeetOffset = parseFloat(value, state->entityFeetOffset);
                    ++applied;
                }
            }

            const auto profileValue = [&](const std::wstring& key) -> std::wstring {
                for (const std::wstring& profileLine : profileLines)
                {
                    std::wstring value = ConfigValue(profileLine, key.c_str());
                    if (!value.empty())
                    {
                        return value;
                    }
                }
                return {};
            };
            const auto parseAddress = [](const std::wstring& value) -> uintptr_t {
                if (value.empty())
                {
                    return 0;
                }
                wchar_t* end = nullptr;
                const unsigned long long parsed = std::wcstoull(value.c_str(), &end, 0);
                return end && *end == L'\0' ? static_cast<uintptr_t>(parsed) : 0;
            };

            std::size_t loadedProfileTargets = 0;
            if (state->reader.IsOpen())
            {
                const auto addProfileTarget = [&](const char* label, uintptr_t objectAddress, uintptr_t componentAddress, uintptr_t transformAddress) {
                    if (transformAddress == 0 ||
                        state->fastTargets.size() >= static_cast<std::size_t>(std::clamp(state->maxFastTargets, 1, 4096)))
                    {
                        return;
                    }

                    uintptr_t transformBase = transformAddress;
                    Vec3 position{};
                    uintptr_t positionAddress = 0;
                    if (!ReadTransformAccessWorldPosition(*state, transformBase, &position, &positionAddress))
                    {
                        const std::optional<uintptr_t> nativeTransform =
                            state->reader.Read<uintptr_t>(OffsetAddress(transformAddress, state->objectCachedPtrOffset));
                        if (!nativeTransform ||
                            !ReadTransformAccessWorldPosition(*state, *nativeTransform, &position, &positionAddress))
                        {
                            return;
                        }
                        transformBase = *nativeTransform;
                    }

                    const auto exists = std::find_if(state->fastTargets.begin(), state->fastTargets.end(), [&](const FastTrackedTarget& existing) {
                        return existing.transformBase == transformBase ||
                            (objectAddress != 0 && existing.objectAddress == objectAddress);
                    });
                    if (exists != state->fastTargets.end())
                    {
                        return;
                    }

                    FastTrackedTarget target;
                    target.objectAddress = componentAddress != 0
                        ? componentAddress
                        : (objectAddress != 0 ? objectAddress : transformAddress);
                    target.positionAddress = positionAddress;
                    target.transformBase = transformBase;
                    target.position = position;
                    target.lastGoodPosition = position;
                    target.hasPosition = true;
                    target.hasLastGoodPosition = true;
                    target.positionFromTransform = true;
                    target.lastGoodReadTick = GetTickCount64();
                    target.positionShareCount = 1;
                    target.label = label ? label : "internal profile";
                    target.source = "internal exported profile";
                    target.positionRoute = "internal profile -> TransformAccess hierarchy";
                    target.offsetSummary = "internal profile";
                    if (componentAddress != 0)
                    {
                        target.offsetSummary += " component " + FormatHex(componentAddress);
                    }
                    if (objectAddress != 0)
                    {
                        target.offsetSummary += " gameobject " + FormatHex(objectAddress);
                    }
                    target.offsetSummary += " transform " + FormatHex(transformBase);
                    target.score = 900;
                    state->fastTargets.push_back(std::move(target));
                    ++loadedProfileTargets;
                };

                for (const char* prefix : { "player", "object" })
                {
                    for (int index = 0; index < 64; ++index)
                    {
                        const std::wstring baseKey = Utf8ToWide(prefix) + L"_" + std::to_wstring(index);
                        const uintptr_t objectAddress = parseAddress(profileValue(baseKey + L"_gameobject"));
                        const uintptr_t componentAddress = parseAddress(profileValue(baseKey + L"_component"));
                        const uintptr_t transformAddress = parseAddress(profileValue(baseKey + L"_transform"));
                        if (objectAddress == 0 && transformAddress == 0)
                        {
                            continue;
                        }

                        const std::string label = WideToUtf8(profileValue(baseKey + L"_component_name"));
                        addProfileTarget(label.empty() ? prefix : label.c_str(), objectAddress, componentAddress, transformAddress);
                    }
                }

                if (loadedProfileTargets > 0)
                {
                    RescoreFastTargets(state);
                    state->entitySource = 2;
                    state->espEnabled = true;
                    state->espBoxes = true;
                    state->espSnaplines = true;
                }
            }

            AddLog(
                state,
                "Loaded internal external-profile from %s (%d setting(s), %zu live profile target(s)).",
                WideToUtf8(path.wstring()).c_str(),
                applied,
                loadedProfileTargets);
            return applied > 0 || loadedProfileTargets > 0;
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        std::string TrimAscii(std::string value)
        {
            const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
                return std::isspace(ch) != 0;
            });
            const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
                return std::isspace(ch) != 0;
            }).base();

            if (first >= last)
            {
                return {};
            }

            return std::string(first, last);
        }

        bool ClassNameMatches(std::string className, std::string requested)
        {
            className = ToLowerAscii(TrimAscii(std::move(className)));
            requested = ToLowerAscii(TrimAscii(std::move(requested)));
            if (className.empty() || requested.empty())
            {
                return false;
            }

            if (className == requested)
            {
                return true;
            }

            return className.size() > requested.size() &&
                className.compare(className.size() - requested.size(), requested.size(), requested) == 0 &&
                className[className.size() - requested.size() - 1] == '.';
        }

        bool ContainsInsensitiveAscii(const std::string& text, const std::string& needle)
        {
            if (needle.empty())
            {
                return true;
            }

            return ToLowerAscii(text).find(ToLowerAscii(needle)) != std::string::npos;
        }

        bool EqualsInsensitiveAscii(const std::string& left, const std::string& right)
        {
            return ToLowerAscii(left) == ToLowerAscii(right);
        }

        bool IsFinite(const Vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool ReadVec3(const ExternalMemoryReader& reader, uintptr_t address, Vec3* out)
        {
            if (!out)
            {
                return false;
            }

            const std::optional<Vec3> value = reader.Read<Vec3>(address);
            if (!value || !IsFinite(*value))
            {
                return false;
            }

            *out = *value;
            return true;
        }

        bool IsFinite(const Vec4& value)
        {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z) &&
                std::isfinite(value.w);
        }

        Vec3 AddVec3(const Vec3& left, const Vec3& right)
        {
            return Vec3{ left.x + right.x, left.y + right.y, left.z + right.z };
        }

        Vec3 ScaleVec3(const Vec3& left, const Vec3& right)
        {
            return Vec3{ left.x * right.x, left.y * right.y, left.z * right.z };
        }

        Vec3 CrossVec3(const Vec3& left, const Vec3& right)
        {
            return Vec3{
                left.y * right.z - left.z * right.y,
                left.z * right.x - left.x * right.z,
                left.x * right.y - left.y * right.x
            };
        }

        Vec3 RotateVec3ByQuaternion(Vec3 value, Vec4 rotation)
        {
            const float lengthSquared =
                rotation.x * rotation.x +
                rotation.y * rotation.y +
                rotation.z * rotation.z +
                rotation.w * rotation.w;
            if (!std::isfinite(lengthSquared) || lengthSquared < 0.000001f)
            {
                return value;
            }

            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            rotation.x *= inverseLength;
            rotation.y *= inverseLength;
            rotation.z *= inverseLength;
            rotation.w *= inverseLength;

            const Vec3 q{ rotation.x, rotation.y, rotation.z };
            const Vec3 t = CrossVec3(q, value);
            const Vec3 doubledT{ t.x * 2.0f, t.y * 2.0f, t.z * 2.0f };
            const Vec3 qCrossT = CrossVec3(q, doubledT);
            return Vec3{
                value.x + rotation.w * doubledT.x + qCrossT.x,
                value.y + rotation.w * doubledT.y + qCrossT.y,
                value.z + rotation.w * doubledT.z + qCrossT.z
            };
        }

        bool IsPlausibleObjectPosition(const GuiState& state, const Vec3& value)
        {
            const float maxAbs = std::max(state.objectPositionMaxAbs, 1.0f);
            const float minMagnitude = std::max(state.objectPositionMinMagnitude, 0.0f);
            const float magnitudeSquared = value.x * value.x + value.y * value.y + value.z * value.z;
            return IsFinite(value) &&
                magnitudeSquared >= (minMagnitude * minMagnitude) &&
                std::abs(value.x) <= maxAbs &&
                std::abs(value.y) <= maxAbs &&
                std::abs(value.z) <= maxAbs;
        }

        bool IsLikelyDynamicDataAddress(const GuiState& state, uintptr_t address, std::size_t size)
        {
            if (!state.reader.ProcessHandle() || address == 0 || size == 0)
            {
                return false;
            }

            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQueryEx(state.reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != sizeof(mbi))
            {
                return false;
            }

            const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t end = address + size;
            if (end <= address || address < base || end > base + mbi.RegionSize)
            {
                return false;
            }

            const DWORD baseProtect = mbi.Protect & 0xFF;
            return mbi.State == MEM_COMMIT &&
                IsAllowedMemoryType(mbi.Type, false) &&
                IsReadableMemoryProtection(mbi.Protect) &&
                baseProtect != PAGE_EXECUTE_READ &&
                baseProtect != PAGE_EXECUTE_READWRITE &&
                baseProtect != PAGE_EXECUTE_WRITECOPY;
        }

        bool TryReadObjectPositionAt(
            const GuiState& state,
            uintptr_t readAddress,
            Vec3* position,
            uintptr_t* positionAddress)
        {
            if (!position || !positionAddress || readAddress == 0 || (readAddress % alignof(float)) != 0)
            {
                return false;
            }

            Vec3 value{};
            if (!IsLikelyDynamicDataAddress(state, readAddress, sizeof(Vec3)) ||
                !ReadVec3(state.reader, readAddress, &value) ||
                !IsPlausibleObjectPosition(state, value))
            {
                return false;
            }

            *position = value;
            *positionAddress = readAddress;
            return true;
        }

        uintptr_t OffsetAddress(uintptr_t base, int offset)
        {
            if (offset >= 0)
            {
                return base + static_cast<uintptr_t>(offset);
            }

            return base - static_cast<uintptr_t>(-offset);
        }

        bool AddUniqueProbeBase(std::vector<uintptr_t>* bases, uintptr_t value)
        {
            if (!bases || value == 0 || (value % sizeof(uintptr_t)) != 0 ||
                std::find(bases->begin(), bases->end(), value) != bases->end())
            {
                return false;
            }

            bases->push_back(value);
            return true;
        }

        struct TransformHierarchyEntry
        {
            Vec4 translation{};
            Vec4 rotation{};
            Vec4 scale{};
        };

        bool ReadTransformHierarchyEntry(
            const GuiState& state,
            uintptr_t matrixList,
            int index,
            TransformHierarchyEntry* entry,
            uintptr_t* entryAddress)
        {
            if (!entry || index < 0 || index > 0x200000)
            {
                return false;
            }

            constexpr uintptr_t kMatrixStride = 0x30;
            const uintptr_t address = matrixList + static_cast<uintptr_t>(index) * kMatrixStride;
            if (!IsLikelyDynamicDataAddress(state, address, sizeof(TransformHierarchyEntry)) ||
                !state.reader.ReadRaw(address, entry, sizeof(TransformHierarchyEntry)) ||
                !IsFinite(entry->translation) ||
                !IsFinite(entry->rotation) ||
                !IsFinite(entry->scale))
            {
                return false;
            }

            const float rotationLengthSquared =
                entry->rotation.x * entry->rotation.x +
                entry->rotation.y * entry->rotation.y +
                entry->rotation.z * entry->rotation.z +
                entry->rotation.w * entry->rotation.w;
            if (!std::isfinite(rotationLengthSquared) ||
                rotationLengthSquared < 0.20f ||
                rotationLengthSquared > 2.50f)
            {
                return false;
            }

            if (std::abs(entry->scale.x) > 10000.0f ||
                std::abs(entry->scale.y) > 10000.0f ||
                std::abs(entry->scale.z) > 10000.0f ||
                (std::abs(entry->scale.x) < 0.000001f &&
                    std::abs(entry->scale.y) < 0.000001f &&
                    std::abs(entry->scale.z) < 0.000001f))
            {
                return false;
            }

            if (entryAddress)
            {
                *entryAddress = address;
            }
            return true;
        }

        bool ReadTransformHierarchyWorldPosition(
            const GuiState& state,
            uintptr_t matrixList,
            uintptr_t parentIndexList,
            int transformIndex,
            Vec3* position,
            uintptr_t* positionAddress,
            int* hierarchyDepth)
        {
            if (!position || !positionAddress ||
                matrixList == 0 ||
                parentIndexList == 0 ||
                transformIndex < 0 ||
                transformIndex > 0x200000)
            {
                return false;
            }

            TransformHierarchyEntry entry{};
            uintptr_t localAddress = 0;
            if (!ReadTransformHierarchyEntry(state, matrixList, transformIndex, &entry, &localAddress))
            {
                return false;
            }

            Vec3 accumulated{ entry.translation.x, entry.translation.y, entry.translation.z };
            if (!IsPlausibleObjectPosition(state, accumulated))
            {
                return false;
            }

            const uintptr_t firstParentIndexAddress = parentIndexList + static_cast<uintptr_t>(transformIndex) * sizeof(int);
            if (!IsLikelyDynamicDataAddress(state, firstParentIndexAddress, sizeof(int)))
            {
                return false;
            }

            const std::optional<int> firstParentIndex = state.reader.Read<int>(firstParentIndexAddress);
            if (!firstParentIndex || *firstParentIndex < -1 || *firstParentIndex > 0x200000)
            {
                return false;
            }

            int depth = 0;
            int currentIndex = transformIndex;
            std::array<int, 128> visited{};
            std::size_t visitedCount = 0;
            while (depth < 127)
            {
                const uintptr_t parentIndexAddress = parentIndexList + static_cast<uintptr_t>(currentIndex) * sizeof(int);
                if (!IsLikelyDynamicDataAddress(state, parentIndexAddress, sizeof(int)))
                {
                    break;
                }

                const std::optional<int> parentIndex = state.reader.Read<int>(parentIndexAddress);
                if (!parentIndex || *parentIndex < -1 || *parentIndex > 0x200000)
                {
                    return false;
                }
                if (*parentIndex < 0 || *parentIndex == currentIndex)
                {
                    break;
                }

                bool seen = false;
                for (std::size_t index = 0; index < visitedCount; ++index)
                {
                    if (visited[index] == *parentIndex)
                    {
                        seen = true;
                        break;
                    }
                }
                if (seen)
                {
                    break;
                }
                if (visitedCount < visited.size())
                {
                    visited[visitedCount++] = *parentIndex;
                }

                TransformHierarchyEntry parent{};
                if (!ReadTransformHierarchyEntry(state, matrixList, *parentIndex, &parent, nullptr))
                {
                    break;
                }

                accumulated = AddVec3(
                    Vec3{ parent.translation.x, parent.translation.y, parent.translation.z },
                    RotateVec3ByQuaternion(
                        ScaleVec3(accumulated, Vec3{ parent.scale.x, parent.scale.y, parent.scale.z }),
                        parent.rotation));
                if (!IsPlausibleObjectPosition(state, accumulated))
                {
                    return false;
                }

                currentIndex = *parentIndex;
                ++depth;
            }

            *position = accumulated;
            *positionAddress = localAddress;
            if (hierarchyDepth)
            {
                *hierarchyDepth = depth;
            }
            return true;
        }

        bool ReadTransformAccessWorldPosition(
            const GuiState& state,
            uintptr_t transformBase,
            Vec3* position,
            uintptr_t* positionAddress)
        {
            if (!position || !positionAddress || transformBase == 0 || !state.reader.IsOpen())
            {
                return false;
            }

            std::vector<uintptr_t> accessBases;
            AddUniqueProbeBase(&accessBases, transformBase);
            for (int pointerOffset : { 0x8, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 })
            {
                const std::optional<uintptr_t> accessBase =
                    state.reader.Read<uintptr_t>(OffsetAddress(transformBase, pointerOffset));
                if (accessBase &&
                    *accessBase != 0 &&
                    *accessBase != transformBase &&
                    IsLikelyDynamicDataAddress(state, *accessBase, sizeof(uintptr_t) * 8))
                {
                    AddUniqueProbeBase(&accessBases, *accessBase);
                }
            }

            struct Candidate
            {
                Vec3 position{};
                uintptr_t address = 0;
                int score = std::numeric_limits<int>::min();
            } best;

            const auto scorePosition = [&](const Vec3& value, uintptr_t address, int sourceScore, int hierarchyDepth) {
                int score = sourceScore + hierarchyDepth * 6;

                const float magnitude =
                    std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
                if (std::isfinite(magnitude) && magnitude >= 0.05f)
                {
                    score += 8;
                }

                const float vertical = state.upAxis == 1 ? value.z : value.y;
                if (std::isfinite(vertical) && vertical >= -50.0f && vertical <= 500.0f)
                {
                    score += 10;
                }

                if (address < 0x10000000)
                {
                    score -= 24;
                }

                return score;
            };

            const auto consider = [&](const Vec3& value, uintptr_t address, int sourceScore, int hierarchyDepth) {
                if (!IsPlausibleObjectPosition(state, value))
                {
                    return;
                }

                const int score = scorePosition(value, address, sourceScore, hierarchyDepth);
                if (score > best.score)
                {
                    best = Candidate{ value, address, score };
                }
            };

            constexpr std::array<int, 4> kTransformDataOffsets = { 0x38, 0x30, 0x40, 0x28 };
            constexpr std::array<int, 4> kTransformIndexOffsets = { 0x40, 0x38, 0x48, 0x30 };
            struct TransformDataLayout
            {
                int matrixListOffset = 0;
                int parentIndexListOffset = 0;
                int score = 0;
            };
            constexpr std::array<TransformDataLayout, 4> kTransformDataLayouts = {
                TransformDataLayout{ 0x18, 0x20, 40 },
                TransformDataLayout{ 0x10, 0x18, 18 },
                TransformDataLayout{ 0x20, 0x28, 12 },
                TransformDataLayout{ 0x0, 0x8, 4 }
            };
            for (uintptr_t accessBase : accessBases)
            {
                for (int transformDataOffset : kTransformDataOffsets)
                {
                    const std::optional<uintptr_t> transformData =
                        state.reader.Read<uintptr_t>(OffsetAddress(accessBase, transformDataOffset));
                    if (!transformData || *transformData == 0 ||
                        !IsReadableProcessRange(state.reader.ProcessHandle(), *transformData, sizeof(uintptr_t) * 4))
                    {
                        continue;
                    }

                    for (int transformIndexOffset : kTransformIndexOffsets)
                    {
                        const std::optional<int> transformIndex =
                            state.reader.Read<int>(OffsetAddress(accessBase, transformIndexOffset));
                        if (!transformIndex || *transformIndex < 0 || *transformIndex > 0x200000)
                        {
                            continue;
                        }

                        for (const TransformDataLayout& layout : kTransformDataLayouts)
                        {
                            const std::optional<uintptr_t> matrixList =
                                state.reader.Read<uintptr_t>(OffsetAddress(*transformData, layout.matrixListOffset));
                            const std::optional<uintptr_t> parentIndexList =
                                state.reader.Read<uintptr_t>(OffsetAddress(*transformData, layout.parentIndexListOffset));
                            if (!matrixList || *matrixList == 0 ||
                                !parentIndexList || *parentIndexList == 0)
                            {
                                continue;
                            }

                            Vec3 hierarchyPosition{};
                            uintptr_t hierarchyAddress = 0;
                            int hierarchyDepth = 0;
                            if (ReadTransformHierarchyWorldPosition(
                                state,
                                *matrixList,
                                *parentIndexList,
                                *transformIndex,
                                &hierarchyPosition,
                                &hierarchyAddress,
                                &hierarchyDepth))
                            {
                                int sourceScore = 180 + layout.score;
                                sourceScore -= transformDataOffset == 0x38 ? 0 : 10;
                                sourceScore -= transformIndexOffset == 0x40 ? 0 : 10;
                                consider(hierarchyPosition, hierarchyAddress, sourceScore, hierarchyDepth);
                            }

                        }
                    }
                }
            }

            if (best.address == 0)
            {
                return false;
            }

            *position = best.position;
            *positionAddress = best.address;
            return true;
        }

        bool IsLikelyUnityNativePointer(uintptr_t managedObjectAddress, uintptr_t nativePointer)
        {
            if (managedObjectAddress == 0 || nativePointer == 0 || (nativePointer % sizeof(uintptr_t)) != 0)
            {
                return false;
            }

            const uintptr_t distance =
                nativePointer > managedObjectAddress
                ? nativePointer - managedObjectAddress
                : managedObjectAddress - nativePointer;
            return distance >= 0x10000;
        }

        bool IsProcessImageAddress(const GuiState& state, uintptr_t address)
        {
            if (!state.reader.ProcessHandle() || address == 0)
            {
                return false;
            }

            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQueryEx(state.reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != sizeof(mbi))
            {
                return false;
            }

            const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            return address >= base &&
                address < base + mbi.RegionSize &&
                mbi.State == MEM_COMMIT &&
                mbi.Type == MEM_IMAGE &&
                IsReadableMemoryProtection(mbi.Protect);
        }

        bool IsLikelyUnityNativeObjectPointer(const GuiState& state, uintptr_t managedObjectAddress, uintptr_t nativePointer)
        {
            if (!IsLikelyUnityNativePointer(managedObjectAddress, nativePointer) ||
                !IsReadableProcessRange(state.reader.ProcessHandle(), nativePointer, sizeof(uintptr_t)))
            {
                return false;
            }

            if (state.process && state.process->modules.Backend() == RuntimeBackend::Mono)
            {
                return true;
            }

            const std::optional<uintptr_t> nativeVTable = state.reader.Read<uintptr_t>(nativePointer);
            return nativeVTable && IsProcessImageAddress(state, *nativeVTable);
        }

        bool IsAddressNear(uintptr_t left, uintptr_t right, uintptr_t distance)
        {
            if (left == 0 || right == 0)
            {
                return false;
            }

            const uintptr_t delta = left > right ? left - right : right - left;
            return delta < distance;
        }

        void AddUniqueManagedNativeObject(
            std::vector<ManagedNativeObject>* objects,
            ManagedNativeObject object,
            std::size_t maxResults)
        {
            if (!objects || object.managedAddress == 0 || object.nativePointer == 0 || objects->size() >= maxResults)
            {
                return;
            }

            const auto exists = std::find_if(objects->begin(), objects->end(), [&object](const ManagedNativeObject& existing) {
                return existing.managedAddress == object.managedAddress ||
                    existing.nativePointer == object.nativePointer;
            });
            if (exists != objects->end())
            {
                return;
            }

            objects->push_back(std::move(object));
        }

        void SortManagedNativeIndex(std::vector<ManagedNativeObject>* objects)
        {
            if (!objects)
            {
                return;
            }

            std::sort(objects->begin(), objects->end(), [](const ManagedNativeObject& left, const ManagedNativeObject& right) {
                if (left.nativePointer != right.nativePointer)
                {
                    return left.nativePointer < right.nativePointer;
                }
                return left.managedAddress < right.managedAddress;
            });
        }

        const ManagedNativeObject* FindIndexedNativeObject(
            const std::vector<ManagedNativeObject>& objects,
            uintptr_t nativePointer)
        {
            if (nativePointer == 0)
            {
                return nullptr;
            }

            const auto it = std::lower_bound(
                objects.begin(),
                objects.end(),
                nativePointer,
                [](const ManagedNativeObject& object, uintptr_t value) {
                    return object.nativePointer < value;
                });
            return it != objects.end() && it->nativePointer == nativePointer ? &*it : nullptr;
        }

        const ManagedNativeObject* FindIndexedManagedObject(
            const std::vector<ManagedNativeObject>& objects,
            uintptr_t managedAddress)
        {
            if (managedAddress == 0)
            {
                return nullptr;
            }

            const auto it = std::find_if(objects.begin(), objects.end(), [managedAddress](const ManagedNativeObject& object) {
                return object.managedAddress == managedAddress;
            });
            return it == objects.end() ? nullptr : &*it;
        }

        bool ProbeObjectCachePosition(
            const GuiState& state,
            uintptr_t objectAddress,
            Vec3* position,
            uintptr_t* positionAddress,
            uintptr_t* transformBase = nullptr,
            bool* positionFromTransform = nullptr,
            std::string* positionRoute = nullptr)
        {
            if (!position || !positionAddress || objectAddress == 0 || !state.reader.IsOpen())
            {
                return false;
            }

            struct ProbeCandidate
            {
                Vec3 position{};
                uintptr_t address = 0;
                uintptr_t transformBase = 0;
                bool fromTransform = false;
                std::string route;
                int score = std::numeric_limits<int>::min();
            };

            std::array<float, 16> viewProjection{};
            const bool hasViewProjection = ReadMatrix4x4(state.reader, state.viewProjectionAddress, &viewProjection);
            const ImVec2 screenSize =
                ImGui::GetCurrentContext()
                ? ImGui::GetIO().DisplaySize
                : ImVec2(1920.0f, 1080.0f);
            ProbeCandidate best;
            ProbeCandidate bestTrustedTransform;

            const auto scorePositionValue = [&](const Vec3& value, uintptr_t readAddress, int sourceScore) -> int {
                int score = sourceScore;

                const float horizontalMagnitude =
                    state.upAxis == 1
                    ? std::sqrt(value.x * value.x + value.y * value.y)
                    : std::sqrt(value.x * value.x + value.z * value.z);
                const float vertical =
                    state.upAxis == 1
                    ? value.z
                    : value.y;

                if (std::isfinite(horizontalMagnitude))
                {
                    if (horizontalMagnitude >= 0.05f && horizontalMagnitude <= 5000.0f)
                    {
                        score += 18;
                    }
                    else if (horizontalMagnitude < 0.05f)
                    {
                        score -= 20;
                    }
                }

                if (std::isfinite(vertical) && vertical >= -20.0f && vertical <= 200.0f)
                {
                    score += 12;
                }

                int nearZeroComponents = 0;
                nearZeroComponents += std::abs(value.x) < 0.0001f ? 1 : 0;
                nearZeroComponents += std::abs(value.y) < 0.0001f ? 1 : 0;
                nearZeroComponents += std::abs(value.z) < 0.0001f ? 1 : 0;
                if (nearZeroComponents >= 2)
                {
                    score -= 14;
                }

                if (readAddress < 0x10000000)
                {
                    score -= 25;
                }

                if (hasViewProjection && screenSize.x > 1.0f && screenSize.y > 1.0f)
                {
                    ImVec2 feet{};
                    ImVec2 head{};
                    if (WorldToScreen(EntityFeetFromPosition(state, value), viewProjection, state.matrixLayout, screenSize, &feet) &&
                        WorldToScreen(EntityHeadFromPosition(state, value), viewProjection, state.matrixLayout, screenSize, &head))
                    {
                        score += 18;
                        const bool feetOnScreen = IsOnScreen(feet, screenSize);
                        const bool headOnScreen = IsOnScreen(head, screenSize);
                        const bool feetNear = IsNearScreen(feet, screenSize, 0.25f);
                        const bool headNear = IsNearScreen(head, screenSize, 0.25f);
                        if (feetOnScreen || headOnScreen)
                        {
                            score += 42;
                        }
                        else if (feetNear || headNear)
                        {
                            score += 18;
                        }

                        const float height = std::abs(feet.y - head.y);
                        if (height >= 8.0f && height <= screenSize.y * 0.8f)
                        {
                            score += 28;
                        }
                        else if (height > screenSize.y * 1.5f)
                        {
                            score -= 18;
                        }
                    }
                }

                return score;
            };

            const auto considerCandidate = [&](uintptr_t readAddress, int sourceScore, const char* route) {
                Vec3 value{};
                uintptr_t confirmedAddress = 0;
                if (!TryReadObjectPositionAt(state, readAddress, &value, &confirmedAddress))
                {
                    return;
                }

                const int score = scorePositionValue(value, confirmedAddress, sourceScore);
                if (score > best.score)
                {
                    best = ProbeCandidate{ value, confirmedAddress, 0, false, route ? route : "direct Vec3 probe", score };
                }
            };

            const auto considerTransformAccess = [&](uintptr_t transformBase, int sourceScore, bool trustedTransform, const char* route) {
                Vec3 value{};
                uintptr_t valueAddress = 0;
                if (!ReadTransformAccessWorldPosition(state, transformBase, &value, &valueAddress))
                {
                    return;
                }

                const int score = scorePositionValue(value, valueAddress, sourceScore + (trustedTransform ? 210 : 10));
                ProbeCandidate& targetBest = trustedTransform ? bestTrustedTransform : best;
                if (score > targetBest.score)
                {
                    targetBest = ProbeCandidate{ value, valueAddress, transformBase, true, route ? route : "TransformAccess probe", score };
                }
            };

            const std::size_t companionScanBytes = static_cast<std::size_t>(
                std::clamp(state.nativeCompanionScanBytes, 0, 0x4000));
            const auto scanManagedForTransformReference = [&](uintptr_t managedAddress, int sourceScore) {
                if (managedAddress == 0)
                {
                    return;
                }

                for (std::size_t offset = sizeof(uintptr_t); offset <= companionScanBytes; offset += sizeof(uintptr_t))
                {
                    const std::optional<uintptr_t> pointer = state.reader.Read<uintptr_t>(managedAddress + offset);
                    if (!pointer || *pointer == 0)
                    {
                        continue;
                    }

                    if (const ManagedNativeObject* transform = FindIndexedManagedObject(state.transformIndex, *pointer))
                    {
                        considerTransformAccess(transform->nativePointer, sourceScore + 145, true, "managed field -> indexed Transform");
                    }
                    else if (FindIndexedNativeObject(state.transformIndex, *pointer))
                    {
                        considerTransformAccess(*pointer, sourceScore + 135, true, "managed field -> native Transform");
                    }
                }
            };

            const auto scanNativeGameObjectForTransform = [&](uintptr_t nativeGameObject, int sourceScore) {
                if (nativeGameObject == 0 || state.transformIndex.empty())
                {
                    return;
                }

                for (std::size_t offset = 0; offset <= companionScanBytes; offset += sizeof(uintptr_t))
                {
                    const std::optional<uintptr_t> pointer = state.reader.Read<uintptr_t>(nativeGameObject + offset);
                    if (!pointer || *pointer == 0)
                    {
                        continue;
                    }

                    if (FindIndexedNativeObject(state.transformIndex, *pointer))
                    {
                        considerTransformAccess(*pointer, sourceScore + 150, true, "native GameObject -> indexed Transform");
                    }
                }
            };

            const auto considerUnityCompanionPointer = [&](uintptr_t pointer, int sourceScore) {
                if (pointer == 0)
                {
                    return;
                }

                if (FindIndexedNativeObject(state.transformIndex, pointer))
                {
                    considerTransformAccess(pointer, sourceScore + 160, true, "indexed native Transform");
                }
                if (const ManagedNativeObject* transform = FindIndexedManagedObject(state.transformIndex, pointer))
                {
                    considerTransformAccess(transform->nativePointer, sourceScore + 165, true, "indexed managed Transform");
                }
                if (FindIndexedNativeObject(state.gameObjectIndex, pointer))
                {
                    scanNativeGameObjectForTransform(pointer, sourceScore + 120);
                }
                if (const ManagedNativeObject* gameObject = FindIndexedManagedObject(state.gameObjectIndex, pointer))
                {
                    scanNativeGameObjectForTransform(gameObject->nativePointer, sourceScore + 125);
                    scanManagedForTransformReference(gameObject->managedAddress, sourceScore + 100);
                }
            };

            std::vector<uintptr_t> nativeBases;
            if (const std::optional<uintptr_t> nativePointer =
                state.reader.Read<uintptr_t>(OffsetAddress(objectAddress, state.objectCachedPtrOffset));
                nativePointer && IsLikelyUnityNativeObjectPointer(state, objectAddress, *nativePointer))
            {
                AddUniqueProbeBase(&nativeBases, *nativePointer);
            }

            scanManagedForTransformReference(objectAddress, 80);

            if (state.objectPointerOffset != 0)
            {
                if (const std::optional<uintptr_t> objectPointer =
                    state.reader.Read<uintptr_t>(OffsetAddress(objectAddress, state.objectPointerOffset));
                    objectPointer && *objectPointer != 0 && *objectPointer != objectAddress)
                {
                    if (const std::optional<uintptr_t> objectNativePointer =
                        state.reader.Read<uintptr_t>(OffsetAddress(*objectPointer, state.objectCachedPtrOffset));
                        objectNativePointer && IsLikelyUnityNativeObjectPointer(state, objectAddress, *objectNativePointer))
                    {
                        AddUniqueProbeBase(&nativeBases, *objectNativePointer);
                    }
                }
            }

            std::vector<uintptr_t> oneHopBases;
            for (uintptr_t base : nativeBases)
            {
                considerUnityCompanionPointer(base, 90);
                considerTransformAccess(base, 45, false, "native object as TransformAccess fallback");

                for (std::size_t pointerOffset = 0; pointerOffset <= companionScanBytes; pointerOffset += sizeof(uintptr_t))
                {
                    const std::optional<uintptr_t> pointer = state.reader.Read<uintptr_t>(base + pointerOffset);
                    if (!pointer || *pointer == 0 || *pointer == base)
                    {
                        continue;
                    }

                    considerUnityCompanionPointer(*pointer, 110);

                    if (!IsLikelyDynamicDataAddress(state, *pointer, sizeof(uintptr_t)) &&
                        !IsReadableProcessRange(state.reader.ProcessHandle(), *pointer, sizeof(uintptr_t)))
                    {
                        continue;
                    }

                    AddUniqueProbeBase(&oneHopBases, *pointer);
                    if (oneHopBases.size() >= 64)
                    {
                        break;
                    }
                }

                if (oneHopBases.size() >= 64)
                {
                    break;
                }
            }

            std::vector<uintptr_t> twoHopBases;
            std::size_t inspectedOneHopBases = 0;
            for (uintptr_t base : oneHopBases)
            {
                if (++inspectedOneHopBases > 32)
                {
                    break;
                }

                for (std::size_t pointerOffset = 0; pointerOffset <= companionScanBytes; pointerOffset += sizeof(uintptr_t))
                {
                    const std::optional<uintptr_t> pointer = state.reader.Read<uintptr_t>(base + pointerOffset);
                    if (!pointer || *pointer == 0 || *pointer == base)
                    {
                        continue;
                    }

                    considerUnityCompanionPointer(*pointer, 130);
                    considerTransformAccess(*pointer, 105, false, "two-hop TransformAccess fallback");

                    if ((IsLikelyDynamicDataAddress(state, *pointer, sizeof(uintptr_t)) ||
                        IsReadableProcessRange(state.reader.ProcessHandle(), *pointer, sizeof(uintptr_t))) &&
                        AddUniqueProbeBase(&twoHopBases, *pointer) &&
                        twoHopBases.size() >= 64)
                    {
                        break;
                    }
                }

                if (twoHopBases.size() >= 64)
                {
                    break;
                }
            }

            for (uintptr_t base : twoHopBases)
            {
                considerTransformAccess(base, 75, false, "two-hop native TransformAccess fallback");
            }

            constexpr std::array<std::size_t, 28> kTransformPositionOffsets = {
                0x90, 0xA0, 0xAC, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
                0x100, 0x110, 0x120, 0x130, 0x140, 0x150, 0x160, 0x170,
                0x180, 0x190, 0x1A0, 0x1A4, 0x1A8, 0x1AC, 0x1B0, 0x1C0,
                0x1D0, 0x1E0, 0x1F0, 0x200
            };

            for (uintptr_t base : oneHopBases)
            {
                considerTransformAccess(base, 65, false, "one-hop native TransformAccess fallback");

                for (std::size_t offset : kTransformPositionOffsets)
                {
                    considerCandidate(base + offset, 42, "one-hop common Vec3 offset");
                }
            }

            for (uintptr_t base : oneHopBases)
            {
                for (std::size_t offset = 0; offset <= 0x240; offset += alignof(float))
                {
                    considerCandidate(base + offset, 8, "one-hop broad Vec3 scan");
                }
            }

            const ProbeCandidate& selected = bestTrustedTransform.address != 0 ? bestTrustedTransform : best;
            if (selected.address == 0)
            {
                return false;
            }

            *position = selected.position;
            *positionAddress = selected.address;
            if (transformBase)
            {
                *transformBase = selected.transformBase;
            }
            if (positionFromTransform)
            {
                *positionFromTransform = selected.fromTransform;
            }
            if (positionRoute)
            {
                *positionRoute = selected.route;
            }
            return true;
        }

        bool ReadObjectCachePosition(
            const GuiState& state,
            uintptr_t objectAddress,
            Vec3* position,
            uintptr_t* positionAddress,
            uintptr_t* transformBase = nullptr,
            bool* positionFromTransform = nullptr,
            std::string* positionRoute = nullptr)
        {
            if (!position || !positionAddress || objectAddress == 0 || !state.reader.IsOpen())
            {
                return false;
            }

            if (transformBase)
            {
                *transformBase = 0;
            }
            if (positionFromTransform)
            {
                *positionFromTransform = false;
            }
            if (positionRoute)
            {
                positionRoute->clear();
            }

            uintptr_t readAddress = 0;
            switch (state.objectPositionMode)
            {
            case 0:
                readAddress = OffsetAddress(objectAddress, state.positionOffset);
                if (positionRoute)
                {
                    *positionRoute = "object + Vec3 offset";
                }
                break;
            case 1:
            {
                const std::optional<uintptr_t> pointer =
                    state.reader.Read<uintptr_t>(OffsetAddress(objectAddress, state.objectPointerOffset));
                if (!pointer || *pointer == 0)
                {
                    return false;
                }
                readAddress = OffsetAddress(*pointer, state.positionOffset);
                if (positionRoute)
                {
                    *positionRoute = "pointer field -> Vec3 offset";
                }
                break;
            }
            case 2:
            {
                const std::optional<uintptr_t> nativePointer =
                    state.reader.Read<uintptr_t>(OffsetAddress(objectAddress, state.objectCachedPtrOffset));
                if (!nativePointer || *nativePointer == 0)
                {
                    return false;
                }
                readAddress = OffsetAddress(*nativePointer, state.positionOffset);
                if (positionRoute)
                {
                    *positionRoute = "m_CachedPtr -> native Vec3 offset";
                }
                break;
            }
            case 3:
            {
                const std::optional<uintptr_t> nativePointer =
                    state.reader.Read<uintptr_t>(OffsetAddress(objectAddress, state.objectCachedPtrOffset));
                if (!nativePointer || *nativePointer == 0)
                {
                    return false;
                }

                const std::optional<uintptr_t> transformPointer =
                    state.reader.Read<uintptr_t>(OffsetAddress(*nativePointer, state.objectTransformPointerOffset));
                if (!transformPointer || *transformPointer == 0)
                {
                    return false;
                }
                readAddress = OffsetAddress(*transformPointer, state.positionOffset);
                if (positionRoute)
                {
                    *positionRoute = "m_CachedPtr -> transform ptr -> Vec3 offset";
                }
                break;
            }
            case 4:
            {
                const std::optional<uintptr_t> componentObject =
                    state.reader.Read<uintptr_t>(OffsetAddress(objectAddress, state.objectPointerOffset));
                if (!componentObject || *componentObject == 0)
                {
                    return false;
                }

                const std::optional<uintptr_t> nativePointer =
                    state.reader.Read<uintptr_t>(OffsetAddress(*componentObject, state.objectCachedPtrOffset));
                if (!nativePointer || *nativePointer == 0)
                {
                    return false;
                }
                readAddress = OffsetAddress(*nativePointer, state.positionOffset);
                if (positionRoute)
                {
                    *positionRoute = "reference field -> m_CachedPtr -> Vec3 offset";
                }
                break;
            }
            case 5:
                return ProbeObjectCachePosition(state, objectAddress, position, positionAddress, transformBase, positionFromTransform, positionRoute);
            default:
                return false;
            }

            const bool read = TryReadObjectPositionAt(state, readAddress, position, positionAddress);
            if (!read && positionRoute)
            {
                positionRoute->clear();
            }
            return read;
        }

        bool RefreshObjectCacheEntryPosition(const GuiState& state, ObjectCacheEntry* entry)
        {
            if (!entry)
            {
                return false;
            }

            entry->hasPosition = ReadObjectCachePosition(
                state,
                entry->address,
                &entry->position,
                &entry->positionAddress,
                &entry->transformBase,
                &entry->positionFromTransform,
                &entry->positionRoute);
            if (!entry->hasPosition)
            {
                entry->transformBase = 0;
                entry->positionFromTransform = false;
                entry->positionRoute.clear();
                return false;
            }

            if (entry->matchedPointer != 0 &&
                IsAddressNear(entry->positionAddress, entry->matchedPointer, 0x1000000))
            {
                entry->hasPosition = false;
                entry->positionAddress = 0;
                entry->transformBase = 0;
                entry->positionFromTransform = false;
                entry->positionRoute.clear();
                entry->position = {};
                return false;
            }

            if (state.objectPositionMode == 5 &&
                !entry->positionFromTransform &&
                IsAddressNear(entry->positionAddress, entry->address, 0x10000))
            {
                entry->hasPosition = false;
                entry->positionAddress = 0;
                entry->transformBase = 0;
                entry->positionFromTransform = false;
                entry->positionRoute.clear();
                entry->position = {};
                return false;
            }

            return true;
        }

        bool ObjectCacheHasSamePositionSource(const std::vector<ObjectCacheEntry>& entries, const ObjectCacheEntry& candidate)
        {
            if (!candidate.hasPosition)
            {
                return false;
            }

            return std::any_of(entries.begin(), entries.end(), [&candidate](const ObjectCacheEntry& existing) {
                if (!existing.hasPosition)
                {
                    return false;
                }
                if (candidate.positionFromTransform &&
                    candidate.transformBase != 0 &&
                    existing.transformBase == candidate.transformBase)
                {
                    return true;
                }
                return candidate.positionAddress != 0 &&
                    existing.positionAddress == candidate.positionAddress;
            });
        }

        void RefreshObjectCacheLivePositions(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            state->objectCachePositionsReadThisFrame = 0;
            state->objectCachePositionFailuresThisFrame = 0;
            state->objectCacheLastLiveReadTick = GetTickCount64();
            for (ObjectCacheEntry& entry : state->objectCache)
            {
                if (RefreshObjectCacheEntryPosition(*state, &entry))
                {
                    ++state->objectCachePositionsReadThisFrame;
                }
                else
                {
                    ++state->objectCachePositionFailuresThisFrame;
                }
            }
        }

        bool ReadMatrix4x4(const ExternalMemoryReader& reader, const std::string& addressText, std::array<float, 16>* matrix)
        {
            if (!matrix)
            {
                return false;
            }

            const std::optional<std::uint64_t> address = ParseUnsignedInteger(addressText);
            if (!address)
            {
                return false;
            }

            return reader.ReadRaw(static_cast<uintptr_t>(*address), matrix->data(), sizeof(float) * matrix->size());
        }

        bool WorldToScreen(
            const Vec3& world,
            const std::array<float, 16>& matrix,
            int matrixLayout,
            const ImVec2& screenSize,
            ImVec2* screen)
        {
            if (!screen || screenSize.x <= 1.0f || screenSize.y <= 1.0f)
            {
                return false;
            }

            float clipX = 0.0f;
            float clipY = 0.0f;
            float clipW = 0.0f;
            if (matrixLayout == 0)
            {
                clipX = world.x * matrix[0] + world.y * matrix[1] + world.z * matrix[2] + matrix[3];
                clipY = world.x * matrix[4] + world.y * matrix[5] + world.z * matrix[6] + matrix[7];
                clipW = world.x * matrix[12] + world.y * matrix[13] + world.z * matrix[14] + matrix[15];
            }
            else
            {
                clipX = world.x * matrix[0] + world.y * matrix[4] + world.z * matrix[8] + matrix[12];
                clipY = world.x * matrix[1] + world.y * matrix[5] + world.z * matrix[9] + matrix[13];
                clipW = world.x * matrix[3] + world.y * matrix[7] + world.z * matrix[11] + matrix[15];
            }

            if (clipW < 0.001f)
            {
                return false;
            }

            const float ndcX = clipX / clipW;
            const float ndcY = clipY / clipW;
            screen->x = (screenSize.x * 0.5f) * (1.0f + ndcX);
            screen->y = (screenSize.y * 0.5f) * (1.0f - ndcY);
            return std::isfinite(screen->x) && std::isfinite(screen->y);
        }

        Vec3 EntityHeadPosition(const Vec3& feet, int upAxis, float height)
        {
            Vec3 head = feet;
            if (upAxis == 1)
            {
                head.z += height;
            }
            else
            {
                head.y += height;
            }
            return head;
        }

        Vec3 OffsetAlongUp(Vec3 value, int upAxis, float amount)
        {
            if (upAxis == 1)
            {
                value.z += amount;
            }
            else
            {
                value.y += amount;
            }
            return value;
        }

        Vec3 EntityFeetFromPosition(const GuiState& state, const Vec3& position)
        {
            if (state.entityPositionAnchor == 1)
            {
                return position;
            }

            return OffsetAlongUp(position, state.upAxis, -std::clamp(state.entityFeetOffset, -10.0f, 30.0f));
        }

        Vec3 EntityHeadFromPosition(const GuiState& state, const Vec3& position)
        {
            if (state.entityPositionAnchor == 1)
            {
                return EntityHeadPosition(position, state.upAxis, std::clamp(state.entityHeight, 0.1f, 30.0f));
            }

            return OffsetAlongUp(position, state.upAxis, std::clamp(state.entityHeadOffset, -10.0f, 30.0f));
        }

        bool IsNearScreen(const ImVec2& point, const ImVec2& screenSize, float marginScale = 0.25f)
        {
            const float marginX = std::max(screenSize.x * marginScale, 64.0f);
            const float marginY = std::max(screenSize.y * marginScale, 64.0f);
            return point.x >= -marginX &&
                point.x <= screenSize.x + marginX &&
                point.y >= -marginY &&
                point.y <= screenSize.y + marginY;
        }

        bool IsOnScreen(const ImVec2& point, const ImVec2& screenSize)
        {
            return point.x >= 0.0f &&
                point.x <= screenSize.x &&
                point.y >= 0.0f &&
                point.y <= screenSize.y;
        }

        bool IsWeakAutoProbeRoute(const std::string& route)
        {
            return ContainsInsensitiveAscii(route, "one-hop common Vec3 offset") ||
                ContainsInsensitiveAscii(route, "one-hop broad Vec3 scan") ||
                ContainsInsensitiveAscii(route, "broad Vec3 scan");
        }

        bool CanUsePositionForVisuals(const GuiState& state, bool fromTransform, const std::string& route)
        {
            if (fromTransform || state.allowWeakAutoProbeVisuals)
            {
                return true;
            }

            return !IsWeakAutoProbeRoute(route);
        }

        bool MatrixValuesLookPlausible(const std::array<float, 16>& matrix)
        {
            int nonZero = 0;
            int positive = 0;
            int negative = 0;
            float maxAbs = 0.0f;
            for (float value : matrix)
            {
                if (!std::isfinite(value))
                {
                    return false;
                }

                const float absValue = std::abs(value);
                if (absValue > 100000.0f)
                {
                    return false;
                }
                if (absValue > 0.0001f)
                {
                    ++nonZero;
                    maxAbs = std::max(maxAbs, absValue);
                    if (value > 0.0001f)
                    {
                        ++positive;
                    }
                    else if (value < -0.0001f)
                    {
                        ++negative;
                    }
                }
            }

            return nonZero >= 6 && positive > 0 && negative > 0 && maxAbs >= 0.01f;
        }

        bool AddUniquePositionSample(std::vector<Vec3>* samples, const Vec3& value, std::size_t maxSamples)
        {
            if (!samples || samples->size() >= maxSamples)
            {
                return false;
            }

            for (const Vec3& existing : *samples)
            {
                const float dx = existing.x - value.x;
                const float dy = existing.y - value.y;
                const float dz = existing.z - value.z;
                if ((dx * dx + dy * dy + dz * dz) < 0.0625f)
                {
                    return false;
                }
            }

            samples->push_back(value);
            return true;
        }

        std::vector<Vec3> ObjectCachePositionSamples(GuiState* state, std::size_t maxSamples)
        {
            std::vector<Vec3> samples;
            if (!state || maxSamples == 0)
            {
                return samples;
            }

            for (ObjectCacheEntry& entry : state->objectCache)
            {
                if (!entry.hasPosition)
                {
                    RefreshObjectCacheEntryPosition(*state, &entry);
                }

                if (!entry.hasPosition ||
                    !CanUsePositionForVisuals(*state, entry.positionFromTransform, entry.positionRoute) ||
                    !IsPlausibleObjectPosition(*state, entry.position))
                {
                    continue;
                }

                AddUniquePositionSample(&samples, entry.position, maxSamples);
                if (samples.size() >= maxSamples)
                {
                    break;
                }
            }

            return samples;
        }

        float Vec3Dot(const Vec3& left, const Vec3& right)
        {
            return left.x * right.x + left.y * right.y + left.z * right.z;
        }

        float Vec3Length(const Vec3& value)
        {
            return std::sqrt(Vec3Dot(value, value));
        }

        int ViewProjectionStructureScore(const std::array<float, 16>& matrix, int layout)
        {
            Vec3 xVector{};
            Vec3 yVector{};
            Vec3 wVector{};
            if (layout == 0)
            {
                xVector = { matrix[0], matrix[1], matrix[2] };
                yVector = { matrix[4], matrix[5], matrix[6] };
                wVector = { matrix[12], matrix[13], matrix[14] };
            }
            else
            {
                xVector = { matrix[0], matrix[4], matrix[8] };
                yVector = { matrix[1], matrix[5], matrix[9] };
                wVector = { matrix[3], matrix[7], matrix[11] };
            }

            const float xLength = Vec3Length(xVector);
            const float yLength = Vec3Length(yVector);
            const float wLength = Vec3Length(wVector);
            if (!std::isfinite(xLength) || !std::isfinite(yLength) || !std::isfinite(wLength))
            {
                return -40;
            }

            int score = 0;
            if (wLength >= 0.0001f && wLength <= 10000.0f)
            {
                score += 18;
            }
            else
            {
                score -= 32;
            }

            if (xLength >= 0.0001f && xLength <= 10000.0f)
            {
                score += 8;
            }
            if (yLength >= 0.0001f && yLength <= 10000.0f)
            {
                score += 8;
            }
            if (xLength > 0.0001f && yLength > 0.0001f)
            {
                const float ratio = xLength > yLength ? xLength / yLength : yLength / xLength;
                if (ratio <= 100.0f)
                {
                    score += 8;
                }

                const float xyDot = std::abs(Vec3Dot(xVector, yVector) / (xLength * yLength));
                if (std::isfinite(xyDot) && xyDot < 0.995f)
                {
                    score += 6;
                }
            }

            if (wLength > 0.0001f)
            {
                if (xLength > 0.0001f)
                {
                    const float xwDot = std::abs(Vec3Dot(xVector, wVector) / (xLength * wLength));
                    if (std::isfinite(xwDot) && xwDot < 0.9995f)
                    {
                        score += 3;
                    }
                }
                if (yLength > 0.0001f)
                {
                    const float ywDot = std::abs(Vec3Dot(yVector, wVector) / (yLength * wLength));
                    if (std::isfinite(ywDot) && ywDot < 0.9995f)
                    {
                        score += 3;
                    }
                }
            }

            return score;
        }

        MatrixCandidate ScoreViewProjectionMatrix(
            uintptr_t address,
            const std::array<float, 16>& matrix,
            int layout,
            const std::vector<Vec3>& samples,
            const ImVec2& screenSize,
            int upAxis,
            float entityHeight,
            int entityPositionAnchor,
            float entityHeadOffset,
            float entityFeetOffset)
        {
            MatrixCandidate candidate;
            candidate.address = address;
            candidate.layout = layout;
            candidate.matrix = matrix;
            candidate.structuralScore = ViewProjectionStructureScore(matrix, layout);
            candidate.score += candidate.structuralScore;

            if (samples.empty() || screenSize.x <= 1.0f || screenSize.y <= 1.0f)
            {
                return candidate;
            }

            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();
            int goodHeightCount = 0;

            for (const Vec3& sample : samples)
            {
                const Vec3 feetPosition = entityPositionAnchor == 1
                    ? sample
                    : OffsetAlongUp(sample, upAxis, -std::clamp(entityFeetOffset, -10.0f, 30.0f));
                const Vec3 headPosition = entityPositionAnchor == 1
                    ? EntityHeadPosition(sample, upAxis, std::clamp(entityHeight, 0.1f, 30.0f))
                    : OffsetAlongUp(sample, upAxis, std::clamp(entityHeadOffset, -10.0f, 30.0f));

                ImVec2 feet{};
                ImVec2 head{};
                if (!WorldToScreen(feetPosition, matrix, layout, screenSize, &feet) ||
                    !WorldToScreen(headPosition, matrix, layout, screenSize, &head))
                {
                    continue;
                }

                if (!std::isfinite(feet.x) || !std::isfinite(feet.y) ||
                    !std::isfinite(head.x) || !std::isfinite(head.y))
                {
                    continue;
                }

                ++candidate.validPoints;
                const bool feetNear = IsNearScreen(feet, screenSize);
                const bool headNear = IsNearScreen(head, screenSize);
                const bool feetOnScreen = IsOnScreen(feet, screenSize);
                const bool headOnScreen = IsOnScreen(head, screenSize);
                if (feetOnScreen || headOnScreen)
                {
                    ++candidate.onScreenPoints;
                }

                const float height = std::abs(feet.y - head.y);
                if (height >= 6.0f && height <= screenSize.y * 0.95f)
                {
                    ++goodHeightCount;
                    ++candidate.goodHeightPoints;
                }

                if (feetNear || headNear)
                {
                    candidate.score += 8;
                }
                if (feetOnScreen || headOnScreen)
                {
                    candidate.score += 18;
                }
                if (height >= 8.0f && height <= screenSize.y * 0.75f)
                {
                    candidate.score += 10;
                }
                else if (height > screenSize.y * 1.5f)
                {
                    candidate.score -= 16;
                }

                minX = std::min(minX, std::min(feet.x, head.x));
                minY = std::min(minY, std::min(feet.y, head.y));
                maxX = std::max(maxX, std::max(feet.x, head.x));
                maxY = std::max(maxY, std::max(feet.y, head.y));
            }

            if (candidate.validPoints > 0)
            {
                candidate.score += candidate.validPoints * 4;
            }

            if (candidate.onScreenPoints > 0 && goodHeightCount > 0)
            {
                candidate.score += 20;
            }

            if (candidate.validPoints >= 2)
            {
                const float extentX = maxX - minX;
                const float extentY = maxY - minY;
                if (extentX < 2.0f && extentY < 2.0f)
                {
                    candidate.score -= 24;
                }
                else if (extentX <= screenSize.x * 2.0f && extentY <= screenSize.y * 2.0f)
                {
                    candidate.score += 8;
                }
            }

            return candidate;
        }

        bool CandidateBeats(const MatrixCandidate& candidate, const MatrixCandidate& best)
        {
            if (candidate.score != best.score)
            {
                return candidate.score > best.score;
            }

            if (candidate.onScreenPoints != best.onScreenPoints)
            {
                return candidate.onScreenPoints > best.onScreenPoints;
            }

            return candidate.validPoints > best.validPoints;
        }

        bool TryAutoConfigureViewProjectionMatrix(
            GuiState* state,
            const ImVec2& screenSize,
            bool force,
            int maxMilliseconds)
        {
            if (!state || !state->reader.IsOpen() || !state->reader.ProcessHandle())
            {
                return false;
            }

            if (!force && !state->autoResolveViewProjection)
            {
                return false;
            }

            if (!force && state->viewProjectionAddress[0])
            {
                std::array<float, 16> existing{};
                if (ReadMatrix4x4(state->reader, state->viewProjectionAddress, &existing))
                {
                    return true;
                }
            }

            const ULONGLONG now = GetTickCount64();
            if (!force && state->viewProjectionLastAutoScanTick != 0 &&
                now - state->viewProjectionLastAutoScanTick < 5000)
            {
                return false;
            }
            state->viewProjectionLastAutoScanTick = now;

            std::vector<Vec3> samples = ObjectCachePositionSamples(state, 16);
            if (samples.empty())
            {
                state->viewProjectionAutoStatus = "Auto matrix scan skipped: object cache has no readable position samples.";
                AddLog(state, "%s", state->viewProjectionAutoStatus.c_str());
                return false;
            }

            const bool fewSampleFallback = samples.size() < 5;
            if (fewSampleFallback && !state->allowFewSampleViewProjectionGuess)
            {
                std::ostringstream stream;
                stream << "Auto matrix scan skipped: only " << samples.size()
                    << " unique position sample(s); enable Few-Sample Matrix Guess or provide a manual matrix address.";
                state->viewProjectionAutoStatus = stream.str();
                AddLog(state, "%s", state->viewProjectionAutoStatus.c_str());
                return false;
            }

            const ULONGLONG start = GetTickCount64();
            const int scanBudgetMs = std::clamp(maxMilliseconds, 250, 30000);
            constexpr std::size_t kChunkSize = 1024 * 1024;
            MatrixCandidate best;
            std::size_t scannedRegions = 0;
            std::size_t scannedMatrices = 0;
            std::size_t cameraMatrixRanges = 0;
            bool timedOut = false;

            const auto scoreMatrixAt = [&](uintptr_t candidateAddress, const std::array<float, 16>& matrix, int sourceBonus) {
                if (candidateAddress < 0x10000000)
                {
                    return;
                }

                MatrixCandidate row = ScoreViewProjectionMatrix(
                    candidateAddress,
                    matrix,
                    0,
                    samples,
                    screenSize,
                    state->upAxis,
                    state->entityHeight,
                    state->entityPositionAnchor,
                    state->entityHeadOffset,
                    state->entityFeetOffset);
                row.score += sourceBonus;
                if (CandidateBeats(row, best))
                {
                    best = row;
                }

                MatrixCandidate column = ScoreViewProjectionMatrix(
                    candidateAddress,
                    matrix,
                    1,
                    samples,
                    screenSize,
                    state->upAxis,
                    state->entityHeight,
                    state->entityPositionAnchor,
                    state->entityHeadOffset,
                    state->entityFeetOffset);
                column.score += sourceBonus;
                if (CandidateBeats(column, best))
                {
                    best = column;
                }
            };

            const auto scanBytesForMatrices = [&](uintptr_t chunkBase, const std::vector<std::uint8_t>& bytes, int sourceBonus) {
                if (bytes.size() < sizeof(float) * 16)
                {
                    return;
                }

                for (std::size_t offset = 0; offset + sizeof(float) * 16 <= bytes.size(); offset += 16)
                {
                    std::array<float, 16> matrix{};
                    std::memcpy(matrix.data(), bytes.data() + offset, sizeof(float) * matrix.size());
                    if (!MatrixValuesLookPlausible(matrix))
                    {
                        continue;
                    }

                    ++scannedMatrices;
                    scoreMatrixAt(chunkBase + offset, matrix, sourceBonus);
                }
            };

            const auto scanReadableRangeForMatrices = [&](uintptr_t rangeBase, uintptr_t rangeEnd, int sourceBonus) {
                if (rangeBase == 0 || rangeEnd <= rangeBase)
                {
                    return;
                }

                for (uintptr_t chunkBase = rangeBase; chunkBase < rangeEnd;)
                {
                    if (GetTickCount64() - start > static_cast<ULONGLONG>(scanBudgetMs))
                    {
                        timedOut = true;
                        break;
                    }

                    const std::size_t chunkSize = static_cast<std::size_t>(
                        std::min<std::uint64_t>(kChunkSize, rangeEnd - chunkBase));
                    const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(chunkBase, chunkSize);
                    scanBytesForMatrices(chunkBase, bytes, sourceBonus);

                    const uintptr_t chunkEnd = chunkBase + chunkSize;
                    if (chunkEnd >= rangeEnd)
                    {
                        break;
                    }
                    chunkBase = chunkEnd;
                }
            };

            if (!state->cameraIndex.empty())
            {
                const std::size_t maxCameraScans = std::min<std::size_t>(state->cameraIndex.size(), 32);
                for (std::size_t index = 0; index < maxCameraScans; ++index)
                {
                    if (GetTickCount64() - start > static_cast<ULONGLONG>(scanBudgetMs))
                    {
                        timedOut = true;
                        break;
                    }

                    const std::array<uintptr_t, 2> cameraBases = {
                        state->cameraIndex[index].nativePointer,
                        state->cameraIndex[index].managedAddress
                    };
                    for (uintptr_t cameraBase : cameraBases)
                    {
                        if (cameraBase == 0)
                        {
                            continue;
                        }

                        MEMORY_BASIC_INFORMATION cameraMbi{};
                        if (VirtualQueryEx(state->reader.ProcessHandle(), reinterpret_cast<LPCVOID>(cameraBase), &cameraMbi, sizeof(cameraMbi)) != sizeof(cameraMbi))
                        {
                            continue;
                        }

                        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(cameraMbi.BaseAddress);
                        const uintptr_t regionEnd = regionBase + cameraMbi.RegionSize;
                        if (regionEnd <= regionBase ||
                            cameraBase < regionBase ||
                            cameraMbi.State != MEM_COMMIT ||
                            !IsAllowedMemoryType(cameraMbi.Type, false) ||
                            !IsReadableMemoryProtection(cameraMbi.Protect))
                        {
                            continue;
                        }

                        const uintptr_t scanBase = cameraBase;
                        const uintptr_t scanEnd = std::min<uintptr_t>(regionEnd, cameraBase + 0x8000);
                        if (scanEnd > scanBase)
                        {
                            ++cameraMatrixRanges;
                            scanReadableRangeForMatrices(scanBase, scanEnd, 34);
                        }
                    }
                }
            }

            uintptr_t address = 0;
            MEMORY_BASIC_INFORMATION mbi{};
            while (!timedOut &&
                VirtualQueryEx(state->reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const uintptr_t next = base + mbi.RegionSize;
                if (next <= base)
                {
                    break;
                }

                if (mbi.State == MEM_COMMIT &&
                    IsAllowedMemoryType(mbi.Type, false) &&
                    IsReadableMemoryProtection(mbi.Protect))
                {
                    ++scannedRegions;
                    scanReadableRangeForMatrices(base, next, 0);
                }

                address = next;
            }

            const int requiredScore = fewSampleFallback
                ? (samples.size() <= 2 ? 86 : 78)
                : 44;
            const int requiredStructureScore = fewSampleFallback ? 34 : 0;
            const bool foundConfidentMatrix =
                best.score >= requiredScore &&
                best.structuralScore >= requiredStructureScore &&
                best.onScreenPoints > 0 &&
                best.validPoints > 0 &&
                (!fewSampleFallback || best.goodHeightPoints > 0);

            if (foundConfidentMatrix)
            {
                CopyToBuffer(state->viewProjectionAddress, sizeof(state->viewProjectionAddress), FormatHex(best.address));
                state->matrixLayout = best.layout;
                std::ostringstream stream;
                stream << "Auto matrix selected ";
                if (fewSampleFallback)
                {
                    stream << "few-sample guess ";
                }
                stream << FormatHex(best.address)
                    << " (" << (best.layout == 0 ? "row-major" : "column-major")
                    << ", score " << best.score
                    << ", structure " << best.structuralScore
                    << ", points " << best.onScreenPoints << "/" << best.validPoints
                    << ", height " << best.goodHeightPoints
                    << ", samples " << samples.size() << ")";
                if (cameraMatrixRanges > 0)
                {
                    stream << ", camera ranges " << cameraMatrixRanges;
                }
                if (timedOut)
                {
                    stream << " before scan budget ended";
                }
                if (fewSampleFallback)
                {
                    stream << ". Verify visually; a manual matrix address is still more reliable with only a few samples.";
                }
                state->viewProjectionAutoStatus = stream.str();
                AddLog(state, "%s", state->viewProjectionAutoStatus.c_str());
                if (!state->objectCache.empty())
                {
                    RefreshObjectCacheLivePositions(state);
                    if (state->autoBuildFastTargets)
                    {
                        RebuildFastTargetsFromObjectCache(state, false);
                    }
                    else
                    {
                        RescoreFastTargets(state);
                    }
                }
                else
                {
                    RescoreFastTargets(state);
                }
                return true;
            }

            std::ostringstream stream;
            stream << (fewSampleFallback
                ? "Few-sample matrix scan found no plausible matrix"
                : "Auto matrix scan found no confident matrix")
                << " (best score " << best.score
                << ", structure " << best.structuralScore
                << ", points " << best.onScreenPoints << "/" << best.validPoints
                << ", height " << best.goodHeightPoints
                << ", samples " << samples.size()
                << ", regions " << scannedRegions
                << ", camera ranges " << cameraMatrixRanges
                << ", plausible matrices " << scannedMatrices;
            if (timedOut)
            {
                stream << ", timed out";
            }
            stream << ").";
            state->viewProjectionAutoStatus = stream.str();
            AddLog(state, "%s", state->viewProjectionAutoStatus.c_str());
            return false;
        }

        bool ReadEspEntities(GuiState* state, const std::array<float, 16>& matrix, const ImVec2& screenSize)
        {
            if (!state || !state->reader.IsOpen())
            {
                return false;
            }

            state->espEntities.clear();
            auto appendEntity = [&](uintptr_t address, const Vec3& position) {
                const Vec3 feetPosition = EntityFeetFromPosition(*state, position);
                const Vec3 headPosition = EntityHeadFromPosition(*state, position);
                EspEntity entity;
                entity.address = address;
                entity.position = position;
                entity.onScreen = WorldToScreen(feetPosition, matrix, state->matrixLayout, screenSize, &entity.screen);
                entity.onScreen = WorldToScreen(headPosition, matrix, state->matrixLayout, screenSize, &entity.head)
                    && entity.onScreen;
                state->espEntities.push_back(entity);
            };

            if (state->entitySource == 1)
            {
                RefreshObjectCacheLivePositions(state);
                state->espEntities.reserve(state->objectCache.size());
                for (ObjectCacheEntry& cached : state->objectCache)
                {
                    if (!cached.hasPosition ||
                        !CanUsePositionForVisuals(*state, cached.positionFromTransform, cached.positionRoute))
                    {
                        continue;
                    }

                    appendEntity(cached.address, cached.position);
                }
                return !state->espEntities.empty();
            }

            if (state->entitySource == 2)
            {
                if (state->fastTargets.empty() && state->autoBuildFastTargets && !state->objectCache.empty())
                {
                    RebuildFastTargetsFromObjectCache(state, false);
                }

                RefreshFastTargetLivePositions(state);
                state->espEntities.reserve(state->fastTargets.size());
                for (const FastTrackedTarget& target : state->fastTargets)
                {
                    if (!target.hasPosition ||
                        !CanUsePositionForVisuals(*state, target.positionFromTransform, target.positionRoute))
                    {
                        continue;
                    }

                    appendEntity(target.objectAddress, target.position);
                }

                if (state->espEntities.empty() && state->fastTargetsFallbackToCache && !state->objectCache.empty())
                {
                    RefreshObjectCacheLivePositions(state);
                    for (ObjectCacheEntry& cached : state->objectCache)
                    {
                        if (cached.hasPosition &&
                            CanUsePositionForVisuals(*state, cached.positionFromTransform, cached.positionRoute))
                        {
                            appendEntity(cached.address, cached.position);
                        }
                    }
                }

                return !state->espEntities.empty();
            }

            const std::optional<std::uint64_t> listAddress = ParseUnsignedInteger(state->entityListAddress);
            if (!listAddress)
            {
                return false;
            }

            int count = std::clamp(state->entityCount, 0, 4096);
            if (state->entityCountAddress[0])
            {
                const std::optional<std::uint64_t> countAddress = ParseUnsignedInteger(state->entityCountAddress);
                if (countAddress)
                {
                    if (const std::optional<int> remoteCount = state->reader.Read<int>(static_cast<uintptr_t>(*countAddress)))
                    {
                        count = std::clamp(*remoteCount, 0, 4096);
                    }
                }
            }

            state->espEntities.reserve(static_cast<std::size_t>(std::min(count, 256)));

            for (int index = 0; index < count; ++index)
            {
                uintptr_t entityAddress = 0;
                if (state->entityLayout == 0)
                {
                    const uintptr_t pointerAddress = static_cast<uintptr_t>(*listAddress) + (sizeof(uintptr_t) * static_cast<std::size_t>(index));
                    const std::optional<uintptr_t> pointer = state->reader.Read<uintptr_t>(pointerAddress);
                    if (!pointer || *pointer == 0)
                    {
                        continue;
                    }
                    entityAddress = *pointer;
                }
                else
                {
                    const int stride = std::max(state->entityStride, static_cast<int>(sizeof(Vec3)));
                    entityAddress = static_cast<uintptr_t>(*listAddress) + (static_cast<std::size_t>(stride) * static_cast<std::size_t>(index));
                }

                Vec3 position;
                if (!ReadVec3(state->reader, OffsetAddress(entityAddress, state->positionOffset), &position))
                {
                    continue;
                }

                appendEntity(entityAddress, position);
            }

            return true;
        }

        bool IsReadableMemoryProtection(DWORD protect)
        {
            if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD baseProtect = protect & 0xFF;
            return baseProtect == PAGE_READONLY ||
                baseProtect == PAGE_READWRITE ||
                baseProtect == PAGE_WRITECOPY ||
                baseProtect == PAGE_EXECUTE_READ ||
                baseProtect == PAGE_EXECUTE_READWRITE ||
                baseProtect == PAGE_EXECUTE_WRITECOPY;
        }

        std::string ShortClassName(const std::string& className)
        {
            const std::size_t separator = className.rfind('.');
            return separator == std::string::npos ? className : className.substr(separator + 1);
        }

        int ComponentMetadataPreferenceScore(const std::string& imageName, const std::string& className)
        {
            int score = 0;
            const std::string image = ToLowerAscii(imageName);
            const std::string klass = ToLowerAscii(className);

            if (image == "assembly-csharp")
            {
                score += 250;
            }
            else if (image.rfind("assembly-csharp", 0) == 0)
            {
                score += 220;
            }
            else if (ContainsInsensitiveAscii(image, "assembly"))
            {
                score += 80;
            }

            if (klass.find('.') == std::string::npos)
            {
                score += 30;
            }

            if (ContainsInsensitiveAscii(klass, "player"))
            {
                score += 20;
            }
            if (ContainsInsensitiveAscii(klass, "character"))
            {
                score += 12;
            }

            if (ContainsInsensitiveAscii(image, "rewired") ||
                ContainsInsensitiveAscii(klass, "rewired."))
            {
                score -= 220;
            }
            if (ContainsInsensitiveAscii(image, "unityengine") ||
                ContainsInsensitiveAscii(klass, "unityengine."))
            {
                score -= 120;
            }
            if (ContainsInsensitiveAscii(image, "system") ||
                ContainsInsensitiveAscii(image, "mscorlib") ||
                ContainsInsensitiveAscii(image, "netstandard") ||
                ContainsInsensitiveAscii(klass, "system."))
            {
                score -= 100;
            }

            return score;
        }

        struct ComponentMetadataCandidate
        {
            std::string imageName;
            std::string className;
            std::size_t methodCount = 0;
            int score = 0;
        };

        std::vector<ComponentMetadataCandidate> ComponentMetadataCandidates(
            const GuiState& state,
            const std::string& componentName,
            std::size_t maxCandidates)
        {
            std::vector<ComponentMetadataCandidate> candidates;
            if (!state.methodMap || componentName.empty() || maxCandidates == 0)
            {
                return candidates;
            }

            for (const MethodMap::Entry& entry : state.methodMap->Entries())
            {
                if (!ClassNameMatches(entry.className, componentName))
                {
                    continue;
                }

                auto existing = std::find_if(candidates.begin(), candidates.end(), [&entry](const ComponentMetadataCandidate& candidate) {
                    return EqualsInsensitiveAscii(candidate.imageName, entry.imageName) &&
                        EqualsInsensitiveAscii(candidate.className, entry.className);
                });

                if (existing == candidates.end())
                {
                    ComponentMetadataCandidate candidate;
                    candidate.imageName = entry.imageName;
                    candidate.className = entry.className;
                    candidate.methodCount = 1;
                    candidate.score = ComponentMetadataPreferenceScore(entry.imageName, entry.className);
                    candidates.push_back(std::move(candidate));
                }
                else
                {
                    ++existing->methodCount;
                }
            }

            std::sort(candidates.begin(), candidates.end(), [](const ComponentMetadataCandidate& left, const ComponentMetadataCandidate& right) {
                if (left.score != right.score)
                {
                    return left.score > right.score;
                }
                if (left.methodCount != right.methodCount)
                {
                    return left.methodCount > right.methodCount;
                }
                if (left.imageName != right.imageName)
                {
                    return left.imageName < right.imageName;
                }
                return left.className < right.className;
            });

            if (candidates.size() > maxCandidates)
            {
                candidates.resize(maxCandidates);
            }
            return candidates;
        }

        std::vector<std::string> ComponentResolveLabelsFromMetadata(
            const GuiState& state,
            const std::string& requestedLabel,
            std::size_t maxLabels,
            std::string* detail)
        {
            std::vector<std::string> labels;
            const std::string trimmed = TrimAscii(requestedLabel);
            if (trimmed.empty())
            {
                return labels;
            }

            const bool requestedHasNamespace = trimmed.find('.') != std::string::npos;
            if (requestedHasNamespace)
            {
                AddUniqueLabel(&labels, trimmed);
                return labels;
            }

            const std::vector<ComponentMetadataCandidate> candidates =
                ComponentMetadataCandidates(state, trimmed, maxLabels);
            for (const ComponentMetadataCandidate& candidate : candidates)
            {
                AddUniqueLabel(&labels, candidate.className);
            }
            if (labels.empty())
            {
                AddUniqueLabel(&labels, trimmed);
            }

            if (detail && !candidates.empty())
            {
                std::ostringstream stream;
                stream << "metadata candidates:";
                const std::size_t shown = std::min<std::size_t>(candidates.size(), 5);
                for (std::size_t index = 0; index < shown; ++index)
                {
                    const ComponentMetadataCandidate& candidate = candidates[index];
                    stream << (index == 0 ? " " : ", ")
                        << candidate.className
                        << " in " << candidate.imageName
                        << " score " << candidate.score;
                }
                *detail = stream.str();
            }
            return labels;
        }

        std::string ComponentMetadataSummary(const GuiState& state, const std::string& componentName)
        {
            if (!state.methodMap || componentName.empty())
            {
                return {};
            }

            const std::vector<ComponentMetadataCandidate> candidates =
                ComponentMetadataCandidates(state, componentName, 1);
            if (candidates.empty())
            {
                return "metadata class not found";
            }

            const ComponentMetadataCandidate& candidate = candidates.front();
            std::ostringstream stream;
            stream << "metadata class " << candidate.className;
            if (!candidate.imageName.empty())
            {
                stream << " in " << candidate.imageName;
            }
            stream << " (" << candidate.methodCount << " methods, score " << candidate.score << ")";
            return stream.str();
        }

        bool AddUniqueAddress(std::vector<uintptr_t>* values, uintptr_t value, std::size_t maxValues)
        {
            if (!values || value == 0 || values->size() >= maxValues ||
                std::find(values->begin(), values->end(), value) != values->end())
            {
                return false;
            }

            values->push_back(value);
            return true;
        }

        void AddUniqueInt(std::vector<int>* values, int value)
        {
            if (values && value >= 0 && std::find(values->begin(), values->end(), value) == values->end())
            {
                values->push_back(value);
            }
        }

        bool ContainsAddress(const std::vector<uintptr_t>& values, uintptr_t value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        std::vector<uintptr_t> SortedUniqueAddresses(std::vector<uintptr_t> values)
        {
            values.erase(std::remove(values.begin(), values.end(), 0), values.end());
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
            return values;
        }

        bool AddUniqueObjectCacheTarget(std::vector<ObjectCacheTarget>* targets, ObjectCacheTarget target)
        {
            if (!targets || target.pointer == 0 ||
                std::find_if(targets->begin(), targets->end(), [target](const ObjectCacheTarget& existing) {
                    return existing.pointer == target.pointer;
                }) != targets->end())
            {
                return false;
            }

            targets->push_back(std::move(target));
            return true;
        }

        bool IsAllowedMemoryType(DWORD type, bool includeImages)
        {
            return type == MEM_PRIVATE ||
                type == MEM_MAPPED ||
                (includeImages && type == MEM_IMAGE);
        }

        bool IsReadableProcessRange(HANDLE process, uintptr_t address, std::size_t size)
        {
            if (!process || address == 0 || size == 0)
            {
                return false;
            }

            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQueryEx(process, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != sizeof(mbi))
            {
                return false;
            }

            const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t end = address + size;
            if (end <= address || address < base || end > base + mbi.RegionSize)
            {
                return false;
            }

            return mbi.State == MEM_COMMIT &&
                IsAllowedMemoryType(mbi.Type, false) &&
                IsReadableMemoryProtection(mbi.Protect);
        }

        std::string ReadAsciiCString(const ExternalMemoryReader& reader, uintptr_t address, std::size_t maxLength)
        {
            if (address == 0 || maxLength == 0)
            {
                return {};
            }

            const std::vector<std::uint8_t> bytes = reader.ReadBytes(address, maxLength);
            std::string value;
            value.reserve(bytes.size());
            for (std::uint8_t byte : bytes)
            {
                if (byte == 0)
                {
                    return value;
                }

                if (byte < 0x20 || byte > 0x7E)
                {
                    return {};
                }

                value.push_back(static_cast<char>(byte));
            }

            return {};
        }

        std::vector<uintptr_t> FindAsciiStringAddresses(
            const GuiState& state,
            const std::string& text,
            std::size_t maxResults)
        {
            std::vector<uintptr_t> results;
            if (!state.reader.IsOpen() || !state.reader.ProcessHandle() || text.empty() || maxResults == 0)
            {
                return results;
            }

            std::vector<std::uint8_t> needle(text.begin(), text.end());
            needle.push_back(0);

            constexpr std::size_t kChunkSize = 1024 * 1024;
            const std::size_t overlap = std::min<std::size_t>(needle.size() > 0 ? needle.size() - 1 : 0, 4096);
            const ULONGLONG start = GetTickCount64();
            const ULONGLONG budget = static_cast<ULONGLONG>(std::clamp(state.classScanMaxMs, 1000, 120000));
            uintptr_t address = 0;
            MEMORY_BASIC_INFORMATION mbi{};
            while (results.size() < maxResults &&
                VirtualQueryEx(state.reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                if (GetTickCount64() - start > budget)
                {
                    break;
                }

                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const uintptr_t next = base + mbi.RegionSize;
                if (next <= base)
                {
                    break;
                }

                if (mbi.State == MEM_COMMIT &&
                    IsAllowedMemoryType(mbi.Type, true) &&
                    IsReadableMemoryProtection(mbi.Protect))
                {
                    for (uintptr_t chunkBase = base; chunkBase < next && results.size() < maxResults;)
                    {
                        const std::size_t chunkSize = static_cast<std::size_t>(std::min<std::uint64_t>(kChunkSize, next - chunkBase));
                        const std::vector<std::uint8_t> bytes = state.reader.ReadBytes(chunkBase, chunkSize);
                        if (bytes.size() >= needle.size())
                        {
                            for (std::size_t offset = 0; offset + needle.size() <= bytes.size(); ++offset)
                            {
                                if (std::memcmp(bytes.data() + offset, needle.data(), needle.size()) == 0)
                                {
                                    AddUniqueAddress(&results, chunkBase + offset, maxResults);
                                    if (results.size() >= maxResults)
                                    {
                                        break;
                                    }
                                }
                            }
                        }

                        const uintptr_t chunkEnd = chunkBase + chunkSize;
                        if (chunkEnd >= next)
                        {
                            break;
                        }
                        chunkBase = overlap > 0 && chunkSize > overlap ? chunkEnd - overlap : chunkEnd;
                    }
                }

                address = next;
            }

            return results;
        }

        std::vector<uintptr_t> FindPointerReferences(
            const GuiState& state,
            const std::vector<uintptr_t>& pointerValues,
            std::size_t maxResults)
        {
            std::vector<uintptr_t> results;
            if (!state.reader.IsOpen() || !state.reader.ProcessHandle() || pointerValues.empty() || maxResults == 0)
            {
                return results;
            }

            const std::vector<uintptr_t> sortedPointers = SortedUniqueAddresses(pointerValues);
            if (sortedPointers.empty())
            {
                return results;
            }

            constexpr std::size_t kChunkSize = 1024 * 1024;
            const ULONGLONG start = GetTickCount64();
            const ULONGLONG budget = static_cast<ULONGLONG>(std::clamp(state.classScanMaxMs, 1000, 120000));
            uintptr_t address = 0;
            MEMORY_BASIC_INFORMATION mbi{};
            while (results.size() < maxResults &&
                VirtualQueryEx(state.reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                if (GetTickCount64() - start > budget)
                {
                    break;
                }

                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const uintptr_t next = base + mbi.RegionSize;
                if (next <= base)
                {
                    break;
                }

                if (mbi.State == MEM_COMMIT &&
                    IsAllowedMemoryType(mbi.Type, true) &&
                    IsReadableMemoryProtection(mbi.Protect))
                {
                    for (uintptr_t chunkBase = base; chunkBase < next && results.size() < maxResults;)
                    {
                        const std::size_t chunkSize = static_cast<std::size_t>(std::min<std::uint64_t>(kChunkSize, next - chunkBase));
                        const std::vector<std::uint8_t> bytes = state.reader.ReadBytes(chunkBase, chunkSize);
                        if (bytes.size() >= sizeof(uintptr_t))
                        {
                            for (std::size_t offset = 0; offset + sizeof(uintptr_t) <= bytes.size(); offset += sizeof(uintptr_t))
                            {
                                uintptr_t candidate = 0;
                                std::memcpy(&candidate, bytes.data() + offset, sizeof(candidate));
                                if (!std::binary_search(sortedPointers.begin(), sortedPointers.end(), candidate))
                                {
                                    continue;
                                }

                                AddUniqueAddress(&results, chunkBase + offset, maxResults);
                                if (results.size() >= maxResults)
                                {
                                    break;
                                }
                            }
                        }

                        chunkBase += chunkSize;
                    }
                }

                address = next;
            }

            return results;
        }

        struct TypeNameParts
        {
            std::string namespaceName;
            std::string className;
            bool hasNamespace = false;
        };

        TypeNameParts SplitTypeName(std::string typeName)
        {
            typeName = TrimAscii(std::move(typeName));
            const std::size_t separator = typeName.rfind('.');
            if (separator == std::string::npos || separator + 1 >= typeName.size())
            {
                return TypeNameParts{ {}, typeName, false };
            }

            return TypeNameParts{
                typeName.substr(0, separator),
                typeName.substr(separator + 1),
                true
            };
        }

        void AddUniqueLabel(std::vector<std::string>* labels, std::string label)
        {
            if (!labels)
            {
                return;
            }

            label = TrimAscii(std::move(label));
            if (label.empty() ||
                std::find_if(labels->begin(), labels->end(), [&label](const std::string& existing) {
                    return EqualsInsensitiveAscii(existing, label);
                }) != labels->end())
            {
                return;
            }

            labels->push_back(std::move(label));
        }

        std::vector<std::string> ObjectCacheResolveLabels(
            const GuiState& state,
            RuntimeBackend backend,
            const std::string& primaryLabel,
            const std::string& fallbackLabel,
            bool useConfiguredFallback)
        {
            std::vector<std::string> labels;
            AddUniqueLabel(&labels, primaryLabel);
            if (useConfiguredFallback)
            {
                AddUniqueLabel(&labels, fallbackLabel);
            }

            if (backend == RuntimeBackend::IL2CPP &&
                EqualsInsensitiveAscii(primaryLabel, "CharacterVisualController"))
            {
                AddUniqueLabel(&labels, "PlayerVisualController");
                AddUniqueLabel(&labels, "FallGirlVisualController");
            }

            const bool wantsPlayerLike =
                ContainsInsensitiveAscii(primaryLabel, "player") ||
                ContainsInsensitiveAscii(primaryLabel, "character") ||
                ContainsInsensitiveAscii(fallbackLabel, "player") ||
                ContainsInsensitiveAscii(fallbackLabel, "character");
            if (state.objectCacheUseMetadataFallbacks && wantsPlayerLike && state.methodMap)
            {
                struct RankedLabel
                {
                    std::string label;
                    int score = 0;
                    std::size_t methodCount = 0;
                };

                std::vector<RankedLabel> ranked;
                for (const MethodMap::Entry& entry : state.methodMap->Entries())
                {
                    if (entry.className.empty())
                    {
                        continue;
                    }

                    const std::string image = ToLowerAscii(entry.imageName);
                    const std::string klass = ToLowerAscii(entry.className);
                    if (ContainsInsensitiveAscii(image, "unityengine") ||
                        ContainsInsensitiveAscii(image, "system") ||
                        ContainsInsensitiveAscii(image, "mscorlib") ||
                        ContainsInsensitiveAscii(image, "netstandard") ||
                        ContainsInsensitiveAscii(image, "rewired") ||
                        ContainsInsensitiveAscii(image, "photon") ||
                        ContainsInsensitiveAscii(image, "playfab") ||
                        ContainsInsensitiveAscii(klass, "rewired.") ||
                        ContainsInsensitiveAscii(klass, "photon") ||
                        ContainsInsensitiveAscii(klass, "playfab") ||
                        ContainsInsensitiveAscii(klass, "ui") ||
                        ContainsInsensitiveAscii(klass, "camera"))
                    {
                        continue;
                    }

                    int score = ComponentMetadataPreferenceScore(entry.imageName, entry.className);
                    if (ContainsInsensitiveAscii(klass, "player"))
                    {
                        score += 95;
                    }
                    if (ContainsInsensitiveAscii(klass, "character"))
                    {
                        score += 65;
                    }
                    if (ContainsInsensitiveAscii(klass, "controller"))
                    {
                        score += 45;
                    }
                    if (ContainsInsensitiveAscii(klass, "movement") ||
                        ContainsInsensitiveAscii(klass, "motor"))
                    {
                        score += 30;
                    }
                    if (ContainsInsensitiveAscii(klass, "visual"))
                    {
                        score += 18;
                    }
                    if (!ContainsInsensitiveAscii(klass, "player") &&
                        !ContainsInsensitiveAscii(klass, "character") &&
                        !ContainsInsensitiveAscii(klass, "controller") &&
                        !ContainsInsensitiveAscii(klass, "movement") &&
                        !ContainsInsensitiveAscii(klass, "motor"))
                    {
                        continue;
                    }

                    auto existing = std::find_if(ranked.begin(), ranked.end(), [&entry](const RankedLabel& label) {
                        return EqualsInsensitiveAscii(label.label, entry.className);
                    });
                    if (existing == ranked.end())
                    {
                        ranked.push_back(RankedLabel{ entry.className, score, 1 });
                    }
                    else
                    {
                        ++existing->methodCount;
                        existing->score = std::max(existing->score, score);
                    }
                }

                std::sort(ranked.begin(), ranked.end(), [](const RankedLabel& left, const RankedLabel& right) {
                    if (left.score != right.score)
                    {
                        return left.score > right.score;
                    }
                    if (left.methodCount != right.methodCount)
                    {
                        return left.methodCount > right.methodCount;
                    }
                    return left.label < right.label;
                });

                std::size_t added = 0;
                for (const RankedLabel& label : ranked)
                {
                    const std::size_t before = labels.size();
                    AddUniqueLabel(&labels, label.label);
                    if (labels.size() != before && ++added >= 3)
                    {
                        break;
                    }
                }
            }

            return labels;
        }

        std::vector<uintptr_t> ResolveClassPointersByName(
            const GuiState& state,
            const std::string& componentName,
            const std::vector<int>& nameOffsets,
            const std::vector<int>& namespaceOffsets,
            const char* runtimeLabel,
            std::size_t maxClassPointers,
            std::string* detail)
        {
            std::vector<uintptr_t> classPointers;
            const TypeNameParts parts = SplitTypeName(componentName);
            if (parts.className.empty())
            {
                if (detail)
                {
                    *detail = "empty class name";
                }
                return classPointers;
            }

            if (nameOffsets.empty())
            {
                if (detail)
                {
                    *detail = "no class-name offsets configured";
                }
                return classPointers;
            }

            const std::vector<uintptr_t> nameStrings = FindAsciiStringAddresses(state, parts.className, 128);
            if (nameStrings.empty())
            {
                if (detail)
                {
                    *detail = "class-name string not found in readable target memory";
                }
                return classPointers;
            }

            std::vector<uintptr_t> namespaceStrings;

            const std::vector<uintptr_t> nameReferences = FindPointerReferences(state, nameStrings, 512);
            for (uintptr_t reference : nameReferences)
            {
                for (int nameOffset : nameOffsets)
                {
                    if (nameOffset < 0 || reference < static_cast<uintptr_t>(nameOffset))
                    {
                        continue;
                    }

                    const uintptr_t classPointer = reference - static_cast<uintptr_t>(nameOffset);
                    if (classPointer == 0 || (classPointer % sizeof(uintptr_t)) != 0)
                    {
                        continue;
                    }

                    const std::optional<uintptr_t> namePointer =
                        state.reader.Read<uintptr_t>(OffsetAddress(classPointer, nameOffset));
                    if (!namePointer || !ContainsAddress(nameStrings, *namePointer))
                    {
                        continue;
                    }

                    if (parts.hasNamespace)
                    {
                        std::vector<int> namespaceCandidates = namespaceOffsets;
                        AddUniqueInt(&namespaceCandidates, nameOffset + static_cast<int>(sizeof(uintptr_t)));

                        bool namespaceMatches = false;
                        for (int namespaceOffset : namespaceCandidates)
                        {
                            if (namespaceOffset < 0)
                            {
                                continue;
                            }

                            const std::optional<uintptr_t> namespacePointer =
                                state.reader.Read<uintptr_t>(OffsetAddress(classPointer, namespaceOffset));
                            if (!namespacePointer)
                            {
                                continue;
                            }

                            namespaceMatches =
                                ReadAsciiCString(state.reader, *namespacePointer, 256) == parts.namespaceName;
                            if (namespaceMatches)
                            {
                                break;
                            }
                        }

                        if (!namespaceMatches)
                        {
                            continue;
                        }
                    }

                    AddUniqueAddress(&classPointers, classPointer, maxClassPointers);
                    if (classPointers.size() >= maxClassPointers)
                    {
                        break;
                    }
                }

                if (classPointers.size() >= maxClassPointers)
                {
                    break;
                }
            }

            if (detail)
            {
                std::ostringstream stream;
                stream << "found " << classPointers.size()
                    << " candidate " << (runtimeLabel ? runtimeLabel : "class") << " pointer(s), "
                    << nameStrings.size() << " class-name string(s), "
                    << nameReferences.size() << " name reference(s)";
                if (parts.hasNamespace)
                {
                    stream << ", " << namespaceStrings.size() << " namespace string(s)";
                }
                *detail = stream.str();
            }

            return classPointers;
        }

        std::vector<uintptr_t> ResolveIl2CppClassPointersByName(
            const GuiState& state,
            const std::string& componentName,
            std::string* detail)
        {
            std::vector<int> preferredNameOffsets;
            AddUniqueInt(&preferredNameOffsets, state.il2cppClassNameOffset);
            AddUniqueInt(&preferredNameOffsets, 0x10);

            std::vector<int> preferredNamespaceOffsets;
            AddUniqueInt(&preferredNamespaceOffsets, state.il2cppClassNamespaceOffset);
            AddUniqueInt(&preferredNamespaceOffsets, 0x18);

            std::string preferredDetail;
            std::vector<uintptr_t> classPointers = ResolveClassPointersByName(
                state,
                componentName,
                preferredNameOffsets,
                preferredNamespaceOffsets,
                "Il2CppClass",
                32,
                &preferredDetail);

            if (!classPointers.empty())
            {
                if (detail)
                {
                    *detail = preferredDetail + " using preferred offsets";
                }
                return classPointers;
            }

            std::vector<int> nameOffsets = preferredNameOffsets;
            for (int offset : { 0x18, 0x20, 0x28, 0x30, 0x38, 0x40 })
            {
                AddUniqueInt(&nameOffsets, offset);
            }

            std::vector<int> namespaceOffsets = preferredNamespaceOffsets;
            for (int offset : { 0x20, 0x28, 0x30, 0x38, 0x40, 0x48 })
            {
                AddUniqueInt(&namespaceOffsets, offset);
            }

            std::string fallbackDetail;
            classPointers = ResolveClassPointersByName(
                state,
                componentName,
                nameOffsets,
                namespaceOffsets,
                "Il2CppClass",
                32,
                &fallbackDetail);
            if (detail)
            {
                *detail = fallbackDetail + " using fallback offsets; preferred offsets: " + preferredDetail;
            }
            return classPointers;
        }

        void AddIl2CppAutoClassTargets(GuiState* state, std::vector<ObjectCacheTarget>* targets, const std::string& componentName)
        {
            if (!state || !targets || !state->objectCacheAutoResolveIl2Cpp || !state->process ||
                state->process->modules.Backend() != RuntimeBackend::IL2CPP)
            {
                return;
            }

            const std::string label = TrimAscii(componentName);
            if (label.empty())
            {
                return;
            }

            std::string metadataDetail;
            const std::vector<std::string> resolveLabels =
                ComponentResolveLabelsFromMetadata(*state, label, 1, &metadataDetail);
            if (!metadataDetail.empty())
            {
                AddLog(state, "IL2CPP metadata priority for %s: %s", label.c_str(), metadataDetail.c_str());
            }

            for (const std::string& resolveLabel : resolveLabels)
            {
                std::string detail;
                const std::vector<uintptr_t> classPointers = ResolveIl2CppClassPointersByName(*state, resolveLabel, &detail);
                AddLog(state, "IL2CPP class scan for %s: %s", resolveLabel.c_str(), detail.c_str());

                for (uintptr_t classPointer : classPointers)
                {
                    AddUniqueObjectCacheTarget(targets, ObjectCacheTarget{
                        classPointer,
                        resolveLabel,
                        ComponentMetadataSummary(*state, resolveLabel),
                        "il2cpp class-name scan"
                    });
                }
            }
        }

        std::vector<uintptr_t> ResolveMonoClassPointersByName(
            const GuiState& state,
            const std::string& componentName,
            std::string* detail)
        {
            std::vector<int> nameOffsets;
            AddUniqueInt(&nameOffsets, state.monoClassNameOffset);
            for (int offset : { 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80 })
            {
                AddUniqueInt(&nameOffsets, offset);
            }

            std::vector<int> namespaceOffsets;
            AddUniqueInt(&namespaceOffsets, state.monoClassNamespaceOffset);
            for (int offset : { 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88 })
            {
                AddUniqueInt(&namespaceOffsets, offset);
            }

            return ResolveClassPointersByName(
                state,
                componentName,
                nameOffsets,
                namespaceOffsets,
                "MonoClass",
                64,
                detail);
        }

        std::vector<uintptr_t> ResolveMonoVTablePointersByClassPointers(
            const GuiState& state,
            const std::vector<uintptr_t>& classPointers,
            std::string* detail)
        {
            std::vector<uintptr_t> vtablePointers;
            if (classPointers.empty())
            {
                if (detail)
                {
                    *detail = "no MonoClass candidates";
                }
                return vtablePointers;
            }

            const std::vector<uintptr_t> classReferences = FindPointerReferences(state, classPointers, 1024);
            std::vector<int> vtableClassOffsets;
            AddUniqueInt(&vtableClassOffsets, state.monoVTableClassOffset);
            AddUniqueInt(&vtableClassOffsets, 0x0);

            for (uintptr_t reference : classReferences)
            {
                for (int classOffset : vtableClassOffsets)
                {
                    if (reference < static_cast<uintptr_t>(classOffset))
                    {
                        continue;
                    }

                    const uintptr_t vtablePointer = reference - static_cast<uintptr_t>(classOffset);
                    if (vtablePointer == 0 || (vtablePointer % sizeof(uintptr_t)) != 0)
                    {
                        continue;
                    }

                    const std::optional<uintptr_t> classPointer =
                        state.reader.Read<uintptr_t>(OffsetAddress(vtablePointer, classOffset));
                    if (!classPointer || !ContainsAddress(classPointers, *classPointer))
                    {
                        continue;
                    }

                    if (const std::optional<uintptr_t> cachedPointer =
                        state.reader.Read<uintptr_t>(OffsetAddress(vtablePointer, state.objectCachedPtrOffset));
                        cachedPointer &&
                        IsLikelyUnityNativePointer(vtablePointer, *cachedPointer) &&
                        IsReadableProcessRange(state.reader.ProcessHandle(), *cachedPointer, sizeof(uintptr_t)))
                    {
                        continue;
                    }

                    AddUniqueAddress(&vtablePointers, vtablePointer, 256);
                    if (vtablePointers.size() >= 256)
                    {
                        break;
                    }
                }

                if (vtablePointers.size() >= 256)
                {
                    break;
                }
            }

            if (detail)
            {
                std::ostringstream stream;
                stream << "found " << vtablePointers.size()
                    << " candidate MonoVTable pointer(s), "
                    << classPointers.size() << " MonoClass candidate(s), "
                    << classReferences.size() << " class reference(s)";
                *detail = stream.str();
            }

            return vtablePointers;
        }

        void AddMonoAutoVTableTargets(GuiState* state, std::vector<ObjectCacheTarget>* targets, const std::string& componentName)
        {
            if (!state || !targets || !state->objectCacheAutoResolveMono || !state->process ||
                state->process->modules.Backend() != RuntimeBackend::Mono)
            {
                return;
            }

            const std::string label = TrimAscii(componentName);
            if (label.empty())
            {
                return;
            }

            std::string metadataDetail;
            const std::vector<std::string> resolveLabels =
                ComponentResolveLabelsFromMetadata(*state, label, 1, &metadataDetail);
            if (!metadataDetail.empty())
            {
                AddLog(state, "Mono metadata priority for %s: %s", label.c_str(), metadataDetail.c_str());
            }

            for (const std::string& resolveLabel : resolveLabels)
            {
                std::string classDetail;
                const std::vector<uintptr_t> classPointers = ResolveMonoClassPointersByName(*state, resolveLabel, &classDetail);
                AddLog(state, "Mono class scan for %s: %s", resolveLabel.c_str(), classDetail.c_str());

                std::string vtableDetail;
                const std::vector<uintptr_t> vtablePointers = ResolveMonoVTablePointersByClassPointers(*state, classPointers, &vtableDetail);
                AddLog(state, "Mono vtable scan for %s: %s", resolveLabel.c_str(), vtableDetail.c_str());

                for (uintptr_t vtablePointer : vtablePointers)
                {
                    AddUniqueObjectCacheTarget(targets, ObjectCacheTarget{
                        vtablePointer,
                        resolveLabel,
                        ComponentMetadataSummary(*state, resolveLabel),
                        "mono class-name vtable scan"
                    });
                }
            }
        }

        void ScanUnityObjectIndexes(GuiState* state, const std::vector<ObjectCacheTarget>& targets)
        {
            if (!state)
            {
                return;
            }

            state->transformIndex.clear();
            state->gameObjectIndex.clear();
            state->cameraIndex.clear();
            state->unityObjectIndexStatus.clear();
            if (!state->autoBuildUnityObjectIndex ||
                targets.empty() ||
                !state->reader.IsOpen() ||
                !state->reader.ProcessHandle())
            {
                return;
            }

            std::vector<uintptr_t> targetPointers;
            targetPointers.reserve(targets.size());
            for (const ObjectCacheTarget& target : targets)
            {
                targetPointers.push_back(target.pointer);
            }
            targetPointers = SortedUniqueAddresses(std::move(targetPointers));
            if (targetPointers.empty())
            {
                return;
            }

            const std::size_t maxResults = static_cast<std::size_t>(
                std::clamp(state->unityObjectIndexMaxResults, 16, 65536));
            const ULONGLONG scanStart = GetTickCount64();
            const ULONGLONG scanBudget = static_cast<ULONGLONG>(
                std::clamp(state->objectCacheScanMaxMs, 1000, 300000));
            constexpr std::size_t kChunkSize = 1024 * 1024;
            std::size_t scannedRegions = 0;
            bool timedOut = false;

            uintptr_t address = 0;
            MEMORY_BASIC_INFORMATION mbi{};
            while ((state->transformIndex.size() < maxResults ||
                state->gameObjectIndex.size() < maxResults ||
                state->cameraIndex.size() < maxResults) &&
                VirtualQueryEx(state->reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                if (GetTickCount64() - scanStart > scanBudget)
                {
                    timedOut = true;
                    break;
                }

                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const uintptr_t next = base + mbi.RegionSize;
                if (next <= base)
                {
                    break;
                }

                if (mbi.State == MEM_COMMIT &&
                    IsAllowedMemoryType(mbi.Type, false) &&
                    IsReadableMemoryProtection(mbi.Protect))
                {
                    ++scannedRegions;
                    for (uintptr_t chunkBase = base; chunkBase < next;)
                    {
                        if (GetTickCount64() - scanStart > scanBudget)
                        {
                            timedOut = true;
                            break;
                        }

                        const std::size_t chunkSize = static_cast<std::size_t>(
                            std::min<std::uint64_t>(kChunkSize, next - chunkBase));
                        const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(chunkBase, chunkSize);
                        if (bytes.size() >= sizeof(uintptr_t))
                        {
                            for (std::size_t offset = 0; offset + sizeof(uintptr_t) <= bytes.size(); offset += sizeof(uintptr_t))
                            {
                                uintptr_t candidate = 0;
                                std::memcpy(&candidate, bytes.data() + offset, sizeof(candidate));
                                if (!std::binary_search(targetPointers.begin(), targetPointers.end(), candidate))
                                {
                                    continue;
                                }

                                const uintptr_t managedAddress = chunkBase + offset;
                                if (managedAddress == candidate || IsAddressNear(managedAddress, candidate, 0x1000000))
                                {
                                    continue;
                                }

                                const std::optional<uintptr_t> nativePointer =
                                    state->reader.Read<uintptr_t>(OffsetAddress(managedAddress, state->objectCachedPtrOffset));
                                if (!nativePointer ||
                                    !IsLikelyUnityNativeObjectPointer(*state, managedAddress, *nativePointer))
                                {
                                    continue;
                                }

                                const auto matchedTarget = std::find_if(targets.begin(), targets.end(), [candidate](const ObjectCacheTarget& target) {
                                    return target.pointer == candidate;
                                });
                                if (matchedTarget == targets.end())
                                {
                                    continue;
                                }

                                const bool isTransform = ContainsInsensitiveAscii(matchedTarget->label, "Transform");
                                const bool isGameObject = ContainsInsensitiveAscii(matchedTarget->label, "GameObject");
                                const bool isCamera = ContainsInsensitiveAscii(matchedTarget->label, "Camera");
                                ManagedNativeObject object;
                                object.managedAddress = managedAddress;
                                object.nativePointer = *nativePointer;
                                object.matchedPointer = candidate;
                                object.label = matchedTarget->label;
                                object.source = matchedTarget->source;
                                if (isTransform && state->transformIndex.size() < maxResults)
                                {
                                    AddUniqueManagedNativeObject(&state->transformIndex, std::move(object), maxResults);
                                }
                                else if (isGameObject && state->gameObjectIndex.size() < maxResults)
                                {
                                    AddUniqueManagedNativeObject(&state->gameObjectIndex, std::move(object), maxResults);
                                }
                                else if (isCamera && state->cameraIndex.size() < maxResults)
                                {
                                    AddUniqueManagedNativeObject(&state->cameraIndex, std::move(object), maxResults);
                                }
                            }
                        }

                        const uintptr_t chunkEnd = chunkBase + chunkSize;
                        if (chunkEnd >= next)
                        {
                            break;
                        }
                        chunkBase = chunkEnd;
                    }
                }

                if (timedOut)
                {
                    break;
                }

                address = next;
            }

            SortManagedNativeIndex(&state->transformIndex);
            SortManagedNativeIndex(&state->gameObjectIndex);
            SortManagedNativeIndex(&state->cameraIndex);

            std::ostringstream status;
            status << "Unity object index: " << state->transformIndex.size()
                << " Transform wrapper(s), " << state->gameObjectIndex.size()
                << " GameObject wrapper(s), " << state->cameraIndex.size()
                << " Camera wrapper(s)";
            if (timedOut)
            {
                status << " before " << scanBudget << " ms budget ended";
            }
            status << ", scanned " << scannedRegions << " readable region(s).";
            state->unityObjectIndexStatus = status.str();
            AddLog(state, "%s", state->unityObjectIndexStatus.c_str());
        }

        void RefreshUnityObjectIndex(GuiState* state)
        {
            if (!state || !state->process)
            {
                return;
            }

            state->transformIndex.clear();
            state->gameObjectIndex.clear();
            state->cameraIndex.clear();
            state->unityObjectIndexStatus.clear();
            if (!state->autoBuildUnityObjectIndex)
            {
                return;
            }

            const RuntimeBackend backend = state->process->modules.Backend();
            std::vector<ObjectCacheTarget> targets;
            if (backend == RuntimeBackend::IL2CPP)
            {
                AddIl2CppAutoClassTargets(state, &targets, "UnityEngine.Transform");
                AddIl2CppAutoClassTargets(state, &targets, "UnityEngine.GameObject");
                AddIl2CppAutoClassTargets(state, &targets, "UnityEngine.Camera");
            }
            else if (backend == RuntimeBackend::Mono)
            {
                AddMonoAutoVTableTargets(state, &targets, "UnityEngine.Transform");
                AddMonoAutoVTableTargets(state, &targets, "UnityEngine.GameObject");
                AddMonoAutoVTableTargets(state, &targets, "UnityEngine.Camera");
            }

            ScanUnityObjectIndexes(state, targets);
        }

        void RefreshObjectCache(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            state->objectCacheLastFullScanTick = GetTickCount64();
            state->objectCachePositionsReadThisFrame = 0;
            state->objectCachePositionFailuresThisFrame = 0;
            state->objectCache.clear();
            state->transformIndex.clear();
            state->gameObjectIndex.clear();
            state->cameraIndex.clear();
            state->fastTargets.clear();
            state->fastTargetsReadThisFrame = 0;
            state->fastTargetsFallbacksThisFrame = 0;
            state->fastTargetsHeldThisFrame = 0;
            state->fastTargetsFailedThisFrame = 0;
            state->objectCacheStatus.clear();
            state->unityObjectIndexStatus.clear();
            state->viewProjectionAutoStatus.clear();
            state->viewProjectionLastAutoScanTick = 0;

            if (!state->process ||
                (state->process->modules.Backend() != RuntimeBackend::Mono &&
                    state->process->modules.Backend() != RuntimeBackend::IL2CPP))
            {
                state->objectCacheStatus = "Attach to a Mono or IL2CPP target first.";
                AddLog(state, "Object cache scan skipped: target is not Mono or IL2CPP.");
                return;
            }

            if (!state->reader.IsOpen() || !state->reader.ProcessHandle())
            {
                state->objectCacheStatus = "Target memory is not open for reading.";
                AddLog(state, "Object cache scan skipped: no read handle.");
                return;
            }

            const RuntimeBackend backend = state->process->modules.Backend();
            const std::string backendName = WideToUtf8(RuntimeBackendName(backend));
            const std::string primaryLabel = TrimAscii(state->objectCacheComponentName[0] ? state->objectCacheComponentName : "UnityEngine.Rigidbody");
            const std::string fallbackLabel = TrimAscii(state->objectCacheFallbackComponentName[0] ? state->objectCacheFallbackComponentName : "UnityEngine.Rigidbody");

            std::vector<ObjectCacheTarget> targets;
            if (const std::optional<std::uint64_t> pointer = ParseUnsignedInteger(state->objectCachePointer);
                pointer && *pointer != 0)
            {
                AddUniqueObjectCacheTarget(&targets, ObjectCacheTarget{
                    static_cast<uintptr_t>(*pointer),
                    primaryLabel,
                    ComponentMetadataSummary(*state, primaryLabel),
                    backend == RuntimeBackend::IL2CPP ? "manual Il2CppClass/header pointer" : "manual Mono object/vtable pointer"
                });
            }

            if (state->objectCacheUseFallback)
            {
                if (const std::optional<std::uint64_t> pointer = ParseUnsignedInteger(state->objectCacheFallbackPointer);
                    pointer && *pointer != 0)
                {
                    AddUniqueObjectCacheTarget(&targets, ObjectCacheTarget{
                        static_cast<uintptr_t>(*pointer),
                        fallbackLabel,
                        ComponentMetadataSummary(*state, fallbackLabel),
                        backend == RuntimeBackend::IL2CPP ? "manual Il2CppClass/header pointer" : "manual Mono object/vtable pointer"
                    });
                }
            }

            const std::vector<std::string> autoResolveLabels =
                ObjectCacheResolveLabels(*state, backend, primaryLabel, fallbackLabel, state->objectCacheUseFallback);
            if (!autoResolveLabels.empty())
            {
                std::ostringstream labelsStream;
                labelsStream << "Object cache resolve labels:";
                for (const std::string& label : autoResolveLabels)
                {
                    labelsStream << " " << label;
                }
                AddLog(state, "%s", labelsStream.str().c_str());
            }
            if (backend == RuntimeBackend::IL2CPP)
            {
                for (const std::string& label : autoResolveLabels)
                {
                    AddIl2CppAutoClassTargets(state, &targets, label);
                }
            }
            else if (backend == RuntimeBackend::Mono)
            {
                for (const std::string& label : autoResolveLabels)
                {
                    AddMonoAutoVTableTargets(state, &targets, label);
                }
            }

            if (targets.empty())
            {
                state->objectCacheStatus =
                    backend == RuntimeBackend::IL2CPP
                    ? "No IL2CPP class pointer found. Paste an Il2CppClass/header pointer or adjust class offsets."
                    : "Type a Rigidbody object/vtable pointer first.";
                AddLog(state, "Object cache scan skipped: no component class/header pointer.");
                return;
            }

            RefreshUnityObjectIndex(state);

            const std::size_t maxResults = static_cast<std::size_t>(std::clamp(state->objectCacheMaxResults, 1, 4096));
            constexpr std::size_t kChunkSize = 1024 * 1024;
            std::size_t positionedCount = 0;
            const ULONGLONG scanStart = GetTickCount64();
            const ULONGLONG scanBudget = static_cast<ULONGLONG>(std::clamp(state->objectCacheScanMaxMs, 1000, 300000));
            bool scanTimedOut = false;
            std::vector<uintptr_t> targetPointers;
            targetPointers.reserve(targets.size());
            for (const ObjectCacheTarget& target : targets)
            {
                targetPointers.push_back(target.pointer);
            }
            targetPointers = SortedUniqueAddresses(std::move(targetPointers));

            uintptr_t address = 0;
            MEMORY_BASIC_INFORMATION mbi{};
            while (state->objectCache.size() < maxResults &&
                VirtualQueryEx(state->reader.ProcessHandle(), reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi))
            {
                if (GetTickCount64() - scanStart > scanBudget)
                {
                    scanTimedOut = true;
                    break;
                }

                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const uintptr_t next = base + mbi.RegionSize;
                if (next <= base)
                {
                    break;
                }

                if (mbi.State == MEM_COMMIT &&
                    IsAllowedMemoryType(mbi.Type, false) &&
                    IsReadableMemoryProtection(mbi.Protect))
                {
                    for (uintptr_t chunkBase = base; chunkBase < next && state->objectCache.size() < maxResults;)
                    {
                        if (GetTickCount64() - scanStart > scanBudget)
                        {
                            scanTimedOut = true;
                            break;
                        }

                        const std::size_t chunkSize = static_cast<std::size_t>(std::min<std::uint64_t>(kChunkSize, next - chunkBase));
                        const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(chunkBase, chunkSize);
                        if (bytes.size() >= sizeof(uintptr_t))
                        {
                            for (std::size_t offset = 0; offset + sizeof(uintptr_t) <= bytes.size(); offset += sizeof(uintptr_t))
                            {
                                uintptr_t candidate = 0;
                                std::memcpy(&candidate, bytes.data() + offset, sizeof(candidate));
                                if (!std::binary_search(targetPointers.begin(), targetPointers.end(), candidate))
                                {
                                    continue;
                                }

                                const auto matchedTarget = std::find_if(targets.begin(), targets.end(), [candidate](const ObjectCacheTarget& target) {
                                    return target.pointer == candidate;
                                });
                                if (matchedTarget == targets.end())
                                {
                                    continue;
                                }

                                if (chunkBase + offset == candidate)
                                {
                                    continue;
                                }

                                if (IsAddressNear(chunkBase + offset, matchedTarget->pointer, 0x1000000))
                                {
                                    continue;
                                }

                                if (state->objectCacheRequireCachedPtr)
                                {
                                    const std::optional<uintptr_t> nativePointer =
                                        state->reader.Read<uintptr_t>(OffsetAddress(chunkBase + offset, state->objectCachedPtrOffset));
                                    if (!nativePointer ||
                                        !IsLikelyUnityNativeObjectPointer(*state, chunkBase + offset, *nativePointer))
                                    {
                                        continue;
                                    }
                                }

                                ObjectCacheEntry entry;
                                entry.address = chunkBase + offset;
                                entry.matchedPointer = candidate;
                                entry.label = matchedTarget->label;
                                entry.source = matchedTarget->source;
                                entry.hasPosition = RefreshObjectCacheEntryPosition(*state, &entry);
                                if (ObjectCacheHasSamePositionSource(state->objectCache, entry))
                                {
                                    continue;
                                }
                                if (entry.hasPosition)
                                {
                                    ++positionedCount;
                                }
                                state->objectCache.push_back(std::move(entry));

                                if (state->objectCache.size() >= maxResults)
                                {
                                    break;
                                }
                            }
                        }

                        chunkBase += chunkSize;
                    }
                }

                if (scanTimedOut)
                {
                    break;
                }

                address = next;
            }

            if (positionedCount == 0 && state->objectPositionMode != 5 && !state->objectCache.empty())
            {
                const int previousMode = state->objectPositionMode;
                state->objectPositionMode = 5;
                for (ObjectCacheEntry& entry : state->objectCache)
                {
                    if (RefreshObjectCacheEntryPosition(*state, &entry))
                    {
                        ++positionedCount;
                    }
                }

                if (positionedCount > 0)
                {
                    AddLog(state, "Object cache position mode switched to auto probe after configured mode produced no positions.");
                }
                else
                {
                    state->objectPositionMode = previousMode;
                }
            }

            std::ostringstream status;
            status << backendName << " cached " << state->objectCache.size()
                << " live candidate object(s), " << positionedCount
                << " with readable positions.";
            if (scanTimedOut)
            {
                status << " Scan stopped at " << scanBudget << " ms; raise Object Cache Scan MS for deeper discovery.";
            }
            state->objectCacheStatus = status.str();
            AddLog(state, "%s", state->objectCacheStatus.c_str());
            const std::size_t loggedTargets = std::min<std::size_t>(targets.size(), 32);
            for (std::size_t index = 0; index < loggedTargets; ++index)
            {
                const ObjectCacheTarget& target = targets[index];
                AddLog(
                    state,
                    "  %s pointer %s [%s] (%s)",
                    target.label.c_str(),
                    FormatHex(target.pointer).c_str(),
                    target.source.c_str(),
                    target.metadata.empty() ? "metadata not loaded" : target.metadata.c_str());
            }
            if (targets.size() > loggedTargets)
            {
                AddLog(state, "  ... %zu more target pointer(s)", targets.size() - loggedTargets);
            }
        }

        void UpdateFastTargetOffsets(const GuiState& state, FastTrackedTarget* target)
        {
            if (!target)
            {
                return;
            }

            target->objectToPositionOffset = SignedAddressDelta(target->positionAddress, target->objectAddress);
            target->cachedPtr = 0;
            target->cachedPtrToPositionOffset = 0;

            if (const std::optional<uintptr_t> cachedPtr =
                state.reader.Read<uintptr_t>(OffsetAddress(target->objectAddress, state.objectCachedPtrOffset));
                cachedPtr && IsLikelyUnityNativeObjectPointer(state, target->objectAddress, *cachedPtr))
            {
                target->cachedPtr = *cachedPtr;
                target->cachedPtrToPositionOffset = SignedAddressDelta(target->positionAddress, *cachedPtr);
            }

            std::ostringstream stream;
            if (!target->positionRoute.empty())
            {
                stream << target->positionRoute << "; ";
            }
            stream << "pos " << FormatHex(target->positionAddress)
                << ", object" << FormatSignedHexOffset(target->objectToPositionOffset);
            if (target->positionFromTransform && target->transformBase != 0)
            {
                stream << ", transform " << FormatHex(target->transformBase);
            }
            if (target->cachedPtr != 0)
            {
                stream << ", m_CachedPtr " << FormatHex(target->cachedPtr)
                    << FormatSignedHexOffset(target->cachedPtrToPositionOffset);
            }
            if (target->positionShareCount > 1)
            {
                stream << ", shared x" << target->positionShareCount;
            }
            target->offsetSummary = stream.str();
        }

        int ScoreFastTargetCandidate(const GuiState& state, const FastTrackedTarget& target)
        {
            int score = 0;
            if (target.hasPosition)
            {
                score += 30;
            }

            if (target.positionFromTransform && target.transformBase != 0)
            {
                score += 150;
            }
            else if (ContainsInsensitiveAscii(target.positionRoute, "broad Vec3 scan"))
            {
                score -= 55;
            }
            else if (ContainsInsensitiveAscii(target.positionRoute, "Vec3"))
            {
                score -= 18;
            }

            if (ContainsInsensitiveAscii(target.source, "internal exported profile") ||
                ContainsInsensitiveAscii(target.positionRoute, "internal profile"))
            {
                score += 220;
            }

            if (target.positionShareCount <= 1)
            {
                score += 8;
            }
            else
            {
                score -= static_cast<int>(std::min<std::size_t>((target.positionShareCount - 1) * 6, 48));
            }

            const auto absOffset = [](std::int64_t value) -> std::uint64_t {
                return value < 0
                    ? static_cast<std::uint64_t>(-value)
                    : static_cast<std::uint64_t>(value);
            };

            if (target.positionAddress < 0x10000000)
            {
                score -= 45;
            }

            const std::uint64_t objectOffsetAbs = absOffset(target.objectToPositionOffset);
            if (objectOffsetAbs <= 0x10000)
            {
                score += 25;
            }
            else if (objectOffsetAbs <= 0x100000)
            {
                score += 12;
            }
            else
            {
                score -= 18;
            }

            if (target.cachedPtr != 0)
            {
                const std::uint64_t cachedPtrOffsetAbs = absOffset(target.cachedPtrToPositionOffset);
                if (cachedPtrOffsetAbs <= 0x400)
                {
                    score += 30;
                }
                else if (cachedPtrOffsetAbs <= 0x10000)
                {
                    score += 12;
                }
                else
                {
                    score -= 12;
                }
            }

            if (ContainsInsensitiveAscii(target.label, "player"))
            {
                score += 80;
            }
            if (ContainsInsensitiveAscii(target.label, "character"))
            {
                score += 55;
            }
            if (ContainsInsensitiveAscii(target.label, "controller"))
            {
                score += 45;
            }
            if (ContainsInsensitiveAscii(target.label, "local"))
            {
                score += 30;
            }
            if (ContainsInsensitiveAscii(target.label, "rigidbody"))
            {
                score += 10;
            }

            const float horizontalMagnitude =
                state.upAxis == 1
                ? std::sqrt(target.position.x * target.position.x + target.position.y * target.position.y)
                : std::sqrt(target.position.x * target.position.x + target.position.z * target.position.z);
            if (std::isfinite(horizontalMagnitude) && horizontalMagnitude <= 500.0f)
            {
                score += 10;
            }

            const float vertical =
                state.upAxis == 1
                ? target.position.z
                : target.position.y;
            if (std::isfinite(vertical) && vertical >= -10.0f && vertical <= 50.0f)
            {
                score += 12;
            }

            std::array<float, 16> matrix{};
            if (ReadMatrix4x4(state.reader, state.viewProjectionAddress, &matrix))
            {
                const ImVec2 screenSize =
                    ImGui::GetCurrentContext()
                    ? ImGui::GetIO().DisplaySize
                    : ImVec2(1920.0f, 1080.0f);
                ImVec2 feet{};
                ImVec2 head{};
                if (WorldToScreen(EntityFeetFromPosition(state, target.position), matrix, state.matrixLayout, screenSize, &feet) &&
                    WorldToScreen(EntityHeadFromPosition(state, target.position), matrix, state.matrixLayout, screenSize, &head))
                {
                    score += 25;
                    if (IsOnScreen(feet, screenSize) || IsOnScreen(head, screenSize))
                    {
                        score += 30;
                    }

                    const float centerDx = (feet.x - screenSize.x * 0.5f) / std::max(screenSize.x, 1.0f);
                    const float centerDy = (feet.y - screenSize.y * 0.5f) / std::max(screenSize.y, 1.0f);
                    const float centerDistance = std::sqrt(centerDx * centerDx + centerDy * centerDy);
                    if (std::isfinite(centerDistance))
                    {
                        score += static_cast<int>(std::clamp(0.45f - centerDistance, 0.0f, 0.45f) * 80.0f);
                    }

                    const float height = std::abs(feet.y - head.y);
                    if (height >= 8.0f && height <= screenSize.y * 0.8f)
                    {
                        score += 18;
                    }
                }
            }

            return score;
        }

        void RescoreFastTargets(GuiState* state)
        {
            if (!state || state->fastTargets.empty())
            {
                return;
            }

            for (FastTrackedTarget& target : state->fastTargets)
            {
                target.score = ScoreFastTargetCandidate(*state, target);
                target.likelyPlayer = false;
            }

            std::sort(state->fastTargets.begin(), state->fastTargets.end(), [](const FastTrackedTarget& left, const FastTrackedTarget& right) {
                if (left.score != right.score)
                {
                    return left.score > right.score;
                }
                if (left.positionShareCount != right.positionShareCount)
                {
                    return left.positionShareCount < right.positionShareCount;
                }
                return left.positionAddress < right.positionAddress;
            });

            state->fastTargets.front().likelyPlayer = true;
        }

        bool AddUniqueFastTarget(std::vector<FastTrackedTarget>* targets, FastTrackedTarget target, std::size_t maxTargets)
        {
            if (!targets || target.positionAddress == 0 || target.objectAddress == 0 || targets->size() >= maxTargets)
            {
                return false;
            }

            for (const FastTrackedTarget& existing : *targets)
            {
                if (existing.positionAddress == target.positionAddress ||
                    existing.objectAddress == target.objectAddress)
                {
                    return false;
                }
            }

            targets->push_back(std::move(target));
            return true;
        }

        void RebuildFastTargetsFromObjectCache(GuiState* state, bool logResult)
        {
            if (!state)
            {
                return;
            }

            state->fastTargets.clear();
            const std::size_t maxTargets = static_cast<std::size_t>(std::clamp(state->maxFastTargets, 1, 4096));
            for (ObjectCacheEntry& entry : state->objectCache)
            {
                if (!entry.hasPosition)
                {
                    RefreshObjectCacheEntryPosition(*state, &entry);
                }
            }

            const auto positionShareCount = [&](uintptr_t positionAddress) -> std::size_t {
                return static_cast<std::size_t>(std::count_if(state->objectCache.begin(), state->objectCache.end(), [&](const ObjectCacheEntry& entry) {
                    return entry.hasPosition && entry.positionAddress == positionAddress;
                }));
            };

            for (const ObjectCacheEntry& entry : state->objectCache)
            {
                if (!entry.hasPosition ||
                    entry.positionAddress == 0 ||
                    !CanUsePositionForVisuals(*state, entry.positionFromTransform, entry.positionRoute))
                {
                    continue;
                }

                FastTrackedTarget target;
                target.objectAddress = entry.address;
                target.matchedPointer = entry.matchedPointer;
                target.positionAddress = entry.positionAddress;
                target.transformBase = entry.transformBase;
                target.position = entry.position;
                target.lastGoodPosition = entry.position;
                target.hasPosition = true;
                target.hasLastGoodPosition = true;
                target.positionFromTransform = entry.positionFromTransform;
                target.positionRoute = entry.positionRoute;
                target.lastGoodReadTick = GetTickCount64();
                target.positionShareCount = std::max<std::size_t>(positionShareCount(entry.positionAddress), 1);
                target.label = entry.label;
                target.source = "fast target from " + entry.source;
                UpdateFastTargetOffsets(*state, &target);
                target.score = ScoreFastTargetCandidate(*state, target);
                AddUniqueFastTarget(&state->fastTargets, std::move(target), maxTargets);
            }

            RescoreFastTargets(state);

            if (logResult)
            {
                AddLog(
                    state,
                    "Built %zu fast tracked target(s) from object cache%s.",
                    state->fastTargets.size(),
                    state->fastTargets.empty() ? "" : "; highest score marked likely player");
            }
        }

        void UpdateFastTargetFromEntry(const GuiState& state, const ObjectCacheEntry& entry, FastTrackedTarget* target)
        {
            if (!target)
            {
                return;
            }

            target->objectAddress = entry.address;
            target->matchedPointer = entry.matchedPointer;
            target->positionAddress = entry.positionAddress;
            target->transformBase = entry.transformBase;
            target->position = entry.position;
            target->hasPosition = entry.hasPosition;
            target->positionFromTransform = entry.positionFromTransform;
            target->positionRoute = entry.positionRoute;
            if (entry.hasPosition)
            {
                target->lastGoodPosition = entry.position;
                target->hasLastGoodPosition = true;
                target->lastGoodReadTick = GetTickCount64();
            }
            target->positionShareCount = 1;
            target->label = entry.label;
            target->source = "fast target from " + entry.source;
            target->staleReads = 0;
            UpdateFastTargetOffsets(state, target);
            target->score = ScoreFastTargetCandidate(state, *target);
        }

        enum class FastTargetReadResult
        {
            Direct,
            Fallback,
            Held,
            Failed
        };

        FastTargetReadResult RefreshFastTargetPosition(GuiState* state, FastTrackedTarget* target)
        {
            if (!state || !target)
            {
                return FastTargetReadResult::Failed;
            }

            const ULONGLONG now = GetTickCount64();
            const ULONGLONG holdMs = static_cast<ULONGLONG>(
                std::clamp(state->fastTargetLastGoodMs, 0, 5000));
            const ULONGLONG fallbackProbeMs = static_cast<ULONGLONG>(
                std::clamp(state->fastTargetFallbackProbeMs, 50, 5000));
            const auto useLastGood = [&]() -> bool {
                if (!state->fastTargetsUseLastGood || !target->hasLastGoodPosition || holdMs == 0)
                {
                    return false;
                }
                if (target->lastGoodReadTick == 0 || now - target->lastGoodReadTick > holdMs)
                {
                    return false;
                }

                target->position = target->lastGoodPosition;
                target->hasPosition = true;
                return true;
            };

            if (target->positionAddress == 0)
            {
                ++target->staleReads;
                return useLastGood() ? FastTargetReadResult::Held : FastTargetReadResult::Failed;
            }

            bool objectHeaderMatches = true;
            if (target->matchedPointer != 0 && target->objectAddress != 0)
            {
                const std::optional<uintptr_t> header = state->reader.Read<uintptr_t>(target->objectAddress);
                objectHeaderMatches = header && *header == target->matchedPointer;
            }

            if (objectHeaderMatches &&
                target->positionFromTransform &&
                target->transformBase != 0)
            {
                Vec3 transformPosition{};
                uintptr_t transformPositionAddress = 0;
                if (ReadTransformAccessWorldPosition(*state, target->transformBase, &transformPosition, &transformPositionAddress))
                {
                    target->position = transformPosition;
                    target->positionAddress = transformPositionAddress;
                    target->hasPosition = true;
                    target->lastGoodPosition = transformPosition;
                    target->hasLastGoodPosition = true;
                    target->lastGoodReadTick = now;
                    target->staleReads = 0;
                    return FastTargetReadResult::Direct;
                }
            }

            const auto absOffset = [](std::int64_t value) -> std::uint64_t {
                return value < 0
                    ? static_cast<std::uint64_t>(-value)
                    : static_cast<std::uint64_t>(value);
            };
            const bool positionLooksIndirect =
                absOffset(target->objectToPositionOffset) > 0x100000 &&
                (target->cachedPtr == 0 || absOffset(target->cachedPtrToPositionOffset) > 0x100000);
            if (objectHeaderMatches &&
                positionLooksIndirect &&
                state->fastTargetsFallbackToCache &&
                target->objectAddress != 0)
            {
                ObjectCacheEntry fallback;
                fallback.address = target->objectAddress;
                fallback.matchedPointer = target->matchedPointer;
                fallback.label = target->label;
                fallback.source = "fast-target transform reprobe";
                if (RefreshObjectCacheEntryPosition(*state, &fallback))
                {
                    UpdateFastTargetFromEntry(*state, fallback, target);
                    return FastTargetReadResult::Fallback;
                }
            }

            uintptr_t directPositionAddress = target->positionAddress;
            Vec3 position{};
            uintptr_t readAddress = 0;
            if (objectHeaderMatches &&
                TryReadObjectPositionAt(*state, directPositionAddress, &position, &readAddress))
            {
                target->position = position;
                target->positionAddress = readAddress;
                target->hasPosition = true;
                target->lastGoodPosition = position;
                target->hasLastGoodPosition = true;
                target->lastGoodReadTick = now;
                target->staleReads = 0;
                return FastTargetReadResult::Direct;
            }

            ++target->staleReads;
            const bool shouldProbeFallback =
                objectHeaderMatches &&
                state->fastTargetsFallbackToCache &&
                target->objectAddress != 0 &&
                (target->lastFallbackProbeTick == 0 ||
                    target->staleReads == 1 ||
                    now - target->lastFallbackProbeTick >= fallbackProbeMs ||
                    !target->hasLastGoodPosition);

            if (shouldProbeFallback)
            {
                target->lastFallbackProbeTick = now;
                ObjectCacheEntry fallback;
                fallback.address = target->objectAddress;
                fallback.matchedPointer = target->matchedPointer;
                fallback.label = target->label;
                fallback.source = "fast-target fallback";
                if (RefreshObjectCacheEntryPosition(*state, &fallback))
                {
                    UpdateFastTargetFromEntry(*state, fallback, target);
                    return FastTargetReadResult::Fallback;
                }
            }

            if (useLastGood())
            {
                return FastTargetReadResult::Held;
            }

            target->hasPosition = false;
            return FastTargetReadResult::Failed;
        }

        void RefreshFastTargetLivePositions(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            state->fastTargetsReadThisFrame = 0;
            state->fastTargetsFallbacksThisFrame = 0;
            state->fastTargetsHeldThisFrame = 0;
            state->fastTargetsFailedThisFrame = 0;
            for (FastTrackedTarget& target : state->fastTargets)
            {
                switch (RefreshFastTargetPosition(state, &target))
                {
                case FastTargetReadResult::Direct:
                    ++state->fastTargetsReadThisFrame;
                    break;
                case FastTargetReadResult::Fallback:
                    ++state->fastTargetsReadThisFrame;
                    ++state->fastTargetsFallbacksThisFrame;
                    break;
                case FastTargetReadResult::Held:
                    ++state->fastTargetsHeldThisFrame;
                    break;
                case FastTargetReadResult::Failed:
                    ++state->fastTargetsFailedThisFrame;
                    break;
                }
            }
        }

        std::optional<std::vector<PatternByte>> ParseAobPattern(const std::string& text, std::string* error)
        {
            std::vector<PatternByte> pattern;
            std::istringstream stream(text);
            std::string token;
            while (stream >> token)
            {
                if (token == "?" || token == "??")
                {
                    pattern.push_back(PatternByte{ 0, true });
                    continue;
                }

                if (token.size() != 2 ||
                    !std::isxdigit(static_cast<unsigned char>(token[0])) ||
                    !std::isxdigit(static_cast<unsigned char>(token[1])))
                {
                    if (error)
                    {
                        *error = "Pattern tokens must be hex bytes or ? wildcards, e.g. 48 8B ?? ?? 89.";
                    }
                    return std::nullopt;
                }

                char* end = nullptr;
                const unsigned long value = std::strtoul(token.c_str(), &end, 16);
                if (!end || *end != '\0' || value > 0xFF)
                {
                    if (error)
                    {
                        *error = "Pattern byte was out of range.";
                    }
                    return std::nullopt;
                }

                pattern.push_back(PatternByte{ static_cast<std::uint8_t>(value), false });
            }

            if (pattern.empty())
            {
                if (error)
                {
                    *error = "Type an AOB pattern first.";
                }
                return std::nullopt;
            }

            return pattern;
        }

        bool PatternMatches(const std::vector<std::uint8_t>& bytes, std::size_t offset, const std::vector<PatternByte>& pattern)
        {
            if (offset + pattern.size() > bytes.size())
            {
                return false;
            }

            for (std::size_t index = 0; index < pattern.size(); ++index)
            {
                if (!pattern[index].wildcard && bytes[offset + index] != pattern[index].value)
                {
                    return false;
                }
            }

            return true;
        }

        const ModuleInfo* SelectedScanModule(const GuiState& state)
        {
            if (!state.process)
            {
                return nullptr;
            }

            const UnityRuntimeModules& modules = state.process->modules;
            switch (state.scanModule)
            {
            case 1:
                return modules.gameAssembly ? &*modules.gameAssembly : nullptr;
            case 2:
                return modules.unityPlayer ? &*modules.unityPlayer : nullptr;
            case 3:
                return modules.mono ? &*modules.mono : nullptr;
            default:
                if (modules.Backend() == RuntimeBackend::IL2CPP && modules.gameAssembly)
                {
                    return &*modules.gameAssembly;
                }
                if (modules.Backend() == RuntimeBackend::Mono && modules.mono)
                {
                    return &*modules.mono;
                }
                if (modules.gameAssembly)
                {
                    return &*modules.gameAssembly;
                }
                if (modules.mono)
                {
                    return &*modules.mono;
                }
                return modules.unityPlayer ? &*modules.unityPlayer : nullptr;
            }
        }

        void RunAobScan(GuiState* state)
        {
            if (!state || !state->reader.IsOpen())
            {
                AddLog(state, "Attach to a readable target before scanning.");
                return;
            }

            std::string error;
            const std::optional<std::vector<PatternByte>> pattern = ParseAobPattern(state->scanPattern, &error);
            if (!pattern)
            {
                AddLog(state, "AOB scan failed: %s", error.c_str());
                return;
            }

            const ModuleInfo* module = SelectedScanModule(*state);
            if (!module)
            {
                AddLog(state, "Selected scan module is not loaded in the target.");
                return;
            }

            state->scanResults.clear();
            constexpr std::size_t chunkSize = 1024 * 1024;
            constexpr std::size_t maxResults = 1024;
            const uintptr_t base = module->base;
            const std::size_t patternSize = pattern->size();
            const std::size_t moduleSize = module->size;

            for (std::size_t offset = 0; offset < moduleSize && state->scanResults.size() < maxResults; offset += chunkSize)
            {
                const std::size_t remaining = moduleSize - offset;
                const std::size_t requested = std::min(remaining, chunkSize + patternSize - 1);
                const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(base + offset, requested);
                if (bytes.size() < patternSize)
                {
                    continue;
                }

                for (std::size_t index = 0; index + patternSize <= bytes.size() && state->scanResults.size() < maxResults; ++index)
                {
                    if (PatternMatches(bytes, index, *pattern))
                    {
                        const uintptr_t address = base + offset + index;
                        state->scanResults.push_back(ScanResult{
                            address,
                            static_cast<std::uint32_t>(address - base),
                            WideToUtf8(module->name)
                        });
                    }
                }
            }

            AddLog(
                state,
                "AOB scan in %s found %zu result(s)%s.",
                WideToUtf8(module->name).c_str(),
                state->scanResults.size(),
                state->scanResults.size() == maxResults ? " (truncated)" : "");
        }

        HWND FindTargetWindow(DWORD pid)
        {
            if (pid == 0)
            {
                return nullptr;
            }

            struct FindWindowContext
            {
                DWORD pid = 0;
                HWND hwnd = nullptr;
            } context{ pid, nullptr };

            EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
                auto* context = reinterpret_cast<FindWindowContext*>(param);
                DWORD windowPid = 0;
                GetWindowThreadProcessId(hwnd, &windowPid);
                if (windowPid != context->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
                {
                    return TRUE;
                }

                RECT rect = {};
                GetWindowRect(hwnd, &rect);
                if ((rect.right - rect.left) < 80 || (rect.bottom - rect.top) < 80)
                {
                    return TRUE;
                }

                context->hwnd = hwnd;
                return FALSE;
            }, reinterpret_cast<LPARAM>(&context));

            return context.hwnd;
        }

        ImVec2 TargetClientSizeOrDefault(const GuiState& state, const ImVec2& fallback)
        {
            if (!state.process)
            {
                return fallback;
            }

            HWND targetWindow = FindTargetWindow(state.process->pid);
            if (!targetWindow)
            {
                return fallback;
            }

            RECT client = {};
            if (!GetClientRect(targetWindow, &client))
            {
                return fallback;
            }

            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            if (width <= 0 || height <= 0)
            {
                return fallback;
            }

            return ImVec2(static_cast<float>(width), static_cast<float>(height));
        }

        void AlignOverlayToTargetWindow(const GuiState& state)
        {
            if (!state.overlayMode || !state.alignToTargetWindow || !state.process)
            {
                return;
            }

            HWND targetWindow = FindTargetWindow(state.process->pid);
            if (!targetWindow)
            {
                return;
            }

            RECT client = {};
            if (!GetClientRect(targetWindow, &client))
            {
                return;
            }

            POINT topLeft{ client.left, client.top };
            ClientToScreen(targetWindow, &topLeft);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            if (width > 0 && height > 0)
            {
                RECT current = {};
                if (GetWindowRect(gWindow, &current) &&
                    current.left == topLeft.x &&
                    current.top == topLeft.y &&
                    (current.right - current.left) == width &&
                    (current.bottom - current.top) == height)
                {
                    return;
                }

                SetWindowPos(gWindow, HWND_TOPMOST, topLeft.x, topLeft.y, width, height, SWP_NOACTIVATE);
            }
        }

        void ApplyOverlayWindowStyle(const GuiState& state)
        {
            static bool initialized = false;
            static bool lastOverlayMode = false;
            static bool lastClickThrough = false;
            if (initialized && state.overlayMode == lastOverlayMode && state.clickThroughOverlay == lastClickThrough)
            {
                return;
            }

            LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_APPWINDOW;
            if (state.overlayMode)
            {
                exStyle |= WS_EX_TOPMOST;
                if (state.clickThroughOverlay)
                {
                    exStyle |= WS_EX_TRANSPARENT;
                }
            }

            LONG_PTR style = WS_POPUP;
            if (IsWindowVisible(gWindow))
            {
                style |= WS_VISIBLE;
            }
            SetWindowLongPtrW(gWindow, GWL_STYLE, style);
            SetWindowLongPtrW(gWindow, GWL_EXSTYLE, exStyle);
            SetLayeredWindowAttributes(gWindow, kTransparentWindowColorKey, 255, LWA_COLORKEY);
            SetWindowPos(
                gWindow,
                state.overlayMode ? HWND_TOPMOST : HWND_NOTOPMOST,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (state.overlayMode ? SWP_NOACTIVATE : 0));

            initialized = true;
            lastOverlayMode = state.overlayMode;
            lastClickThrough = state.clickThroughOverlay;
        }

        void MaybeAutoRebuildObjectCache(GuiState* state)
        {
            if (!state || !state->objectCacheAutoRebuild ||
                (state->entitySource != 1 && state->entitySource != 2) ||
                !state->process || !state->reader.IsOpen())
            {
                return;
            }

            const ULONGLONG now = GetTickCount64();
            const ULONGLONG interval = static_cast<ULONGLONG>(
                std::clamp(state->objectCacheAutoRebuildMs, 5000, 300000));
            if (state->objectCacheLastFullScanTick != 0 &&
                now - state->objectCacheLastFullScanTick < interval)
            {
                return;
            }

            AddLog(state, "Auto rebuilding object cache after %llu ms.", interval);
            RefreshObjectCache(state);
            if (state->autoBuildFastTargets)
            {
                RebuildFastTargetsFromObjectCache(state, true);
            }
        }

        void DrawExternalEspOverlay(GuiState* state)
        {
            if (!state || (!state->espEnabled && !state->radarEnabled))
            {
                return;
            }

            MaybeAutoRebuildObjectCache(state);

            const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
            std::array<float, 16> matrix = {};
            if (!ReadMatrix4x4(state->reader, state->viewProjectionAddress, &matrix) &&
                state->autoResolveViewProjection &&
                (state->entitySource == 1 || state->entitySource == 2) &&
                !state->objectCache.empty())
            {
                TryAutoConfigureViewProjectionMatrix(state, screenSize, false, state->viewProjectionScanMaxMs);
            }

            if (!ReadMatrix4x4(state->reader, state->viewProjectionAddress, &matrix))
            {
                const bool hasCacheDebug = state->entitySource == 1 && !state->objectCache.empty();
                const bool hasFastDebug = state->entitySource == 2 && !state->fastTargets.empty();
                if (hasCacheDebug || hasFastDebug)
                {
                    if (hasFastDebug)
                    {
                        RefreshFastTargetLivePositions(state);
                    }
                    else
                    {
                        RefreshObjectCacheLivePositions(state);
                    }

                    ImDrawList* drawList = ImGui::GetForegroundDrawList();
                    const ImU32 noticeColor = IM_COL32(255, 206, 92, 235);
                    const ImU32 lineColor = IM_COL32(255, 206, 92, 160);
                    drawList->AddText(
                        ImVec2(18.0f, 18.0f),
                        noticeColor,
                        hasFastDebug
                        ? "Fast targets are active, but ViewProjection Matrix is not configured. Debug lines are not world-to-screen."
                        : "Object cache is active, but ViewProjection Matrix is not configured. Debug lines are not world-to-screen.");
                    if (!state->viewProjectionAutoStatus.empty())
                    {
                        drawList->AddText(
                            ImVec2(18.0f, 36.0f),
                            noticeColor,
                            state->viewProjectionAutoStatus.c_str());
                    }

                    const std::size_t debugCount = hasFastDebug
                        ? std::min<std::size_t>(state->fastTargets.size(), 5)
                        : std::min<std::size_t>(state->objectCache.size(), 5);
                    for (std::size_t index = 0; index < debugCount; ++index)
                    {
                        const std::string label = hasFastDebug
                            ? (state->fastTargets[index].likelyPlayer ? "* " + state->fastTargets[index].label : state->fastTargets[index].label)
                            : state->objectCache[index].label;
                        const ImVec2 marker(80.0f + static_cast<float>(index) * 140.0f, 58.0f);
                        drawList->AddLine(ImVec2(screenSize.x * 0.5f, screenSize.y), marker, lineColor, 1.0f);
                        drawList->AddCircleFilled(marker, 4.0f, noticeColor, 12);
                        drawList->AddText(ImVec2(marker.x + 7.0f, marker.y - 8.0f), noticeColor, label.c_str());
                    }
                }
                return;
            }

            ReadEspEntities(state, matrix, screenSize);

            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const ImU32 boxColor = IM_COL32(0, 228, 190, 230);
            const ImU32 snapColor = IM_COL32(135, 214, 240, 200);
            const ImU32 radarColor = IM_COL32(0, 228, 190, 220);
            const ImU32 radarBg = IM_COL32(10, 18, 24, 180);

            if (state->espEnabled)
            {
                for (const EspEntity& entity : state->espEntities)
                {
                    if (!entity.onScreen)
                    {
                        continue;
                    }

                    const float topY = std::min(entity.head.y, entity.screen.y);
                    const float bottomY = std::max(entity.head.y, entity.screen.y);
                    const float height = bottomY - topY;
                    if (height < 2.0f || height > screenSize.y * 2.0f)
                    {
                        continue;
                    }

                    const float width = height * 0.6f;
                    const ImVec2 topLeft(entity.screen.x - width * 0.5f, topY);
                    const ImVec2 bottomRight(entity.screen.x + width * 0.5f, bottomY);

                    if (state->espBoxes)
                    {
                        drawList->AddRect(topLeft, bottomRight, boxColor, 0.0f, 0, 1.5f);
                    }
                    if (state->espSnaplines)
                    {
                        drawList->AddLine(ImVec2(screenSize.x * 0.5f, screenSize.y), entity.screen, snapColor, 1.2f);
                    }
                }
            }

            if (state->radarEnabled)
            {
                Vec3 local{};
                const std::optional<std::uint64_t> localAddress = ParseUnsignedInteger(state->localPositionAddress);
                if (localAddress)
                {
                    ReadVec3(state->reader, static_cast<uintptr_t>(*localAddress), &local);
                }

                const ImVec2 radarMin(state->radarPosX, state->radarPosY);
                const ImVec2 radarMax(state->radarPosX + state->radarSize, state->radarPosY + state->radarSize);
                const ImVec2 radarCenter((radarMin.x + radarMax.x) * 0.5f, (radarMin.y + radarMax.y) * 0.5f);
                drawList->AddRectFilled(radarMin, radarMax, radarBg, 6.0f);
                drawList->AddRect(radarMin, radarMax, boxColor, 6.0f, 0, 1.0f);
                drawList->AddLine(ImVec2(radarCenter.x, radarMin.y), ImVec2(radarCenter.x, radarMax.y), IM_COL32(255, 255, 255, 55));
                drawList->AddLine(ImVec2(radarMin.x, radarCenter.y), ImVec2(radarMax.x, radarCenter.y), IM_COL32(255, 255, 255, 55));

                const float range = std::max(state->radarRange, 1.0f);
                for (const EspEntity& entity : state->espEntities)
                {
                    const float dx = entity.position.x - local.x;
                    const float dy = state->upAxis == 1 ? entity.position.y - local.y : entity.position.z - local.z;
                    const float px = std::clamp(dx / range, -1.0f, 1.0f) * (state->radarSize * 0.5f);
                    const float py = std::clamp(dy / range, -1.0f, 1.0f) * (state->radarSize * 0.5f);
                    drawList->AddCircleFilled(ImVec2(radarCenter.x + px, radarCenter.y + py), 3.0f, radarColor, 12);
                }
            }
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
                { "UnityEngine.Shader::Find", "UnityEngine.CoreModule", "UnityEngine.Shader", "Find", 1 },
                { "UnityEngine.Time::get_timeScale", "UnityEngine.CoreModule", "UnityEngine.Time", "get_timeScale", 0 },
                { "UnityEngine.Time::set_timeScale", "UnityEngine.CoreModule", "UnityEngine.Time", "set_timeScale", 1 },
                { "UnityEngine.Camera::get_main", "UnityEngine.CoreModule", "UnityEngine.Camera", "get_main", 0 },
                { "UnityEngine.Camera::set_fieldOfView", "UnityEngine.CoreModule", "UnityEngine.Camera", "set_fieldOfView", 1 },
                { "UnityEngine.Camera::WorldToScreenPoint", "UnityEngine.CoreModule", "UnityEngine.Camera", "WorldToScreenPoint", 2 },
                { "UnityEngine.GameObject::Find", "UnityEngine.CoreModule", "UnityEngine.GameObject", "Find", 1 },
                { "UnityEngine.GameObject::GetComponent", "UnityEngine.CoreModule", "UnityEngine.GameObject", "GetComponent", 1 },
                { "UnityEngine.Component::get_transform", "UnityEngine.CoreModule", "UnityEngine.Component", "get_transform", 0 },
                { "UnityEngine.Transform::get_position", "UnityEngine.CoreModule", "UnityEngine.Transform", "get_position", 0 }
            };
        }

        void ApplyAegisTheme()
        {
            ImGui::StyleColorsDark();
            ImVec4* colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_Text] = ImVec4(0.86f, 0.92f, 0.96f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.53f, 0.59f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.065f, 0.075f, 0.96f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.105f, 0.140f, 0.180f, 0.48f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.080f, 0.110f, 0.145f, 0.98f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.090f, 0.125f, 0.165f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.125f, 0.170f, 0.215f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.145f, 0.205f, 0.250f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.090f, 0.125f, 0.165f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.090f, 0.125f, 0.165f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.090f, 0.125f, 0.165f, 0.80f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.090f, 0.125f, 0.165f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.060f, 0.080f, 0.105f, 0.35f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.240f, 0.310f, 0.360f, 0.85f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.000f, 0.690f, 0.600f, 0.85f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.590f, 0.840f, 0.940f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.105f, 0.140f, 0.180f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.145f, 0.200f, 0.245f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.000f, 0.500f, 0.455f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.120f, 0.170f, 0.215f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.145f, 0.220f, 0.265f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.000f, 0.500f, 0.455f, 1.00f);
            colors[ImGuiCol_Separator] = ImVec4(0.240f, 0.310f, 0.360f, 0.45f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.000f, 0.895f, 0.740f, 0.80f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.000f, 0.895f, 0.740f, 0.18f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.000f, 0.895f, 0.740f, 0.42f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.000f, 0.895f, 0.740f, 0.80f);
            colors[ImGuiCol_Tab] = ImVec4(0.105f, 0.140f, 0.180f, 1.00f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.145f, 0.220f, 0.265f, 1.00f);
            colors[ImGuiCol_TabActive] = ImVec4(0.130f, 0.205f, 0.250f, 1.00f);
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.080f, 0.110f, 0.145f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.105f, 0.150f, 0.190f, 1.00f);
            colors[ImGuiCol_PlotLines] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.590f, 0.840f, 0.940f, 1.00f);
            colors[ImGuiCol_PlotHistogram] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.590f, 0.840f, 0.940f, 1.00f);
            colors[ImGuiCol_TextSelectedBg] = ImVec4(0.000f, 0.895f, 0.740f, 0.22f);
            colors[ImGuiCol_DragDropTarget] = ImVec4(0.000f, 0.895f, 0.740f, 0.90f);
            colors[ImGuiCol_NavHighlight] = ImVec4(0.000f, 0.895f, 0.740f, 1.00f);
            colors[ImGuiCol_Border] = ImVec4(0.240f, 0.310f, 0.360f, 0.75f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);

            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowPadding = ImVec2(18, 14);
            style.FramePadding = ImVec2(10, 6);
            style.ItemSpacing = ImVec2(10, 9);
            style.ItemInnerSpacing = ImVec2(8, 6);
            style.IndentSpacing = 18;
            style.GrabMinSize = 9;
            style.ChildBorderSize = 1;
            style.PopupBorderSize = 1;
            style.FrameBorderSize = 0;
            style.WindowRounding = 7;
            style.ChildRounding = 6;
            style.FrameRounding = 5;
            style.PopupRounding = 6;
            style.ScrollbarRounding = 12;
            style.ScrollbarSize = 12;
            style.GrabRounding = 5;
            style.TabRounding = 5;
            style.WindowBorderSize = 1;
            style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        }

        void CreateRenderTarget()
        {
            ID3D11Texture2D* backBuffer = nullptr;
            gSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
            if (backBuffer)
            {
                gDevice->CreateRenderTargetView(backBuffer, nullptr, &gRenderTargetView);
                backBuffer->Release();
            }
        }

        void CleanupRenderTarget()
        {
            if (gRenderTargetView)
            {
                gRenderTargetView->Release();
                gRenderTargetView = nullptr;
            }
        }

        bool CreateDeviceD3D(HWND window)
        {
            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            swapChainDesc.BufferCount = 2;
            swapChainDesc.BufferDesc.Width = 0;
            swapChainDesc.BufferDesc.Height = 0;
            swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
            swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.OutputWindow = window;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.SampleDesc.Quality = 0;
            swapChainDesc.Windowed = TRUE;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            constexpr D3D_FEATURE_LEVEL featureLevelArray[2] = {
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_0
            };
            D3D_FEATURE_LEVEL featureLevel = {};

            const HRESULT result = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                0,
                featureLevelArray,
                2,
                D3D11_SDK_VERSION,
                &swapChainDesc,
                &gSwapChain,
                &gDevice,
                &featureLevel,
                &gDeviceContext);

            if (result == DXGI_ERROR_UNSUPPORTED)
            {
                return SUCCEEDED(D3D11CreateDeviceAndSwapChain(
                    nullptr,
                    D3D_DRIVER_TYPE_WARP,
                    nullptr,
                    0,
                    featureLevelArray,
                    2,
                    D3D11_SDK_VERSION,
                    &swapChainDesc,
                    &gSwapChain,
                    &gDevice,
                    &featureLevel,
                    &gDeviceContext));
            }

            if (FAILED(result))
            {
                return false;
            }

            CreateRenderTarget();
            return true;
        }

        void CleanupDeviceD3D()
        {
            CleanupRenderTarget();
            if (gSwapChain)
            {
                gSwapChain->Release();
                gSwapChain = nullptr;
            }
            if (gDeviceContext)
            {
                gDeviceContext->Release();
                gDeviceContext = nullptr;
            }
            if (gDevice)
            {
                gDevice->Release();
                gDevice = nullptr;
            }
        }

        LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            {
                return true;
            }

            switch (msg)
            {
            case WM_SIZE:
                if (wParam != SIZE_MINIMIZED && gDevice)
                {
                    CleanupRenderTarget();
                    gSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                    CreateRenderTarget();
                }
                return 0;
            case WM_SYSCOMMAND:
                if ((wParam & 0xfff0) == SC_KEYMENU)
                {
                    return 0;
                }
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                break;
            }

            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        void AttachProcess(GuiState* state, UnityProcess process)
        {
            if (!state)
            {
                return;
            }

            state->process = std::move(process);
            state->reader.Close();
            const bool hasReadAccess = state->reader.Open(state->process->pid);
            state->objectCache.clear();
            state->fastTargets.clear();
            state->espEntities.clear();
            state->objectCacheStatus.clear();
            state->viewProjectionAutoStatus.clear();
            state->objectCachePositionsReadThisFrame = 0;
            state->objectCachePositionFailuresThisFrame = 0;
            state->fastTargetsReadThisFrame = 0;
            state->fastTargetsFallbacksThisFrame = 0;
            state->fastTargetsHeldThisFrame = 0;
            state->fastTargetsFailedThisFrame = 0;
            state->resolver.emplace(state->process->modules);
            if (state->methodMap)
            {
                state->resolver->SetMethodMap(*state->methodMap);
            }

            CopyToBuffer(state->target, sizeof(state->target), WideToUtf8(state->process->executable));
            SaveGuiConfig(*state);

            AddLog(
                state,
                "Attached to %s [pid %lu], runtime=%s, read=%s",
                WideToUtf8(state->process->executable).c_str(),
                static_cast<unsigned long>(state->process->pid),
                WideToUtf8(RuntimeBackendName(state->process->modules.Backend())).c_str(),
                hasReadAccess ? "yes" : "no");
        }

        void AttachFromTargetInput(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            const std::wstring target = Trim(Utf8ToWide(state->target));
            std::optional<DWORD> pid;
            std::optional<std::wstring> executable;
            DWORD parsedPid = 0;
            if (TryParsePidText(target, &parsedPid))
            {
                pid = parsedPid;
            }
            else if (!target.empty())
            {
                executable = target;
            }

            if (!pid && (!executable || executable->empty()))
            {
                AddLog(state, "Type a process id or executable name first.");
                return;
            }

            state->scannedProcesses = EnumerateUnityProcesses(pid, executable);
            if (state->scannedProcesses.empty())
            {
                AddLog(state, "No matching process found for target '%s'.", state->target);
                return;
            }

            if (state->scannedProcesses.size() > 1)
            {
                AddLog(state, "Found %zu matching processes; attaching to the first row.", state->scannedProcesses.size());
            }

            AttachProcess(state, state->scannedProcesses.front());
        }

        void RefreshAttachedProcess(GuiState* state)
        {
            if (!state || !state->process)
            {
                AddLog(state, "No attached target to refresh.");
                return;
            }

            state->process->modules = InspectRuntimeModules(state->process->pid);
            state->resolver.emplace(state->process->modules);
            if (state->methodMap)
            {
                state->resolver->SetMethodMap(*state->methodMap);
            }

            AddLog(
                state,
                "Refreshed target modules. Runtime=%s UnityPlayer=%s GameAssembly=%s Mono=%s",
                WideToUtf8(RuntimeBackendName(state->process->modules.Backend())).c_str(),
                ModuleSummary(state->process->modules.unityPlayer).c_str(),
                ModuleSummary(state->process->modules.gameAssembly).c_str(),
                ModuleSummary(state->process->modules.mono).c_str());
        }

        void LoadMethodMap(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            const std::wstring path = Trim(Utf8ToWide(state->methodMapPath));
            if (path.empty())
            {
                AddLog(state, "Type a method map path first.");
                return;
            }

            MethodMap map;
            std::string error;
            if (!map.Load(path, &error))
            {
                AddLog(state, "Method map load failed: %s", error.c_str());
                return;
            }

            state->methodMap = map;
            if (state->resolver)
            {
                state->resolver->SetMethodMap(map);
            }
            SaveGuiConfig(*state);
            AddLog(state, "Loaded method map '%s' (%zu entries).", state->methodMapPath, state->methodMap->Count());
        }

        void ResolveExport(GuiState* state, const char* exportName)
        {
            if (!state || !state->resolver)
            {
                AddLog(state, "Attach to a target before resolving exports.");
                return;
            }

            const ResolveResult result = state->resolver->ResolveRuntimeExport(exportName ? exportName : "");
            if (result.value)
            {
                AddLog(state, "%s -> %s", exportName, FormatHex(result.value->address).c_str());
            }
            else
            {
                AddLog(state, "%s -> missing (%s)", exportName, result.error ? result.error->message.c_str() : "unknown error");
            }
        }

        void ResolveMethod(GuiState* state, const MethodQuery& query, const char* label)
        {
            if (!state || !state->resolver)
            {
                AddLog(state, "Attach to a target before resolving methods.");
                return;
            }

            const ResolveResult result = state->resolver->ResolveMethod(query);
            if (result.value)
            {
                AddLog(state, "%s -> %s", label, FormatResolvedValue(*result.value).c_str());
            }
            else
            {
                AddLog(state, "%s -> missing (%s)", label, result.error ? result.error->message.c_str() : "unknown error");
            }
        }

        void ResolveMethodInput(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            MethodQuery query;
            query.imageName = state->imageName;
            query.className = state->className;
            query.methodName = state->methodName;
            query.argumentCount = state->anyArgumentCount ? -1 : state->argumentCount;

            const std::string label = query.className + "::" + query.methodName;
            ResolveMethod(state, query, label.c_str());
        }

        void ResolveStartupAddresses(GuiState* state)
        {
            if (!state || !state->resolver)
            {
                AddLog(state, "Startup resolve skipped: no attached resolver.");
                return;
            }

            const RuntimeBackend backend = state->resolver->Backend();
            AddLog(state, "Startup resolving standard %s runtime exports...", WideToUtf8(RuntimeBackendName(backend)).c_str());
            const std::vector<std::string> exports = StandardExports(backend);
            if (exports.empty())
            {
                AddLog(state, "No standard export set exists for this runtime backend.");
            }
            for (const std::string& exportName : exports)
            {
                const ResolveResult result = state->resolver->ResolveRuntimeExport(exportName);
                if (result.value)
                {
                    AddLog(state, "  export %-38s %s", exportName.c_str(), FormatHex(result.value->address).c_str());
                }
                else
                {
                    AddLog(state, "  export %-38s missing: %s",
                        exportName.c_str(),
                        result.error ? result.error->message.c_str() : "unknown error");
                }
            }

            const std::vector<KnownMethod> methods = KnownUnityMethods(backend);
            if (!methods.empty())
            {
                if (backend == RuntimeBackend::IL2CPP && !state->resolver->HasMethodMap())
                {
                    AddLog(
                        state,
                        "IL2CPP managed method presets skipped: no method map/dump loaded. Runtime exports above are resolved.");
                    return;
                }

                if (backend == RuntimeBackend::Mono && !state->resolver->HasMethodMap())
                {
                    AddLog(
                        state,
                        "Mono managed method presets skipped: no metadata map loaded. Runtime exports above are resolved.");
                    return;
                }

                AddLog(state, "Startup resolving known Unity method presets...");
                for (const KnownMethod& method : methods)
                {
                    MethodQuery query;
                    query.imageName = method.imageName;
                    query.className = method.className;
                    query.methodName = method.methodName;
                    query.argumentCount = method.argumentCount;

                    const ResolveResult result = state->resolver->ResolveMethod(query);
                    if (result.value)
                    {
                        AddLog(state, "  method %-42s %s", method.label, FormatResolvedValue(*result.value).c_str());
                    }
                    else
                    {
                        AddLog(state, "  method %-42s missing: %s",
                            method.label,
                            result.error ? result.error->message.c_str() : "unknown error");
                    }
                }
            }
            else if (backend == RuntimeBackend::Mono)
            {
                AddLog(state, "Mono detected. Managed method metadata can be resolved from the auto-generated assembly map; direct target invocation remains external-only blocked.");
            }
        }

        void AddUniquePath(std::vector<std::filesystem::path>* paths, const std::filesystem::path& path)
        {
            if (!paths || path.empty())
            {
                return;
            }

            std::error_code error;
            const std::filesystem::path normalized = path.lexically_normal();
            const auto exists = std::find_if(paths->begin(), paths->end(), [&](const std::filesystem::path& existing) {
                return std::filesystem::equivalent(existing, normalized, error) ||
                    existing.lexically_normal().wstring() == normalized.wstring();
            });
            if (exists == paths->end())
            {
                paths->push_back(normalized);
            }
        }

        void AddDirectoryIfPresent(std::vector<std::filesystem::path>* directories, const std::filesystem::path& directory)
        {
            if (!directories || directory.empty())
            {
                return;
            }

            std::error_code error;
            if (std::filesystem::exists(directory, error) && std::filesystem::is_directory(directory, error))
            {
                AddUniquePath(directories, directory);
            }
        }

        std::vector<std::filesystem::path> StartupMethodMapCandidates(const GuiState& state)
        {
            std::vector<std::filesystem::path> directories;
            AddDirectoryIfPresent(&directories, ExternalExecutableDirectory());
            AddDirectoryIfPresent(&directories, std::filesystem::current_path());

            if (state.process)
            {
                if (!state.process->executablePath.empty())
                {
                    AddDirectoryIfPresent(&directories, std::filesystem::path(state.process->executablePath).parent_path());
                }
                if (state.process->modules.gameAssembly)
                {
                    AddDirectoryIfPresent(&directories, std::filesystem::path(state.process->modules.gameAssembly->path).parent_path());
                }
                if (state.process->modules.mono)
                {
                    AddDirectoryIfPresent(&directories, std::filesystem::path(state.process->modules.mono->path).parent_path());
                }
            }

            const std::array<const wchar_t*, 7> names = {
                L"methods.txt",
                L"method_map.txt",
                L"il2cpp_methods.txt",
                L"aegis_methods.txt",
                L"aegis_unity_methods.txt",
                L"script.json",
                L"dump.cs"
            };

            std::vector<std::filesystem::path> candidates;
            for (const std::filesystem::path& directory : directories)
            {
                for (const wchar_t* name : names)
                {
                    AddUniquePath(&candidates, directory / name);
                }
            }
            return candidates;
        }

        bool TryLoadMethodMapPath(GuiState* state, const std::wstring& path)
        {
            if (!state || path.empty())
            {
                return false;
            }

            CopyToBuffer(state->methodMapPath, sizeof(state->methodMapPath), WideToUtf8(path));
            MethodMap map;
            std::string error;
            if (!map.Load(path, &error))
            {
                AddLog(state, "Method map load failed for '%s': %s", WideToUtf8(path).c_str(), error.c_str());
                return false;
            }

            state->methodMap = map;
            if (state->resolver)
            {
                state->resolver->SetMethodMap(map);
            }
            SaveGuiConfig(*state);
            AddLog(state, "Loaded method map '%s' (%zu entries).", state->methodMapPath, state->methodMap->Count());
            return true;
        }

        bool TryGenerateIl2CppMethodMap(GuiState* state)
        {
            if (!state || !state->process || state->process->modules.Backend() != RuntimeBackend::IL2CPP)
            {
                return false;
            }

            AddLog(state, "No compatible map file found; attempting IL2CPP metadata auto-generation...");
            GeneratedIl2CppMethodMap generated = GenerateIl2CppMethodMap(*state->process);
            if (!generated.success)
            {
                AddLog(state, "IL2CPP metadata auto-generation failed: %s", generated.message.c_str());
                return false;
            }

            state->methodMap = generated.methodMap;
            if (state->resolver)
            {
                state->resolver->SetMethodMap(generated.methodMap);
            }

            const std::string displayPath = "auto:" + WideToUtf8(generated.metadataPath.wstring());
            CopyToBuffer(state->methodMapPath, sizeof(state->methodMapPath), displayPath);
            AddLog(
                state,
                "Generated IL2CPP method map from metadata (%zu entries, %zu/%zu modules matched): %s",
                generated.resolvedMethods,
                generated.matchedModules,
                generated.imageCount,
                WideToUtf8(generated.metadataPath.wstring()).c_str());
            return true;
        }

        bool TryGenerateMonoMethodMap(GuiState* state)
        {
            if (!state || !state->process || state->process->modules.Backend() != RuntimeBackend::Mono)
            {
                return false;
            }

            AddLog(state, "No compatible map file found; attempting Mono assembly metadata generation...");
            GeneratedMonoMethodMap generated = GenerateMonoMethodMap(*state->process);
            if (!generated.success)
            {
                AddLog(state, "Mono metadata generation failed: %s", generated.message.c_str());
                return false;
            }

            state->methodMap = generated.methodMap;
            if (state->resolver)
            {
                state->resolver->SetMethodMap(generated.methodMap);
            }

            const std::string displayPath = "auto:" + WideToUtf8(generated.managedDirectory.wstring());
            CopyToBuffer(state->methodMapPath, sizeof(state->methodMapPath), displayPath);
            AddLog(
                state,
                "Generated Mono metadata map (%zu methods, %zu types, %zu assemblies): %s",
                generated.methodCount,
                generated.typeCount,
                generated.assemblyCount,
                WideToUtf8(generated.managedDirectory.wstring()).c_str());
            return true;
        }

        void AutoLoadStartupMethodMap(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            const std::wstring savedPath = Trim(Utf8ToWide(state->methodMapPath));
            if (!savedPath.empty() && savedPath.rfind(L"auto:", 0) != 0 && TryLoadMethodMapPath(state, savedPath))
            {
                return;
            }

            const std::vector<std::filesystem::path> candidates = StartupMethodMapCandidates(*state);
            for (const std::filesystem::path& candidate : candidates)
            {
                std::error_code error;
                if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
                {
                    if (TryLoadMethodMapPath(state, candidate.wstring()))
                    {
                        return;
                    }
                }
            }

            if (TryGenerateIl2CppMethodMap(state))
            {
                return;
            }

            if (TryGenerateMonoMethodMap(state))
            {
                return;
            }

            AddLog(
                state,
                "No method map/dump auto-loaded after checking %zu candidates. Supported names include methods.txt, method_map.txt, script.json, and dump.cs.",
                candidates.size());
        }

        void PromptForStartupTarget(GuiState* state, bool forcePrompt)
        {
            if (!state || !forcePrompt)
            {
                return;
            }

            std::wcout << L"\nAegis Unity Universal External startup setup\n";
            std::wcout << L"Target exe or pid";
            if (state->target[0])
            {
                std::wcout << L" [" << Utf8ToWide(state->target) << L"]";
            }
            std::wcout << L": ";

            std::wstring input;
            if (std::getline(std::wcin, input))
            {
                input = Trim(input);
                if (!input.empty())
                {
                    CopyToBuffer(state->target, sizeof(state->target), WideToUtf8(input));
                }
            }
            else
            {
                AddLog(state, "Console input was not available; using saved/argument target if present.");
            }
        }

        void RunStartupSetup(GuiState* state, bool promptForTarget)
        {
            if (!state)
            {
                return;
            }

            AddLog(state, "Startup setup beginning.");
            PromptForStartupTarget(state, promptForTarget);

            if (!state->target[0])
            {
                AddLog(state, "Startup setup could not attach: no target was provided.");
                return;
            }

            AttachFromTargetInput(state);
            if (!state->process)
            {
                AddLog(state, "Startup setup could not attach to '%s'. GUI will open for manual retry.", state->target);
                return;
            }

            AutoLoadStartupMethodMap(state);
            ResolveStartupAddresses(state);
            RefreshObjectCache(state);
            if (!state->objectCache.empty())
            {
                if (state->autoBuildFastTargets)
                {
                    RebuildFastTargetsFromObjectCache(state, true);
                }
                state->entitySource = state->fastTargets.empty() ? 1 : 2;
                state->espEnabled = true;
                state->espSnaplines = true;
                state->overlayMode = true;
                state->alignToTargetWindow = true;
                AddLog(
                    state,
                    "Startup object cache populated; Visual entity source set to %s.",
                    state->entitySource == 2 ? "fast tracked positions" : "object cache");
                const ImVec2 targetSize = TargetClientSizeOrDefault(*state, ImVec2(1920.0f, 1080.0f));
                TryAutoConfigureViewProjectionMatrix(state, targetSize, false, state->viewProjectionScanMaxMs);
            }
            AddLog(state, "Startup setup finished. Opening GUI...");
        }

        std::string BytesToAscii(const std::vector<std::uint8_t>& bytes)
        {
            std::string text;
            text.reserve(bytes.size());
            for (std::uint8_t byte : bytes)
            {
                if (byte == 0)
                {
                    break;
                }
                text.push_back(byte >= 0x20 && byte < 0x7F ? static_cast<char>(byte) : '.');
            }
            return text;
        }

        std::string BytesToUtf16Preview(const std::vector<std::uint8_t>& bytes)
        {
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
            return WideToUtf8(text);
        }

        void LogHexDump(GuiState* state, uintptr_t address, const std::vector<std::uint8_t>& bytes)
        {
            constexpr std::size_t bytesPerLine = 16;
            for (std::size_t offset = 0; offset < bytes.size(); offset += bytesPerLine)
            {
                std::ostringstream line;
                line << FormatHex(address + offset) << ": ";
                for (std::size_t index = 0; index < bytesPerLine; ++index)
                {
                    if (offset + index < bytes.size())
                    {
                        line << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
                            << static_cast<unsigned int>(bytes[offset + index]) << ' ';
                    }
                    else
                    {
                        line << "   ";
                    }
                }

                line << " ";
                for (std::size_t index = 0; index < bytesPerLine && offset + index < bytes.size(); ++index)
                {
                    const std::uint8_t byte = bytes[offset + index];
                    line << (byte >= 0x20 && byte < 0x7F ? static_cast<char>(byte) : '.');
                }
                AddLog(state, "%s", line.str().c_str());
            }
        }

        template <typename T>
        void ReadIntegerValue(GuiState* state, uintptr_t address, const char* typeName)
        {
            const std::optional<T> value = state->reader.Read<T>(address);
            if (!value)
            {
                AddLog(state, "Read failed. GetLastError=%lu", static_cast<unsigned long>(state->reader.LastErrorCode()));
                return;
            }

            using UnsignedT = std::make_unsigned_t<T>;
            const auto raw = static_cast<unsigned long long>(static_cast<UnsignedT>(*value));
            if constexpr (std::is_signed_v<T>)
            {
                AddLog(state, "%s @ %s = %lld (%s)", typeName, FormatHex(address).c_str(), static_cast<long long>(*value), FormatHex(raw).c_str());
            }
            else
            {
                AddLog(state, "%s @ %s = %llu (%s)", typeName, FormatHex(address).c_str(), static_cast<unsigned long long>(*value), FormatHex(raw).c_str());
            }
        }

        template <typename T>
        void ReadFloatingValue(GuiState* state, uintptr_t address, const char* typeName)
        {
            const std::optional<T> value = state->reader.Read<T>(address);
            if (!value)
            {
                AddLog(state, "Read failed. GetLastError=%lu", static_cast<unsigned long>(state->reader.LastErrorCode()));
                return;
            }

            std::ostringstream stream;
            stream << typeName << " @ " << FormatHex(address) << " = "
                << std::setprecision(std::numeric_limits<T>::max_digits10) << *value;
            AddLog(state, "%s", stream.str().c_str());
        }

        template <typename T>
        std::optional<std::string> FormatIntegerRead(const ExternalMemoryReader& reader, uintptr_t address, const char* typeName)
        {
            const std::optional<T> value = reader.Read<T>(address);
            if (!value)
            {
                return std::nullopt;
            }

            using UnsignedT = std::make_unsigned_t<T>;
            const auto raw = static_cast<unsigned long long>(static_cast<UnsignedT>(*value));
            std::ostringstream stream;
            stream << typeName << " ";
            if constexpr (std::is_signed_v<T>)
            {
                stream << static_cast<long long>(*value);
            }
            else
            {
                stream << static_cast<unsigned long long>(*value);
            }
            stream << " (" << FormatHex(raw) << ")";
            return stream.str();
        }

        template <typename T>
        std::optional<std::string> FormatFloatingRead(const ExternalMemoryReader& reader, uintptr_t address, const char* typeName)
        {
            const std::optional<T> value = reader.Read<T>(address);
            if (!value)
            {
                return std::nullopt;
            }

            std::ostringstream stream;
            stream << typeName << " " << std::setprecision(std::numeric_limits<T>::max_digits10) << *value;
            return stream.str();
        }

        std::string FormatBytesPreview(const std::vector<std::uint8_t>& bytes)
        {
            std::ostringstream stream;
            stream << "bytes ";
            const std::size_t shown = std::min<std::size_t>(bytes.size(), 32);
            for (std::size_t index = 0; index < shown; ++index)
            {
                stream << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
                    << static_cast<unsigned int>(bytes[index]) << ' ';
            }
            if (bytes.size() > shown)
            {
                stream << "...";
            }
            return stream.str();
        }

        std::string FormatReadValue(const ExternalMemoryReader& reader, const std::string& addressText, int readType, int readSize)
        {
            if (!reader.IsOpen())
            {
                return "read unavailable";
            }

            const std::optional<std::uint64_t> parsedAddress = ParseUnsignedInteger(addressText);
            if (!parsedAddress)
            {
                return "invalid address";
            }

            const uintptr_t address = static_cast<uintptr_t>(*parsedAddress);
            const int size = std::clamp(readSize, 1, 4096);

            std::optional<std::string> formatted;
            switch (readType)
            {
            case 0:
            {
                const std::vector<std::uint8_t> bytes = reader.ReadBytes(address, static_cast<std::size_t>(size));
                formatted = bytes.empty() ? std::optional<std::string>() : FormatBytesPreview(bytes);
                break;
            }
            case 1:
                formatted = FormatIntegerRead<std::uint8_t>(reader, address, "u8");
                break;
            case 2:
                formatted = FormatIntegerRead<std::int8_t>(reader, address, "i8");
                break;
            case 3:
                formatted = FormatIntegerRead<std::uint16_t>(reader, address, "u16");
                break;
            case 4:
                formatted = FormatIntegerRead<std::int32_t>(reader, address, "i32");
                break;
            case 5:
                formatted = FormatIntegerRead<std::uint32_t>(reader, address, "u32");
                break;
            case 6:
                formatted = FormatIntegerRead<std::int64_t>(reader, address, "i64");
                break;
            case 7:
                formatted = FormatIntegerRead<std::uint64_t>(reader, address, "u64");
                break;
            case 8:
                formatted = FormatFloatingRead<float>(reader, address, "float");
                break;
            case 9:
                formatted = FormatFloatingRead<double>(reader, address, "double");
                break;
            case 10:
                formatted = FormatIntegerRead<uintptr_t>(reader, address, "ptr");
                break;
            case 11:
            {
                const std::vector<std::uint8_t> bytes = reader.ReadBytes(address, static_cast<std::size_t>(size));
                formatted = bytes.empty() ? std::optional<std::string>() : "ascii \"" + BytesToAscii(bytes) + "\"";
                break;
            }
            case 12:
            {
                const std::vector<std::uint8_t> bytes = reader.ReadBytes(address, static_cast<std::size_t>(size * 2));
                formatted = bytes.empty() ? std::optional<std::string>() : "utf16 \"" + BytesToUtf16Preview(bytes) + "\"";
                break;
            }
            default:
                return "unknown type";
            }

            return formatted ? *formatted : "read failed";
        }

        void RefreshWatches(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            for (WatchEntry& watch : state->watches)
            {
                watch.value = FormatReadValue(state->reader, watch.addressText, watch.type, watch.size);
            }
        }

        void AddWatch(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            const std::string address = state->watchAddress;
            if (!ParseUnsignedInteger(address))
            {
                AddLog(state, "Cannot add watch: invalid address.");
                return;
            }

            WatchEntry watch;
            watch.label = state->watchLabel[0] ? state->watchLabel : "watch";
            watch.addressText = address;
            watch.type = state->watchType;
            watch.size = std::clamp(state->watchSize, 1, 4096);
            watch.value = FormatReadValue(state->reader, watch.addressText, watch.type, watch.size);
            state->watches.push_back(std::move(watch));
            AddLog(state, "Added watch at %s.", address.c_str());
        }

        void ReadMemory(GuiState* state)
        {
            if (!state || !state->reader.IsOpen())
            {
                AddLog(state, "Process memory is not open for reading.");
                return;
            }

            const std::optional<std::uint64_t> parsedAddress = ParseUnsignedInteger(state->readAddress);
            if (!parsedAddress)
            {
                AddLog(state, "Invalid read address.");
                return;
            }

            const uintptr_t address = static_cast<uintptr_t>(*parsedAddress);
            const int size = std::clamp(state->readSize, 1, 4096);
            switch (state->readType)
            {
            case 0:
            {
                const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(address, static_cast<std::size_t>(size));
                if (bytes.empty())
                {
                    AddLog(state, "Read failed. GetLastError=%lu", static_cast<unsigned long>(state->reader.LastErrorCode()));
                    return;
                }
                LogHexDump(state, address, bytes);
                break;
            }
            case 1:
                ReadIntegerValue<std::uint8_t>(state, address, "u8");
                break;
            case 2:
                ReadIntegerValue<std::int8_t>(state, address, "i8");
                break;
            case 3:
                ReadIntegerValue<std::uint16_t>(state, address, "u16");
                break;
            case 4:
                ReadIntegerValue<std::int32_t>(state, address, "i32");
                break;
            case 5:
                ReadIntegerValue<std::uint32_t>(state, address, "u32");
                break;
            case 6:
                ReadIntegerValue<std::int64_t>(state, address, "i64");
                break;
            case 7:
                ReadIntegerValue<std::uint64_t>(state, address, "u64");
                break;
            case 8:
                ReadFloatingValue<float>(state, address, "float");
                break;
            case 9:
                ReadFloatingValue<double>(state, address, "double");
                break;
            case 10:
                ReadIntegerValue<uintptr_t>(state, address, "ptr");
                break;
            case 11:
            {
                const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(address, static_cast<std::size_t>(size));
                if (bytes.empty())
                {
                    AddLog(state, "Read failed. GetLastError=%lu", static_cast<unsigned long>(state->reader.LastErrorCode()));
                    return;
                }
                AddLog(state, "ascii @ %s = \"%s\"", FormatHex(address).c_str(), BytesToAscii(bytes).c_str());
                break;
            }
            case 12:
            {
                const std::vector<std::uint8_t> bytes = state->reader.ReadBytes(address, static_cast<std::size_t>(size * 2));
                if (bytes.empty())
                {
                    AddLog(state, "Read failed. GetLastError=%lu", static_cast<unsigned long>(state->reader.LastErrorCode()));
                    return;
                }
                AddLog(state, "utf16 @ %s = \"%s\"", FormatHex(address).c_str(), BytesToUtf16Preview(bytes).c_str());
                break;
            }
            default:
                AddLog(state, "Unknown read type.");
                break;
            }
        }

        void DrawTabButton(GuiState* state, const char* label, GuiTab tab)
        {
            const bool selected = state && state->tab == tab;
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }
            if (ImGui::Button(label) && state)
            {
                state->tab = tab;
            }
            if (selected)
            {
                ImGui::PopStyleColor();
            }
        }

        void DrawDisabledFeatureMirror()
        {
            ImGui::TextWrapped("These controls mirror the internal menu, but external mode cannot drive in-process overlay or gameplay loops.");
            ImGui::Separator();
        }

        void DrawVisualTab(GuiState* state)
        {
            ImGui::Text("External Read-Only ESP");
            ImGui::Checkbox("Enable External Overlay Mode", &state->overlayMode);
            ImGui::SameLine();
            ImGui::Checkbox("Align To Target Window", &state->alignToTargetWindow);
            ImGui::SameLine();
            ImGui::Checkbox("Click Through", &state->clickThroughOverlay);

            ImGui::Checkbox("Draw Visuals", &state->espEnabled);
            ImGui::SameLine();
            ImGui::Checkbox("Boxes", &state->espBoxes);
            ImGui::SameLine();
            ImGui::Checkbox("Snaplines", &state->espSnaplines);
            ImGui::SameLine();
            ImGui::Checkbox("Radar", &state->radarEnabled);

            const char* entitySources[] = { "Manual entity list", "Object cache", "Fast tracked positions" };
            ImGui::Combo("Entity Source", &state->entitySource, entitySources, IM_ARRAYSIZE(entitySources));
            if (state->entitySource == 1)
            {
                ImGui::TextWrapped("Object cache source re-reads cached object positions every rendered frame. Full cache rebuilds for newly spawned/despawned objects are configured in Developer.");
            }
            else if (state->entitySource == 2)
            {
                ImGui::TextWrapped("Fast tracked positions read pinned Vec3 addresses first and use object-cache probing only as a stale-target fallback.");
            }

            ImGui::InputTextWithHint("Entity List", "pointer array or first entity address", state->entityListAddress, sizeof(state->entityListAddress));
            ImGui::InputTextWithHint("Entity Count Address", "optional int count address", state->entityCountAddress, sizeof(state->entityCountAddress));
            ImGui::InputInt("Entity Count", &state->entityCount);
            const char* layouts[] = { "Pointer array", "Contiguous structs" };
            ImGui::Combo("Entity Layout", &state->entityLayout, layouts, IM_ARRAYSIZE(layouts));
            ImGui::InputInt("Entity Stride", &state->entityStride);
            ImGui::InputInt("Position Offset", &state->positionOffset);

            const char* positionModes[] = {
                "Direct Vec3: object + position offset",
                "Pointer field -> Vec3",
                "m_CachedPtr -> native Vec3",
                "m_CachedPtr -> native Transform ptr -> Vec3",
                "Reference field -> m_CachedPtr -> native Vec3",
                "Auto probe object/native Vec3"
            };
            ImGui::Combo("Object Cache Position Mode", &state->objectPositionMode, positionModes, IM_ARRAYSIZE(positionModes));
            ImGui::Checkbox("Draw Weak Auto-Probe Vec3", &state->allowWeakAutoProbeVisuals);
            ImGui::InputInt("Object Pointer Offset", &state->objectPointerOffset);
            ImGui::InputInt("m_CachedPtr Offset", &state->objectCachedPtrOffset);
            ImGui::InputInt("Transform Pointer Offset", &state->objectTransformPointerOffset);
            ImGui::InputFloat("Min Position Magnitude", &state->objectPositionMinMagnitude, 0.001f, 0.01f, "%.4f");
            ImGui::InputFloat("Max Abs Position", &state->objectPositionMaxAbs, 100.0f, 1000.0f, "%.0f");

            ImGui::InputTextWithHint("ViewProjection Matrix", "address of 16-float view-projection matrix", state->viewProjectionAddress, sizeof(state->viewProjectionAddress));
            ImGui::Checkbox("Auto Find ViewProjection", &state->autoResolveViewProjection);
            ImGui::SameLine();
            ImGui::Checkbox("Few-Sample Matrix Guess", &state->allowFewSampleViewProjectionGuess);
            ImGui::SameLine();
            if (ImGui::Button("Find ViewProjection"))
            {
                const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
                TryAutoConfigureViewProjectionMatrix(state, screenSize, true, std::max(state->viewProjectionScanMaxMs, 8000));
            }
            ImGui::InputInt("Matrix Scan Budget MS", &state->viewProjectionScanMaxMs);
            if (!state->viewProjectionAutoStatus.empty())
            {
                ImGui::TextWrapped("%s", state->viewProjectionAutoStatus.c_str());
            }
            const char* matrixLayouts[] = { "Row-major", "Column-major" };
            ImGui::Combo("Matrix Layout", &state->matrixLayout, matrixLayouts, IM_ARRAYSIZE(matrixLayouts));
            const char* upAxes[] = { "Y Up", "Z Up" };
            ImGui::Combo("Up Axis", &state->upAxis, upAxes, IM_ARRAYSIZE(upAxes));
            const char* positionAnchors[] = { "Root / Transform", "Feet" };
            ImGui::Combo("Position Anchor", &state->entityPositionAnchor, positionAnchors, IM_ARRAYSIZE(positionAnchors));
            ImGui::SliderFloat("Entity Height", &state->entityHeight, 0.1f, 4.0f, "%.2f");
            ImGui::InputFloat("Head Offset", &state->entityHeadOffset, 0.05f, 0.25f, "%.2f");
            ImGui::InputFloat("Feet Offset", &state->entityFeetOffset, 0.05f, 0.25f, "%.2f");

            ImGui::InputTextWithHint("Local Position", "optional Vec3 address for radar center", state->localPositionAddress, sizeof(state->localPositionAddress));
            ImGui::SliderFloat("Radar Range", &state->radarRange, 1.0f, 1000.0f, "%.0f");
            ImGui::SliderFloat("Radar Size", &state->radarSize, 60.0f, 360.0f, "%.0f");
            ImGui::SliderFloat("Radar X", &state->radarPosX, 0.0f, 1200.0f, "%.0f");
            ImGui::SameLine();
            ImGui::SliderFloat("Radar Y", &state->radarPosY, 0.0f, 900.0f, "%.0f");
            ImGui::Text(
                "Entities read this frame: %zu",
                state->espEntities.size());
            if (state->entitySource == 1)
            {
                ImGui::Text(
                    "Live cache positions: %zu ok / %zu failed",
                    state->objectCachePositionsReadThisFrame,
                    state->objectCachePositionFailuresThisFrame);
            }
            else if (state->entitySource == 2)
            {
                ImGui::Text(
                    "Fast targets: %zu direct/fallback ok / %zu held / %zu fallback / %zu failed",
                    state->fastTargetsReadThisFrame,
                    state->fastTargetsHeldThisFrame,
                    state->fastTargetsFallbacksThisFrame,
                    state->fastTargetsFailedThisFrame);
            }

            ImGui::Separator();
            DrawDisabledFeatureMirror();
            bool value = false;
            int slider = 0;
            float floatValue = 1.0f;
            ImVec4 color = ImVec4(1, 1, 1, 1);
            ImGui::BeginDisabled();
            ImGui::Checkbox("Players Snapline", &value);
            ImGui::SameLine();
            ImGui::ColorEdit3("##PlayersSnaplineColor", &color.x, ImGuiColorEditFlags_NoInputs);
            ImGui::Text("Snapline Type");
            ImGui::SameLine();
            ImGui::SliderInt("##PlayersSnaplineType", &slider, 0, 2);
            ImGui::Checkbox("Bot Checker", &value);
            ImGui::Checkbox("Players Box", &value);
            ImGui::SameLine();
            ImGui::ColorEdit3("##PlayersBoxColor", &color.x, ImGuiColorEditFlags_NoInputs);
            ImGui::Checkbox("Players Skeleton", &value);
            ImGui::Checkbox("Players Name", &value);
            ImGui::SameLine();
            ImGui::Checkbox("Players Distance", &value);
            ImGui::Checkbox("Player Count Overlay", &value);
            ImGui::Checkbox("Player List Overlay", &value);
            ImGui::Checkbox("Offscreen Arrows", &value);
            ImGui::Checkbox("Target Marker", &value);
            ImGui::Checkbox("2D Radar", &value);
            ImGui::Text("Radar Range");
            ImGui::SameLine();
            ImGui::SliderFloat("##RadarRange", &floatValue, 25.0f, 500.0f, "%.0fm");
            ImGui::Checkbox("Players Chams", &value);
            ImGui::EndDisabled();
        }

        void DrawAimTab()
        {
            DrawDisabledFeatureMirror();
            bool value = false;
            int mode = 0;
            float floatValue = 1.0f;
            const char* targetModes[] = { "First visible", "Closest crosshair" };
            ImGui::BeginDisabled();
            ImGui::Text("Aimbot Height");
            ImGui::Text("Head Diff Pos");
            ImGui::SameLine();
            ImGui::SliderFloat("##Head Pos", &floatValue, -10.0f, 30.0f);
            ImGui::Text("Feet Diff Pos");
            ImGui::SameLine();
            ImGui::SliderFloat("##Feet Pos", &floatValue, -10.0f, 30.0f);
            ImGui::Separator();
            ImGui::Checkbox("Enable Aimbot", &value);
            ImGui::Checkbox("Aimbot FOV Check", &value);
            ImGui::Text("Aimbot Target");
            ImGui::SameLine();
            ImGui::Combo("##AimbotTargetMode", &mode, targetModes, IM_ARRAYSIZE(targetModes));
            ImGui::Text("Aimbot FOV");
            ImGui::SameLine();
            ImGui::SliderFloat("##Aimbot FOV", &floatValue, 0.1f, 800.0f);
            ImGui::Checkbox("Filled FOV Ring", &value);
            ImGui::Text("Aimbot Smoothness");
            ImGui::SameLine();
            ImGui::SliderFloat("##Aimbot Smooth", &floatValue, 0.0f, 30.0f);
            ImGui::EndDisabled();
        }

        void DrawExploitsTab(GuiState* state)
        {
            ImGui::TextWrapped("External mode can resolve the Timescale method address from a map, but it does not call or patch it.");
            ImGui::Separator();
            static float gameSpeed = 1.0f;
            ImGui::BeginDisabled();
            ImGui::Text("Game timescale");
            ImGui::SliderFloat("##timescale", &gameSpeed, 0.0f, 100.0f);
            ImGui::Checkbox("Spinbot", &state->autoRefresh);
            ImGui::EndDisabled();

            if (ImGui::Button("Resolve UnityEngine.Time::set_timeScale"))
            {
                MethodQuery query;
                query.imageName = "UnityEngine.CoreModule";
                query.className = "UnityEngine.Time";
                query.methodName = "set_timeScale";
                query.argumentCount = 1;
                ResolveMethod(state, query, "UnityEngine.Time::set_timeScale");
            }
        }

        void DrawMiscTab(GuiState* state)
        {
            bool disabledValue = false;
            float disabledFloat = 1.0f;
            ImGui::Text("External Window");
            if (ImGui::Checkbox("Keep On Top", &state->keepOnTop))
            {
                SetWindowPos(gWindow, state->keepOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
            ImGui::Checkbox("Auto Refresh Target Modules", &state->autoRefresh);
            ImGui::Separator();
            DrawDisabledFeatureMirror();
            ImGui::BeginDisabled();
            ImGui::Checkbox("Show Watermark", &disabledValue);
            ImGui::Checkbox("Streamer Mode", &disabledValue);
            ImGui::Checkbox("Fun Mode", &disabledValue);
            ImGui::Checkbox("Status Overlay", &disabledValue);
            ImGui::Checkbox("Draw mouse", &disabledValue);
            ImGui::Checkbox("Crosshair", &disabledValue);
            ImGui::Text("Camera Custom FOV");
            ImGui::SameLine();
            ImGui::SliderFloat("##Camera Custom FOV", &disabledFloat, 1.0f, 300.0f);
            ImGui::EndDisabled();
        }

        void DrawTargetStatus(const GuiState& state)
        {
            if (!state.process)
            {
                ImGui::TextDisabled("No target attached.");
                return;
            }

            ImGui::Text("Target: %s [pid %lu]",
                WideToUtf8(state.process->executable).c_str(),
                static_cast<unsigned long>(state.process->pid));
            ImGui::Text("Runtime: %s", WideToUtf8(RuntimeBackendName(state.process->modules.Backend())).c_str());
            ImGui::Text("UnityPlayer: %s", ModuleSummary(state.process->modules.unityPlayer).c_str());
            ImGui::Text("GameAssembly: %s", ModuleSummary(state.process->modules.gameAssembly).c_str());
            ImGui::Text("Mono: %s", ModuleSummary(state.process->modules.mono).c_str());
            ImGui::Text("Read access: %s", state.reader.IsOpen() ? "yes" : "no");
            if (!state.process->executablePath.empty())
            {
                ImGui::TextWrapped("Path: %s", WideToUtf8(state.process->executablePath).c_str());
            }
        }

        void DrawUniversalTab(GuiState* state)
        {
            ImGui::Text("Target");
            ImGui::InputTextWithHint("##Target", "Game.exe or pid", state->target, sizeof(state->target));
            ImGui::SameLine();
            if (ImGui::Button("Attach"))
            {
                AttachFromTargetInput(state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh"))
            {
                RefreshAttachedProcess(state);
            }
            DrawTargetStatus(*state);

            ImGui::Separator();
            ImGui::Text("Runtime Export Resolver");
            ImGui::InputTextWithHint("##Export", "il2cpp_class_get_method_from_name", state->exportName, sizeof(state->exportName));
            ImGui::SameLine();
            if (ImGui::Button("Resolve Export"))
            {
                ResolveExport(state, state->exportName);
            }

            ImGui::Separator();
            ImGui::Text("Method Map");
            ImGui::InputTextWithHint("##MethodMap", "methods.txt", state->methodMapPath, sizeof(state->methodMapPath));
            ImGui::SameLine();
            if (ImGui::Button("Load Map"))
            {
                LoadMethodMap(state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Auto Generate"))
            {
                if (!TryGenerateIl2CppMethodMap(state) && !TryGenerateMonoMethodMap(state))
                {
                    AddLog(state, "No automatic method map generator is available for the current target/runtime.");
                }
            }
            ImGui::SameLine();
            ImGui::Text("Entries: %zu", state->methodMap ? state->methodMap->Count() : 0);

            ImGui::InputTextWithHint("Image", "UnityEngine.CoreModule", state->imageName, sizeof(state->imageName));
            ImGui::InputTextWithHint("Class", "UnityEngine.Time", state->className, sizeof(state->className));
            ImGui::InputTextWithHint("Method", "get_timeScale", state->methodName, sizeof(state->methodName));
            ImGui::InputInt("Argument Count", &state->argumentCount);
            ImGui::SameLine();
            ImGui::Checkbox("Any argc", &state->anyArgumentCount);
            if (ImGui::Button("Resolve Managed Method"))
            {
                ResolveMethodInput(state);
            }

            ImGui::Separator();
            ImGui::Text("Read-Only Memory");
            ImGui::InputTextWithHint("Address", "0x00000000", state->readAddress, sizeof(state->readAddress));
            const char* readTypes[] = {
                "bytes", "u8", "i8", "u16", "i32", "u32", "i64", "u64", "float", "double", "ptr", "ascii", "utf16"
            };
            ImGui::Combo("Type", &state->readType, readTypes, IM_ARRAYSIZE(readTypes));
            ImGui::InputInt("Size / Max Chars", &state->readSize);
            if (ImGui::Button("Read"))
            {
                ReadMemory(state);
            }

            ImGui::Separator();
            ImGui::Text("Read Watch List");
            ImGui::InputTextWithHint("Watch Label", "health / camera / pointer", state->watchLabel, sizeof(state->watchLabel));
            ImGui::InputTextWithHint("Watch Address", "0x00000000", state->watchAddress, sizeof(state->watchAddress));
            ImGui::Combo("Watch Type", &state->watchType, readTypes, IM_ARRAYSIZE(readTypes));
            ImGui::InputInt("Watch Size / Max Chars", &state->watchSize);
            if (ImGui::Button("Add Watch"))
            {
                AddWatch(state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh Watches"))
            {
                RefreshWatches(state);
            }

            if (ImGui::BeginTable("##WatchTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 120)))
            {
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();

                for (std::size_t index = 0; index < state->watches.size();)
                {
                    WatchEntry& watch = state->watches[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(watch.label.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(watch.addressText.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(readTypes[std::clamp(watch.type, 0, IM_ARRAYSIZE(readTypes) - 1)]);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(watch.value.c_str());
                    ImGui::TableSetColumnIndex(4);
                    bool removed = false;
                    if (ImGui::SmallButton("Remove"))
                    {
                        state->watches.erase(state->watches.begin() + static_cast<std::ptrdiff_t>(index));
                        removed = true;
                    }
                    ImGui::PopID();
                    if (!removed)
                    {
                        ++index;
                    }
                }
                ImGui::EndTable();
            }

            ImGui::Separator();
            ImGui::Text("Read-Only AOB Scan");
            ImGui::InputTextWithHint("Pattern", "48 8B ?? ?? 89", state->scanPattern, sizeof(state->scanPattern));
            const char* scanModules[] = { "Runtime module", "GameAssembly.dll", "UnityPlayer.dll", "Mono module" };
            ImGui::Combo("Module", &state->scanModule, scanModules, IM_ARRAYSIZE(scanModules));
            if (ImGui::Button("Scan Module"))
            {
                RunAobScan(state);
            }

            if (ImGui::BeginTable("##ScanResults", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 120)))
            {
                ImGui::TableSetupColumn("Module");
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("RVA", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();

                for (std::size_t index = 0; index < state->scanResults.size(); ++index)
                {
                    const ScanResult& result = state->scanResults[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(result.moduleName.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(FormatHex(result.address).c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(FormatHex(result.rva).c_str());
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::SmallButton("Use"))
                    {
                        CopyToBuffer(state->readAddress, sizeof(state->readAddress), FormatHex(result.address));
                        CopyToBuffer(state->watchAddress, sizeof(state->watchAddress), FormatHex(result.address));
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        void DrawProcessTable(GuiState* state)
        {
            if (ImGui::Button("Scan Unity Processes"))
            {
                state->scannedProcesses = EnumerateUnityProcesses(std::nullopt, std::nullopt);
                AddLog(state, "Scan found %zu Unity-looking process(es).", state->scannedProcesses.size());
            }

            if (ImGui::BeginTable("##ProcessTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 145)))
            {
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Exe");
                ImGui::TableSetupColumn("Runtime", ImGuiTableColumnFlags_WidthFixed, 85.0f);
                ImGui::TableSetupColumn("Modules");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();
                for (std::size_t index = 0; index < state->scannedProcesses.size(); ++index)
                {
                    UnityProcess& process = state->scannedProcesses[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%lu", static_cast<unsigned long>(process.pid));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(WideToUtf8(process.executable).c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(WideToUtf8(RuntimeBackendName(process.modules.Backend())).c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("UP:%s GA:%s Mono:%s",
                        process.modules.HasUnityPlayer() ? "y" : "n",
                        process.modules.HasGameAssembly() ? "y" : "n",
                        process.modules.HasMono() ? "y" : "n");
                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::SmallButton("Attach"))
                    {
                        AttachProcess(state, process);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        void DrawDeveloperTab(GuiState* state)
        {
            ImGui::Text("External Diagnostics");
            DrawTargetStatus(*state);
            ImGui::Text("Config: %s", WideToUtf8(ConfigPath().wstring()).c_str());
            ImGui::Separator();

            DrawProcessTable(state);
            ImGui::Separator();

            ImGui::Text("Runtime Object Cache");
            ImGui::TextWrapped("Mono scans for object/vtable pointers and can try to discover MonoVTables by readable class-name metadata. IL2CPP scans for Il2CppClass/header pointers.");
            if (ImGui::Button("Load Internal Profile"))
            {
                LoadInternalExternalProfile(state);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", WideToUtf8(SharedExternalProfilePath().wstring()).c_str());
            ImGui::InputTextWithHint("Primary Component##ObjectCacheComponent", "UnityEngine.Rigidbody", state->objectCacheComponentName, sizeof(state->objectCacheComponentName));
            ImGui::InputTextWithHint("Primary VTable/Class Pointer##ObjectCachePointer", "0x00000000", state->objectCachePointer, sizeof(state->objectCachePointer));
            ImGui::InputTextWithHint("Fallback Component##ObjectCacheFallbackComponent", "UnityEngine.Rigidbody", state->objectCacheFallbackComponentName, sizeof(state->objectCacheFallbackComponentName));
            ImGui::InputTextWithHint("Fallback VTable/Class Pointer##ObjectCacheFallbackPointer", "0x00000000", state->objectCacheFallbackPointer, sizeof(state->objectCacheFallbackPointer));
            ImGui::Checkbox("Use Fallback Component", &state->objectCacheUseFallback);
            ImGui::Checkbox("Require Readable m_CachedPtr", &state->objectCacheRequireCachedPtr);
            ImGui::Checkbox("Auto Resolve Mono VTables", &state->objectCacheAutoResolveMono);
            ImGui::Checkbox("Auto Resolve IL2CPP Class Pointers", &state->objectCacheAutoResolveIl2Cpp);
            ImGui::Checkbox("Deep Metadata Fallback Labels", &state->objectCacheUseMetadataFallbacks);
            ImGui::InputInt("Class Scan Budget MS", &state->classScanMaxMs);
            ImGui::InputInt("Object Cache Scan MS", &state->objectCacheScanMaxMs);
            ImGui::Checkbox("Build Unity Transform/GameObject Index", &state->autoBuildUnityObjectIndex);
            ImGui::InputInt("Unity Index Max Results", &state->unityObjectIndexMaxResults);
            ImGui::InputInt("Companion Scan Bytes", &state->nativeCompanionScanBytes);
            ImGui::Checkbox("Auto Rebuild Cache", &state->objectCacheAutoRebuild);
            ImGui::SameLine();
            ImGui::InputInt("Rebuild Interval MS", &state->objectCacheAutoRebuildMs);
            ImGui::Checkbox("Auto Build Fast Targets", &state->autoBuildFastTargets);
            ImGui::SameLine();
            ImGui::Checkbox("Fast Targets Cache Fallback", &state->fastTargetsFallbackToCache);
            ImGui::Checkbox("Smooth Fast Targets", &state->fastTargetsUseLastGood);
            ImGui::Checkbox("Allow Weak Auto-Probe Visuals", &state->allowWeakAutoProbeVisuals);
            ImGui::InputInt("Fast Hold Last Good MS", &state->fastTargetLastGoodMs);
            ImGui::InputInt("Fast Fallback Probe MS", &state->fastTargetFallbackProbeMs);
            ImGui::InputInt("Max Fast Targets", &state->maxFastTargets);
            ImGui::InputInt("MonoClass name offset", &state->monoClassNameOffset);
            ImGui::InputInt("MonoClass namespace offset", &state->monoClassNamespaceOffset);
            ImGui::InputInt("MonoVTable class offset", &state->monoVTableClassOffset);
            ImGui::InputInt("Il2CppClass name offset", &state->il2cppClassNameOffset);
            ImGui::InputInt("Il2CppClass namespace offset", &state->il2cppClassNamespaceOffset);
            ImGui::InputInt("Max Results##ObjectCacheMax", &state->objectCacheMaxResults);
            if (state->objectCacheAutoRebuild && state->objectCacheAutoRebuildMs < 5000)
            {
                ImGui::TextDisabled("Auto rebuild is clamped to at least 5000 ms because full memory scans are expensive.");
            }
            if (ImGui::Button("Refresh Object Cache"))
            {
                RefreshObjectCache(state);
                if (state->autoBuildFastTargets)
                {
                    RebuildFastTargetsFromObjectCache(state, true);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Build Fast Targets"))
            {
                RebuildFastTargetsFromObjectCache(state, true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Draw Cache In Visual"))
            {
                if (state->fastTargets.empty() && state->autoBuildFastTargets)
                {
                    RebuildFastTargetsFromObjectCache(state, true);
                }
                state->entitySource = state->fastTargets.empty() ? 1 : 2;
                state->espEnabled = true;
                state->espBoxes = true;
                state->espSnaplines = true;
                AddLog(
                    state,
                    "Visual entity source set to %s.",
                    state->entitySource == 2 ? "fast tracked positions" : "object cache");
            }
            if (!state->objectCacheStatus.empty())
            {
                ImGui::TextWrapped("%s", state->objectCacheStatus.c_str());
            }
            if (!state->unityObjectIndexStatus.empty())
            {
                ImGui::TextWrapped("%s", state->unityObjectIndexStatus.c_str());
            }
            ImGui::Text(
                "Live position reads: %zu ok / %zu failed",
                state->objectCachePositionsReadThisFrame,
                state->objectCachePositionFailuresThisFrame);
            ImGui::Text(
                "Fast target reads: %zu ok / %zu held / %zu fallback / %zu failed",
                state->fastTargetsReadThisFrame,
                state->fastTargetsHeldThisFrame,
                state->fastTargetsFallbacksThisFrame,
                state->fastTargetsFailedThisFrame);
            if (ImGui::BeginTable("##FastTargetTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 130)))
            {
                ImGui::TableSetupColumn("Likely", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Vec3");
                ImGui::TableSetupColumn("Offsets");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();
                for (std::size_t index = 0; index < state->fastTargets.size(); ++index)
                {
                    const FastTrackedTarget& target = state->fastTargets[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(target.likelyPlayer ? "yes" : "");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(target.label.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(FormatHex(target.objectAddress).c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(FormatHex(target.positionAddress).c_str());
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", target.score);
                    ImGui::TableSetColumnIndex(5);
                    if (target.hasPosition)
                    {
                        ImGui::Text("%.2f, %.2f, %.2f", target.position.x, target.position.y, target.position.z);
                    }
                    else
                    {
                        ImGui::TextDisabled("stale");
                    }
                    ImGui::TableSetColumnIndex(6);
                    ImGui::TextUnformatted(target.offsetSummary.c_str());
                    ImGui::TableSetColumnIndex(7);
                    if (ImGui::SmallButton("Use"))
                    {
                        CopyToBuffer(state->readAddress, sizeof(state->readAddress), FormatHex(target.positionAddress));
                        CopyToBuffer(state->watchAddress, sizeof(state->watchAddress), FormatHex(target.positionAddress));
                        CopyToBuffer(state->localPositionAddress, sizeof(state->localPositionAddress), FormatHex(target.positionAddress));
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (ImGui::BeginTable("##ObjectCacheTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 140)))
            {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Matched Pointer", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Route", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("Position");
                ImGui::TableSetupColumn("Position Address", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();
                for (std::size_t index = 0; index < state->objectCache.size(); ++index)
                {
                    const ObjectCacheEntry& entry = state->objectCache[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(entry.label.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(FormatHex(entry.address).c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(FormatHex(entry.matchedPointer).c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(entry.source.c_str());
                    ImGui::TableSetColumnIndex(4);
                    if (!entry.positionRoute.empty())
                    {
                        ImGui::TextUnformatted(entry.positionRoute.c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("n/a");
                    }
                    ImGui::TableSetColumnIndex(5);
                    if (entry.hasPosition)
                    {
                        ImGui::Text("%.2f, %.2f, %.2f", entry.position.x, entry.position.y, entry.position.z);
                    }
                    else
                    {
                        ImGui::TextDisabled("n/a");
                    }
                    ImGui::TableSetColumnIndex(6);
                    if (entry.hasPosition)
                    {
                        ImGui::TextUnformatted(FormatHex(entry.positionAddress).c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("n/a");
                    }
                    ImGui::TableSetColumnIndex(7);
                    if (ImGui::SmallButton("Use"))
                    {
                        const std::string address = FormatHex(entry.address);
                        CopyToBuffer(state->readAddress, sizeof(state->readAddress), address);
                        CopyToBuffer(state->watchAddress, sizeof(state->watchAddress), address);
                        if (entry.hasPosition)
                        {
                            CopyToBuffer(state->localPositionAddress, sizeof(state->localPositionAddress), FormatHex(entry.positionAddress));
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::Separator();

            ImGui::Text("Method Map Explorer");
            ImGui::InputTextWithHint("Filter##MethodMapFilter", "class / method / image", state->methodFilter, sizeof(state->methodFilter));
            if (!state->methodMap)
            {
                ImGui::TextDisabled("No method map loaded.");
            }
            else if (ImGui::BeginTable("##MethodMapExplorer", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 170)))
            {
                ImGui::TableSetupColumn("Image");
                ImGui::TableSetupColumn("Class");
                ImGui::TableSetupColumn("Method");
                ImGui::TableSetupColumn("Argc", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 85.0f);
                ImGui::TableSetupColumn("RVA/Token", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("VA", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();

                const std::string filter = state->methodFilter;
                const uintptr_t gameAssemblyBase =
                    state->process && state->process->modules.gameAssembly
                    ? state->process->modules.gameAssembly->base
                    : 0;
                int shown = 0;
                for (const MethodMap::Entry& entry : state->methodMap->Entries())
                {
                    const std::string searchable = entry.imageName + " " + entry.className + " " + entry.methodName;
                    if (!ContainsInsensitiveAscii(searchable, filter))
                    {
                        continue;
                    }

                    ImGui::PushID(shown);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(entry.imageName.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(entry.className.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(entry.methodName.c_str());
                    ImGui::TableSetColumnIndex(3);
                    if (entry.argumentCount < 0)
                    {
                        ImGui::TextUnformatted("*");
                    }
                    else
                    {
                        ImGui::Text("%d", entry.argumentCount);
                    }
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(MethodEntryKindName(entry.kind));
                    ImGui::TableSetColumnIndex(5);
                    if (entry.kind == MethodMapEntryKind::MonoMetadataToken)
                    {
                        ImGui::TextUnformatted(FormatHex(entry.metadataToken).c_str());
                        if (entry.rva != 0)
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("IL %s", FormatHex(entry.rva).c_str());
                        }
                    }
                    else
                    {
                        ImGui::TextUnformatted(FormatHex(entry.rva).c_str());
                    }
                    ImGui::TableSetColumnIndex(6);
                    if (entry.kind == MethodMapEntryKind::NativeRva && gameAssemblyBase)
                    {
                        ImGui::TextUnformatted(FormatHex(gameAssemblyBase + entry.rva).c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("n/a");
                    }
                    ImGui::TableSetColumnIndex(7);
                    if (ImGui::SmallButton("Use"))
                    {
                        CopyToBuffer(state->imageName, sizeof(state->imageName), entry.imageName);
                        CopyToBuffer(state->className, sizeof(state->className), entry.className);
                        CopyToBuffer(state->methodName, sizeof(state->methodName), entry.methodName);
                        state->argumentCount = entry.argumentCount;
                        state->anyArgumentCount = entry.argumentCount < 0;
                        if (entry.kind == MethodMapEntryKind::NativeRva && gameAssemblyBase)
                        {
                            const std::string address = FormatHex(gameAssemblyBase + entry.rva);
                            CopyToBuffer(state->readAddress, sizeof(state->readAddress), address);
                            CopyToBuffer(state->watchAddress, sizeof(state->watchAddress), address);
                        }
                    }
                    ImGui::PopID();

                    if (++shown >= 300)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("Showing first 300 matching entries.");
                        break;
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Separator();

            if (ImGui::Button("Resolve Standard Runtime Exports"))
            {
                if (!state->resolver)
                {
                    AddLog(state, "Attach to a target before resolving exports.");
                }
                else
                {
                    for (const std::string& exportName : StandardExports(state->resolver->Backend()))
                    {
                        ResolveExport(state, exportName.c_str());
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Resolve Known Unity Methods"))
            {
                if (!state->resolver)
                {
                    AddLog(state, "Attach to a target before resolving methods.");
                }
                else
                {
                    for (const KnownMethod& method : KnownUnityMethods(state->resolver->Backend()))
                    {
                        MethodQuery query;
                        query.imageName = method.imageName;
                        query.className = method.className;
                        query.methodName = method.methodName;
                        query.argumentCount = method.argumentCount;
                        ResolveMethod(state, query, method.label);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy Log"))
            {
                std::string text;
                for (const std::string& line : state->log)
                {
                    text += line;
                    text += "\n";
                }
                ImGui::SetClipboardText(text.c_str());
                AddLog(state, "Copied log to clipboard.");
            }

            if (ImGui::BeginChild("##ExternalLog", ImVec2(0, 125), true))
            {
                for (const std::string& line : state->log)
                {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }

        void DrawExternalMenu(GuiState* state)
        {
            if (!state)
            {
                return;
            }

            if (state->autoRefresh && state->process)
            {
                static double nextRefresh = 0.0;
                const double now = ImGui::GetTime();
                if (now >= nextRefresh)
                {
                    RefreshAttachedProcess(state);
                    RefreshWatches(state);
                    nextRefresh = now + 2.0;
                }
            }

            ImGui::SetNextWindowPos(ImVec2(120, 90), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(720, 460), ImGuiCond_Once);
            if (ImGui::Begin("Aegis Unity Universal", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
            {
                DrawTabButton(state, "Visual", GuiTab::Visual);
                ImGui::SameLine();
                DrawTabButton(state, "Aim", GuiTab::Aim);
                ImGui::SameLine();
                DrawTabButton(state, "Exploits", GuiTab::Exploits);
                ImGui::SameLine();
                DrawTabButton(state, "Misc", GuiTab::Misc);
                ImGui::SameLine();
                DrawTabButton(state, "Universal", GuiTab::Universal);
                ImGui::SameLine();
                DrawTabButton(state, "Developer", GuiTab::Developer);

                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginChild("##TabContent", ImVec2(0, 0), false))
                {
                    switch (state->tab)
                    {
                    case GuiTab::Visual:
                        DrawVisualTab(state);
                        break;
                    case GuiTab::Aim:
                        DrawAimTab();
                        break;
                    case GuiTab::Exploits:
                        DrawExploitsTab(state);
                        break;
                    case GuiTab::Misc:
                        DrawMiscTab(state);
                        break;
                    case GuiTab::Universal:
                        DrawUniversalTab(state);
                        break;
                    case GuiTab::Developer:
                        DrawDeveloperTab(state);
                        break;
                    default:
                        break;
                    }
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
    }

    int RunExternalObjectCacheDiagnostic(
        const std::wstring& target,
        const std::string& primaryComponent,
        const std::string& fallbackComponent)
    {
        GuiState state;
        CopyToBuffer(state.target, sizeof(state.target), WideToUtf8(target));
        if (!primaryComponent.empty())
        {
            CopyToBuffer(state.objectCacheComponentName, sizeof(state.objectCacheComponentName), primaryComponent);
        }
        state.objectPositionMode = 5;
        state.objectCacheMaxResults = 32;
        if (!fallbackComponent.empty())
        {
            CopyToBuffer(state.objectCacheFallbackComponentName, sizeof(state.objectCacheFallbackComponentName), fallbackComponent);
            state.objectCacheUseFallback = true;
        }
        else
        {
            state.objectCacheUseFallback = false;
        }

        AttachFromTargetInput(&state);
        if (!state.process)
        {
            std::wcout << L"Object cache diagnostic failed: target was not attached.\n";
            return 1;
        }

        AutoLoadStartupMethodMap(&state);
        RefreshObjectCache(&state);
        RebuildFastTargetsFromObjectCache(&state, true);
        const ImVec2 targetSize = TargetClientSizeOrDefault(state, ImVec2(1920.0f, 1080.0f));
        TryAutoConfigureViewProjectionMatrix(&state, targetSize, true, 12000);

        std::size_t transformSampleCount = 0;
        Vec3 firstTransformSample{};
        uintptr_t firstTransformBase = 0;
        uintptr_t firstTransformPositionAddress = 0;
        const std::size_t transformProbeLimit = std::min<std::size_t>(state.transformIndex.size(), 128);
        for (std::size_t index = 0; index < transformProbeLimit; ++index)
        {
            Vec3 sample{};
            uintptr_t sampleAddress = 0;
            if (ReadTransformAccessWorldPosition(state, state.transformIndex[index].nativePointer, &sample, &sampleAddress))
            {
                if (transformSampleCount == 0)
                {
                    firstTransformSample = sample;
                    firstTransformBase = state.transformIndex[index].nativePointer;
                    firstTransformPositionAddress = sampleAddress;
                }
                ++transformSampleCount;
            }
        }

        std::wcout
            << L"\nObject cache diagnostic\n"
            << L"Target: " << state.process->executable << L" [pid " << state.process->pid << L"]\n"
            << L"Runtime: " << RuntimeBackendName(state.process->modules.Backend()) << L"\n"
            << L"Status: " << Utf8ToWide(state.objectCacheStatus) << L"\n"
            << L"Unity Index: " << Utf8ToWide(state.unityObjectIndexStatus) << L"\n"
            << L"Transform Samples: " << transformSampleCount << L"/" << transformProbeLimit;
        if (transformSampleCount > 0)
        {
            std::wcout
                << L" first=" << Utf8ToWide(FormatHex(firstTransformBase))
                << L" pos=(" << firstTransformSample.x
                << L", " << firstTransformSample.y
                << L", " << firstTransformSample.z
                << L") @ " << Utf8ToWide(FormatHex(firstTransformPositionAddress));
        }
        std::wcout
            << L"\n"
            << L"Matrix: " << Utf8ToWide(state.viewProjectionAutoStatus) << L"\n";
        const std::size_t transformDebugShown = std::min<std::size_t>(state.transformIndex.size(), 3);
        for (std::size_t index = 0; index < transformDebugShown; ++index)
        {
            const ManagedNativeObject& transform = state.transformIndex[index];
            const auto ptr10 = state.reader.Read<uintptr_t>(OffsetAddress(transform.nativePointer, 0x10));
            const auto ptr38 = state.reader.Read<uintptr_t>(OffsetAddress(transform.nativePointer, 0x38));
            const auto idx40 = state.reader.Read<int>(OffsetAddress(transform.nativePointer, 0x40));
            std::optional<uintptr_t> matrixList;
            std::optional<uintptr_t> parentList;
            if (ptr38)
            {
                matrixList = state.reader.Read<uintptr_t>(OffsetAddress(*ptr38, 0x18));
                parentList = state.reader.Read<uintptr_t>(OffsetAddress(*ptr38, 0x20));
            }
            std::wcout
                << L"  transform-index[" << index << L"] managed="
                << Utf8ToWide(FormatHex(transform.managedAddress))
                << L" native=" << Utf8ToWide(FormatHex(transform.nativePointer))
                << L" native+10=" << Utf8ToWide(ptr10 ? FormatHex(*ptr10) : std::string("n/a"))
                << L" native+38=" << Utf8ToWide(ptr38 ? FormatHex(*ptr38) : std::string("n/a"))
                << L" native+40=";
            if (idx40)
            {
                std::wcout << *idx40;
            }
            else
            {
                std::wcout << L"n/a";
            }
            std::wcout
                << L" matrices=" << Utf8ToWide(matrixList ? FormatHex(*matrixList) : std::string("n/a"))
                << L" parents=" << Utf8ToWide(parentList ? FormatHex(*parentList) : std::string("n/a"));
            std::wcout << L"\n";
        }
        if (state.viewProjectionAddress[0])
        {
            std::wcout
                << L"Matrix Address: " << Utf8ToWide(state.viewProjectionAddress)
                << L" [" << (state.matrixLayout == 0 ? L"row-major" : L"column-major") << L"]\n";
        }

        const std::size_t fastShown = std::min<std::size_t>(state.fastTargets.size(), 16);
        for (std::size_t index = 0; index < fastShown; ++index)
        {
            const FastTrackedTarget& targetEntry = state.fastTargets[index];
            std::wcout
                << L"  fast[" << index << L"] "
                << (targetEntry.likelyPlayer ? L"likely " : L"")
                << Utf8ToWide(targetEntry.label)
                << L" score=" << targetEntry.score
                << L" object=" << Utf8ToWide(FormatHex(targetEntry.objectAddress))
                << L" pos@" << Utf8ToWide(FormatHex(targetEntry.positionAddress))
                << (targetEntry.positionFromTransform ? L" transform-backed" : L" direct-vec3")
                << L" route=" << Utf8ToWide(targetEntry.positionRoute.empty() ? std::string("n/a") : targetEntry.positionRoute)
                << L" offsets=" << Utf8ToWide(targetEntry.offsetSummary)
                << L"\n";
        }
        if (state.fastTargets.size() > fastShown)
        {
            std::wcout << L"  ... " << (state.fastTargets.size() - fastShown) << L" more fast target(s)\n";
        }

        const std::size_t shown = std::min<std::size_t>(state.objectCache.size(), 32);
        for (std::size_t index = 0; index < shown; ++index)
        {
            const ObjectCacheEntry& entry = state.objectCache[index];
            std::wcout
                << L"  [" << index << L"] "
                << Utf8ToWide(entry.label)
                << L" object=" << Utf8ToWide(FormatHex(entry.address))
                << L" matched=" << Utf8ToWide(FormatHex(entry.matchedPointer))
                << L" source=" << Utf8ToWide(entry.source);
            if (entry.hasPosition)
            {
                std::wcout
                    << L" pos=(" << entry.position.x
                    << L", " << entry.position.y
                    << L", " << entry.position.z
                    << L") @ " << Utf8ToWide(FormatHex(entry.positionAddress))
                    << (entry.positionFromTransform ? L" transform-backed" : L" direct-vec3")
                    << L" route=" << Utf8ToWide(entry.positionRoute.empty() ? std::string("n/a") : entry.positionRoute);
                if (entry.transformBase != 0)
                {
                    std::wcout << L" transform=" << Utf8ToWide(FormatHex(entry.transformBase));
                }
            }
            else
            {
                std::wcout << L" pos=n/a";
            }
            std::wcout << L"\n";
        }
        if (state.objectCache.size() > shown)
        {
            std::wcout << L"  ... " << (state.objectCache.size() - shown) << L" more\n";
        }

        return state.objectCache.empty() ? 2 : 0;
    }

    int RunExternalGui(const std::wstring& initialTarget)
    {
        GuiState state;
        LoadGuiConfig(&state, initialTarget);
        RunStartupSetup(&state, initialTarget.empty());

        WNDCLASSEXW windowClass = {
            sizeof(WNDCLASSEXW),
            CS_HREDRAW | CS_VREDRAW,
            WndProc,
            0L,
            0L,
            GetModuleHandleW(nullptr),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            L"AegisUnityUniversalExternalGui",
            nullptr
        };
        RegisterClassExW(&windowClass);

        gWindow = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_APPWINDOW,
            windowClass.lpszClassName,
            L"Aegis Unity Universal External",
            WS_POPUP,
            100,
            100,
            960,
            640,
            nullptr,
            nullptr,
            windowClass.hInstance,
            nullptr);

        if (!gWindow || !CreateDeviceD3D(gWindow))
        {
            CleanupDeviceD3D();
            UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
            return 1;
        }
        SetLayeredWindowAttributes(gWindow, kTransparentWindowColorKey, 255, LWA_COLORKEY);

        ShowWindow(gWindow, SW_SHOW);
        UpdateWindow(gWindow);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ApplyAegisTheme();

        ImGui_ImplWin32_Init(gWindow);
        ImGui_ImplDX11_Init(gDevice, gDeviceContext);

        AddLog(&state, "External GUI ready.");

        bool done = false;
        while (!done)
        {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
                if (message.message == WM_QUIT)
                {
                    done = true;
                }
            }
            if (done)
            {
                break;
            }

            ApplyOverlayWindowStyle(state);
            AlignOverlayToTargetWindow(state);

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            DrawExternalMenu(&state);
            DrawExternalEspOverlay(&state);

            ImGui::Render();
            const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            gDeviceContext->OMSetRenderTargets(1, &gRenderTargetView, nullptr);
            gDeviceContext->ClearRenderTargetView(gRenderTargetView, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            gSwapChain->Present(1, 0);
        }

        SaveGuiConfig(state);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        DestroyWindow(gWindow);
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        gWindow = nullptr;

        return 0;
    }
}
