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

        struct EspEntity
        {
            uintptr_t address = 0;
            Vec3 position;
            bool onScreen = false;
            ImVec2 screen;
            ImVec2 head;
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
            bool overlayMode = false;
            bool alignToTargetWindow = true;
            bool clickThroughOverlay = false;
            bool espEnabled = false;
            bool espBoxes = true;
            bool espSnaplines = true;
            bool radarEnabled = false;
            char entityListAddress[80] = "";
            char entityCountAddress[80] = "";
            int entityCount = 32;
            int entityStride = 0x8;
            int entityLayout = 0;
            int positionOffset = 0;
            char viewProjectionAddress[80] = "";
            int matrixLayout = 0;
            int upAxis = 0;
            float entityHeight = 1.8f;
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
            std::vector<std::string> log;
        };

        ID3D11Device* gDevice = nullptr;
        ID3D11DeviceContext* gDeviceContext = nullptr;
        IDXGISwapChain* gSwapChain = nullptr;
        ID3D11RenderTargetView* gRenderTargetView = nullptr;
        HWND gWindow = nullptr;
        constexpr COLORREF kTransparentWindowColorKey = RGB(0, 0, 0);

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
            while (std::getline(file, line))
            {
                if (std::wstring value = ConfigValue(Trim(line), L"last_target"); !value.empty())
                {
                    lastTarget = value;
                }
                else if (value = ConfigValue(Trim(line), L"last_method_map"); !value.empty())
                {
                    lastMethodMap = value;
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

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        bool ContainsInsensitiveAscii(const std::string& text, const std::string& needle)
        {
            if (needle.empty())
            {
                return true;
            }

            return ToLowerAscii(text).find(ToLowerAscii(needle)) != std::string::npos;
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

        bool ReadEspEntities(GuiState* state, const std::array<float, 16>& matrix, const ImVec2& screenSize)
        {
            if (!state || !state->reader.IsOpen())
            {
                return false;
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

            state->espEntities.clear();
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
                if (!ReadVec3(state->reader, entityAddress + static_cast<std::ptrdiff_t>(state->positionOffset), &position))
                {
                    continue;
                }

                EspEntity entity;
                entity.address = entityAddress;
                entity.position = position;
                entity.onScreen = WorldToScreen(position, matrix, state->matrixLayout, screenSize, &entity.screen);
                entity.onScreen = WorldToScreen(EntityHeadPosition(position, state->upAxis, state->entityHeight), matrix, state->matrixLayout, screenSize, &entity.head)
                    && entity.onScreen;
                state->espEntities.push_back(entity);
            }

            return true;
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

        void AlignOverlayToTargetWindow(const GuiState& state)
        {
            if (!state.overlayMode || !state.alignToTargetWindow || !state.process)
            {
                return;
            }

            struct FindWindowContext
            {
                DWORD pid = 0;
                HWND hwnd = nullptr;
            } context{ state.process->pid, nullptr };

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

            if (!context.hwnd)
            {
                return;
            }

            RECT client = {};
            if (!GetClientRect(context.hwnd, &client))
            {
                return;
            }

            POINT topLeft{ client.left, client.top };
            ClientToScreen(context.hwnd, &topLeft);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            if (width > 0 && height > 0)
            {
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

        void DrawExternalEspOverlay(GuiState* state)
        {
            if (!state || (!state->espEnabled && !state->radarEnabled))
            {
                return;
            }

            std::array<float, 16> matrix = {};
            if (!ReadMatrix4x4(state->reader, state->viewProjectionAddress, &matrix))
            {
                return;
            }

            const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
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

                    const float height = std::abs(entity.screen.y - entity.head.y);
                    if (height < 2.0f || height > screenSize.y * 2.0f)
                    {
                        continue;
                    }

                    const float width = height * 0.45f;
                    const ImVec2 topLeft(entity.head.x - width * 0.5f, entity.head.y);
                    const ImVec2 bottomRight(entity.head.x + width * 0.5f, entity.screen.y);

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
            if (backend != RuntimeBackend::IL2CPP)
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
                AddLog(state, "%s -> %s", label, FormatHex(result.value->address).c_str());
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
                        AddLog(state, "  method %-42s %s", method.label, FormatHex(result.value->address).c_str());
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
                AddLog(state, "Mono detected. Managed method addresses require Mono metadata/JIT symbols or a map; runtime exports were resolved above.");
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

            ImGui::Checkbox("ESP Boxes", &state->espEnabled);
            ImGui::SameLine();
            ImGui::Checkbox("Boxes", &state->espBoxes);
            ImGui::SameLine();
            ImGui::Checkbox("Snaplines", &state->espSnaplines);
            ImGui::SameLine();
            ImGui::Checkbox("Radar", &state->radarEnabled);

            ImGui::InputTextWithHint("Entity List", "pointer array or first entity address", state->entityListAddress, sizeof(state->entityListAddress));
            ImGui::InputTextWithHint("Entity Count Address", "optional int count address", state->entityCountAddress, sizeof(state->entityCountAddress));
            ImGui::InputInt("Entity Count", &state->entityCount);
            const char* layouts[] = { "Pointer array", "Contiguous structs" };
            ImGui::Combo("Entity Layout", &state->entityLayout, layouts, IM_ARRAYSIZE(layouts));
            ImGui::InputInt("Entity Stride", &state->entityStride);
            ImGui::InputInt("Position Offset", &state->positionOffset);

            ImGui::InputTextWithHint("ViewProjection Matrix", "address of 16-float view-projection matrix", state->viewProjectionAddress, sizeof(state->viewProjectionAddress));
            const char* matrixLayouts[] = { "Row-major", "Column-major" };
            ImGui::Combo("Matrix Layout", &state->matrixLayout, matrixLayouts, IM_ARRAYSIZE(matrixLayouts));
            const char* upAxes[] = { "Y Up", "Z Up" };
            ImGui::Combo("Up Axis", &state->upAxis, upAxes, IM_ARRAYSIZE(upAxes));
            ImGui::SliderFloat("Entity Height", &state->entityHeight, 0.1f, 4.0f, "%.2f");

            ImGui::InputTextWithHint("Local Position", "optional Vec3 address for radar center", state->localPositionAddress, sizeof(state->localPositionAddress));
            ImGui::SliderFloat("Radar Range", &state->radarRange, 1.0f, 1000.0f, "%.0f");
            ImGui::SliderFloat("Radar Size", &state->radarSize, 60.0f, 360.0f, "%.0f");
            ImGui::SliderFloat("Radar X", &state->radarPosX, 0.0f, 1200.0f, "%.0f");
            ImGui::SameLine();
            ImGui::SliderFloat("Radar Y", &state->radarPosY, 0.0f, 900.0f, "%.0f");
            ImGui::Text("Entities read this frame: %zu", state->espEntities.size());

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

            ImGui::Text("Method Map Explorer");
            ImGui::InputTextWithHint("Filter##MethodMapFilter", "class / method / image", state->methodFilter, sizeof(state->methodFilter));
            if (!state->methodMap)
            {
                ImGui::TextDisabled("No method map loaded.");
            }
            else if (ImGui::BeginTable("##MethodMapExplorer", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 170)))
            {
                ImGui::TableSetupColumn("Image");
                ImGui::TableSetupColumn("Class");
                ImGui::TableSetupColumn("Method");
                ImGui::TableSetupColumn("Argc", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("RVA", ImGuiTableColumnFlags_WidthFixed, 90.0f);
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
                    ImGui::TextUnformatted(FormatHex(entry.rva).c_str());
                    ImGui::TableSetColumnIndex(5);
                    if (gameAssemblyBase)
                    {
                        ImGui::TextUnformatted(FormatHex(gameAssemblyBase + entry.rva).c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("n/a");
                    }
                    ImGui::TableSetColumnIndex(6);
                    if (ImGui::SmallButton("Use"))
                    {
                        CopyToBuffer(state->imageName, sizeof(state->imageName), entry.imageName);
                        CopyToBuffer(state->className, sizeof(state->className), entry.className);
                        CopyToBuffer(state->methodName, sizeof(state->methodName), entry.methodName);
                        state->argumentCount = entry.argumentCount;
                        state->anyArgumentCount = entry.argumentCount < 0;
                        if (gameAssemblyBase)
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
