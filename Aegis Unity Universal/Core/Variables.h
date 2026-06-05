#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <mutex>
#include <vector>
#include <lua.hpp>
#include <Utils/Includes.h>

// Opaque IL2CPP type; generated dumps may provide the full definition.
struct UnityEngine_Shader_o;

namespace Variables
{
	inline constexpr const char* AppName = "Aegis Unity Universal";
	inline std::string Prefix = AppName;
	inline constexpr const char* GameAssemblyName = "GameAssembly.dll";
	inline constexpr const char* UnityPlayerName = "UnityPlayer.dll";
	inline bool DEBUG = true;

	enum MenuTab
	{
		TAB_VISUALS,
		TAB_AIM,
		TAB_EXPLOITS,
		TAB_MISC,
		TAB_UNIVERSAL,
		TAB_DEV
	};

	enum class UnityRuntimeBackend
	{
		Unknown,
		IL2CPP,
		Mono
	};

	namespace System 
	{
		inline bool Init = false;
		inline bool InitIL2Cpp = false;
		inline UnityRuntimeBackend RuntimeBackend = UnityRuntimeBackend::Unknown;
		inline std::atomic_bool Running = true;
		inline std::atomic_bool ShutdownRequested = false;

		inline POINT MousePos = { 0, 0 };

		inline UINT vps = 1;
		inline Vector2 ScreenSize = { 0, 0 };
		inline Vector2 ScreenCenter = { 0, 0 };
		inline D3D11_VIEWPORT Viewport = {};
	}

	namespace Offsets
	{
		inline uintptr_t UnityEngineShader__FindShader_Offset = 0x0;
		inline uintptr_t UnityEngineTime__GetTimeScale_Offset = 0x0;
		inline uintptr_t UnityEngineTime__SetTimeScale_Offset = 0x0;
		//uintptr_t Health__TakeDamage_Offset = 0x0;
	}

	namespace Lua {
		inline lua_State* LuaState = nullptr;
		inline bool ShowEditor = false;

		inline char LuaScript[9999] = R"(
print("Hello World!")

local test = 0
print(test)

function TestFunction()
	print("TestFunction")
end

TestFunction()

-- call c++ function
testFn()
		)";
	}

	namespace CheatVariables 
	{
		inline Unity::CGameObject* LocalPlayer = nullptr;
		inline std::vector<Unity::CGameObject*> PlayersList;
		inline std::mutex PlayersListMutex;
		inline std::atomic<int> PlayersCacheCount = 0;
		inline std::atomic<int> PlayersCacheSource = 0;
		inline std::atomic_bool ForcePlayerCacheRefresh = false;
		inline char PlayerComponentName[128] = "PlayerController";
		inline char PlayerFallbackComponentName[128] = "UnityEngine.CharacterController";
		inline bool UsePlayerFallbackComponent = true;
		inline int CacheRefreshMs = 2000;

		inline Unity::CGameObject* TargetPlayer = nullptr;
		inline ImColor TargetPlayerColor = ImColor(255, 0, 0);
		inline float CameraFovBaseline = 0.0f;
		inline bool CameraFovBaselineCaptured = false;
		inline float CameraFovLastDesired = 0.0f;
		inline float CameraFovLastApplied = 0.0f;
		inline ULONGLONG CameraFovLastApplyTick = 0;
		inline ULONGLONG CameraFovLastSweepTick = 0;
		inline bool CameraFovHookEnabled = false;
		inline bool CameraFovHookFailed = false;

		inline UnityEngine_Shader_o* ChamsShader = nullptr;

		inline std::vector<std::pair<int, int>> BonePairs = {
			// left foot
			{2, 1},
			{1, 0},
			{0, 3},
			// right foot
			{31, 30},
			{30, 20},
			{20, 3},
			// Spine
			{3, 4},
			// Right Hand
			{4, 9},
			// Left Hand
			{4, 7},
			// Neck
			{4, 5},
			// Head
			{5, 6},
		};

		inline ImVec4 Rainbow = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Global rainbow color
		inline ImColor RainbowColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255); // Global rainbow color

		namespace TestObjects {// developers test scope
			inline std::vector<Unity::CGameObject*> List;
			inline std::mutex ListMutex;
			inline Unity::CGameObject* PinnedObject = nullptr;
			inline bool UsePinnedOnly = false;
			inline char PinnedObjectName[128] = "";
			inline std::atomic<int> CacheCount = 0;
			inline char Name[200] = "UnityEngine.CapsuleCollider";
			inline int MaxRows = 80;
			inline bool Chams = false;
			inline bool Snapline = false;
			inline ImColor SnaplineColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
			inline bool Box = false;
			inline ImColor BoxColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
			inline bool Aimbot = false;
		}

		inline ULONGLONG LastShotTime = 0;
		inline ULONGLONG LastTick = 0;
		inline ULONGLONG LastChamsTick = 0;
		inline int SpinbotSupport = 0;
	}

	namespace CheatMenuVariables {

		inline bool ShowMenu = false;
		inline bool Watermark = false;

		inline bool CameraFovChanger = false;
		inline float CameraCustomFOV = 80.0f;
		inline bool CameraFovAdditive = false;
		inline float CameraFovOffset = 15.0f;

		inline bool EnableDeveloperOptions = false;
		inline bool StabilityMode = true;
		inline int MaxPlayersPerFrame = 96;
		inline int MaxTestObjectsPerFrame = 64;

		inline bool ShowInspector = false;
		inline bool StreamerMode = false;
		inline bool FunMode = false;
		inline bool FunConfetti = true;
		inline bool FunRainbowBorder = true;
		inline bool FunCrosshairOrbit = true;
		inline bool FunBouncyWatermark = true;
		inline int FunConfettiCount = 70;
		inline float FunIntensity = 1.0f;
		inline float FunSpeed = 1.0f;
		inline float FunBorderThickness = 3.0f;

		inline float GameSpeed = 1.0f;
		
		inline bool ShowMouse = true;
		inline bool RainbowMouse = false;
		inline ImColor MouseColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline int MouseType = 0;

		inline bool Crosshair = false;
		inline bool RainbowCrosshair = false;
		inline ImColor CrosshairColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline float CrosshairSize = 5.0f;
		inline float CrosshairGap = 2.0f;
		inline float CrosshairThickness = 1.2f;
		inline bool CrosshairDot = false;
		inline float CrosshairDotSize = 2.0f;
		inline bool CrosshairOutline = true;
		inline int CrosshairType = 0;

		inline bool PlayersSnapline = false;
		inline bool RainbowPlayersSnapline = false;
		inline ImColor PlayersSnaplineColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline int PlayersSnaplineType = 2;
		inline float PlayersSnaplineOffsetX = 0.0f;
		inline float PlayersSnaplineOffsetY = 0.0f;

		inline bool PlayerChams = false;
		inline bool RainbowPlayerChams = false;

		inline bool PlayerSkeleton = false;
		inline bool RainbowPlayerSkeleton = false;
		inline ImColor PlayerSkeletonColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline bool PlayersName = false;
		inline bool PlayersDistance = false;
		inline bool PlayersMaxDistance = false;
		inline float PlayersMaxDistanceValue = 250.0f;
		inline bool PlayersCountOverlay = false;
		inline bool PlayersListOverlay = false;
		inline int PlayersListMaxRows = 8;
		inline float PlayersListPosX = 18.0f;
		inline float PlayersListPosY = 130.0f;
		inline bool PlayersTargetInfoOverlay = false;
		inline float TargetInfoPosX = 18.0f;
		inline float TargetInfoPosY = 320.0f;
		inline bool PlayersDrawLimit = false;
		inline int PlayersMaxDrawCount = 60;
		inline bool PlayersDistanceFade = false;
		inline float PlayersFadeStartDistance = 80.0f;
		inline float PlayersFadeEndDistance = 300.0f;
		inline float PlayersFadeMinAlpha = 0.25f;
		inline bool PlayersOffscreenArrows = false;
		inline bool OffscreenArrowLabels = false;
		inline bool RainbowOffscreenArrows = false;
		inline ImColor OffscreenArrowColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline float OffscreenArrowSize = 13.0f;
		inline float OffscreenArrowRadius = 260.0f;
		inline bool PlayersTargetMarker = false;
		inline bool RainbowTargetMarker = false;
		inline ImColor TargetMarkerColor = ImColor(64.0f / 255, 207.0f / 255, 255.0f / 255);
		inline float TargetMarkerSize = 7.0f;
		inline ImColor PlayersTextColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline float PlayersLabelSize = 13.0f;
		inline float ESPLineThickness = 1.5f;
		inline float ESPBoxThickness = 1.5f;
		inline float ESPGlobalAlpha = 1.0f;
		inline int DistanceUnit = 0;
		
		inline bool PlayersBox = false;
		inline bool RainbowPlayersBox = false;
		inline ImColor PlayersBoxColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline bool PlayersBoxFilled = false;
		inline float PlayersBoxFillAlpha = 0.25f;
		inline int PlayersBoxStyle = 0;

		inline bool Radar = false;
		inline bool RadarBackground = true;
		inline bool RadarRangeRings = true;
		inline bool RadarLabels = false;
		inline float RadarRange = 150.0f;
		inline float RadarSize = 150.0f;
		inline float RadarPosX = 24.0f;
		inline float RadarPosY = 90.0f;
		inline ImColor RadarColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);

		inline bool BotChecker = false;
		inline bool RainbowBotChecker = false;
		inline ImColor BotCheckerColor = ImColor(0, 0, 255);
		inline bool BotCheckerText = true;

		inline bool Spinbot = false;

		inline bool PlayersHealth = false;

		inline bool GodMode = false;

		inline bool NoRecoil = false;

		inline bool NoSpread = false;

		inline bool RapidFire = false;

		inline bool OneShot = false;

		inline bool InfiniteAmmo = false;

		inline bool SpeedHack = false;
		inline float SpeedValue = 1.0f;

		inline bool EnableAimbot = false;
		inline bool AimbotFOVCheck = false;
		inline bool RainbowAimbotFOV = false;
		inline ImColor AimbotFOVColor = ImColor(255.0f / 255, 255.0f / 255, 255.0f / 255);
		inline float AimbotFOV = 80.0f;
		inline float AimbotFOVThickness = 1.0f;
		inline bool AimbotFOVFilled = false;
		inline float AimbotFOVFillAlpha = 0.08f;
		inline float AimbotSmoothness = 0.5f;
		inline bool AimbotDeadzone = false;
		inline float AimbotDeadzoneRadius = 4.0f;
		inline int AimbotTargetMode = 1;
		inline float FakeHeadPosDiff = 1;
		inline float FakeFeetPosDiff = 1;

		inline bool StatusOverlay = false;
		inline bool StatusOverlayDetailed = true;
		inline bool HudAutoLayout = true;
		inline int HudLayoutMode = 0;
		inline float HudMargin = 18.0f;
		inline float HudGap = 10.0f;
		inline bool AutoSaveConfig = true;
		inline float OverlayPosX = 18.0f;
		inline float OverlayPosY = 18.0f;

		inline bool ConfigLoaded = false;
		inline char LastConfigStatus[160] = "Config not loaded";
	}

	namespace KEYS
	{
		inline int SHOWMENU_KEY = VK_F4;
		inline int DEATTACH_KEY = VK_F9;
		inline int PANIC_KEY = VK_END;
		inline int PLAYERS_BOX_KEY = 0;
		inline int PLAYERS_SNAPLINE_KEY = 0;
		inline int AIMBOT_KEY = 0;
		inline int CROSSHAIR_KEY = 0;
		inline int AIMBOT_ACTIVATION_KEY = VK_RBUTTON;
	}
}
