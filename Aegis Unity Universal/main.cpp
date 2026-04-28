// includes
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cmath>
#include <cctype>
#include <iostream>
#include <intrin.h>
#include <string>

// my includes
#include <Utils/Includes.h>
#include <Utils/SDK.h>
#include <Utils/Utils.h>
#include <Libraries/Fonts/Font.h>
#include <Libraries/kiero/minhook/include/MinHook.h>
#include <Libraries/Il2cpp_Resolver/il2cpp_resolver.hpp>
#include <Libraries/Unity_Runtime/UnityRuntime.hpp>
#include <Libraries/Callback.hpp>
#include <Libraries/Il2cpp_Resolver/Utils/VFunc.hpp>
#include <Libraries/PaternScan.hpp>
#include <Core/HooksFunctions.h>
#include <Core/Cheats.h>
#include <Libraries/luaaa.hpp>

using namespace Variables;
using namespace luaaa;

#pragma region ImGui
	extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	Present oPresent = nullptr;
	using ResizeBuffers = HRESULT(__stdcall*) (IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
	ResizeBuffers oResizeBuffers = nullptr;
	SwapBuffersFn oSwapBuffers = nullptr;
	HWND window = nullptr;
	WNDPROC oWndProc = nullptr;
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pContext = nullptr;
	ID3D11RenderTargetView* mainRenderTargetView = nullptr;
#pragma endregion

enum class OverlayRenderer
{
	None,
	D3D11,
	OpenGL
};

static std::atomic<OverlayRenderer> gOverlayRenderer{ OverlayRenderer::None };
static std::mutex gOverlayInitMutex;
static std::atomic_bool gD3D11HookInstalled{ false };
static std::atomic_bool gOpenGLHookInstalled{ false };

static LRESULT __stdcall WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void RestoreWindowProc()
{
	if (window && oWndProc) {
		SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
		oWndProc = nullptr;
	}
}

static void ReleaseRenderTarget()
{
	if (mainRenderTargetView) {
		mainRenderTargetView->Release();
		mainRenderTargetView = nullptr;
	}
}

static bool CreateRenderTarget(IDXGISwapChain* pSwapChain)
{
	if (!pSwapChain || !pDevice)
		return false;

	ID3D11Texture2D* pBackBuffer = nullptr;
	if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer))) || !pBackBuffer)
		return false;

	const HRESULT result = pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
	pBackBuffer->Release();
	return SUCCEEDED(result) && mainRenderTargetView;
}

static void CleanupRuntime()
{
	System::Running.store(false);
	System::ShutdownRequested.store(false);
	CheatMenuVariables::ShowMenu = false;

	if (CheatMenuVariables::AutoSaveConfig) {
		AegisUniversal::SaveConfig();
	}

	if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP) {
		IL2CPP::Callback::Uninitialize();
	}

	if (Lua::LuaState) {
		lua_close(Lua::LuaState);
		Lua::LuaState = nullptr;
	}

	if (ImGui::GetCurrentContext()) {
		const OverlayRenderer renderer = gOverlayRenderer.load();
		if (renderer == OverlayRenderer::D3D11) {
			ImGui_ImplDX11_Shutdown();
		}
		else if (renderer == OverlayRenderer::OpenGL) {
			ImGui_ImplOpenGL3_Shutdown();
		}
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	RestoreWindowProc();

	ReleaseRenderTarget();

	if (pContext) {
		pContext->Release();
		pContext = nullptr;
	}

	if (pDevice) {
		pDevice->Release();
		pDevice = nullptr;
	}

	System::Init = false;
	System::InitIL2Cpp = false;
	System::RuntimeBackend = UnityRuntimeBackend::Unknown;
	gOverlayRenderer.store(OverlayRenderer::None);
	gD3D11HookInstalled.store(false);
	gOpenGLHookInstalled.store(false);
	UnityRuntime::Shutdown();

	kiero::shutdown();
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}

static const char* RuntimeBackendName()
{
	return UnityRuntime::GetBackendName();
}

static void SyncRuntimeBackendState()
{
	switch (UnityRuntime::GetBackend()) {
	case UnityRuntime::Backend::IL2CPP:
		System::RuntimeBackend = UnityRuntimeBackend::IL2CPP;
		System::InitIL2Cpp = true;
		break;
	case UnityRuntime::Backend::Mono:
		System::RuntimeBackend = UnityRuntimeBackend::Mono;
		System::InitIL2Cpp = false;
		break;
	default:
		System::RuntimeBackend = UnityRuntimeBackend::Unknown;
		System::InitIL2Cpp = false;
		break;
	}
}

static bool InitializeUnityRuntime()
{
	while (System::Running.load())
	{
		if (UnityRuntime::Initialize()) {
			SyncRuntimeBackendState();
			return true;
		}

		Sleep(250);
	}

	return false;
}

static bool FindSigs() {
	Offsets::UnityEngineShader__FindShader_Offset = 0;
	Offsets::UnityEngineTime__GetTimeScale_Offset = 0;
	Offsets::UnityEngineTime__SetTimeScale_Offset = 0;

	if (System::RuntimeBackend != UnityRuntimeBackend::IL2CPP)
		return true;

	// UnityEngine.Shader::Find
	Offsets::UnityEngineShader__FindShader_Offset = reinterpret_cast<uintptr_t>(UnityRuntime::ResolveMethod("UnityEngine.Shader", "Find", 1));

	// UnityEngine.Time::get_timeScale, UnityEngine.Time::set_timeScale
	Offsets::UnityEngineTime__GetTimeScale_Offset = reinterpret_cast<uintptr_t>(UnityRuntime::ResolveMethod("UnityEngine.Time", "get_timeScale", 0));
	Offsets::UnityEngineTime__SetTimeScale_Offset = reinterpret_cast<uintptr_t>(UnityRuntime::ResolveMethod("UnityEngine.Time", "set_timeScale", 1));

	// Example: Health::TakeDamage, Health::Heal
	// Offsets::Health__TakeDamage_Offset = SearchSignatureByClassAndFunctionName("Health", "TakeDamage");
	// Offsets::Health__Heal_Offset = SearchSignatureByClassAndFunctionName("Health", "Heal");

	return true;
}

static void EnableHooks() {

	if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP && Unity::CameraFunctions.m_pSetFieldOfView) {
		const MH_STATUS hookStatus = MH_CreateHook(
			reinterpret_cast<LPVOID>(Unity::CameraFunctions.m_pSetFieldOfView),
			reinterpret_cast<LPVOID>(&HooksFunctions::UnityEngine_Camera__set_fieldOfView_hook),
			reinterpret_cast<LPVOID*>(&HooksFunctions::UnityEngine_Camera__set_fieldOfView));

		if (hookStatus == MH_OK || hookStatus == MH_ERROR_ALREADY_CREATED) {
			MH_EnableHook(reinterpret_cast<LPVOID>(Unity::CameraFunctions.m_pSetFieldOfView));
			CheatVariables::CameraFovHookEnabled = true;
			CheatVariables::CameraFovHookFailed = false;
		}
		else {
			CheatVariables::CameraFovHookEnabled = false;
			CheatVariables::CameraFovHookFailed = true;
		}
	}
	else {
		CheatVariables::CameraFovHookEnabled = false;
		CheatVariables::CameraFovHookFailed = true;
	}

	// EXAMPLE
	//// Health__TakeDamage
	//if (MH_CreateHook(reinterpret_cast<LPVOID*>(
	//	Offsets::Health__TakeDamage_Offset),
	//	&HooksFunctions::Health__TakeDamage_hook,
	//	(LPVOID*)&HooksFunctions::Health__TakeDamage) == MH_OK)
	//{
	//	MH_EnableHook(reinterpret_cast<LPVOID*>(Offsets::Health__TakeDamage_Offset));
	//}

}

static void InitializeOverlayResources()
{
	if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP) {
		CheatVariables::ChamsShader = GameFunctions::UnityEngine_Shader__Find(IL2CPP::String::New("Hidden/Internal-Colored"));
	}
}

static bool InstallWindowProc(HWND targetWindow)
{
	if (!targetWindow)
		return false;

	if (oWndProc && window && window != targetWindow) {
		RestoreWindowProc();
	}

	window = targetWindow;
	if (!oWndProc) {
		SetLastError(0);
		oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
		if (!oWndProc && GetLastError() != 0) {
			return false;
		}
	}
	return true;
}

static void ConfigureImGuiContext()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	io.IniFilename = nullptr;
	io.ConfigWindowsMoveFromTitleBarOnly = false;
	Themes::ImGuiThemeAegis();

	io.Fonts->AddFontDefault();
	ImFontConfig font_cfg = {};
	font_cfg.FontDataOwnedByAtlas = false;
	font_cfg.GlyphExtraAdvanceX = 1.2f;
	gameFont = io.Fonts->AddFontFromMemoryTTF(TTSquaresCondensedBold, sizeof(TTSquaresCondensedBold), 14.0f, &font_cfg);
	if (!gameFont) {
		gameFont = io.FontDefault;
	}
	io.Fonts->AddFontDefault();
}

static bool InitImGuiD3D11()
{
	if (!window || !pDevice || !pContext)
		return false;

	ConfigureImGuiContext();
	if (!ImGui_ImplWin32_Init(window)) {
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplDX11_Init(pDevice, pContext)) {
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	return true;
}

static bool InitImGuiOpenGL()
{
	if (!window)
		return false;

	ConfigureImGuiContext();
	if (!ImGui_ImplWin32_Init(window)) {
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplOpenGL3_Init(nullptr)) {
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	return true;
}

static bool HasActiveOpenGLContext()
{
	using WglGetCurrentContextFn = HGLRC(WINAPI*)();
	HMODULE openGL = GetModuleHandleA("opengl32.dll");
	if (!openGL)
		return false;

	auto getCurrentContext = reinterpret_cast<WglGetCurrentContextFn>(GetProcAddress(openGL, "wglGetCurrentContext"));
	return getCurrentContext && getCurrentContext() != nullptr;
}

static bool InitVars() {
	if (DEBUG) {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0F);
		std::cout << "\n[ " << Prefix << " ] Starting runtime initialization." << std::endl;
	}

	if (!InitializeUnityRuntime()) {
		if (DEBUG)
			std::cout << "[ " << Prefix << " ] Unity runtime initialize failed." << std::endl;
		Sleep(300);
		return false;
	}

	if (DEBUG) {
		std::cout << "[ " << Prefix << " ] Unity runtime --> " << RuntimeBackendName() << "\n" << std::endl;
		std::cout << "[ " << Prefix << " ] Base Address --> " << SDK::Base << "\n" << std::endl;

		if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP) {
			std::cout << "[ " << Prefix << " ] GameAssembly Base Address --> " << SDK::GameAssembly << "\n" << std::endl;
		}

		if (System::RuntimeBackend == UnityRuntimeBackend::Mono) {
			std::cout << "[ " << Prefix << " ] Mono Base Address --> " << SDK::MonoModule << "\n" << std::endl;
		}

		std::cout << "[ " << Prefix << " ] UnityPlayer Base Address --> " << SDK::UnityPlayer << "\n" << std::endl;
	}
	return true;
}

static bool KeyPressed(int key)
{
	return key > 0 && (GetAsyncKeyState(key) & 1);
}

static void HandleInputs() {
	const bool menuPressed = KeyPressed(KEYS::SHOWMENU_KEY);
	if (menuPressed)
	{
		CheatMenuVariables::ShowMenu = !CheatMenuVariables::ShowMenu;
	}

	if (KeyPressed(KEYS::PANIC_KEY))
	{
		AegisUniversal::DisableAllRuntimeFeatures();
	}

	if (KeyPressed(KEYS::DEATTACH_KEY))
	{
		System::ShutdownRequested.store(true);
	}

	if (!CheatMenuVariables::ShowMenu && !menuPressed)
	{
		if (KeyPressed(KEYS::PLAYERS_BOX_KEY))
			CheatMenuVariables::PlayersBox = !CheatMenuVariables::PlayersBox;
		if (KeyPressed(KEYS::PLAYERS_SNAPLINE_KEY))
			CheatMenuVariables::PlayersSnapline = !CheatMenuVariables::PlayersSnapline;
		if (KeyPressed(KEYS::AIMBOT_KEY))
			CheatMenuVariables::EnableAimbot = !CheatMenuVariables::EnableAimbot;
		if (KeyPressed(KEYS::CROSSHAIR_KEY))
			CheatMenuVariables::Crosshair = !CheatMenuVariables::Crosshair;
	}
}

static LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (CheatMenuVariables::ShowMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	if (CheatMenuVariables::ShowMenu)
		return true;

	return oWndProc ? CallWindowProcA(oWndProc, hWnd, uMsg, wParam, lParam) : DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	ReleaseRenderTarget();
	return oResizeBuffers ? oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags) : E_FAIL;
}

static void UpdateScreenMetricsFromWindow()
{
	RECT rect = {};
	if (!window || !GetClientRect(window, &rect)) {
		return;
	}

	const float width = static_cast<float>(std::max<LONG>(rect.right - rect.left, 1));
	const float height = static_cast<float>(std::max<LONG>(rect.bottom - rect.top, 1));
	System::vps = 1;
	System::Viewport.TopLeftX = 0.0f;
	System::Viewport.TopLeftY = 0.0f;
	System::Viewport.Width = width;
	System::Viewport.Height = height;
	System::Viewport.MinDepth = 0.0f;
	System::Viewport.MaxDepth = 1.0f;
	System::ScreenSize = { width, height };
	System::ScreenCenter = { width * 0.5f, height * 0.5f };
}

static void DrawOverlayContents()
{
	if (CheatMenuVariables::Watermark)
	{
		Utils::DrawOutlinedText(gameFont, ImVec2(System::ScreenCenter.x, System::ScreenSize.y - 20), 13.0f, CheatVariables::RainbowColor, true, Prefix.c_str());
		Utils::DrawOutlinedText(gameFont, ImVec2(System::ScreenCenter.x, 5), 13.0f, CheatVariables::RainbowColor, true, "[ %.1f FPS ]", ImGui::GetIO().Framerate);
	}

	GetCursorPos(&System::MousePos);
	ScreenToClient(window, &System::MousePos);

	HandleInputs();

	try { CheatsLoop(); }
	catch (...) {}

	AegisUniversal::DrawStatusOverlay();

	if (CheatMenuVariables::ShowMenu)
	{
		DrawMouse();
		DrawMenu();
	}

	DrawCrosshair();
	AegisUniversal::DrawFunOverlay();

	if (CheatMenuVariables::AimbotFOVCheck) {
		ImColor fovColor = CheatMenuVariables::RainbowAimbotFOV ? CheatVariables::RainbowColor : CheatMenuVariables::AimbotFOVColor;
		const ImVec2 center(System::ScreenCenter.x, System::ScreenCenter.y);
		const float fovRadius = std::clamp(CheatMenuVariables::AimbotFOV, 1.0f, 800.0f);
		if (CheatMenuVariables::AimbotFOVFilled) {
			ImColor fillColor = fovColor;
			fillColor.Value.w = std::clamp(CheatMenuVariables::AimbotFOVFillAlpha, 0.01f, 0.25f);
			ImGui::GetForegroundDrawList()->AddCircleFilled(center, fovRadius, fillColor, 160);
		}
		ImGui::GetForegroundDrawList()->AddCircle(center, fovRadius, fovColor, 160, std::clamp(CheatMenuVariables::AimbotFOVThickness, 1.0f, 6.0f));
	}
}

static void BeginOverlayFrame(OverlayRenderer renderer)
{
	if (renderer == OverlayRenderer::D3D11) {
		ImGui_ImplDX11_NewFrame();
	}
	else if (renderer == OverlayRenderer::OpenGL) {
		ImGui_ImplOpenGL3_NewFrame();
	}

	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	DrawOverlayContents();
	ImGui::Render();
}

static HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!pSwapChain || !oPresent)
		return E_INVALIDARG;

	UnityRuntime::ScopedThread runtimeThread;
	if (!runtimeThread.IsAttached())
		return oPresent(pSwapChain, SyncInterval, Flags);

	if (gOverlayRenderer.load() == OverlayRenderer::OpenGL)
		return oPresent(pSwapChain, SyncInterval, Flags);

	if (!System::Init)
	{
		std::lock_guard<std::mutex> lock(gOverlayInitMutex);
		if (!System::Init)
		{
			if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice))) || !pDevice)
			{
				return oPresent(pSwapChain, SyncInterval, Flags);
			}

			pDevice->GetImmediateContext(&pContext);
			if (!pContext)
			{
				return oPresent(pSwapChain, SyncInterval, Flags);
			}

			DXGI_SWAP_CHAIN_DESC sd = {};
			if (FAILED(pSwapChain->GetDesc(&sd)) || !sd.OutputWindow)
			{
				return oPresent(pSwapChain, SyncInterval, Flags);
			}

			if (!InstallWindowProc(sd.OutputWindow))
			{
				return oPresent(pSwapChain, SyncInterval, Flags);
			}

			if (!CreateRenderTarget(pSwapChain))
			{
				RestoreWindowProc();
				return oPresent(pSwapChain, SyncInterval, Flags);
			}

			if (!InitImGuiD3D11())
			{
				ReleaseRenderTarget();
				RestoreWindowProc();
				return oPresent(pSwapChain, SyncInterval, Flags);
			}

			InitializeOverlayResources();
			gOverlayRenderer.store(OverlayRenderer::D3D11);
			System::Init = true;

			if (DEBUG) {
				std::cout << "[ " << Prefix << " ] Overlay renderer --> D3D11\n" << std::endl;
			}
		}
	}

	System::vps = 1;
	pContext->RSGetViewports(&System::vps, &System::Viewport);
	System::ScreenSize = { System::Viewport.Width, System::Viewport.Height };
	System::ScreenCenter = { System::Viewport.Width * 0.5f, System::Viewport.Height * 0.5f };

	if (!mainRenderTargetView && !CreateRenderTarget(pSwapChain)) {
		return oPresent(pSwapChain, SyncInterval, Flags);
	}

	BeginOverlayFrame(OverlayRenderer::D3D11);

	if (mainRenderTargetView) {
		pContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	const bool shouldShutdown = System::ShutdownRequested.load();

	if (shouldShutdown) {
		CleanupRuntime();
	}

	return oPresent(pSwapChain, SyncInterval, Flags);
}

static BOOL WINAPI hkSwapBuffers(HDC hdc)
{
	if (!oSwapBuffers)
		return FALSE;

	if (gOverlayRenderer.load() == OverlayRenderer::D3D11)
		return oSwapBuffers(hdc);

	if (!HasActiveOpenGLContext())
		return oSwapBuffers(hdc);

	UnityRuntime::ScopedThread runtimeThread;
	if (!runtimeThread.IsAttached())
		return oSwapBuffers(hdc);

	if (!System::Init)
	{
		std::lock_guard<std::mutex> lock(gOverlayInitMutex);
		if (!System::Init)
		{
			HWND targetWindow = WindowFromDC(hdc);
			if (!targetWindow) {
				targetWindow = GetForegroundWindow();
			}

			if (!InstallWindowProc(targetWindow))
			{
				return oSwapBuffers(hdc);
			}

			if (!InitImGuiOpenGL())
			{
				RestoreWindowProc();
				return oSwapBuffers(hdc);
			}

			InitializeOverlayResources();
			gOverlayRenderer.store(OverlayRenderer::OpenGL);
			System::Init = true;

			if (DEBUG) {
				std::cout << "[ " << Prefix << " ] Overlay renderer --> OpenGL\n" << std::endl;
			}
		}
	}

	if (gOverlayRenderer.load() != OverlayRenderer::OpenGL)
		return oSwapBuffers(hdc);

	UpdateScreenMetricsFromWindow();
	BeginOverlayFrame(OverlayRenderer::OpenGL);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	const bool shouldShutdown = System::ShutdownRequested.load();
	if (shouldShutdown) {
		CleanupRuntime();
	}

	return oSwapBuffers(hdc);
}

void bindToLUA(lua_State* L)
{
	LuaModule(L)
		.def("pi", 3.1415926535897932)
		.fun("testFn", []() {
			if (DEBUG)
				std::cout << "Lua testFn called" << std::endl;
		})
		.fun("test", []() {
			ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(System::ScreenCenter.x, System::ScreenCenter.y), 50, ImColor(255, 255, 255), 360);
		});
}

static void Rainbow() {
	float hue = 0.0f;

	while (System::Running.load())
	{
		float red = 0.0f, green = 0.0f, blue = 0.0f;
		ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, red, green, blue);

		CheatVariables::Rainbow = ImVec4(red, green, blue, 1.0f);
		CheatVariables::RainbowColor = ImColor(CheatVariables::Rainbow.x, CheatVariables::Rainbow.y, CheatVariables::Rainbow.z);
		hue = std::fmod(hue + 0.0025f, 1.0f);

		Sleep(20);
	}
}

static DWORD WINAPI CacheThread(LPVOID)
{
	CacheManager();
	return 0;
}

static DWORD WINAPI RainbowThread(LPVOID)
{
	Rainbow();
	return 0;
}

static bool EnsureMinHookInitialized()
{
	const MH_STATUS status = MH_Initialize();
	return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
}

static bool InstallOpenGLSwapHook()
{
	if (gOpenGLHookInstalled.load())
		return true;

	HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
	if (!gdi32)
		return false;

	void* target = reinterpret_cast<void*>(GetProcAddress(gdi32, "SwapBuffers"));
	if (!target)
		return false;

	if (!EnsureMinHookInitialized())
		return false;

	const MH_STATUS createStatus = MH_CreateHook(target, reinterpret_cast<void*>(&hkSwapBuffers), reinterpret_cast<void**>(&oSwapBuffers));
	if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED)
		return false;

	const MH_STATUS enableStatus = MH_EnableHook(target);
	if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED)
		return false;

	gOpenGLHookInstalled.store(true);
	if (DEBUG) {
		std::cout << "[ " << Prefix << " ] OpenGL SwapBuffers watcher armed." << std::endl;
	}
	return true;
}

static bool InstallD3D11Hook()
{
	if (gD3D11HookInstalled.load())
		return true;

	const kiero::Status::Enum initStatus = kiero::init(kiero::RenderType::D3D11);
	if (initStatus != kiero::Status::Success && initStatus != kiero::Status::AlreadyInitializedError)
		return false;

	if (kiero::bind(8, reinterpret_cast<void**>(&oPresent), hkPresent) != kiero::Status::Success) {
		return false;
	}
	kiero::bind(13, reinterpret_cast<void**>(&oResizeBuffers), hkResizeBuffers);

	gD3D11HookInstalled.store(true);
	if (DEBUG) {
		std::cout << "[ " << Prefix << " ] D3D11 Present hook armed." << std::endl;
	}
	return true;
}

static bool InstallRenderHooks()
{
	const bool openGLHooked = InstallOpenGLSwapHook();
	const bool d3d11Hooked = InstallD3D11Hook();

	if (DEBUG) {
		std::cout << "[ " << Prefix << " ] Renderer detection --> waiting for first frame (D3D11/OpenGL)\n" << std::endl;
	}

	return openGLHooked || d3d11Hooked;
}

static bool Setup()
{
	if (DEBUG) {
		Utils::CreateConsole();
	}
	if (!InitVars())
		return false;

	AegisUniversal::LoadConfig();

	FindSigs();

	if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP) {
		IL2CPP::Callback::Initialize();
		EnableHooks();
	}

	Lua::LuaState = luaL_newstate();
	if (Lua::LuaState) {
		luaL_openlibs(Lua::LuaState);
		bindToLUA(Lua::LuaState);
	}

	if (!InstallRenderHooks()) {
		return false;
	}

	// secondary threads
	if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP || System::RuntimeBackend == UnityRuntimeBackend::Mono) {
		if (HANDLE cacheThread = CreateThread(nullptr, 0, CacheThread, nullptr, 0, nullptr)) {
			CloseHandle(cacheThread);
		}
	}
	if (HANDLE rainbowThread = CreateThread(nullptr, 0, RainbowThread, nullptr, 0, nullptr)) {
		CloseHandle(rainbowThread);
	}

	return true;
}

static DWORD WINAPI MainThread(LPVOID lpReserved)
{
	if (!Setup()) {
		System::Running.store(false);
		return TRUE;
	}

	System::InitIL2Cpp = System::RuntimeBackend == UnityRuntimeBackend::IL2CPP;
	return TRUE;
}

BOOL WINAPI DllMain(HMODULE mod, DWORD reason, LPVOID lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(mod);
		if (HANDLE mainThread = CreateThread(nullptr, 0, MainThread, mod, 0, nullptr)) {
			CloseHandle(mainThread);
		}
	}
	return TRUE;
}
