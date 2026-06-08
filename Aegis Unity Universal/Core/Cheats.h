#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// my includes
#include <Libraries/Dumper.hpp>
#include <Libraries/imgui/imgui.h>
#include <Core/Variables.h>
#include <Core/HooksFunctions.h>
#include <Utils/SDK.h>
#include <Utils/Themes.h>
#include <Utils/Utils.h>
#include <Core/InternalGameFunctions.h>
#include <Libraries/Unity_Runtime/UnityRuntime.hpp>

using namespace Variables;

namespace AegisUniversal {
	inline int HotkeyCaptureId = 0;

	const char* RuntimeName();

	void CopyText(char* destination, size_t destinationSize, const char* source)
	{
		if (!destination || destinationSize == 0)
			return;

		strncpy_s(destination, destinationSize, source ? source : "", _TRUNCATE);
	}

	void SetConfigStatus(const char* status)
	{
		CopyText(CheatMenuVariables::LastConfigStatus, sizeof(CheatMenuVariables::LastConfigStatus), status);
	}

	std::string ConfigPath()
	{
		char appData[MAX_PATH] = {};
		const DWORD length = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
		std::string directory = length > 0 ? appData : ".";
		directory += "\\AegisUnityUniversal";
		CreateDirectoryA(directory.c_str(), nullptr);
		return directory + "\\config.ini";
	}

	std::string ExternalProfilePath()
	{
		char appData[MAX_PATH] = {};
		const DWORD length = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
		std::string directory = length > 0 ? appData : ".";
		directory += "\\AegisUnityUniversal";
		CreateDirectoryA(directory.c_str(), nullptr);
		return directory + "\\external_profile.ini";
	}

	void WriteColor(std::ofstream& out, const char* key, const ImColor& color)
	{
		out << key << "=" << color.Value.x << "," << color.Value.y << "," << color.Value.z << "," << color.Value.w << "\n";
	}

	bool ParseColor(const std::string& value, ImColor& color)
	{
		float rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		char comma = 0;
		std::stringstream stream(value);
		if (!(stream >> rgba[0] >> comma >> rgba[1] >> comma >> rgba[2]))
			return false;
		if (stream >> comma >> rgba[3]) {}
		color = ImColor(rgba[0], rgba[1], rgba[2], rgba[3]);
		return true;
	}

	bool ReadBool(const std::unordered_map<std::string, std::string>& values, const char* key, bool& target)
	{
		auto it = values.find(key);
		if (it == values.end())
			return false;
		target = it->second == "1" || it->second == "true" || it->second == "True";
		return true;
	}

	bool ReadInt(const std::unordered_map<std::string, std::string>& values, const char* key, int& target)
	{
		auto it = values.find(key);
		if (it == values.end())
			return false;
		try { target = std::stoi(it->second); return true; }
		catch (...) { return false; }
	}

	bool ReadFloat(const std::unordered_map<std::string, std::string>& values, const char* key, float& target)
	{
		auto it = values.find(key);
		if (it == values.end())
			return false;
		try { target = std::stof(it->second); return true; }
		catch (...) { return false; }
	}

	bool ReadColor(const std::unordered_map<std::string, std::string>& values, const char* key, ImColor& target)
	{
		auto it = values.find(key);
		return it != values.end() && ParseColor(it->second, target);
	}

	bool ReadText(const std::unordered_map<std::string, std::string>& values, const char* key, char* target, size_t targetSize)
	{
		auto it = values.find(key);
		if (it == values.end())
			return false;
		CopyText(target, targetSize, it->second.c_str());
		return true;
	}

	void ClampRuntimeSettings()
	{
		const int minCacheRefresh = CheatMenuVariables::StabilityMode ? 750 : 250;
		CheatVariables::CacheRefreshMs = std::clamp(CheatVariables::CacheRefreshMs, minCacheRefresh, 10000);
		CheatVariables::TestObjects::MaxRows = std::clamp(CheatVariables::TestObjects::MaxRows, 10, 250);
		CheatMenuVariables::MaxPlayersPerFrame = std::clamp(CheatMenuVariables::MaxPlayersPerFrame, 16, 250);
		CheatMenuVariables::MaxTestObjectsPerFrame = std::clamp(CheatMenuVariables::MaxTestObjectsPerFrame, 8, 250);

		CheatMenuVariables::PlayersSnaplineType = std::clamp(CheatMenuVariables::PlayersSnaplineType, 0, 2);
		CheatMenuVariables::PlayersBoxStyle = std::clamp(CheatMenuVariables::PlayersBoxStyle, 0, 1);
		CheatMenuVariables::PlayersListMaxRows = std::clamp(CheatMenuVariables::PlayersListMaxRows, 3, 20);
		CheatMenuVariables::PlayersMaxDrawCount = std::clamp(CheatMenuVariables::PlayersMaxDrawCount, 5, 250);
		CheatMenuVariables::DistanceUnit = std::clamp(CheatMenuVariables::DistanceUnit, 0, 1);
		CheatMenuVariables::HudLayoutMode = std::clamp(CheatMenuVariables::HudLayoutMode, 0, 1);
		CheatMenuVariables::AimbotTargetMode = std::clamp(CheatMenuVariables::AimbotTargetMode, 0, 1);

		CheatMenuVariables::CameraCustomFOV = HooksFunctions::ClampCameraFov(CheatMenuVariables::CameraCustomFOV);
		CheatMenuVariables::CameraFovOffset = std::clamp(CheatMenuVariables::CameraFovOffset, -80.0f, 160.0f);
		CheatMenuVariables::AimbotFOV = std::clamp(CheatMenuVariables::AimbotFOV, 1.0f, 800.0f);
		CheatMenuVariables::AimbotFOVThickness = std::clamp(CheatMenuVariables::AimbotFOVThickness, 1.0f, 6.0f);
		CheatMenuVariables::AimbotFOVFillAlpha = std::clamp(CheatMenuVariables::AimbotFOVFillAlpha, 0.01f, 0.25f);
		CheatMenuVariables::AimbotSmoothness = std::clamp(CheatMenuVariables::AimbotSmoothness, 0.0f, 30.0f);
		CheatMenuVariables::AimbotDeadzoneRadius = std::clamp(CheatMenuVariables::AimbotDeadzoneRadius, 0.0f, 80.0f);
		CheatMenuVariables::FakeHeadPosDiff = std::clamp(CheatMenuVariables::FakeHeadPosDiff, -10.0f, 30.0f);
		CheatMenuVariables::FakeFeetPosDiff = std::clamp(CheatMenuVariables::FakeFeetPosDiff, -10.0f, 30.0f);

		CheatMenuVariables::PlayersMaxDistanceValue = std::clamp(CheatMenuVariables::PlayersMaxDistanceValue, 10.0f, 5000.0f);
		CheatMenuVariables::PlayersFadeStartDistance = std::clamp(CheatMenuVariables::PlayersFadeStartDistance, 1.0f, 5000.0f);
		CheatMenuVariables::PlayersFadeEndDistance = std::clamp(CheatMenuVariables::PlayersFadeEndDistance, CheatMenuVariables::PlayersFadeStartDistance + 1.0f, 6000.0f);
		CheatMenuVariables::PlayersFadeMinAlpha = std::clamp(CheatMenuVariables::PlayersFadeMinAlpha, 0.05f, 1.0f);
		CheatMenuVariables::PlayersLabelSize = std::clamp(CheatMenuVariables::PlayersLabelSize, 9.0f, 24.0f);
		CheatMenuVariables::ESPLineThickness = std::clamp(CheatMenuVariables::ESPLineThickness, 1.0f, 6.0f);
		CheatMenuVariables::ESPBoxThickness = std::clamp(CheatMenuVariables::ESPBoxThickness, 1.0f, 6.0f);
		CheatMenuVariables::ESPGlobalAlpha = std::clamp(CheatMenuVariables::ESPGlobalAlpha, 0.05f, 1.0f);
		CheatMenuVariables::PlayersBoxFillAlpha = std::clamp(CheatMenuVariables::PlayersBoxFillAlpha, 0.05f, 0.8f);

		CheatMenuVariables::RadarRange = std::clamp(CheatMenuVariables::RadarRange, 5.0f, 5000.0f);
		CheatMenuVariables::RadarSize = std::clamp(CheatMenuVariables::RadarSize, 80.0f, 320.0f);
		CheatMenuVariables::OffscreenArrowSize = std::clamp(CheatMenuVariables::OffscreenArrowSize, 6.0f, 30.0f);
		CheatMenuVariables::OffscreenArrowRadius = std::clamp(CheatMenuVariables::OffscreenArrowRadius, 40.0f, 1000.0f);
		CheatMenuVariables::TargetMarkerSize = std::clamp(CheatMenuVariables::TargetMarkerSize, 3.0f, 24.0f);

		CheatMenuVariables::CrosshairType = std::clamp(CheatMenuVariables::CrosshairType, 0, 1);
		CheatMenuVariables::CrosshairSize = std::clamp(CheatMenuVariables::CrosshairSize, 0.1f, 100.0f);
		CheatMenuVariables::CrosshairGap = std::clamp(CheatMenuVariables::CrosshairGap, 0.0f, 50.0f);
		CheatMenuVariables::CrosshairThickness = std::clamp(CheatMenuVariables::CrosshairThickness, 1.0f, 8.0f);
		CheatMenuVariables::CrosshairDotSize = std::clamp(CheatMenuVariables::CrosshairDotSize, 1.0f, 12.0f);

		CheatMenuVariables::FunConfettiCount = std::clamp(CheatMenuVariables::FunConfettiCount, 10, 140);
		CheatMenuVariables::FunIntensity = std::clamp(CheatMenuVariables::FunIntensity, 0.25f, 3.0f);
		CheatMenuVariables::FunSpeed = std::clamp(CheatMenuVariables::FunSpeed, 0.1f, 5.0f);
		CheatMenuVariables::FunBorderThickness = std::clamp(CheatMenuVariables::FunBorderThickness, 1.0f, 10.0f);
		CheatMenuVariables::HudMargin = std::clamp(CheatMenuVariables::HudMargin, 4.0f, 80.0f);
		CheatMenuVariables::HudGap = std::clamp(CheatMenuVariables::HudGap, 2.0f, 40.0f);
	}

	bool SaveConfig()
	{
		ClampRuntimeSettings();

		std::ofstream out(ConfigPath(), std::ios::trunc);
		if (!out.is_open()) {
			SetConfigStatus("Config save failed");
			return false;
		}

		out << "player_component=" << CheatVariables::PlayerComponentName << "\n";
		out << "player_fallback_enabled=" << (CheatVariables::UsePlayerFallbackComponent ? 1 : 0) << "\n";
		out << "player_fallback_component=" << CheatVariables::PlayerFallbackComponentName << "\n";
		out << "cache_refresh_ms=" << CheatVariables::CacheRefreshMs << "\n";
		out << "test_component=" << CheatVariables::TestObjects::Name << "\n";
		out << "object_explorer_max_rows=" << CheatVariables::TestObjects::MaxRows << "\n";

		out << "show_watermark=" << (CheatMenuVariables::Watermark ? 1 : 0) << "\n";
		out << "players_snapline=" << (CheatMenuVariables::PlayersSnapline ? 1 : 0) << "\n";
		out << "players_snapline_rainbow=" << (CheatMenuVariables::RainbowPlayersSnapline ? 1 : 0) << "\n";
		out << "players_snapline_type=" << CheatMenuVariables::PlayersSnaplineType << "\n";
		out << "players_snapline_offset_x=" << CheatMenuVariables::PlayersSnaplineOffsetX << "\n";
		out << "players_snapline_offset_y=" << CheatMenuVariables::PlayersSnaplineOffsetY << "\n";
		WriteColor(out, "players_snapline_color", CheatMenuVariables::PlayersSnaplineColor);
		out << "players_box=" << (CheatMenuVariables::PlayersBox ? 1 : 0) << "\n";
		out << "players_box_rainbow=" << (CheatMenuVariables::RainbowPlayersBox ? 1 : 0) << "\n";
		out << "players_box_filled=" << (CheatMenuVariables::PlayersBoxFilled ? 1 : 0) << "\n";
		out << "players_box_fill_alpha=" << CheatMenuVariables::PlayersBoxFillAlpha << "\n";
		out << "players_box_style=" << CheatMenuVariables::PlayersBoxStyle << "\n";
		WriteColor(out, "players_box_color", CheatMenuVariables::PlayersBoxColor);
		out << "players_skeleton=" << (CheatMenuVariables::PlayerSkeleton ? 1 : 0) << "\n";
		out << "players_name=" << (CheatMenuVariables::PlayersName ? 1 : 0) << "\n";
		out << "players_distance=" << (CheatMenuVariables::PlayersDistance ? 1 : 0) << "\n";
		out << "players_max_distance_enabled=" << (CheatMenuVariables::PlayersMaxDistance ? 1 : 0) << "\n";
		out << "players_max_distance=" << CheatMenuVariables::PlayersMaxDistanceValue << "\n";
		out << "players_count_overlay=" << (CheatMenuVariables::PlayersCountOverlay ? 1 : 0) << "\n";
		out << "players_list_overlay=" << (CheatMenuVariables::PlayersListOverlay ? 1 : 0) << "\n";
		out << "players_list_max_rows=" << CheatMenuVariables::PlayersListMaxRows << "\n";
		out << "players_list_pos_x=" << CheatMenuVariables::PlayersListPosX << "\n";
		out << "players_list_pos_y=" << CheatMenuVariables::PlayersListPosY << "\n";
		out << "players_target_info_overlay=" << (CheatMenuVariables::PlayersTargetInfoOverlay ? 1 : 0) << "\n";
		out << "target_info_pos_x=" << CheatMenuVariables::TargetInfoPosX << "\n";
		out << "target_info_pos_y=" << CheatMenuVariables::TargetInfoPosY << "\n";
		out << "players_draw_limit_enabled=" << (CheatMenuVariables::PlayersDrawLimit ? 1 : 0) << "\n";
		out << "players_draw_limit=" << CheatMenuVariables::PlayersMaxDrawCount << "\n";
		out << "players_distance_fade=" << (CheatMenuVariables::PlayersDistanceFade ? 1 : 0) << "\n";
		out << "players_fade_start_distance=" << CheatMenuVariables::PlayersFadeStartDistance << "\n";
		out << "players_fade_end_distance=" << CheatMenuVariables::PlayersFadeEndDistance << "\n";
		out << "players_fade_min_alpha=" << CheatMenuVariables::PlayersFadeMinAlpha << "\n";
		out << "players_offscreen_arrows=" << (CheatMenuVariables::PlayersOffscreenArrows ? 1 : 0) << "\n";
		out << "offscreen_arrow_labels=" << (CheatMenuVariables::OffscreenArrowLabels ? 1 : 0) << "\n";
		out << "players_offscreen_arrows_rainbow=" << (CheatMenuVariables::RainbowOffscreenArrows ? 1 : 0) << "\n";
		out << "offscreen_arrow_size=" << CheatMenuVariables::OffscreenArrowSize << "\n";
		out << "offscreen_arrow_radius=" << CheatMenuVariables::OffscreenArrowRadius << "\n";
		WriteColor(out, "offscreen_arrow_color", CheatMenuVariables::OffscreenArrowColor);
		out << "players_target_marker=" << (CheatMenuVariables::PlayersTargetMarker ? 1 : 0) << "\n";
		out << "players_target_marker_rainbow=" << (CheatMenuVariables::RainbowTargetMarker ? 1 : 0) << "\n";
		out << "target_marker_size=" << CheatMenuVariables::TargetMarkerSize << "\n";
		WriteColor(out, "target_marker_color", CheatMenuVariables::TargetMarkerColor);
		out << "players_label_size=" << CheatMenuVariables::PlayersLabelSize << "\n";
		out << "esp_line_thickness=" << CheatMenuVariables::ESPLineThickness << "\n";
		out << "esp_box_thickness=" << CheatMenuVariables::ESPBoxThickness << "\n";
		out << "esp_global_alpha=" << CheatMenuVariables::ESPGlobalAlpha << "\n";
		out << "distance_unit=" << CheatMenuVariables::DistanceUnit << "\n";
		WriteColor(out, "players_text_color", CheatMenuVariables::PlayersTextColor);
		out << "radar=" << (CheatMenuVariables::Radar ? 1 : 0) << "\n";
		out << "radar_background=" << (CheatMenuVariables::RadarBackground ? 1 : 0) << "\n";
		out << "radar_range_rings=" << (CheatMenuVariables::RadarRangeRings ? 1 : 0) << "\n";
		out << "radar_labels=" << (CheatMenuVariables::RadarLabels ? 1 : 0) << "\n";
		out << "radar_range=" << CheatMenuVariables::RadarRange << "\n";
		out << "radar_size=" << CheatMenuVariables::RadarSize << "\n";
		out << "radar_pos_x=" << CheatMenuVariables::RadarPosX << "\n";
		out << "radar_pos_y=" << CheatMenuVariables::RadarPosY << "\n";
		WriteColor(out, "radar_color", CheatMenuVariables::RadarColor);

		out << "crosshair=" << (CheatMenuVariables::Crosshair ? 1 : 0) << "\n";
		out << "crosshair_rainbow=" << (CheatMenuVariables::RainbowCrosshair ? 1 : 0) << "\n";
		out << "crosshair_type=" << CheatMenuVariables::CrosshairType << "\n";
		out << "crosshair_size=" << CheatMenuVariables::CrosshairSize << "\n";
		out << "crosshair_gap=" << CheatMenuVariables::CrosshairGap << "\n";
		out << "crosshair_thickness=" << CheatMenuVariables::CrosshairThickness << "\n";
		out << "crosshair_dot=" << (CheatMenuVariables::CrosshairDot ? 1 : 0) << "\n";
		out << "crosshair_dot_size=" << CheatMenuVariables::CrosshairDotSize << "\n";
		out << "crosshair_outline=" << (CheatMenuVariables::CrosshairOutline ? 1 : 0) << "\n";
		WriteColor(out, "crosshair_color", CheatMenuVariables::CrosshairColor);

		out << "aimbot=" << (CheatMenuVariables::EnableAimbot ? 1 : 0) << "\n";
		out << "aimbot_fov_check=" << (CheatMenuVariables::AimbotFOVCheck ? 1 : 0) << "\n";
		out << "aimbot_fov_rainbow=" << (CheatMenuVariables::RainbowAimbotFOV ? 1 : 0) << "\n";
		WriteColor(out, "aimbot_fov_color", CheatMenuVariables::AimbotFOVColor);
		out << "aimbot_fov=" << CheatMenuVariables::AimbotFOV << "\n";
		out << "aimbot_fov_thickness=" << CheatMenuVariables::AimbotFOVThickness << "\n";
		out << "aimbot_fov_filled=" << (CheatMenuVariables::AimbotFOVFilled ? 1 : 0) << "\n";
		out << "aimbot_fov_fill_alpha=" << CheatMenuVariables::AimbotFOVFillAlpha << "\n";
		out << "aimbot_smooth=" << CheatMenuVariables::AimbotSmoothness << "\n";
		out << "aimbot_deadzone=" << (CheatMenuVariables::AimbotDeadzone ? 1 : 0) << "\n";
		out << "aimbot_deadzone_radius=" << CheatMenuVariables::AimbotDeadzoneRadius << "\n";
		out << "aimbot_target_mode=" << CheatMenuVariables::AimbotTargetMode << "\n";
		out << "fake_head_diff=" << CheatMenuVariables::FakeHeadPosDiff << "\n";
		out << "fake_feet_diff=" << CheatMenuVariables::FakeFeetPosDiff << "\n";

		out << "camera_fov_changer=" << (CheatMenuVariables::CameraFovChanger ? 1 : 0) << "\n";
		out << "camera_fov=" << CheatMenuVariables::CameraCustomFOV << "\n";
		out << "camera_fov_additive=" << (CheatMenuVariables::CameraFovAdditive ? 1 : 0) << "\n";
		out << "camera_fov_offset=" << CheatMenuVariables::CameraFovOffset << "\n";
		out << "stability_mode=" << (CheatMenuVariables::StabilityMode ? 1 : 0) << "\n";
		out << "max_players_per_frame=" << CheatMenuVariables::MaxPlayersPerFrame << "\n";
		out << "max_test_objects_per_frame=" << CheatMenuVariables::MaxTestObjectsPerFrame << "\n";
		out << "streamer_mode=" << (CheatMenuVariables::StreamerMode ? 1 : 0) << "\n";
		out << "fun_mode=" << (CheatMenuVariables::FunMode ? 1 : 0) << "\n";
		out << "fun_confetti=" << (CheatMenuVariables::FunConfetti ? 1 : 0) << "\n";
		out << "fun_rainbow_border=" << (CheatMenuVariables::FunRainbowBorder ? 1 : 0) << "\n";
		out << "fun_crosshair_orbit=" << (CheatMenuVariables::FunCrosshairOrbit ? 1 : 0) << "\n";
		out << "fun_bouncy_watermark=" << (CheatMenuVariables::FunBouncyWatermark ? 1 : 0) << "\n";
		out << "fun_confetti_count=" << CheatMenuVariables::FunConfettiCount << "\n";
		out << "fun_intensity=" << CheatMenuVariables::FunIntensity << "\n";
		out << "fun_speed=" << CheatMenuVariables::FunSpeed << "\n";
		out << "fun_border_thickness=" << CheatMenuVariables::FunBorderThickness << "\n";
		out << "status_overlay=" << (CheatMenuVariables::StatusOverlay ? 1 : 0) << "\n";
		out << "status_overlay_detailed=" << (CheatMenuVariables::StatusOverlayDetailed ? 1 : 0) << "\n";
		out << "hud_auto_layout=" << (CheatMenuVariables::HudAutoLayout ? 1 : 0) << "\n";
		out << "hud_layout_mode=" << CheatMenuVariables::HudLayoutMode << "\n";
		out << "hud_margin=" << CheatMenuVariables::HudMargin << "\n";
		out << "hud_gap=" << CheatMenuVariables::HudGap << "\n";
		out << "auto_save_config=" << (CheatMenuVariables::AutoSaveConfig ? 1 : 0) << "\n";
		out << "overlay_pos_x=" << CheatMenuVariables::OverlayPosX << "\n";
		out << "overlay_pos_y=" << CheatMenuVariables::OverlayPosY << "\n";
		out << "menu_key=" << KEYS::SHOWMENU_KEY << "\n";
		out << "detach_key=" << KEYS::DEATTACH_KEY << "\n";
		out << "panic_key=" << KEYS::PANIC_KEY << "\n";
		out << "box_key=" << KEYS::PLAYERS_BOX_KEY << "\n";
		out << "snapline_key=" << KEYS::PLAYERS_SNAPLINE_KEY << "\n";
		out << "aimbot_key=" << KEYS::AIMBOT_KEY << "\n";
		out << "crosshair_key=" << KEYS::CROSSHAIR_KEY << "\n";
		out << "aimbot_activation_key=" << KEYS::AIMBOT_ACTIVATION_KEY << "\n";

		SetConfigStatus("Config saved");
		return true;
	}

	bool LoadConfig()
	{
		std::ifstream in(ConfigPath());
		if (!in.is_open()) {
			SetConfigStatus("Config file not found");
			return false;
		}

		std::unordered_map<std::string, std::string> values;
		std::string line;
		while (std::getline(in, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			const size_t separator = line.find('=');
			if (separator == std::string::npos)
				continue;
			values[line.substr(0, separator)] = line.substr(separator + 1);
		}

		ReadText(values, "player_component", CheatVariables::PlayerComponentName, sizeof(CheatVariables::PlayerComponentName));
		ReadBool(values, "player_fallback_enabled", CheatVariables::UsePlayerFallbackComponent);
		ReadText(values, "player_fallback_component", CheatVariables::PlayerFallbackComponentName, sizeof(CheatVariables::PlayerFallbackComponentName));
		ReadInt(values, "cache_refresh_ms", CheatVariables::CacheRefreshMs);
		ReadText(values, "test_component", CheatVariables::TestObjects::Name, sizeof(CheatVariables::TestObjects::Name));
		ReadInt(values, "object_explorer_max_rows", CheatVariables::TestObjects::MaxRows);

		ReadBool(values, "show_watermark", CheatMenuVariables::Watermark);
		ReadBool(values, "players_snapline", CheatMenuVariables::PlayersSnapline);
		ReadBool(values, "players_snapline_rainbow", CheatMenuVariables::RainbowPlayersSnapline);
		ReadInt(values, "players_snapline_type", CheatMenuVariables::PlayersSnaplineType);
		ReadFloat(values, "players_snapline_offset_x", CheatMenuVariables::PlayersSnaplineOffsetX);
		ReadFloat(values, "players_snapline_offset_y", CheatMenuVariables::PlayersSnaplineOffsetY);
		ReadColor(values, "players_snapline_color", CheatMenuVariables::PlayersSnaplineColor);
		ReadBool(values, "players_box", CheatMenuVariables::PlayersBox);
		ReadBool(values, "players_box_rainbow", CheatMenuVariables::RainbowPlayersBox);
		ReadBool(values, "players_box_filled", CheatMenuVariables::PlayersBoxFilled);
		ReadFloat(values, "players_box_fill_alpha", CheatMenuVariables::PlayersBoxFillAlpha);
		ReadInt(values, "players_box_style", CheatMenuVariables::PlayersBoxStyle);
		ReadColor(values, "players_box_color", CheatMenuVariables::PlayersBoxColor);
		ReadBool(values, "players_skeleton", CheatMenuVariables::PlayerSkeleton);
		ReadBool(values, "players_name", CheatMenuVariables::PlayersName);
		ReadBool(values, "players_distance", CheatMenuVariables::PlayersDistance);
		ReadBool(values, "players_max_distance_enabled", CheatMenuVariables::PlayersMaxDistance);
		ReadFloat(values, "players_max_distance", CheatMenuVariables::PlayersMaxDistanceValue);
		ReadBool(values, "players_count_overlay", CheatMenuVariables::PlayersCountOverlay);
		ReadBool(values, "players_list_overlay", CheatMenuVariables::PlayersListOverlay);
		ReadInt(values, "players_list_max_rows", CheatMenuVariables::PlayersListMaxRows);
		ReadFloat(values, "players_list_pos_x", CheatMenuVariables::PlayersListPosX);
		ReadFloat(values, "players_list_pos_y", CheatMenuVariables::PlayersListPosY);
		ReadBool(values, "players_target_info_overlay", CheatMenuVariables::PlayersTargetInfoOverlay);
		ReadFloat(values, "target_info_pos_x", CheatMenuVariables::TargetInfoPosX);
		ReadFloat(values, "target_info_pos_y", CheatMenuVariables::TargetInfoPosY);
		ReadBool(values, "players_draw_limit_enabled", CheatMenuVariables::PlayersDrawLimit);
		ReadInt(values, "players_draw_limit", CheatMenuVariables::PlayersMaxDrawCount);
		ReadBool(values, "players_distance_fade", CheatMenuVariables::PlayersDistanceFade);
		ReadFloat(values, "players_fade_start_distance", CheatMenuVariables::PlayersFadeStartDistance);
		ReadFloat(values, "players_fade_end_distance", CheatMenuVariables::PlayersFadeEndDistance);
		ReadFloat(values, "players_fade_min_alpha", CheatMenuVariables::PlayersFadeMinAlpha);
		ReadBool(values, "players_offscreen_arrows", CheatMenuVariables::PlayersOffscreenArrows);
		ReadBool(values, "offscreen_arrow_labels", CheatMenuVariables::OffscreenArrowLabels);
		ReadBool(values, "players_offscreen_arrows_rainbow", CheatMenuVariables::RainbowOffscreenArrows);
		ReadFloat(values, "offscreen_arrow_size", CheatMenuVariables::OffscreenArrowSize);
		ReadFloat(values, "offscreen_arrow_radius", CheatMenuVariables::OffscreenArrowRadius);
		ReadColor(values, "offscreen_arrow_color", CheatMenuVariables::OffscreenArrowColor);
		ReadBool(values, "players_target_marker", CheatMenuVariables::PlayersTargetMarker);
		ReadBool(values, "players_target_marker_rainbow", CheatMenuVariables::RainbowTargetMarker);
		ReadFloat(values, "target_marker_size", CheatMenuVariables::TargetMarkerSize);
		ReadColor(values, "target_marker_color", CheatMenuVariables::TargetMarkerColor);
		ReadFloat(values, "players_label_size", CheatMenuVariables::PlayersLabelSize);
		ReadFloat(values, "esp_line_thickness", CheatMenuVariables::ESPLineThickness);
		ReadFloat(values, "esp_box_thickness", CheatMenuVariables::ESPBoxThickness);
		ReadFloat(values, "esp_global_alpha", CheatMenuVariables::ESPGlobalAlpha);
		ReadInt(values, "distance_unit", CheatMenuVariables::DistanceUnit);
		ReadColor(values, "players_text_color", CheatMenuVariables::PlayersTextColor);
		ReadBool(values, "radar", CheatMenuVariables::Radar);
		ReadBool(values, "radar_background", CheatMenuVariables::RadarBackground);
		ReadBool(values, "radar_range_rings", CheatMenuVariables::RadarRangeRings);
		ReadBool(values, "radar_labels", CheatMenuVariables::RadarLabels);
		ReadFloat(values, "radar_range", CheatMenuVariables::RadarRange);
		ReadFloat(values, "radar_size", CheatMenuVariables::RadarSize);
		ReadFloat(values, "radar_pos_x", CheatMenuVariables::RadarPosX);
		ReadFloat(values, "radar_pos_y", CheatMenuVariables::RadarPosY);
		ReadColor(values, "radar_color", CheatMenuVariables::RadarColor);

		ReadBool(values, "crosshair", CheatMenuVariables::Crosshair);
		ReadBool(values, "crosshair_rainbow", CheatMenuVariables::RainbowCrosshair);
		ReadInt(values, "crosshair_type", CheatMenuVariables::CrosshairType);
		ReadFloat(values, "crosshair_size", CheatMenuVariables::CrosshairSize);
		ReadFloat(values, "crosshair_gap", CheatMenuVariables::CrosshairGap);
		ReadFloat(values, "crosshair_thickness", CheatMenuVariables::CrosshairThickness);
		ReadBool(values, "crosshair_dot", CheatMenuVariables::CrosshairDot);
		ReadFloat(values, "crosshair_dot_size", CheatMenuVariables::CrosshairDotSize);
		ReadBool(values, "crosshair_outline", CheatMenuVariables::CrosshairOutline);
		ReadColor(values, "crosshair_color", CheatMenuVariables::CrosshairColor);

		ReadBool(values, "aimbot", CheatMenuVariables::EnableAimbot);
		ReadBool(values, "aimbot_fov_check", CheatMenuVariables::AimbotFOVCheck);
		ReadBool(values, "aimbot_fov_rainbow", CheatMenuVariables::RainbowAimbotFOV);
		ReadColor(values, "aimbot_fov_color", CheatMenuVariables::AimbotFOVColor);
		ReadFloat(values, "aimbot_fov", CheatMenuVariables::AimbotFOV);
		ReadFloat(values, "aimbot_fov_thickness", CheatMenuVariables::AimbotFOVThickness);
		ReadBool(values, "aimbot_fov_filled", CheatMenuVariables::AimbotFOVFilled);
		ReadFloat(values, "aimbot_fov_fill_alpha", CheatMenuVariables::AimbotFOVFillAlpha);
		ReadFloat(values, "aimbot_smooth", CheatMenuVariables::AimbotSmoothness);
		ReadBool(values, "aimbot_deadzone", CheatMenuVariables::AimbotDeadzone);
		ReadFloat(values, "aimbot_deadzone_radius", CheatMenuVariables::AimbotDeadzoneRadius);
		ReadInt(values, "aimbot_target_mode", CheatMenuVariables::AimbotTargetMode);
		ReadFloat(values, "fake_head_diff", CheatMenuVariables::FakeHeadPosDiff);
		ReadFloat(values, "fake_feet_diff", CheatMenuVariables::FakeFeetPosDiff);

		ReadBool(values, "camera_fov_changer", CheatMenuVariables::CameraFovChanger);
		ReadFloat(values, "camera_fov", CheatMenuVariables::CameraCustomFOV);
		ReadBool(values, "camera_fov_additive", CheatMenuVariables::CameraFovAdditive);
		ReadFloat(values, "camera_fov_offset", CheatMenuVariables::CameraFovOffset);
		ReadBool(values, "stability_mode", CheatMenuVariables::StabilityMode);
		ReadInt(values, "max_players_per_frame", CheatMenuVariables::MaxPlayersPerFrame);
		ReadInt(values, "max_test_objects_per_frame", CheatMenuVariables::MaxTestObjectsPerFrame);
		ReadBool(values, "streamer_mode", CheatMenuVariables::StreamerMode);
		ReadBool(values, "fun_mode", CheatMenuVariables::FunMode);
		ReadBool(values, "fun_confetti", CheatMenuVariables::FunConfetti);
		ReadBool(values, "fun_rainbow_border", CheatMenuVariables::FunRainbowBorder);
		ReadBool(values, "fun_crosshair_orbit", CheatMenuVariables::FunCrosshairOrbit);
		ReadBool(values, "fun_bouncy_watermark", CheatMenuVariables::FunBouncyWatermark);
		ReadInt(values, "fun_confetti_count", CheatMenuVariables::FunConfettiCount);
		ReadFloat(values, "fun_intensity", CheatMenuVariables::FunIntensity);
		ReadFloat(values, "fun_speed", CheatMenuVariables::FunSpeed);
		ReadFloat(values, "fun_border_thickness", CheatMenuVariables::FunBorderThickness);
		ReadBool(values, "status_overlay", CheatMenuVariables::StatusOverlay);
		ReadBool(values, "status_overlay_detailed", CheatMenuVariables::StatusOverlayDetailed);
		ReadBool(values, "hud_auto_layout", CheatMenuVariables::HudAutoLayout);
		ReadInt(values, "hud_layout_mode", CheatMenuVariables::HudLayoutMode);
		ReadFloat(values, "hud_margin", CheatMenuVariables::HudMargin);
		ReadFloat(values, "hud_gap", CheatMenuVariables::HudGap);
		ReadBool(values, "auto_save_config", CheatMenuVariables::AutoSaveConfig);
		ReadFloat(values, "overlay_pos_x", CheatMenuVariables::OverlayPosX);
		ReadFloat(values, "overlay_pos_y", CheatMenuVariables::OverlayPosY);
		ReadInt(values, "menu_key", KEYS::SHOWMENU_KEY);
		ReadInt(values, "detach_key", KEYS::DEATTACH_KEY);
		ReadInt(values, "panic_key", KEYS::PANIC_KEY);
		ReadInt(values, "box_key", KEYS::PLAYERS_BOX_KEY);
		ReadInt(values, "snapline_key", KEYS::PLAYERS_SNAPLINE_KEY);
		ReadInt(values, "aimbot_key", KEYS::AIMBOT_KEY);
		ReadInt(values, "crosshair_key", KEYS::CROSSHAIR_KEY);
		ReadInt(values, "aimbot_activation_key", KEYS::AIMBOT_ACTIVATION_KEY);

		ClampRuntimeSettings();
		CheatMenuVariables::ConfigLoaded = true;
		CheatVariables::ForcePlayerCacheRefresh.store(true);
		SetConfigStatus("Config loaded");
		return true;
	}

	const char* KeyName(int key)
	{
		static char buffers[8][64] = {};
		static int bufferIndex = 0;
		char* buffer = buffers[bufferIndex++ % 8];

		if (key <= 0) {
			CopyText(buffer, 64, "None");
			return buffer;
		}

		UINT scanCode = MapVirtualKeyA(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
		switch (key) {
		case VK_LEFT:
		case VK_RIGHT:
		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_END:
		case VK_HOME:
		case VK_INSERT:
		case VK_DELETE:
			scanCode |= 0x100;
			break;
		}

		const LONG lParam = static_cast<LONG>(scanCode << 16);
		if (GetKeyNameTextA(lParam, buffer, 64) > 0)
			return buffer;

		sprintf_s(buffer, 64, "VK_%02X", key);
		return buffer;
	}

	bool HotkeyControl(const char* label, int& key, int captureId)
	{
		bool changed = false;
		ImGui::PushID(captureId);
		if (HotkeyCaptureId == captureId) {
			ImGui::Button("Press key...", ImVec2(110, 0));
			for (int vk = 8; vk < 255; ++vk) {
				if (GetAsyncKeyState(vk) & 1) {
					key = vk == VK_ESCAPE ? 0 : vk;
					HotkeyCaptureId = 0;
					changed = true;
					break;
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear")) {
				key = 0;
				HotkeyCaptureId = 0;
				changed = true;
			}
		}
		else {
			if (ImGui::Button(KeyName(key), ImVec2(110, 0))) {
				HotkeyCaptureId = captureId;
			}
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(label);
		ImGui::PopID();
		return changed;
	}

	void DisableAllRuntimeFeatures()
	{
		CheatMenuVariables::PlayersSnapline = false;
		CheatMenuVariables::PlayersBox = false;
		CheatMenuVariables::PlayerSkeleton = false;
		CheatMenuVariables::PlayerChams = false;
		CheatMenuVariables::PlayersHealth = false;
		CheatMenuVariables::PlayersName = false;
		CheatMenuVariables::PlayersDistance = false;
		CheatMenuVariables::PlayersMaxDistance = false;
		CheatMenuVariables::PlayersCountOverlay = false;
		CheatMenuVariables::PlayersListOverlay = false;
		CheatMenuVariables::PlayersTargetInfoOverlay = false;
		CheatMenuVariables::PlayersDrawLimit = false;
		CheatMenuVariables::PlayersDistanceFade = false;
		CheatMenuVariables::PlayersOffscreenArrows = false;
		CheatMenuVariables::OffscreenArrowLabels = false;
		CheatMenuVariables::PlayersTargetMarker = false;
		CheatMenuVariables::Radar = false;
		CheatMenuVariables::StatusOverlay = false;
		CheatMenuVariables::EnableAimbot = false;
		CheatMenuVariables::AimbotFOVCheck = false;
		CheatMenuVariables::Crosshair = false;
		CheatMenuVariables::CameraFovChanger = false;
		CheatMenuVariables::StabilityMode = true;
		CheatMenuVariables::Spinbot = false;
		CheatMenuVariables::FunMode = false;
		CheatVariables::TargetPlayer = nullptr;
		CheatVariables::TestObjects::Chams = false;
		CheatVariables::TestObjects::Snapline = false;
		CheatVariables::TestObjects::Box = false;
		CheatVariables::TestObjects::Aimbot = false;
	}

	void ApplyQuickPreset(int preset)
	{
		DisableAllRuntimeFeatures();

		switch (preset) {
		case 0: // clean visuals
			CheatMenuVariables::PlayersBox = true;
			CheatMenuVariables::PlayersName = true;
			CheatMenuVariables::PlayersDistance = true;
			CheatMenuVariables::PlayersMaxDistance = true;
			CheatMenuVariables::PlayersMaxDistanceValue = 250.0f;
			CheatMenuVariables::PlayersDrawLimit = true;
			CheatMenuVariables::PlayersMaxDrawCount = 50;
			CheatMenuVariables::PlayersDistanceFade = true;
			CheatMenuVariables::Crosshair = true;
			CheatMenuVariables::CrosshairDot = true;
			CheatMenuVariables::CrosshairOutline = true;
			CheatMenuVariables::StatusOverlay = true;
			break;
		case 1: // scout
			CheatMenuVariables::PlayersName = true;
			CheatMenuVariables::PlayersDistance = true;
			CheatMenuVariables::PlayersOffscreenArrows = true;
			CheatMenuVariables::OffscreenArrowLabels = true;
			CheatMenuVariables::PlayersTargetMarker = true;
			CheatMenuVariables::PlayersTargetInfoOverlay = true;
			CheatMenuVariables::Radar = true;
			CheatMenuVariables::RadarRangeRings = true;
			CheatMenuVariables::RadarLabels = true;
			CheatMenuVariables::PlayersListOverlay = true;
			CheatMenuVariables::PlayersCountOverlay = true;
			CheatMenuVariables::StatusOverlay = true;
			break;
		case 2: // diagnostics
			CheatMenuVariables::EnableDeveloperOptions = true;
			CheatMenuVariables::StatusOverlay = true;
			CheatMenuVariables::StatusOverlayDetailed = true;
			CheatMenuVariables::PlayersCountOverlay = true;
			CheatMenuVariables::PlayersListOverlay = true;
			CheatMenuVariables::PlayersDrawLimit = true;
			CheatMenuVariables::PlayersDistanceFade = true;
			CheatMenuVariables::PlayersTargetInfoOverlay = true;
			CheatVariables::ForcePlayerCacheRefresh.store(true);
			break;
		case 3: // party
			CheatMenuVariables::FunMode = true;
			CheatMenuVariables::FunConfetti = true;
			CheatMenuVariables::FunRainbowBorder = true;
			CheatMenuVariables::FunCrosshairOrbit = true;
			CheatMenuVariables::FunBouncyWatermark = true;
			CheatMenuVariables::Crosshair = true;
			CheatMenuVariables::CrosshairDot = true;
			CheatMenuVariables::RainbowCrosshair = true;
			CheatMenuVariables::Watermark = true;
			break;
		default:
			break;
		}
	}

	std::string BuildDiagnostics()
	{
		std::ostringstream out;
		out << AppName << "\r\n";
		out << "Runtime: " << RuntimeName() << "\r\n";
		out << "Screen: " << System::ScreenSize.x << "x" << System::ScreenSize.y << "\r\n";
		out << "FPS: " << ImGui::GetIO().Framerate << "\r\n";
		out << "Player component: " << CheatVariables::PlayerComponentName << "\r\n";
		out << "Fallback component: " << (CheatVariables::UsePlayerFallbackComponent ? CheatVariables::PlayerFallbackComponentName : "disabled") << "\r\n";
		out << "Cached players: " << CheatVariables::PlayersCacheCount.load() << "\r\n";
		out << "Cache source: " << CheatVariables::PlayersCacheSource.load() << "\r\n";
		out << "Streamer mode: " << (CheatMenuVariables::StreamerMode ? "enabled" : "disabled") << "\r\n";
		out << "Fun mode: " << (CheatMenuVariables::FunMode ? "enabled" : "disabled") << "\r\n";
		out << "Stability mode: " << (CheatMenuVariables::StabilityMode ? "enabled" : "disabled") << "\r\n";
		out << "Max players/frame: " << CheatMenuVariables::MaxPlayersPerFrame << "\r\n";
		out << "HUD auto layout: " << (CheatMenuVariables::HudAutoLayout ? "enabled" : "disabled") << "\r\n";
		out << "Distance unit: " << (CheatMenuVariables::DistanceUnit == 1 ? "feet" : "meters") << "\r\n";
		out << "ESP alpha: " << CheatMenuVariables::ESPGlobalAlpha << "\r\n";
		out << "ESP draw cap: " << (CheatMenuVariables::PlayersDrawLimit ? CheatMenuVariables::PlayersMaxDrawCount : 0) << "\r\n";
		out << "Player list overlay: " << (CheatMenuVariables::PlayersListOverlay ? "enabled" : "disabled") << "\r\n";
		out << "Distance fade: " << (CheatMenuVariables::PlayersDistanceFade ? "enabled" : "disabled") << "\r\n";
		out << "Aimbot deadzone: " << (CheatMenuVariables::AimbotDeadzone ? CheatMenuVariables::AimbotDeadzoneRadius : 0.0f) << "\r\n";
		out << "Object component: " << CheatVariables::TestObjects::Name << "\r\n";
		out << "Cached objects: " << CheatVariables::TestObjects::CacheCount.load() << "\r\n";
		out << "FOV hook: " << (CheatVariables::CameraFovHookEnabled ? "active" : (CheatVariables::CameraFovHookFailed ? "failed" : "unavailable")) << "\r\n";
		out << "FOV desired: " << CheatVariables::CameraFovLastDesired << "\r\n";
		out << "Config: " << ConfigPath() << "\r\n";
		out << "Status: " << CheatMenuVariables::LastConfigStatus << "\r\n";
		return out.str();
	}

	bool CopyTextToClipboard(const std::string& text)
	{
		if (!OpenClipboard(nullptr))
			return false;

		EmptyClipboard();
		HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
		if (!memory) {
			CloseClipboard();
			return false;
		}

		void* locked = GlobalLock(memory);
		if (!locked) {
			GlobalFree(memory);
			CloseClipboard();
			return false;
		}

		memcpy(locked, text.c_str(), text.size() + 1);
		GlobalUnlock(memory);
		SetClipboardData(CF_TEXT, memory);
		CloseClipboard();
		return true;
	}

	bool CopyDiagnosticsToClipboard()
	{
		const bool copied = CopyTextToClipboard(BuildDiagnostics());
		SetConfigStatus(copied ? "Diagnostics copied" : "Clipboard copy failed");
		return copied;
	}

	std::string ObjectName(Unity::CGameObject* object)
	{
		if (!object)
			return "null";

		if (System::RuntimeBackend == UnityRuntimeBackend::Mono) {
			static Mono::Method* getNameMethod = nullptr;
			if (!getNameMethod)
				getNameMethod = UnityRuntime::ResolveMonoMethod(nullptr, "UnityEngine.Object", "get_name", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = getNameMethod ? Mono::Invoke(getNameMethod, object, nullptr, &exception) : nullptr;
			if (!exception && result) {
				const std::string name = Mono::StringToUtf8(reinterpret_cast<Mono::String*>(result));
				if (!name.empty())
					return name;
			}

			return "unnamed";
		}

		try {
			Unity::System_String* name = object->GetName();
			if (name)
				return name->ToString();
		}
		catch (...) {}

		return "unnamed";
	}

	namespace MonoUnity
	{
		bool Active()
		{
			return System::RuntimeBackend == UnityRuntimeBackend::Mono && Mono::IsInitialized();
		}

		Mono::Method* Method(const char* className, const char* methodName, int argumentCount)
		{
			return UnityRuntime::ResolveMonoMethod(nullptr, className, methodName, argumentCount);
		}

		Mono::Method* MethodDesc(const char* className, const char* description, const char* fallbackName, int fallbackArgumentCount)
		{
			Mono::Method* method = UnityRuntime::ResolveMonoMethodDesc(nullptr, className, description);
			return method ? method : Method(className, fallbackName, fallbackArgumentCount);
		}

		Unity::CCamera* GetCamera(const char* methodName)
		{
			static Mono::Method* getMain = nullptr;
			static Mono::Method* getCurrent = nullptr;
			Mono::Method*& method = strcmp(methodName, "get_current") == 0 ? getCurrent : getMain;
			if (!method)
				method = Method("UnityEngine.Camera", methodName, 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, nullptr, nullptr, &exception) : nullptr;
			return exception ? nullptr : reinterpret_cast<Unity::CCamera*>(result);
		}

		Unity::CTransform* GetComponentTransform(void* component)
		{
			if (!component)
				return nullptr;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Component", "get_transform", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, component, nullptr, &exception) : nullptr;
			return exception ? nullptr : reinterpret_cast<Unity::CTransform*>(result);
		}

		Unity::CTransform* GetGameObjectTransform(Unity::CGameObject* gameObject)
		{
			if (!gameObject)
				return nullptr;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.GameObject", "get_transform", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, gameObject, nullptr, &exception) : nullptr;
			return exception ? nullptr : reinterpret_cast<Unity::CTransform*>(result);
		}

		Unity::CGameObject* GetComponentGameObject(void* component)
		{
			if (!component)
				return nullptr;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Component", "get_gameObject", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, component, nullptr, &exception) : nullptr;
			return exception ? nullptr : reinterpret_cast<Unity::CGameObject*>(result);
		}

		Unity::CComponent* GetComponentByName(Unity::CGameObject* gameObject, const char* componentName)
		{
			if (!gameObject || !componentName || !componentName[0])
				return nullptr;

			Mono::Class* klass = UnityRuntime::ResolveMonoClass(nullptr, componentName);
			Mono::Type* type = Mono::GetClassType(klass);
			Mono::Object* typeObject = Mono::GetTypeObject(type);
			if (!typeObject)
				return nullptr;

			static Mono::Method* method = nullptr;
			if (!method)
				method = MethodDesc("UnityEngine.GameObject", "UnityEngine.GameObject:GetComponent(System.Type)", "GetComponent", 1);
			if (!method)
				return nullptr;

			void* args[1] = { typeObject };
			Mono::Object* exception = nullptr;
			Mono::Object* result = Mono::Invoke(method, gameObject, args, &exception);
			return exception ? nullptr : reinterpret_cast<Unity::CComponent*>(result);
		}

		std::vector<void*> GetComponentsInChildren(Unity::CGameObject* gameObject, const char* componentName, int maxResults)
		{
			std::vector<void*> results;
			if (!gameObject || !componentName || !componentName[0])
				return results;

			maxResults = std::clamp(maxResults, 1, 512);
			Mono::Class* klass = UnityRuntime::ResolveMonoClass(nullptr, componentName);
			Mono::Type* type = Mono::GetClassType(klass);
			Mono::Object* typeObject = Mono::GetTypeObject(type);
			if (!typeObject)
				return results;

			static Mono::Method* method = nullptr;
			if (!method)
				method = MethodDesc("UnityEngine.GameObject", "UnityEngine.GameObject:GetComponentsInChildren(System.Type)", "GetComponentsInChildren", 1);
			if (!method)
				return results;

			void* args[1] = { typeObject };
			Mono::Object* exception = nullptr;
			Mono::Object* result = Mono::Invoke(method, gameObject, args, &exception);
			if (exception || !result)
				return results;

			Mono::Array* array = reinterpret_cast<Mono::Array*>(result);
			const int count = static_cast<int>(std::min<uintptr_t>(Mono::GetArrayLength(array), static_cast<uintptr_t>(maxResults)));
			results.reserve(count);
			for (int i = 0; i < count; ++i) {
				if (Mono::Object* entry = Mono::GetArrayObject(array, static_cast<uintptr_t>(i)))
					results.push_back(entry);
			}
			return results;
		}

		template <typename T>
		bool ReadFieldValue(void* object, const char* className, const char* fieldName, T& value)
		{
			if (!object || !className || !fieldName)
				return false;

			Mono::Class* klass = UnityRuntime::ResolveMonoClass(nullptr, className);
			Mono::Field* field = Mono::FindField(klass, fieldName);
			const int offset = Mono::GetFieldOffset(field);
			if (offset < 0)
				return false;

			value = *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object) + static_cast<uintptr_t>(offset));
			return true;
		}

		template <typename T>
		bool ReadPropertyValue(void* object, const char* className, const char* propertyName, T& value)
		{
			if (!object || !className || !propertyName)
				return false;

			char getterName[128] = {};
			sprintf_s(getterName, "get_%s", propertyName);
			Mono::Method* method = Method(className, getterName, 0);
			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, object, nullptr, &exception) : nullptr;
			if (exception || !result || !Mono::Functions.object_unbox)
				return false;

			void* unboxed = Mono::Functions.object_unbox(result);
			if (!unboxed)
				return false;

			value = *reinterpret_cast<T*>(unboxed);
			return true;
		}

		template <typename T>
		bool ReadMemberValue(void* object, const char* className, const char* memberName, T& value)
		{
			return ReadFieldValue<T>(object, className, memberName, value) ||
				ReadPropertyValue<T>(object, className, memberName, value);
		}

		bool GetPosition(Unity::CTransform* transform, Unity::Vector3& position)
		{
			if (!transform)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Transform", "get_position", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, transform, nullptr, &exception) : nullptr;
			if (exception || !result)
				return false;

			void* value = Mono::Functions.object_unbox ? Mono::Functions.object_unbox(result) : nullptr;
			if (!value)
				return false;

			position = *reinterpret_cast<Unity::Vector3*>(value);
			return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
		}

		bool WorldToScreen(Unity::CCamera* camera, const Unity::Vector3& world, Vector2& screen, bool rejectOffscreen)
		{
			if (!camera || System::ScreenSize.x <= 0.0f || System::ScreenSize.y <= 0.0f)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Camera", "WorldToScreenPoint", 1);

			Unity::Vector3 worldCopy = world;
			void* args[1] = { &worldCopy };
			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, camera, args, &exception) : nullptr;
			if (exception || !result)
				return false;

			void* value = Mono::Functions.object_unbox ? Mono::Functions.object_unbox(result) : nullptr;
			if (!value)
				return false;

			Unity::Vector3 buffer = *reinterpret_cast<Unity::Vector3*>(value);
			if (!std::isfinite(buffer.x) || !std::isfinite(buffer.y) || !std::isfinite(buffer.z) || buffer.z < 0.01f)
				return false;

			if (rejectOffscreen && (buffer.x < 0.0f || buffer.x > System::ScreenSize.x || buffer.y < 0.0f || buffer.y > System::ScreenSize.y))
				return false;

			screen = Vector2(buffer.x, System::ScreenSize.y - buffer.y);
			return std::isfinite(screen.x) && std::isfinite(screen.y);
		}

		bool SetCameraFov(Unity::CCamera* camera, float fov)
		{
			if (!camera)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Camera", "set_fieldOfView", 1);

			float fovCopy = HooksFunctions::ClampCameraFov(fov);
			void* args[1] = { &fovCopy };
			Mono::Object* exception = nullptr;
			if (method)
				Mono::Invoke(method, camera, args, &exception);
			return method && !exception;
		}

		bool GetCameraFov(Unity::CCamera* camera, float& fov)
		{
			if (!camera)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Camera", "get_fieldOfView", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, camera, nullptr, &exception) : nullptr;
			if (exception || !result)
				return false;

			void* value = Mono::Functions.object_unbox ? Mono::Functions.object_unbox(result) : nullptr;
			if (!value)
				return false;

			fov = *reinterpret_cast<float*>(value);
			return std::isfinite(fov);
		}

		Mono::Object* FindShader(const char* shaderName)
		{
			if (!shaderName || !shaderName[0])
				return nullptr;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Shader", "Find", 1);
			if (!method)
				return nullptr;

			Mono::String* name = Mono::NewString(shaderName);
			void* args[1] = { name };
			Mono::Object* exception = nullptr;
			Mono::Object* result = Mono::Invoke(method, nullptr, args, &exception);
			return exception ? nullptr : result;
		}

		bool GetRendererEnabled(void* renderer)
		{
			bool enabled = false;
			if (ReadPropertyValue<bool>(renderer, "UnityEngine.Renderer", "enabled", enabled))
				return enabled;
			return false;
		}

		Mono::Object* GetRendererMaterial(void* renderer)
		{
			if (!renderer)
				return nullptr;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Renderer", "get_material", 0);

			Mono::Object* exception = nullptr;
			Mono::Object* result = method ? Mono::Invoke(method, renderer, nullptr, &exception) : nullptr;
			return exception ? nullptr : result;
		}

		bool SetMaterialShader(Mono::Object* material, Mono::Object* shader)
		{
			if (!material || !shader)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Material", "set_shader", 1);
			if (!method)
				return false;

			void* args[1] = { shader };
			Mono::Object* exception = nullptr;
			Mono::Invoke(method, material, args, &exception);
			return !exception;
		}

		bool SetMaterialInt(Mono::Object* material, const char* propertyName, int value)
		{
			if (!material || !propertyName)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = MethodDesc("UnityEngine.Material", "UnityEngine.Material:SetInt(System.String,System.Int32)", "SetInt", 2);
			if (!method)
				return false;

			Mono::String* name = Mono::NewString(propertyName);
			void* args[2] = { name, &value };
			Mono::Object* exception = nullptr;
			Mono::Invoke(method, material, args, &exception);
			return !exception;
		}

		bool SetMaterialColor(Mono::Object* material, const Unity::Color& color)
		{
			if (!material)
				return false;

			static Mono::Method* method = nullptr;
			if (!method)
				method = Method("UnityEngine.Material", "set_color", 1);
			if (!method)
				return false;

			Unity::Color colorCopy = color;
			void* args[1] = { &colorCopy };
			Mono::Object* exception = nullptr;
			Mono::Invoke(method, material, args, &exception);
			return !exception;
		}

		bool ObjectsCache(std::vector<Unity::CGameObject*>* originalList, const char* componentName, int maxResults)
		{
			if (!originalList || !componentName || !componentName[0])
				return false;

			originalList->clear();
			maxResults = std::clamp(maxResults, 1, 4096);

			Mono::Class* klass = UnityRuntime::ResolveMonoClass(nullptr, componentName);
			Mono::Type* type = Mono::GetClassType(klass);
			Mono::Object* typeObject = Mono::GetTypeObject(type);
			if (!typeObject)
				return false;

			static Mono::Method* findObjectsOfType = nullptr;
			if (!findObjectsOfType)
				findObjectsOfType = MethodDesc("UnityEngine.Object", "UnityEngine.Object:FindObjectsOfType(System.Type)", "FindObjectsOfType", 1);
			if (!findObjectsOfType)
				return false;

			void* args[1] = { typeObject };
			Mono::Object* exception = nullptr;
			Mono::Object* result = Mono::Invoke(findObjectsOfType, nullptr, args, &exception);
			if (exception || !result)
				return false;

			Mono::Array* array = reinterpret_cast<Mono::Array*>(result);
			const uintptr_t length = Mono::GetArrayLength(array);
			const int count = static_cast<int>(std::min<uintptr_t>(length, static_cast<uintptr_t>(maxResults)));
			const std::string requestedType = componentName;
			const bool arrayContainsGameObjects = requestedType == "GameObject" || requestedType == "UnityEngine.GameObject";
			for (int i = 0; i < count; ++i) {
				Mono::Object* entry = Mono::GetArrayObject(array, static_cast<uintptr_t>(i));
				Unity::CGameObject* gameObject = arrayContainsGameObjects ? reinterpret_cast<Unity::CGameObject*>(entry) : GetComponentGameObject(entry);
				if (gameObject)
					originalList->push_back(gameObject);
			}

			return true;
		}
	}

	std::string CachedObjectName(Unity::CGameObject* object, ULONGLONG ttlMs = 2000)
	{
		if (!object)
			return "null";

		struct NameCacheEntry
		{
			std::string name;
			ULONGLONG tick;
		};

		static std::unordered_map<uintptr_t, NameCacheEntry> cache;
		const uintptr_t key = reinterpret_cast<uintptr_t>(object);
		const ULONGLONG now = GetTickCount64();
		auto it = cache.find(key);
		if (it != cache.end() && now - it->second.tick <= ttlMs)
			return it->second.name;

		if (cache.size() > 512) {
			for (auto cacheIt = cache.begin(); cacheIt != cache.end();) {
				if (now - cacheIt->second.tick > 5000)
					cacheIt = cache.erase(cacheIt);
				else
					++cacheIt;
			}

			if (cache.size() > 512)
				cache.clear();
		}

		std::string name = ObjectName(object);
		cache[key] = { name, now };
		return name;
	}

	Unity::CCamera* GetFrameCamera()
	{
		if (MonoUnity::Active()) {
			Unity::CCamera* camera = MonoUnity::GetCamera("get_main");
			if (!camera)
				camera = MonoUnity::GetCamera("get_current");
			return camera;
		}

		Unity::CCamera* camera = Unity::Camera::GetMain();
		if (!camera)
			camera = Unity::Camera::GetCurrent();
		return camera;
	}

	Unity::CTransform* GetObjectTransform(Unity::CGameObject* object)
	{
		if (!object)
			return nullptr;

		if (MonoUnity::Active())
			return MonoUnity::GetGameObjectTransform(object);

		return object->GetTransform();
	}

	Unity::CComponent* GetObjectComponent(Unity::CGameObject* object, const char* componentName)
	{
		if (!object || !componentName || !componentName[0])
			return nullptr;

		if (MonoUnity::Active())
			return MonoUnity::GetComponentByName(object, componentName);

		return object->GetComponent(componentName);
	}

	bool GetTransformPosition(Unity::CTransform* transform, Unity::Vector3& position)
	{
		if (!transform)
			return false;

		if (MonoUnity::Active())
			return MonoUnity::GetPosition(transform, position);

		position = transform->GetPosition();
		return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
	}

	bool ExportExternalProfile()
	{
		ClampRuntimeSettings();

		std::ofstream out(ExternalProfilePath(), std::ios::trunc);
		if (!out.is_open()) {
			SetConfigStatus("External profile export failed");
			return false;
		}

		out << "# Aegis Unity Universal internal-to-external read-only profile\n";
		out << "runtime=" << RuntimeName() << "\n";
		out << "object_cache_component=" << CheatVariables::PlayerComponentName << "\n";
		out << "object_cache_fallback=" << CheatVariables::PlayerFallbackComponentName << "\n";
		out << "object_cache_use_fallback=" << (CheatVariables::UsePlayerFallbackComponent ? 1 : 0) << "\n";
		out << "object_position_mode=5\n";
		out << "cached_ptr_offset=16\n";
		out << "unity_object_index=1\n";
		out << "auto_build_fast_targets=1\n";
		out << "fast_targets_cache_fallback=1\n";
		out << "entity_position_anchor=0\n";
		out << "up_axis=0\n";
		out << "entity_head_offset=" << CheatMenuVariables::FakeHeadPosDiff << "\n";
		out << "entity_feet_offset=" << CheatMenuVariables::FakeFeetPosDiff << "\n";
		out << "entity_height=" << (CheatMenuVariables::FakeHeadPosDiff + CheatMenuVariables::FakeFeetPosDiff) << "\n";
		out << "test_component=" << CheatVariables::TestObjects::Name << "\n";
		out << "players_cached=" << CheatVariables::PlayersCacheCount.load() << "\n";
		out << "test_objects_cached=" << CheatVariables::TestObjects::CacheCount.load() << "\n";

		auto writeObjectSample = [&](const char* prefix, int index, Unity::CGameObject* object, const char* componentName) {
			if (!object)
				return;

			Unity::CComponent* component = nullptr;
			Unity::CTransform* transform = nullptr;
			Unity::Vector3 position = {};
			try {
				component = GetObjectComponent(object, componentName);
				transform = GetObjectTransform(object);
				if (!transform || !GetTransformPosition(transform, position))
					return;
			}
			catch (...) {
				return;
			}

			out << prefix << "_" << index << "_gameobject=0x" << std::hex << reinterpret_cast<uintptr_t>(object) << std::dec << "\n";
			if (component)
				out << prefix << "_" << index << "_component=0x" << std::hex << reinterpret_cast<uintptr_t>(component) << std::dec << "\n";
			out << prefix << "_" << index << "_component_name=" << (componentName ? componentName : "") << "\n";
			out << prefix << "_" << index << "_transform=0x" << std::hex << reinterpret_cast<uintptr_t>(transform) << std::dec << "\n";
			out << prefix << "_" << index << "_position=" << position.x << "," << position.y << "," << position.z << "\n";
			out << prefix << "_" << index << "_name=" << CachedObjectName(object) << "\n";
		};

		const int playerCacheSource = CheatVariables::PlayersCacheSource.load();
		const char* playerComponentName = playerCacheSource == 2
			? CheatVariables::PlayerFallbackComponentName
			: CheatVariables::PlayerComponentName;
		std::vector<Unity::CGameObject*> players;
		{
			std::scoped_lock lock(CheatVariables::PlayersListMutex);
			players = CheatVariables::PlayersList;
		}
		const int playerSamples = static_cast<int>(std::min<std::size_t>(players.size(), 16));
		out << "player_sample_count=" << playerSamples << "\n";
		for (int index = 0; index < playerSamples; ++index)
			writeObjectSample("player", index, players[static_cast<std::size_t>(index)], playerComponentName);

		std::vector<Unity::CGameObject*> objects;
		{
			std::scoped_lock lock(CheatVariables::TestObjects::ListMutex);
			objects = CheatVariables::TestObjects::List;
		}
		const int objectSamples = static_cast<int>(std::min<std::size_t>(objects.size(), 16));
		out << "object_sample_count=" << objectSamples << "\n";
		for (int index = 0; index < objectSamples; ++index)
			writeObjectSample("object", index, objects[static_cast<std::size_t>(index)], CheatVariables::TestObjects::Name);

		SetConfigStatus("External profile exported");
		return true;
	}

	bool TryGetMainCameraPosition(Unity::Vector3& position)
	{
		if (!Unity::ComponentFunctions.m_pGetTransform)
		{
			if (!MonoUnity::Active())
				return false;
		}

		Unity::CCamera* camera = GetFrameCamera();
		if (!camera)
			return false;

		Unity::CTransform* transform = MonoUnity::Active() ? MonoUnity::GetComponentTransform(camera) : reinterpret_cast<Unity::CComponent*>(camera)->GetTransform();
		if (!transform)
			return false;

		return GetTransformPosition(transform, position);
	}

	bool WorldToScreen(Unity::CCamera* camera, const Unity::Vector3& world, Vector2& screen, bool rejectOffscreen = true)
	{
		if (MonoUnity::Active())
			return MonoUnity::WorldToScreen(camera, world, screen, rejectOffscreen);

		if (!camera || System::ScreenSize.x <= 0.0f || System::ScreenSize.y <= 0.0f)
			return false;

		Unity::Vector3 buffer = camera->CallMethodSafe<Unity::Vector3>("WorldToScreenPoint", world, Unity::eye::mono);
		if (!std::isfinite(buffer.x) || !std::isfinite(buffer.y) || !std::isfinite(buffer.z) || buffer.z < 0.01f)
			return false;

		if (rejectOffscreen && (buffer.x < 0.0f || buffer.x > System::ScreenSize.x || buffer.y < 0.0f || buffer.y > System::ScreenSize.y))
			return false;

		screen = Vector2(buffer.x, System::ScreenSize.y - buffer.y);
		return std::isfinite(screen.x) && std::isfinite(screen.y);
	}

	ImColor ApplyDistanceFade(ImColor color, float distance)
	{
		color.Value.w *= std::clamp(CheatMenuVariables::ESPGlobalAlpha, 0.05f, 1.0f);
		if (!CheatMenuVariables::PlayersDistanceFade || distance < 0.0f)
			return color;

		const float start = CheatMenuVariables::PlayersFadeStartDistance;
		const float end = CheatMenuVariables::PlayersFadeEndDistance <= start ? start + 1.0f : CheatMenuVariables::PlayersFadeEndDistance;
		const float t = std::clamp((distance - start) / (end - start), 0.0f, 1.0f);
		const float minAlpha = std::clamp(CheatMenuVariables::PlayersFadeMinAlpha, 0.05f, 1.0f);
		color.Value.w *= std::clamp(1.0f - t, minAlpha, 1.0f);
		return color;
	}

	void FormatDistance(float meters, char* destination, size_t destinationSize)
	{
		if (!destination || destinationSize == 0)
			return;

		if (meters < 0.0f) {
			CopyText(destination, destinationSize, "--");
			return;
		}

		if (CheatMenuVariables::DistanceUnit == 1) {
			const float feet = meters * 3.28084f;
			if (feet >= 100.0f)
				sprintf_s(destination, destinationSize, "%.0fft", feet);
			else
				sprintf_s(destination, destinationSize, "%.1fft", feet);
			return;
		}

		if (meters >= 100.0f)
			sprintf_s(destination, destinationSize, "%.0fm", meters);
		else
			sprintf_s(destination, destinationSize, "%.1fm", meters);
	}

	std::string PlayerDisplayName(Unity::CGameObject* object, int playerIndex)
	{
		if (CheatMenuVariables::StreamerMode) {
			char masked[32] = {};
			sprintf_s(masked, "Player %02d", playerIndex < 1 ? 1 : playerIndex);
			return masked;
		}

		return CachedObjectName(object);
	}

	void ClampHudPositions()
	{
		const float screenX = System::ScreenSize.x > 1.0f ? System::ScreenSize.x : 1920.0f;
		const float screenY = System::ScreenSize.y > 1.0f ? System::ScreenSize.y : 1080.0f;
		const float radarSize = std::clamp(CheatMenuVariables::RadarSize, 80.0f, 320.0f);
		const float overlayMaxX = screenX > 200.0f ? screenX - 200.0f : 0.0f;
		const float overlayMaxY = screenY > 100.0f ? screenY - 100.0f : 0.0f;
		const float radarMaxX = screenX > radarSize ? screenX - radarSize : 0.0f;
		const float radarMaxY = screenY > radarSize ? screenY - radarSize : 0.0f;
		const float listMaxX = screenX > 265.0f ? screenX - 265.0f : 0.0f;
		const float listMaxY = screenY > 120.0f ? screenY - 120.0f : 0.0f;
		const float targetMaxX = screenX > 255.0f ? screenX - 255.0f : 0.0f;
		const float targetMaxY = screenY > 80.0f ? screenY - 80.0f : 0.0f;

		CheatMenuVariables::OverlayPosX = std::clamp(CheatMenuVariables::OverlayPosX, 0.0f, overlayMaxX);
		CheatMenuVariables::OverlayPosY = std::clamp(CheatMenuVariables::OverlayPosY, 0.0f, overlayMaxY);
		CheatMenuVariables::RadarPosX = std::clamp(CheatMenuVariables::RadarPosX, 0.0f, radarMaxX);
		CheatMenuVariables::RadarPosY = std::clamp(CheatMenuVariables::RadarPosY, 0.0f, radarMaxY);
		CheatMenuVariables::PlayersListPosX = std::clamp(CheatMenuVariables::PlayersListPosX, 0.0f, listMaxX);
		CheatMenuVariables::PlayersListPosY = std::clamp(CheatMenuVariables::PlayersListPosY, 0.0f, listMaxY);
		CheatMenuVariables::TargetInfoPosX = std::clamp(CheatMenuVariables::TargetInfoPosX, 0.0f, targetMaxX);
		CheatMenuVariables::TargetInfoPosY = std::clamp(CheatMenuVariables::TargetInfoPosY, 0.0f, targetMaxY);
		SetConfigStatus("HUD positions clamped");
	}

	float HudMargin()
	{
		return std::clamp(CheatMenuVariables::HudMargin, 4.0f, 80.0f);
	}

	float HudGap()
	{
		return std::clamp(CheatMenuVariables::HudGap, 2.0f, 40.0f);
	}

	ImVec2 StatusOverlaySize()
	{
		return CheatMenuVariables::StatusOverlayDetailed ? ImVec2(255.0f, 92.0f) : ImVec2(190.0f, 42.0f);
	}

	ImVec2 PlayerListOverlaySize(int rowCount)
	{
		const int rows = std::clamp(rowCount, 0, std::clamp(CheatMenuVariables::PlayersListMaxRows, 3, 20));
		return ImVec2(265.0f, 32.0f + static_cast<float>(rows) * 18.0f);
	}

	ImVec2 TargetInfoOverlaySize()
	{
		return ImVec2(255.0f, 78.0f);
	}

	ImVec2 HudScreenSize()
	{
		return ImVec2(System::ScreenSize.x > 1.0f ? System::ScreenSize.x : 1920.0f, System::ScreenSize.y > 1.0f ? System::ScreenSize.y : 1080.0f);
	}

	ImVec2 HudStatusPos()
	{
		if (!CheatMenuVariables::HudAutoLayout)
			return ImVec2(CheatMenuVariables::OverlayPosX, CheatMenuVariables::OverlayPosY);

		const float margin = HudMargin();
		return ImVec2(margin, margin);
	}

	ImVec2 HudPlayerCountPos()
	{
		if (!CheatMenuVariables::HudAutoLayout)
			return ImVec2(24.0f, 58.0f);

		const float margin = HudMargin();
		const float gap = HudGap();
		float y = margin;
		if (CheatMenuVariables::StatusOverlay)
			y += StatusOverlaySize().y + gap;

		if (CheatMenuVariables::HudLayoutMode == 1)
			return ImVec2(HudScreenSize().x * 0.5f - 90.0f, margin);

		return ImVec2(margin, y);
	}

	ImVec2 HudPlayerListPos(int rowCount)
	{
		if (!CheatMenuVariables::HudAutoLayout)
			return ImVec2(CheatMenuVariables::PlayersListPosX, CheatMenuVariables::PlayersListPosY);

		const ImVec2 screen = HudScreenSize();
		const ImVec2 listSize = PlayerListOverlaySize(rowCount);
		const float margin = HudMargin();
		const float gap = HudGap();

		if (CheatMenuVariables::HudLayoutMode == 1)
			return ImVec2(screen.x - listSize.x - margin, margin);

		float y = margin;
		if (CheatMenuVariables::StatusOverlay)
			y += StatusOverlaySize().y + gap;
		if (CheatMenuVariables::PlayersCountOverlay)
			y += CheatMenuVariables::PlayersLabelSize + gap;
		return ImVec2(margin, y);
	}

	ImVec2 HudTargetInfoPos(int playerListRows)
	{
		if (!CheatMenuVariables::HudAutoLayout)
			return ImVec2(CheatMenuVariables::TargetInfoPosX, CheatMenuVariables::TargetInfoPosY);

		const ImVec2 screen = HudScreenSize();
		const ImVec2 targetSize = TargetInfoOverlaySize();
		const float margin = HudMargin();
		const float gap = HudGap();
		float y = margin;

		if (CheatMenuVariables::HudLayoutMode == 1 && CheatMenuVariables::PlayersListOverlay) {
			y += PlayerListOverlaySize(playerListRows).y + gap;
		}

		return ImVec2(screen.x - targetSize.x - margin, y);
	}

	ImVec2 HudRadarPos()
	{
		if (!CheatMenuVariables::HudAutoLayout)
			return ImVec2(CheatMenuVariables::RadarPosX, CheatMenuVariables::RadarPosY);

		const ImVec2 screen = HudScreenSize();
		const float margin = HudMargin();
		const float size = std::clamp(CheatMenuVariables::RadarSize, 80.0f, 320.0f);
		return ImVec2(margin, screen.y - size - margin);
	}

	float FunNoise(int seed)
	{
		const float value = std::sin(static_cast<float>(seed) * 12.9898f) * 43758.5453f;
		return value - std::floor(value);
	}

	ImColor FunColor(float offset, float alpha = 1.0f)
	{
		float hue = std::fmod(static_cast<float>(ImGui::GetTime()) * 0.08f * std::clamp(CheatMenuVariables::FunSpeed, 0.1f, 5.0f) + offset, 1.0f);
		if (hue < 0.0f)
			hue += 1.0f;

		float r = 1.0f, g = 1.0f, b = 1.0f;
		ImGui::ColorConvertHSVtoRGB(hue, 0.82f, 1.0f, r, g, b);
		return ImColor(r, g, b, alpha);
	}

	void DrawFunConfetti()
	{
		if (!CheatMenuVariables::FunMode || !CheatMenuVariables::FunConfetti)
			return;

		const int count = std::clamp(CheatMenuVariables::FunConfettiCount, 10, 180);
		const float speed = std::clamp(CheatMenuVariables::FunSpeed, 0.1f, 5.0f);
		const float intensity = std::clamp(CheatMenuVariables::FunIntensity, 0.25f, 3.0f);
		const float screenX = System::ScreenSize.x > 1.0f ? System::ScreenSize.x : 1920.0f;
		const float screenY = System::ScreenSize.y > 1.0f ? System::ScreenSize.y : 1080.0f;
		const float time = static_cast<float>(ImGui::GetTime());
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		for (int i = 0; i < count; ++i) {
			const float baseX = FunNoise(i * 11 + 3) * screenX;
			const float fallSpeed = (30.0f + FunNoise(i * 17 + 9) * 120.0f) * speed;
			const float drift = std::sin(time * (0.7f + FunNoise(i * 5) * 2.0f) + FunNoise(i * 19) * 6.28318f) * 28.0f * intensity;
			const float x = std::fmod(baseX + drift + screenX, screenX);
			const float y = std::fmod((FunNoise(i * 13 + 31) * (screenY + 80.0f)) + time * fallSpeed, screenY + 80.0f) - 40.0f;
			const float size = (2.5f + FunNoise(i * 23 + 7) * 5.5f) * intensity;
			ImColor color = FunColor(FunNoise(i * 29 + 1), 0.72f);

			if ((i % 3) == 0) {
				drawList->AddCircleFilled(ImVec2(x, y), size * 0.55f, color, 12);
			}
			else {
				drawList->AddRectFilled(ImVec2(x - size, y - size * 0.35f), ImVec2(x + size, y + size * 0.35f), color, 1.5f);
			}
		}
	}

	void DrawFunRainbowBorder()
	{
		if (!CheatMenuVariables::FunMode || !CheatMenuVariables::FunRainbowBorder)
			return;

		const float screenX = System::ScreenSize.x > 1.0f ? System::ScreenSize.x : 1920.0f;
		const float screenY = System::ScreenSize.y > 1.0f ? System::ScreenSize.y : 1080.0f;
		const int layers = std::clamp(static_cast<int>(CheatMenuVariables::FunBorderThickness), 1, 10);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		for (int i = 0; i < layers; ++i) {
			const float inset = static_cast<float>(i) * 2.0f + 1.0f;
			drawList->AddRect(ImVec2(inset, inset), ImVec2(screenX - inset, screenY - inset), FunColor(static_cast<float>(i) * 0.08f, 0.52f), 0.0f, 0, 1.5f);
		}
	}

	void DrawFunCrosshairOrbit()
	{
		if (!CheatMenuVariables::FunMode || !CheatMenuVariables::FunCrosshairOrbit)
			return;

		const float speed = std::clamp(CheatMenuVariables::FunSpeed, 0.1f, 5.0f);
		const float intensity = std::clamp(CheatMenuVariables::FunIntensity, 0.25f, 3.0f);
		const float time = static_cast<float>(ImGui::GetTime()) * speed;
		const float radius = std::clamp(CheatMenuVariables::CrosshairSize + CheatMenuVariables::CrosshairGap + 15.0f * intensity, 14.0f, 90.0f);
		const ImVec2 center(System::ScreenCenter.x, System::ScreenCenter.y);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		for (int i = 0; i < 6; ++i) {
			const float phase = (static_cast<float>(i) / 6.0f) * 6.28318f + time;
			const ImVec2 dot(center.x + std::cos(phase) * radius, center.y + std::sin(phase) * radius);
			drawList->AddCircleFilled(dot, 2.4f + intensity, FunColor(static_cast<float>(i) / 6.0f, 0.9f), 16);
		}
	}

	void DrawFunBouncyWatermark()
	{
		if (!CheatMenuVariables::FunMode || !CheatMenuVariables::FunBouncyWatermark)
			return;

		const float time = static_cast<float>(ImGui::GetTime()) * std::clamp(CheatMenuVariables::FunSpeed, 0.1f, 5.0f);
		const float x = System::ScreenCenter.x + std::sin(time * 0.9f) * 45.0f * std::clamp(CheatMenuVariables::FunIntensity, 0.25f, 3.0f);
		const float y = 24.0f + std::sin(time * 2.2f) * 7.0f;
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(x, y), 15.0f, FunColor(0.0f, 0.95f), true, "Aegis Party Mode");
	}

	void DrawFunOverlay()
	{
		DrawFunConfetti();
		DrawFunRainbowBorder();
		DrawFunCrosshairOrbit();
		DrawFunBouncyWatermark();
	}

	struct PlayerOverlayRow
	{
		Unity::CGameObject* object = nullptr;
		std::string name;
		float distance = -1.0f;
		bool visible = false;
	};

	void DrawTargetMarker(Vector2 top, Vector2 bottom, ImColor color)
	{
		if (!CheatMenuVariables::PlayersTargetMarker)
			return;

		const float height = std::fabs(bottom.y - top.y);
		const float baseSize = std::clamp(CheatMenuVariables::TargetMarkerSize, 3.0f, 24.0f);
		const float radius = std::clamp(height * 0.08f, baseSize, baseSize * 2.6f);
		const ImVec2 center(top.x, top.y);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		drawList->AddCircle(center, radius + 2.0f, IM_COL32(0, 0, 0, 210), 40, 3.0f);
		drawList->AddCircle(center, radius, color, 40, 2.0f);
		drawList->AddLine(ImVec2(center.x - radius - 6.0f, center.y), ImVec2(center.x - radius * 0.45f, center.y), color, 2.0f);
		drawList->AddLine(ImVec2(center.x + radius * 0.45f, center.y), ImVec2(center.x + radius + 6.0f, center.y), color, 2.0f);
		drawList->AddLine(ImVec2(center.x, center.y - radius - 6.0f), ImVec2(center.x, center.y - radius * 0.45f), color, 2.0f);
		drawList->AddLine(ImVec2(center.x, center.y + radius * 0.45f), ImVec2(center.x, center.y + radius + 6.0f), color, 2.0f);
	}

	void DrawPlayerListOverlay(std::vector<PlayerOverlayRow> rows)
	{
		if (!CheatMenuVariables::PlayersListOverlay)
			return;

		std::sort(rows.begin(), rows.end(), [](const PlayerOverlayRow& left, const PlayerOverlayRow& right) {
			const float leftDistance = left.distance >= 0.0f ? left.distance : 999999.0f;
			const float rightDistance = right.distance >= 0.0f ? right.distance : 999999.0f;
			return leftDistance < rightDistance;
		});

		const int maxRows = std::clamp(CheatMenuVariables::PlayersListMaxRows, 3, 20);
		const int cachedRows = static_cast<int>(rows.size());
		const int rowCount = cachedRows < maxRows ? cachedRows : maxRows;
		const ImVec2 pos = HudPlayerListPos(rowCount);
		const ImVec2 size = PlayerListOverlaySize(rowCount);
		const float width = size.x;
		const float height = size.y;
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(8, 11, 16, 165), 5.0f);
		drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(255, 255, 255, 65), 5.0f, 0, 1.0f);
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 8.0f), 12.5f, CheatVariables::RainbowColor, false, "Players Nearby");

		for (int i = 0; i < rowCount; ++i) {
			const PlayerOverlayRow& row = rows[i];
			const float y = pos.y + 29.0f + static_cast<float>(i) * 18.0f;
			const bool isTarget = CheatVariables::TargetPlayer == row.object;
			ImU32 dotColor = row.visible ? IM_COL32(95, 225, 150, 240) : IM_COL32(255, 190, 80, 230);
			if (isTarget)
				dotColor = CheatVariables::TargetPlayerColor;
			drawList->AddCircleFilled(ImVec2(pos.x + 13.0f, y + 6.0f), 3.5f, dotColor);

			std::string name = row.name.empty() ? "unnamed" : row.name;
			if (name.length() > 22)
				name = name.substr(0, 21) + ".";

			char rowText[128] = {};
			if (row.distance >= 0.0f) {
				char distanceText[32] = {};
				FormatDistance(row.distance, distanceText, sizeof(distanceText));
				sprintf_s(rowText, "%s  %s", name.c_str(), distanceText);
			}
			else {
				sprintf_s(rowText, "%s", name.c_str());
			}
			Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 23.0f, y), 12.0f, ImColor(255, 255, 255, 225), false, "%s", rowText);
		}
	}

	void DrawTargetInfoOverlay(const char* name, float distance, float crosshairDistance, bool locked, int playerListRows = 0)
	{
		if (!CheatMenuVariables::PlayersTargetInfoOverlay)
			return;

		const ImVec2 pos = HudTargetInfoPos(playerListRows);
		const ImVec2 size = TargetInfoOverlaySize();
		const float width = size.x;
		const float height = size.y;
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(8, 11, 16, 165), 5.0f);
		drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(255, 255, 255, 65), 5.0f, 0, 1.0f);

		std::string displayName = name && name[0] ? name : "nearest target";
		if (displayName.length() > 28)
			displayName = displayName.substr(0, 27) + ".";

		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 8.0f), 12.5f, CheatVariables::RainbowColor, false, "Target");
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 27.0f), 12.0f, ImColor(255, 255, 255, 225), false, "%s", displayName.c_str());
		if (distance >= 0.0f) {
			char distanceText[32] = {};
			FormatDistance(distance, distanceText, sizeof(distanceText));
			Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 45.0f), 12.0f, ImColor(255, 255, 255, 220), false, "%s | %.0fpx", distanceText, crosshairDistance);
		}
		else {
			Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 45.0f), 12.0f, ImColor(255, 255, 255, 220), false, "%.0fpx from crosshair", crosshairDistance);
		}
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 61.0f), 11.5f, locked ? CheatVariables::TargetPlayerColor : ImColor(255, 255, 255, 180), false, locked ? "Locked" : "Candidate");
	}

	bool WorldToScreenRaw(const Unity::Vector3& world, Vector2& screen, bool& behindCamera)
	{
		if (System::ScreenSize.x <= 0.0f || System::ScreenSize.y <= 0.0f)
			return false;

		Unity::CCamera* camera = GetFrameCamera();
		if (!camera)
			return false;

		if (MonoUnity::Active()) {
			if (!MonoUnity::WorldToScreen(camera, world, screen, false))
				return false;

			behindCamera = false;
			return true;
		}

		Unity::Vector3 buffer = camera->CallMethodSafe<Unity::Vector3>("WorldToScreenPoint", world, Unity::eye::mono);
		if (!std::isfinite(buffer.x) || !std::isfinite(buffer.y) || !std::isfinite(buffer.z))
			return false;

		behindCamera = buffer.z < 0.01f;
		screen = Vector2(buffer.x, System::ScreenSize.y - buffer.y);

		if (behindCamera) {
			screen.x = System::ScreenSize.x - screen.x;
			screen.y = System::ScreenSize.y - screen.y;
		}

		return true;
	}

	bool SetCameraFov(Unity::CCamera* camera, float fov)
	{
		if (!camera)
			return false;

		if (MonoUnity::Active())
			return MonoUnity::SetCameraFov(camera, fov);

		if (HooksFunctions::UnityEngine_Camera__set_fieldOfView || Unity::CameraFunctions.m_pSetFieldOfView) {
			HooksFunctions::SetCameraFovOriginal(camera, fov);
			return true;
		}

		camera->CallMethodSafe<void*>("set_fieldOfView", fov);
		return true;
	}

	bool ApplyCameraFov()
	{
		if (!CheatMenuVariables::CameraFovChanger) {
			CheatVariables::CameraFovLastApplyTick = 0;
			CheatVariables::CameraFovLastSweepTick = 0;
			return false;
		}

		const ULONGLONG now = GetTickCount64();

		Unity::CCamera* mainCamera = GetFrameCamera();

		float fov = HooksFunctions::ClampCameraFov(CheatMenuVariables::CameraCustomFOV);
		if (CheatMenuVariables::CameraFovAdditive) {
			if (!CheatVariables::CameraFovBaselineCaptured && mainCamera) {
				if (MonoUnity::Active()) {
					float baseline = 0.0f;
					if (MonoUnity::GetCameraFov(mainCamera, baseline)) {
						CheatVariables::CameraFovBaseline = baseline;
						CheatVariables::CameraFovBaselineCaptured = true;
					}
				}
				else if (Unity::CameraFunctions.m_pGetFieldOfView) {
					CheatVariables::CameraFovBaseline = mainCamera->GetFieldOfView();
					CheatVariables::CameraFovBaselineCaptured = true;
				}
			}

			const float baseline = CheatVariables::CameraFovBaselineCaptured ? CheatVariables::CameraFovBaseline : 80.0f;
			fov = HooksFunctions::ClampCameraFov(baseline + CheatMenuVariables::CameraFovOffset);
		}
		else {
			CheatVariables::CameraFovBaselineCaptured = false;
		}
		CheatVariables::CameraFovLastDesired = fov;

		const int applyIntervalMs = CheatVariables::CameraFovHookEnabled ? 250 : 100;
		const bool fovChanged = std::fabs(CheatVariables::CameraFovLastApplied - fov) > 0.05f;
		if (!fovChanged && now - CheatVariables::CameraFovLastApplyTick < static_cast<ULONGLONG>(applyIntervalMs))
			return false;

		CheatVariables::CameraFovLastApplyTick = now;
		CheatVariables::CameraFovLastApplied = fov;

		bool applied = false;

		applied = SetCameraFov(mainCamera, fov) || applied;
		Unity::CCamera* currentCamera = MonoUnity::Active() ? MonoUnity::GetCamera("get_current") : Unity::Camera::GetCurrent();
		if (currentCamera != mainCamera)
			applied = SetCameraFov(currentCamera, fov) || applied;

		const ULONGLONG sweepIntervalMs = CheatMenuVariables::StabilityMode ? 2500 : 1250;
		if (!MonoUnity::Active() && !CheatVariables::CameraFovHookEnabled && now - CheatVariables::CameraFovLastSweepTick >= sweepIntervalMs) {
			CheatVariables::CameraFovLastSweepTick = now;
			Unity::il2cppArray<Unity::CCamera*>* cameras = Unity::Object::FindObjectsOfType<Unity::CCamera>("UnityEngine.Camera");
			if (cameras) {
				const int maxCameras = static_cast<int>(std::min<uintptr_t>(cameras->m_uMaxLength, CheatMenuVariables::StabilityMode ? 4 : 16));
				for (int i = 0; i < maxCameras; ++i) {
					Unity::CCamera* camera = cameras->operator[](i);
					if (camera && camera != mainCamera && camera != currentCamera)
						applied = SetCameraFov(camera, fov) || applied;
				}
			}
		}

		return applied;
	}

	void DrawRadarBase()
	{
		if (!CheatMenuVariables::Radar)
			return;

		const float size = std::clamp(CheatMenuVariables::RadarSize, 80.0f, 320.0f);
		const ImVec2 topLeft = HudRadarPos();
		const ImVec2 bottomRight(topLeft.x + size, topLeft.y + size);
		const ImVec2 center(topLeft.x + size * 0.5f, topLeft.y + size * 0.5f);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		if (CheatMenuVariables::RadarBackground) {
			drawList->AddRectFilled(topLeft, bottomRight, IM_COL32(8, 11, 16, 155), 4.0f);
		}

		drawList->AddRect(topLeft, bottomRight, IM_COL32(255, 255, 255, 90), 4.0f, 0, 1.0f);
		drawList->AddLine(ImVec2(center.x, topLeft.y), ImVec2(center.x, bottomRight.y), IM_COL32(255, 255, 255, 45), 1.0f);
		drawList->AddLine(ImVec2(topLeft.x, center.y), ImVec2(bottomRight.x, center.y), IM_COL32(255, 255, 255, 45), 1.0f);
		if (CheatMenuVariables::RadarRangeRings) {
			drawList->AddCircle(center, size * 0.25f, IM_COL32(255, 255, 255, 36), 64, 1.0f);
			drawList->AddCircle(center, (size * 0.5f) - 8.0f, IM_COL32(255, 255, 255, 36), 64, 1.0f);
		}
		drawList->AddCircleFilled(center, 3.0f, IM_COL32(255, 255, 255, 220));
	}

	void DrawRadarPoint(const Unity::Vector3& origin, const Unity::Vector3& target, ImColor color, const char* label = nullptr)
	{
		if (!CheatMenuVariables::Radar)
			return;

		const float size = std::clamp(CheatMenuVariables::RadarSize, 80.0f, 320.0f);
		const float range = CheatMenuVariables::RadarRange < 5.0f ? 5.0f : CheatMenuVariables::RadarRange;
		const ImVec2 topLeft = HudRadarPos();
		const ImVec2 center(topLeft.x + size * 0.5f, topLeft.y + size * 0.5f);
		float x = ((target.x - origin.x) / range) * (size * 0.5f);
		float y = ((target.z - origin.z) / range) * (size * 0.5f);
		x = std::clamp(x, -(size * 0.5f - 6.0f), size * 0.5f - 6.0f);
		y = std::clamp(y, -(size * 0.5f - 6.0f), size * 0.5f - 6.0f);

		const ImVec2 point(center.x + x, center.y + y);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddCircleFilled(point, 3.5f, color);
		if (CheatMenuVariables::RadarLabels && label && label[0]) {
			drawList->AddText(gameFont, 11.0f, ImVec2(point.x + 6.0f, point.y - 6.0f), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.82f)), label);
		}
	}

	void DrawOffscreenArrow(const Unity::Vector3& position, ImColor color, const char* label = nullptr, float distance = -1.0f)
	{
		if (!CheatMenuVariables::PlayersOffscreenArrows)
			return;

		Vector2 raw = {};
		bool behindCamera = false;
		if (!WorldToScreenRaw(position, raw, behindCamera))
			return;

		const float margin = 18.0f;
		const bool onscreen = !behindCamera &&
			raw.x >= margin && raw.x <= System::ScreenSize.x - margin &&
			raw.y >= margin && raw.y <= System::ScreenSize.y - margin;
		if (onscreen)
			return;

		const ImVec2 center(System::ScreenCenter.x, System::ScreenCenter.y);
		float dx = raw.x - center.x;
		float dy = raw.y - center.y;
		const float length = std::sqrt((dx * dx) + (dy * dy));
		if (length < 0.001f)
			return;

		dx /= length;
		dy /= length;

		const float maxRadiusRaw = (System::ScreenSize.x < System::ScreenSize.y ? System::ScreenSize.x : System::ScreenSize.y) * 0.5f - 32.0f;
		const float maxRadius = maxRadiusRaw < 40.0f ? 40.0f : maxRadiusRaw;
		const float radius = std::clamp(CheatMenuVariables::OffscreenArrowRadius, 40.0f, maxRadius);
		const float size = std::clamp(CheatMenuVariables::OffscreenArrowSize, 6.0f, 30.0f);

		const ImVec2 tip(center.x + dx * radius, center.y + dy * radius);
		const ImVec2 base(tip.x - dx * size, tip.y - dy * size);
		const ImVec2 perpendicular(-dy, dx);
		const ImVec2 left(base.x + perpendicular.x * size * 0.55f, base.y + perpendicular.y * size * 0.55f);
		const ImVec2 right(base.x - perpendicular.x * size * 0.55f, base.y - perpendicular.y * size * 0.55f);

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddTriangleFilled(tip, left, right, color);
		drawList->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 190), 1.0f);
		if (CheatMenuVariables::OffscreenArrowLabels) {
			char text[96] = {};
			if (distance >= 0.0f) {
				FormatDistance(distance, text, sizeof(text));
			}
			else if (label && label[0]) {
				CopyText(text, sizeof(text), label);
			}

			if (text[0]) {
				Utils::DrawOutlinedTextForeground(gameFont, ImVec2(base.x, base.y + size * 0.75f), 11.5f, color, true, "%s", text);
			}
		}
	}

	const char* RuntimeName()
	{
		switch (System::RuntimeBackend) {
		case UnityRuntimeBackend::IL2CPP:
			return "IL2CPP";
		case UnityRuntimeBackend::Mono:
			return "Mono";
		default:
			return "Unknown";
		}
	}

	void DrawStatusOverlay()
	{
		if (!CheatMenuVariables::StatusOverlay)
			return;

		const ImVec2 pos = HudStatusPos();
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const ImVec2 size = StatusOverlaySize();
		const float width = size.x;
		const float height = size.y;

		drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(8, 11, 16, 175), 5.0f);
		drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(255, 255, 255, 70), 5.0f, 0, 1.0f);
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 8.0f), 13.0f, CheatVariables::RainbowColor, false, "%s", AppName);
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 25.0f), 12.0f, ImColor(255, 255, 255, 220), false, "FPS %.0f | %s", ImGui::GetIO().Framerate, RuntimeName());

		if (CheatMenuVariables::StatusOverlayDetailed) {
			Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 44.0f), 12.0f, ImColor(255, 255, 255, 220), false, "Players %d | Source %d", CheatVariables::PlayersCacheCount.load(), CheatVariables::PlayersCacheSource.load());
			Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 61.0f), 12.0f, ImColor(255, 255, 255, 220), false, "FOV %.1f | Hook %s", CheatVariables::CameraFovLastDesired, CheatVariables::CameraFovHookEnabled ? "on" : "off");
			Utils::DrawOutlinedTextForeground(gameFont, ImVec2(pos.x + 10.0f, pos.y + 78.0f), 12.0f, ImColor(255, 255, 255, 220), false, "%s", CheatMenuVariables::LastConfigStatus);
		}
	}
}

namespace Fns {

	bool RenderChams(Unity::CGameObject* obj) {
		if (!obj)
			return false;

		if (AegisUniversal::MonoUnity::Active()) {
			static Mono::Object* monoChamsShader = nullptr;
			if (!monoChamsShader)
				monoChamsShader = AegisUniversal::MonoUnity::FindShader("Hidden/Internal-Colored");
			if (!monoChamsShader)
				return false;

			std::vector<void*> renderers = AegisUniversal::MonoUnity::GetComponentsInChildren(obj, "UnityEngine.Renderer", 32);
			if (renderers.empty())
				return false;

			const Unity::Color color(CheatVariables::Rainbow.x, CheatVariables::Rainbow.y, CheatVariables::Rainbow.z, CheatVariables::Rainbow.w);
			bool applied = false;
			for (void* renderer : renderers) {
				if (!renderer || !AegisUniversal::MonoUnity::GetRendererEnabled(renderer))
					continue;

				Mono::Object* material = AegisUniversal::MonoUnity::GetRendererMaterial(renderer);
				if (!material)
					continue;

				applied = AegisUniversal::MonoUnity::SetMaterialShader(material, monoChamsShader) || applied;
				AegisUniversal::MonoUnity::SetMaterialInt(material, "_Cull", 0);
				AegisUniversal::MonoUnity::SetMaterialInt(material, "_ZTest", 8);
				AegisUniversal::MonoUnity::SetMaterialInt(material, "_ZWrite", 1);
				AegisUniversal::MonoUnity::SetMaterialColor(material, color);
			}

			return applied;
		}

		if (System::RuntimeBackend != UnityRuntimeBackend::IL2CPP || !CheatVariables::ChamsShader)
			return false;

		// MAYBE THERE YOU SHOULD GET ANOTHER COMPONENT INSIDE THE CURRENT PLAYER AND AFTER FOUND THE RENDER LIST FROM THAT SUB GAMEOBJECT
		static Unity::il2cppObject* rendererSystemType = nullptr;
		if (!rendererSystemType) {
			Unity::il2cppClass* CRenderer = IL2CPP::Class::Find("UnityEngine.Renderer");
			if (!CRenderer) return false;

			rendererSystemType = IL2CPP::Class::GetSystemType(CRenderer);
			if (!rendererSystemType) return false;
		}

		auto RenderList = obj->CallMethodSafe<Unity::il2cppArray<Unity::CComponent*>*>("GetComponentsInChildren", rendererSystemType);
		if (!RenderList) return false;

		// THERE YOU SHOULD SKIP SOME RENDERERS, LIKE WEAPONS, OR OTHER STUFF

		const int maxRenderers = static_cast<int>(std::min<uintptr_t>(RenderList->m_uMaxLength, 32));
		for (int t = 0; t < maxRenderers; t++)
		{
			Unity::CComponent* renderer = RenderList->operator[](t);
			if (!renderer)
				continue;

			if (!renderer->GetPropertyValue<bool>("enabled"))
				continue;

			auto Material = renderer->CallMethodSafe<Unity::CComponent*>("GetMaterial");
			if (!Material)
				continue;

			Material->CallMethodSafe<void*>("set_shader", CheatVariables::ChamsShader);
			Material->CallMethodSafe<void*>("SetInt", IL2CPP::String::New("_Cull"), 0);
			Material->CallMethodSafe<void*>("SetInt", IL2CPP::String::New("_ZTest"), 8);
			Material->CallMethodSafe<void*>("SetInt", IL2CPP::String::New("_ZWrite"), 1);
			Material->SetPropertyValue<Unity::Color>("color", Unity::Color(CheatVariables::Rainbow.x, CheatVariables::Rainbow.y, CheatVariables::Rainbow.z, CheatVariables::Rainbow.w));

		}

		return true;
	}

	bool RenderESPSnapline(ImColor color, Vector2 origin) {

		ImVec2 screenPos = ImVec2(origin.x, origin.y);
		ImVec2 dest = ImVec2(System::ScreenSize.x * 0.5f, 0.0f);

		switch (CheatMenuVariables::PlayersSnaplineType)
		{
		case 0:
			dest = ImVec2(System::ScreenSize.x / 2, System::ScreenSize.y);
			break;
		case 1:
			dest = ImVec2(System::ScreenSize.x / 2, System::ScreenSize.y / 2);
			break;
		case 2:
			dest = ImVec2(System::ScreenSize.x / 2, 0);
			break;
		}

		dest.x = std::clamp(dest.x + CheatMenuVariables::PlayersSnaplineOffsetX, 0.0f, System::ScreenSize.x);
		dest.y = std::clamp(dest.y + CheatMenuVariables::PlayersSnaplineOffsetY, 0.0f, System::ScreenSize.y);
		ImGui::GetForegroundDrawList()->AddLine(dest, screenPos, color, std::clamp(CheatMenuVariables::ESPLineThickness, 1.0f, 6.0f));

		return true;
	}

	bool RenderESPBox(ImColor color, Vector2 bottom, Vector2 top) {

		float height = bottom.y - top.y;
		if (height < 0.0f)
			height = -height;
		if (height <= 0.0f)
			return false;

		const float width = height * 0.6f;
		const float left = bottom.x - (width * 0.5f);
		const float right = bottom.x + (width * 0.5f);

		const ImVec2 topLeft(left, top.y);
		const ImVec2 topRight(right, top.y);
		const ImVec2 bottomLeft(left, bottom.y);
		const ImVec2 bottomRight(right, bottom.y);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		if (CheatMenuVariables::PlayersBoxFilled) {
			ImColor fillColor = color;
			fillColor.Value.w = std::clamp(CheatMenuVariables::PlayersBoxFillAlpha * color.Value.w, 0.03f, 0.8f);
			drawList->AddRectFilled(topLeft, bottomRight, fillColor);
		}

		if (CheatMenuVariables::PlayersBoxStyle == 0 || CheatMenuVariables::PlayersBoxFilled) {
			const float originalAlpha = color.Value.w;
			color.Value.w = originalAlpha * 0.3f;
			drawList->AddRect(topLeft, bottomRight, color, 0.0f, 0, std::clamp(CheatMenuVariables::ESPBoxThickness + 1.0f, 1.0f, 8.0f));
			color.Value.w = originalAlpha;
		}

		const float thickness = std::clamp(CheatMenuVariables::ESPBoxThickness, 1.0f, 6.0f);
		if (CheatMenuVariables::PlayersBoxStyle == 1) {
			const float cornerX = width * 0.25f;
			const float cornerY = height * 0.25f;
			drawList->AddLine(topLeft, ImVec2(topLeft.x + cornerX, topLeft.y), color, thickness);
			drawList->AddLine(topLeft, ImVec2(topLeft.x, topLeft.y + cornerY), color, thickness);
			drawList->AddLine(topRight, ImVec2(topRight.x - cornerX, topRight.y), color, thickness);
			drawList->AddLine(topRight, ImVec2(topRight.x, topRight.y + cornerY), color, thickness);
			drawList->AddLine(bottomLeft, ImVec2(bottomLeft.x + cornerX, bottomLeft.y), color, thickness);
			drawList->AddLine(bottomLeft, ImVec2(bottomLeft.x, bottomLeft.y - cornerY), color, thickness);
			drawList->AddLine(bottomRight, ImVec2(bottomRight.x - cornerX, bottomRight.y), color, thickness);
			drawList->AddLine(bottomRight, ImVec2(bottomRight.x, bottomRight.y - cornerY), color, thickness);
		}
		else {
			drawList->AddLine(topLeft, topRight, color, thickness);
			drawList->AddLine(topRight, bottomRight, color, thickness);
			drawList->AddLine(bottomRight, bottomLeft, color, thickness);
			drawList->AddLine(bottomLeft, topLeft, color, thickness);
		}
		
		/*
		float boxWidth = 30;
		float boxHeight = 45;

		ImVec2 boxTopLeft(origin.x - boxWidth / 2, origin.y - boxHeight / 2);
		ImVec2 boxBottomRight(origin.x + boxWidth / 2, origin.y + boxHeight / 2);

		if (CheatMenuVariables::PlayersBoxFilled) {
			color.Value.w = 0.3f;
			ImGui::GetBackgroundDrawList()->AddRectFilled(boxTopLeft, boxBottomRight, color);
			color.Value.w = 1;
			ImGui::GetBackgroundDrawList()->AddRect(boxTopLeft, boxBottomRight, color);
		}
		else {
			ImGui::GetBackgroundDrawList()->AddRect(boxTopLeft, boxBottomRight, color);
		}*/

		return true;
	}

	bool RenderESPSkeleton(ImColor color, Unity::CGameObject* entity, Unity::CCamera* camera) { // THE GAME MUST USE ActorJoint COMPONENT
		if (!entity || !camera)
			return false;

		if (AegisUniversal::MonoUnity::Active()) {
			std::vector<void*> joints = AegisUniversal::MonoUnity::GetComponentsInChildren(entity, "ActorJoint", 128);
			if (joints.empty())
				return false;

			for (std::pair<int, int> bonePair : CheatVariables::BonePairs)
			{
				if (bonePair.first < 0 || bonePair.second < 0 ||
					bonePair.first >= static_cast<int>(joints.size()) || bonePair.second >= static_cast<int>(joints.size())) {
					continue;
				}

				Unity::CTransform* j1T = AegisUniversal::MonoUnity::GetComponentTransform(joints[bonePair.first]);
				Unity::CTransform* j2T = AegisUniversal::MonoUnity::GetComponentTransform(joints[bonePair.second]);
				if (!j1T || !j2T)
					continue;

				Unity::Vector3 startpos = {};
				Unity::Vector3 endpos = {};
				if (!AegisUniversal::MonoUnity::GetPosition(j1T, startpos) || !AegisUniversal::MonoUnity::GetPosition(j2T, endpos))
					continue;

				Vector2 start, end;
				if (AegisUniversal::WorldToScreen(camera, startpos, start) && AegisUniversal::WorldToScreen(camera, endpos, end))
					ImGui::GetBackgroundDrawList()->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), color, 1.5f);
			}

			return true;
		}

		if (System::RuntimeBackend != UnityRuntimeBackend::IL2CPP)
			return false;
		
		static Unity::il2cppObject* actorJointSystemType = nullptr;
		if (!actorJointSystemType) {
			Unity::il2cppClass* CActorJoint = IL2CPP::Class::Find("ActorJoint");
			if(!CActorJoint) return false;

			actorJointSystemType = IL2CPP::Class::GetSystemType(CActorJoint);
			if (!actorJointSystemType) return false;
		}

		Unity::il2cppArray<Unity::CComponent*>* Joints = entity->CallMethodSafe<Unity::il2cppArray<Unity::CComponent*>*>("GetComponentsInChildren", actorJointSystemType);
		if (!Joints) return	false;

		for (std::pair<int, int> bonePair : CheatVariables::BonePairs)
		{
			if (bonePair.first < 0 || bonePair.second < 0 ||
				bonePair.first >= Joints->m_uMaxLength || bonePair.second >= Joints->m_uMaxLength) {
				continue;
			}

			auto j1 = Joints->operator[](bonePair.first);
			auto j2 = Joints->operator[](bonePair.second);

			if (!j1 || !j2)
				continue;

			auto j1T = j1->GetTransform();
			auto j2T = j2->GetTransform();

			if (!j1T || !j2T)
				continue;

			auto startpos = j1T->GetPosition();
			auto endpos = j2T->GetPosition();

			Vector2 start, end;
			if (AegisUniversal::WorldToScreen(camera, startpos, start) && AegisUniversal::WorldToScreen(camera, endpos, end))
			{
				ImGui::GetBackgroundDrawList()->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), color, 1.5f);
			}
		}

		return true;
	}

	bool RenderHealthBar(Unity::CGameObject* obj, Vector2 origin) {
		if (!obj)
			return false;

		int health = 0;
		int maxHealth = 0;
		if (AegisUniversal::MonoUnity::Active()) {
			Unity::CComponent* CHealth = AegisUniversal::MonoUnity::GetComponentByName(obj, "Health");
			if (!CHealth) return false;

			if (!AegisUniversal::MonoUnity::ReadMemberValue<int>(CHealth, "Health", "health", health))
				return false;
			if (!AegisUniversal::MonoUnity::ReadMemberValue<int>(CHealth, "Health", "maxHealth", maxHealth))
				return false;
		}
		else {
			if (System::RuntimeBackend != UnityRuntimeBackend::IL2CPP)
				return false;

			Unity::CComponent* CHealth = obj->GetComponent("Health");
			if (!CHealth) return false;

			health = CHealth->GetMemberValue<int>("health");
			maxHealth = CHealth->GetMemberValue<int>("maxHealth");
		}
		if (maxHealth <= 0) return false;

		ImColor color = CheatMenuVariables::RainbowPlayersSnapline ? CheatVariables::RainbowColor : CheatMenuVariables::PlayersSnaplineColor;

		color = CheatVariables::TargetPlayer == obj ? CheatVariables::TargetPlayerColor : color;

		ImVec2 screenPos = ImVec2(origin.x, origin.y);

		ImVec2 dest = ImVec2(screenPos.x, screenPos.y - 10);

		ImGui::GetForegroundDrawList()->AddText(dest, color, std::to_string(health).c_str());

		float hpWidth = 50.0f;
		float hpHeight = 10.0f;
		const float hpRatio = std::clamp(static_cast<float>(health) / static_cast<float>(maxHealth), 0.0f, 1.0f);

		ImVec2 hpTopLeft(origin.x - hpWidth / 2, origin.y - hpHeight / 2);
		ImVec2 hpBottomRight(origin.x + hpWidth / 2, origin.y + hpHeight / 2);

		ImGui::GetBackgroundDrawList()->AddRectFilled(hpTopLeft, hpBottomRight, ImColor(0, 0, 0, 255));
		ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(hpTopLeft.x, hpTopLeft.y), ImVec2(hpTopLeft.x + hpWidth * hpRatio, hpBottomRight.y), ImColor(0, 255, 0, 255));
		ImGui::GetBackgroundDrawList()->AddRect(hpTopLeft, hpBottomRight, ImColor(0, 0, 0, 255));

		return true;
	}

	bool ExecAimbot(Unity::CGameObject* target, Vector2 playerHead) {

		if (CheatMenuVariables::AimbotFOVCheck)
		{
			if (playerHead.x > (System::ScreenCenter.x + CheatMenuVariables::AimbotFOV))
				return false;
			if (playerHead.x < (System::ScreenCenter.x - CheatMenuVariables::AimbotFOV))
				return false;
			if (playerHead.y > (System::ScreenCenter.y + CheatMenuVariables::AimbotFOV))
				return false;
			if (playerHead.y < (System::ScreenCenter.y - CheatMenuVariables::AimbotFOV))
				return false;
		}

		if (KEYS::AIMBOT_ACTIVATION_KEY > 0 && GetAsyncKeyState(KEYS::AIMBOT_ACTIVATION_KEY)) {
			if (CheatVariables::TargetPlayer == nullptr) {
				CheatVariables::TargetPlayer = target;
			}

			if (CheatVariables::TargetPlayer == target) {
				if (CheatMenuVariables::AimbotDeadzone) {
					const float dx = playerHead.x - System::ScreenCenter.x;
					const float dy = playerHead.y - System::ScreenCenter.y;
					const float deadzone = std::clamp(CheatMenuVariables::AimbotDeadzoneRadius, 0.0f, 80.0f);
					if ((dx * dx) + (dy * dy) <= deadzone * deadzone)
						return true;
				}
				Utils::MouseMove(playerHead.x, playerHead.y, System::ScreenSize.x, System::ScreenSize.y, CheatMenuVariables::AimbotSmoothness);
			}
		}
		else {
			CheatVariables::TargetPlayer = nullptr;
		}

		return true;
	}

	bool Spinbot() {
		if (CheatMenuVariables::ShowMenu) return false;

		CheatVariables::SpinbotSupport = (CheatVariables::SpinbotSupport + 150) % 1000;

		Utils::MouseMove(static_cast<float>(CheatVariables::SpinbotSupport), System::ScreenSize.y, System::ScreenSize.x, System::ScreenSize.y, 0.0f);

		return true;
	}
}

void DrawMouse() {
	if(!CheatMenuVariables::ShowMouse) return;

	ImColor color = CheatMenuVariables::RainbowMouse ? CheatVariables::RainbowColor : CheatMenuVariables::MouseColor;
	if (CheatMenuVariables::MouseType != 2) {
		ImGui::GetIO().MouseDrawCursor = false;
	}

	switch (CheatMenuVariables::MouseType) {
	case 0:
		ImGui::GetForegroundDrawList()->AddCircleFilled(ImGui::GetMousePos(), 4, color);
		break;
	case 1:
		Utils::DrawOutlinedTextForeground(gameFont, ImVec2(static_cast<float>(System::MousePos.x), static_cast<float>(System::MousePos.y)), 13.0f, color, false, "X");
		break;
	case 2:
		if (!ImGui::GetIO().MouseDrawCursor) {
			ImGui::GetIO().MouseDrawCursor = true;
		}
		break;
	default:
		ImGui::GetIO().MouseDrawCursor = false;
		break;
	}
}

void DrawCrosshair() {
	if (CheatMenuVariables::Crosshair) {
		ImColor color = CheatMenuVariables::RainbowCrosshair ? CheatVariables::RainbowColor : CheatMenuVariables::CrosshairColor;
		const float size = std::clamp(CheatMenuVariables::CrosshairSize, 0.1f, 100.0f);
		const float gap = std::clamp(CheatMenuVariables::CrosshairGap, 0.0f, 50.0f);
		const float thickness = std::clamp(CheatMenuVariables::CrosshairThickness, 1.0f, 8.0f);
		const ImVec2 center(System::ScreenCenter.x, System::ScreenCenter.y);
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const ImColor outlineColor = ImColor(0, 0, 0, 210);
		switch (CheatMenuVariables::CrosshairType)
		{
		case 0:
			if (CheatMenuVariables::CrosshairOutline) {
				drawList->AddLine(ImVec2(center.x - gap - size, center.y), ImVec2(center.x - gap, center.y), outlineColor, thickness + 2.0f);
				drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + size, center.y), outlineColor, thickness + 2.0f);
				drawList->AddLine(ImVec2(center.x, center.y - gap - size), ImVec2(center.x, center.y - gap), outlineColor, thickness + 2.0f);
				drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + size), outlineColor, thickness + 2.0f);
			}
			drawList->AddLine(ImVec2(center.x - gap - size, center.y), ImVec2(center.x - gap, center.y), color, thickness);
			drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + size, center.y), color, thickness);
			drawList->AddLine(ImVec2(center.x, center.y - gap - size), ImVec2(center.x, center.y - gap), color, thickness);
			drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + size), color, thickness);
			break;
		case 1:
			if (CheatMenuVariables::CrosshairOutline) {
				drawList->AddCircle(center, size, outlineColor, 100, thickness + 2.0f);
			}
			drawList->AddCircle(center, size, color, 100, thickness);
			break;
		}

		if (CheatMenuVariables::CrosshairDot) {
			const float dotSize = std::clamp(CheatMenuVariables::CrosshairDotSize, 1.0f, 12.0f);
			if (CheatMenuVariables::CrosshairOutline) {
				drawList->AddCircleFilled(center, dotSize + 1.5f, outlineColor, 24);
			}
			drawList->AddCircleFilled(center, dotSize, color, 24);
		}
	}
}

void DrawMenu()
{
	Themes::ImGuiThemeAegis(true);

	if (CheatMenuVariables::ShowInspector && System::RuntimeBackend == UnityRuntimeBackend::IL2CPP) {
		Utils::DrawInspector();
	}

	if (Lua::ShowEditor) {
		Utils::DrawLuaEditor();
	}

	if (ImGui::Begin(Prefix.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::SetWindowPos(ImVec2(500, 500), ImGuiCond_Once);
		ImGui::SetWindowSize(ImVec2(720, 460), ImGuiCond_Once);
		static MenuTab tabIndex = TAB_VISUALS;
		ImGui::SameLine();
		if (ImGui::Button("Visual"))
		{
			tabIndex = TAB_VISUALS;
		}
		ImGui::SameLine();
		if (ImGui::Button("Aim"))
		{
			tabIndex = TAB_AIM;
		}
		ImGui::SameLine();
		if (ImGui::Button("Exploits"))
		{
			tabIndex = TAB_EXPLOITS;
		}
		ImGui::SameLine();
		if (ImGui::Button("Misc"))
		{
			tabIndex = TAB_MISC;
		}
		ImGui::SameLine();
		if (ImGui::Button("Universal"))
		{
			tabIndex = TAB_UNIVERSAL;
		}
		if (DEBUG) {
			ImGui::SameLine();
			if (ImGui::Button("Developer"))
			{
				tabIndex = TAB_DEV;
			}
		}
		ImGui::Separator();
		ImGui::Spacing();
		switch (tabIndex) {
			case TAB_VISUALS: {

				{ // ESP
					ImGui::Checkbox("Players Snapline", &CheatMenuVariables::PlayersSnapline);
					ImGui::SameLine();
					ImGui::ColorEdit3("##PlayersSnaplineColor", (float*)&CheatMenuVariables::PlayersSnaplineColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if(ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the players snapline");
					ImGui::SameLine();
					ImGui::Checkbox("##RGB3", &CheatMenuVariables::RainbowPlayersSnapline);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle rainbow color on the players snapline");
					ImGui::Text("Snapline Type");
					ImGui::SameLine();
					ImGui::SliderInt("##PlayersSnaplineType", &CheatMenuVariables::PlayersSnaplineType, 0, 2);
					ImGui::Text("Snapline Offset");
					ImGui::SameLine();
					ImGui::SliderFloat("##PlayersSnaplineOffsetX", &CheatMenuVariables::PlayersSnaplineOffsetX, -500.0f, 500.0f, "X %.0f");
					ImGui::SameLine();
					ImGui::SliderFloat("##PlayersSnaplineOffsetY", &CheatMenuVariables::PlayersSnaplineOffsetY, -500.0f, 500.0f, "Y %.0f");
				}

				{ // bot checker
					ImGui::Checkbox("Bot Checker", &CheatMenuVariables::BotChecker);
					ImGui::SameLine();
					ImGui::ColorEdit3("##BotCheckerColor", (float*)&CheatMenuVariables::BotCheckerColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the bot checker");
					ImGui::SameLine();
					ImGui::Checkbox("##RGB4", &CheatMenuVariables::RainbowBotChecker);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle rainbow color on the bot checker");
					ImGui::SameLine();
					ImGui::Checkbox("Show Bot Text", &CheatMenuVariables::BotCheckerText);
					if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show text 'Bot' on the bot checker");
				}

				{ // Players Box
					ImGui::Checkbox("Players Box", &CheatMenuVariables::PlayersBox);
					ImGui::SameLine();
					ImGui::ColorEdit3("##PlayersBoxColor", (float*)&CheatMenuVariables::PlayersBoxColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the players box");
					ImGui::SameLine();
					ImGui::Checkbox("##RGB5", &CheatMenuVariables::RainbowPlayersBox);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle rainbow color on the players box");
					ImGui::SameLine();
					ImGui::Checkbox("##Filled", &CheatMenuVariables::PlayersBoxFilled);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fill the players box");
					ImGui::Text("Box Style");
					ImGui::SameLine();
					const char* boxStyles[] = { "Full", "Corner" };
					ImGui::Combo("##PlayersBoxStyle", &CheatMenuVariables::PlayersBoxStyle, boxStyles, IM_ARRAYSIZE(boxStyles));
					if (CheatMenuVariables::PlayersBoxFilled) {
						ImGui::Text("Fill Alpha");
						ImGui::SameLine();
						ImGui::SliderFloat("##PlayersBoxFillAlpha", &CheatMenuVariables::PlayersBoxFillAlpha, 0.05f, 0.8f, "%.2f");
					}
				}

				{ // Player Skeleton
					ImGui::Checkbox("Players Skeleton", &CheatMenuVariables::PlayerSkeleton);
					ImGui::SameLine();
					ImGui::ColorEdit3("##PlayerSkeletonColor", (float*)&CheatMenuVariables::PlayerSkeletonColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the players skeleton");
					ImGui::SameLine();
					ImGui::Checkbox("##RGB6", &CheatMenuVariables::RainbowPlayerSkeleton);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle rainbow color on the players skeleton");
				}

				{ // Player labels
					ImGui::Checkbox("Players Name", &CheatMenuVariables::PlayersName);
					ImGui::SameLine();
					ImGui::Checkbox("Players Distance", &CheatMenuVariables::PlayersDistance);
					ImGui::SameLine();
					ImGui::ColorEdit3("##PlayersTextColor", (float*)&CheatMenuVariables::PlayersTextColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the player labels");
				}

				{ // Filtering and polish
					ImGui::Checkbox("Player Count Overlay", &CheatMenuVariables::PlayersCountOverlay);
					ImGui::Checkbox("Player List Overlay", &CheatMenuVariables::PlayersListOverlay);
					ImGui::SameLine();
					ImGui::SliderInt("##PlayersListMaxRows", &CheatMenuVariables::PlayersListMaxRows, 3, 20, "%d rows");
					if (CheatMenuVariables::PlayersListOverlay) {
						ImGui::Text("List Position");
						ImGui::SameLine();
						ImGui::SliderFloat("##PlayersListX", &CheatMenuVariables::PlayersListPosX, 0.0f, 900.0f, "X %.0f");
						ImGui::SameLine();
						ImGui::SliderFloat("##PlayersListY", &CheatMenuVariables::PlayersListPosY, 0.0f, 900.0f, "Y %.0f");
					}
					ImGui::Checkbox("Target Info Overlay", &CheatMenuVariables::PlayersTargetInfoOverlay);
					if (CheatMenuVariables::PlayersTargetInfoOverlay) {
						ImGui::Text("Target Info Position");
						ImGui::SameLine();
						ImGui::SliderFloat("##TargetInfoX", &CheatMenuVariables::TargetInfoPosX, 0.0f, 900.0f, "X %.0f");
						ImGui::SameLine();
						ImGui::SliderFloat("##TargetInfoY", &CheatMenuVariables::TargetInfoPosY, 0.0f, 900.0f, "Y %.0f");
					}
					ImGui::Checkbox("ESP Draw Limit", &CheatMenuVariables::PlayersDrawLimit);
					ImGui::SameLine();
					ImGui::SliderInt("##PlayersDrawLimit", &CheatMenuVariables::PlayersMaxDrawCount, 5, 250, "%d players");
					ImGui::Checkbox("Distance Fade", &CheatMenuVariables::PlayersDistanceFade);
					if (CheatMenuVariables::PlayersDistanceFade) {
						ImGui::Text("Fade Range");
						ImGui::SameLine();
						ImGui::SliderFloat("##FadeStartDistance", &CheatMenuVariables::PlayersFadeStartDistance, 5.0f, 1000.0f, "Start %.0fm");
						ImGui::SameLine();
						ImGui::SliderFloat("##FadeEndDistance", &CheatMenuVariables::PlayersFadeEndDistance, 10.0f, 1500.0f, "End %.0fm");
						ImGui::Text("Fade Minimum");
						ImGui::SameLine();
						ImGui::SliderFloat("##FadeMinAlpha", &CheatMenuVariables::PlayersFadeMinAlpha, 0.05f, 1.0f, "%.2f");
					}
					ImGui::Checkbox("Max ESP Distance", &CheatMenuVariables::PlayersMaxDistance);
					ImGui::SameLine();
					ImGui::SliderFloat("##MaxESPDistance", &CheatMenuVariables::PlayersMaxDistanceValue, 10.0f, 1000.0f, "%.0fm");
					ImGui::Text("Line Thickness");
					ImGui::SameLine();
					ImGui::SliderFloat("##ESPLineThickness", &CheatMenuVariables::ESPLineThickness, 1.0f, 6.0f, "%.1f");
					ImGui::Text("Box Thickness");
					ImGui::SameLine();
					ImGui::SliderFloat("##ESPBoxThickness", &CheatMenuVariables::ESPBoxThickness, 1.0f, 6.0f, "%.1f");
					ImGui::Text("ESP Alpha");
					ImGui::SameLine();
					ImGui::SliderFloat("##ESPGlobalAlpha", &CheatMenuVariables::ESPGlobalAlpha, 0.05f, 1.0f, "%.2f");
					ImGui::Text("Label Size");
					ImGui::SameLine();
					ImGui::SliderFloat("##PlayersLabelSize", &CheatMenuVariables::PlayersLabelSize, 9.0f, 24.0f, "%.0f");
					const char* distanceUnits[] = { "Meters", "Feet" };
					ImGui::Text("Distance Unit");
					ImGui::SameLine();
					ImGui::Combo("##DistanceUnit", &CheatMenuVariables::DistanceUnit, distanceUnits, IM_ARRAYSIZE(distanceUnits));
				}

				{ // Offscreen indicators
					ImGui::Checkbox("Offscreen Arrows", &CheatMenuVariables::PlayersOffscreenArrows);
					ImGui::SameLine();
					ImGui::Checkbox("Arrow Labels", &CheatMenuVariables::OffscreenArrowLabels);
					ImGui::SameLine();
					ImGui::ColorEdit3("##OffscreenArrowColor", (float*)&CheatMenuVariables::OffscreenArrowColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					ImGui::SameLine();
					ImGui::Checkbox("Rainbow Arrows", &CheatMenuVariables::RainbowOffscreenArrows);
					ImGui::Checkbox("Target Marker", &CheatMenuVariables::PlayersTargetMarker);
					ImGui::SameLine();
					ImGui::ColorEdit3("##TargetMarkerColor", (float*)&CheatMenuVariables::TargetMarkerColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					ImGui::SameLine();
					ImGui::Checkbox("Rainbow Marker", &CheatMenuVariables::RainbowTargetMarker);
					ImGui::Text("Marker Size");
					ImGui::SameLine();
					ImGui::SliderFloat("##TargetMarkerSize", &CheatMenuVariables::TargetMarkerSize, 3.0f, 24.0f, "%.0f");
					ImGui::Text("Arrow Size");
					ImGui::SameLine();
					ImGui::SliderFloat("##OffscreenArrowSize", &CheatMenuVariables::OffscreenArrowSize, 6.0f, 30.0f, "%.0f");
					ImGui::Text("Arrow Radius");
					ImGui::SameLine();
					ImGui::SliderFloat("##OffscreenArrowRadius", &CheatMenuVariables::OffscreenArrowRadius, 40.0f, 600.0f, "%.0f");
				}

				{ // Radar
					ImGui::Checkbox("2D Radar", &CheatMenuVariables::Radar);
					ImGui::SameLine();
					ImGui::Checkbox("Radar Background", &CheatMenuVariables::RadarBackground);
					ImGui::SameLine();
					ImGui::Checkbox("Radar Rings", &CheatMenuVariables::RadarRangeRings);
					ImGui::SameLine();
					ImGui::Checkbox("Radar Labels", &CheatMenuVariables::RadarLabels);
					ImGui::SameLine();
					ImGui::ColorEdit3("##RadarColor", (float*)&CheatMenuVariables::RadarColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					ImGui::Text("Radar Range");
					ImGui::SameLine();
					ImGui::SliderFloat("##RadarRange", &CheatMenuVariables::RadarRange, 25.0f, 500.0f, "%.0fm");
					ImGui::Text("Radar Size");
					ImGui::SameLine();
					ImGui::SliderFloat("##RadarSize", &CheatMenuVariables::RadarSize, 80.0f, 320.0f, "%.0f");
					ImGui::Text("Radar Position");
					ImGui::SameLine();
					ImGui::SliderFloat("##RadarX", &CheatMenuVariables::RadarPosX, 0.0f, 600.0f, "X %.0f");
					ImGui::SameLine();
					ImGui::SliderFloat("##RadarY", &CheatMenuVariables::RadarPosY, 0.0f, 600.0f, "Y %.0f");
				}

				{ // Charms
					ImGui::Checkbox("Players Chams", &CheatMenuVariables::PlayerChams);
				}

				/* { // Players Health
					ImGui::Checkbox("Players Health", &CheatMenuVariables::PlayersHealth);
				}*/

				/*if(ImGui::Button("UnlockAll")) {
					CheatMenuVariables::UnlockAll = true;
				}*/


				break;
			}
			case TAB_AIM: {

				{ // Aimbot
					ImGui::Text("Aimbot Height");
					ImGui::Text("Head Diff Pos");
					ImGui::SameLine();
					ImGui::SliderFloat("##Head Pos", &CheatMenuVariables::FakeHeadPosDiff, -10.0f, 30.0f);
					ImGui::Text("Feet Diff Pos");
					ImGui::SameLine();
					ImGui::SliderFloat("##Feet Pos", &CheatMenuVariables::FakeFeetPosDiff, -10.0f, 30.0f);

					ImGui::Separator();
					ImGui::Spacing();

					ImGui::Checkbox("Enable Aimbot", &CheatMenuVariables::EnableAimbot);
					ImGui::Checkbox("Aimbot FOV Check", &CheatMenuVariables::AimbotFOVCheck);
					ImGui::Text("Aimbot Target");
					ImGui::SameLine();
					const char* targetModes[] = { "First visible", "Closest crosshair" };
					ImGui::Combo("##AimbotTargetMode", &CheatMenuVariables::AimbotTargetMode, targetModes, IM_ARRAYSIZE(targetModes));
					AegisUniversal::HotkeyControl("Aimbot hold key", KEYS::AIMBOT_ACTIVATION_KEY, 8);
					ImGui::Text("Aimbot FOV");
					ImGui::SameLine();
					ImGui::SliderFloat("##Aimbot FOV", &CheatMenuVariables::AimbotFOV, 0.1f, 800.0f);
					ImGui::Text("FOV Ring Thickness");
					ImGui::SameLine();
					ImGui::SliderFloat("##AimbotFOVThickness", &CheatMenuVariables::AimbotFOVThickness, 1.0f, 6.0f, "%.1f");
					ImGui::Checkbox("Filled FOV Ring", &CheatMenuVariables::AimbotFOVFilled);
					ImGui::SameLine();
					ImGui::SliderFloat("##AimbotFOVFillAlpha", &CheatMenuVariables::AimbotFOVFillAlpha, 0.01f, 0.25f, "%.2f");
					ImGui::ColorEdit3("FOV Ring Color", (float*)&CheatMenuVariables::AimbotFOVColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					ImGui::SameLine();
					ImGui::Checkbox("Rainbow FOV", &CheatMenuVariables::RainbowAimbotFOV);
					ImGui::Text("Aimbot Smoothness");
					ImGui::SameLine();
					ImGui::SliderFloat("##Aimbot Smooth", &CheatMenuVariables::AimbotSmoothness, 0.0f, 30.0f);
					ImGui::Checkbox("Aimbot Deadzone", &CheatMenuVariables::AimbotDeadzone);
					ImGui::SameLine();
					ImGui::SliderFloat("##AimbotDeadzoneRadius", &CheatMenuVariables::AimbotDeadzoneRadius, 0.0f, 80.0f, "%.0f px");

				}
				break;
			}
			case TAB_EXPLOITS: {

				ImGui::Text("Game timescale");
				ImGui::SliderFloat("##timescale", &CheatMenuVariables::GameSpeed, 0.0f, 100.0f);
				ImGui::SameLine(); 
				if(ImGui::Button("Set Timescale")) {
					GameFunctions::UnityEngine_Time__set_timeScale(CheatMenuVariables::GameSpeed);
				}	
				ImGui::SameLine(); Utils::HelpMarker("Change the game speed");
				if(ImGui::Button("Reset Timescale")) {
					CheatMenuVariables::GameSpeed = 1.0f;
					GameFunctions::UnityEngine_Time__set_timeScale(CheatMenuVariables::GameSpeed);
				}

				ImGui::Checkbox("Spinbot", &CheatMenuVariables::Spinbot);

				//{ // GodMode
				//	ImGui::Checkbox("GodMode (for you and bots)", &CheatMenuVariables::GodMode);
				//}

				//{ // No Recoil
				//	ImGui::Checkbox("No Recoil", &CheatMenuVariables::NoRecoil);
				//}

				//{ // No Spread
				//	ImGui::Checkbox("No Spread", &CheatMenuVariables::NoSpread);
				//}

				//{ // One Hit Kill
				//	ImGui::Checkbox("One Hit Kill", &CheatMenuVariables::OneShot);
				//}

				//{ // Rapid Fire
				//	ImGui::Checkbox("Rapid Fire", &CheatMenuVariables::RapidFire);
				//}

				//{ // Infinite Ammo
				//	ImGui::Checkbox("Infinite Ammo", &CheatMenuVariables::InfiniteAmmo);
				//}

				//{ // Speed Hack
				//	ImGui::Checkbox("Speed Hack", &CheatMenuVariables::SpeedHack);
				//	ImGui::SliderFloat("Speed", &CheatMenuVariables::SpeedValue, 0.1f, 1000.0f);
				//}
				break;
			}
			case TAB_MISC: {

				{ // Render Things
					ImGui::Checkbox("Show Watermark", &CheatMenuVariables::Watermark);
					ImGui::Checkbox("Streamer Mode", &CheatMenuVariables::StreamerMode);
					ImGui::Checkbox("Fun Mode", &CheatMenuVariables::FunMode);
					if (CheatMenuVariables::FunMode) {
						ImGui::Indent();
						ImGui::Checkbox("Confetti", &CheatMenuVariables::FunConfetti);
						ImGui::SameLine();
						ImGui::Checkbox("Rainbow Border", &CheatMenuVariables::FunRainbowBorder);
						ImGui::SameLine();
						ImGui::Checkbox("Orbit Dots", &CheatMenuVariables::FunCrosshairOrbit);
						ImGui::SameLine();
						ImGui::Checkbox("Bouncy Title", &CheatMenuVariables::FunBouncyWatermark);
						ImGui::Text("Fun Speed");
						ImGui::SameLine();
						ImGui::SliderFloat("##FunSpeed", &CheatMenuVariables::FunSpeed, 0.1f, 5.0f, "%.1f");
						ImGui::Text("Fun Intensity");
						ImGui::SameLine();
						ImGui::SliderFloat("##FunIntensity", &CheatMenuVariables::FunIntensity, 0.25f, 3.0f, "%.1f");
						ImGui::Text("Confetti Count");
						ImGui::SameLine();
						ImGui::SliderInt("##FunConfettiCount", &CheatMenuVariables::FunConfettiCount, 10, 180);
						ImGui::Text("Border Thickness");
						ImGui::SameLine();
						ImGui::SliderFloat("##FunBorderThickness", &CheatMenuVariables::FunBorderThickness, 1.0f, 10.0f, "%.0f");
						ImGui::Unindent();
					}
					ImGui::Checkbox("Status Overlay", &CheatMenuVariables::StatusOverlay);
					ImGui::SameLine();
					ImGui::Checkbox("Detailed Status", &CheatMenuVariables::StatusOverlayDetailed);
					ImGui::Checkbox("Auto Arrange HUD", &CheatMenuVariables::HudAutoLayout);
					if (CheatMenuVariables::HudAutoLayout) {
						const char* layouts[] = { "Compact", "Corners" };
						ImGui::Text("HUD Layout");
						ImGui::SameLine();
						ImGui::Combo("##HudLayoutMode", &CheatMenuVariables::HudLayoutMode, layouts, IM_ARRAYSIZE(layouts));
						ImGui::Text("HUD Margin");
						ImGui::SameLine();
						ImGui::SliderFloat("##HudMargin", &CheatMenuVariables::HudMargin, 4.0f, 80.0f, "%.0f");
						ImGui::Text("HUD Gap");
						ImGui::SameLine();
						ImGui::SliderFloat("##HudGap", &CheatMenuVariables::HudGap, 2.0f, 40.0f, "%.0f");
					}
					ImGui::Text("Overlay Position");
					ImGui::SameLine();
					ImGui::SliderFloat("##OverlayX", &CheatMenuVariables::OverlayPosX, 0.0f, 800.0f, "X %.0f");
					ImGui::SameLine();
					ImGui::SliderFloat("##OverlayY", &CheatMenuVariables::OverlayPosY, 0.0f, 800.0f, "Y %.0f");
					if (ImGui::Button("Clamp HUD To Screen")) {
						AegisUniversal::ClampHudPositions();
					}
				}

				{ // Mouse things
					ImGui::Checkbox("Draw mouse", &CheatMenuVariables::ShowMouse);
					ImGui::SameLine();
					ImGui::ColorEdit3("##MouseColor", (float*)&CheatMenuVariables::MouseColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the mouse");
					ImGui::SameLine();
					ImGui::Checkbox("##RGB1", &CheatMenuVariables::RainbowMouse);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle rainbow color on the mouse");
					ImGui::Text("Mouse Type");
					ImGui::SameLine();
					ImGui::SliderInt("##Mouse type", &CheatMenuVariables::MouseType, 0, 1);
				}

				{ // Crosshair
					ImGui::Checkbox("Crosshair", &CheatMenuVariables::Crosshair);
					ImGui::SameLine();
					ImGui::ColorEdit3("##CrosshairColor", (float*)&CheatMenuVariables::CrosshairColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the crosshair");
					ImGui::SameLine();
					ImGui::Checkbox("##RGB2", &CheatMenuVariables::RainbowCrosshair);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle rainbow color on the crosshair");
					ImGui::Text("Crosshair Size");
					ImGui::SameLine();
					ImGui::SliderFloat("##Crosshair Size", &CheatMenuVariables::CrosshairSize, 0.1f, 10.0f);
					ImGui::Text("Crosshair Gap");
					ImGui::SameLine();
					ImGui::SliderFloat("##Crosshair Gap", &CheatMenuVariables::CrosshairGap, 0.0f, 10.0f);
					ImGui::Text("Crosshair Thickness");
					ImGui::SameLine();
					ImGui::SliderFloat("##Crosshair Thickness", &CheatMenuVariables::CrosshairThickness, 1.0f, 8.0f);
					ImGui::Checkbox("Crosshair Dot", &CheatMenuVariables::CrosshairDot);
					ImGui::SameLine();
					ImGui::SliderFloat("##Crosshair Dot Size", &CheatMenuVariables::CrosshairDotSize, 1.0f, 12.0f, "%.0f");
					ImGui::Checkbox("Crosshair Outline", &CheatMenuVariables::CrosshairOutline);
					ImGui::Text("Crosshair Type");
					ImGui::SameLine();
					ImGui::SliderInt("##Crosshair type", &CheatMenuVariables::CrosshairType, 0, 1);
				}

				{ // Camera fov
					ImGui::Checkbox("Camera Fov Changer", &CheatMenuVariables::CameraFovChanger);
					ImGui::Text("FOV Setter Hook: %s", CheatVariables::CameraFovHookEnabled ? "Active" : (CheatVariables::CameraFovHookFailed ? "Failed" : "Unavailable"));
					ImGui::SameLine();
					ImGui::Text("Desired: %.1f", CheatVariables::CameraFovLastDesired);
					ImGui::Checkbox("Add To Game FOV", &CheatMenuVariables::CameraFovAdditive);
					if (CheatMenuVariables::CameraFovAdditive) {
						ImGui::Text("Camera FOV Add");
						ImGui::SameLine();
						ImGui::SliderFloat("##Camera FOV Add", &CheatMenuVariables::CameraFovOffset, -80.0f, 160.0f, "%+.1f");
						ImGui::Text("Baseline: %.1f", CheatVariables::CameraFovBaselineCaptured ? CheatVariables::CameraFovBaseline : 0.0f);
						ImGui::SameLine();
						if (ImGui::Button("Reset Baseline")) {
							CheatVariables::CameraFovBaselineCaptured = false;
						}
					}
					else {
						ImGui::Text("Camera Custom FOV");
						ImGui::SameLine();
						ImGui::SliderFloat("##Camera Custom FOV", &CheatMenuVariables::CameraCustomFOV, 1.0f, 300.0f);
					}
				}

				break;
			}
			case TAB_UNIVERSAL: {

				ImGui::Text("Runtime Stability");
				ImGui::Checkbox("Stability Mode", &CheatMenuVariables::StabilityMode);
				ImGui::SameLine();
				Utils::HelpMarker("Keeps expensive Unity component scans throttled or guarded for injected DLL stability.");
				ImGui::Text("Max Players / Frame");
				ImGui::SameLine();
				ImGui::SliderInt("##MaxPlayersPerFrame", &CheatMenuVariables::MaxPlayersPerFrame, 16, 250);
				ImGui::Text("Max Explorer / Frame");
				ImGui::SameLine();
				ImGui::SliderInt("##MaxTestObjectsPerFrame", &CheatMenuVariables::MaxTestObjectsPerFrame, 8, 250);
				if (CheatMenuVariables::StabilityMode) {
					ImGui::TextDisabled("Chams, skeleton, health, and bot component reads are guarded.");
				}
				ImGui::Separator();

				ImGui::Text("Player Cache");
				ImGui::Text("Cache Refresh");
				ImGui::SameLine();
				if (ImGui::SliderInt("##CacheRefreshMs", &CheatVariables::CacheRefreshMs, CheatMenuVariables::StabilityMode ? 750 : 250, 10000, "%d ms")) {
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				static int cachePresetIndex = 0;
				const char* cachePresets[] = {
					"PlayerController",
					"UnityEngine.CharacterController",
					"CharacterController",
					"NetworkPlayer",
					"Player",
					"PlayerMovement",
					"FirstPersonController",
					"ThirdPersonController",
					"FPSController",
					"UnityEngine.CapsuleCollider"
				};
				if (ImGui::Combo("Preset##PlayerCachePreset", &cachePresetIndex, cachePresets, IM_ARRAYSIZE(cachePresets))) {
					AegisUniversal::CopyText(CheatVariables::PlayerComponentName, sizeof(CheatVariables::PlayerComponentName), cachePresets[cachePresetIndex]);
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				if (ImGui::InputTextWithHint("Primary Component", "Component name...", CheatVariables::PlayerComponentName, sizeof(CheatVariables::PlayerComponentName))) {
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				ImGui::Checkbox("Use Fallback Component", &CheatVariables::UsePlayerFallbackComponent);
				if (ImGui::InputTextWithHint("Fallback Component", "Fallback component name...", CheatVariables::PlayerFallbackComponentName, sizeof(CheatVariables::PlayerFallbackComponentName))) {
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				if (ImGui::Button("Refresh Player Cache")) {
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				ImGui::SameLine();
				if (ImGui::Button("Use Object Search")) {
					AegisUniversal::CopyText(CheatVariables::PlayerComponentName, sizeof(CheatVariables::PlayerComponentName), CheatVariables::TestObjects::Name);
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				ImGui::SameLine();
				ImGui::Text("Cached: %d", CheatVariables::PlayersCacheCount.load());

				ImGui::Separator();
				ImGui::Text("Config");
				if (ImGui::Button("Save Config")) {
					AegisUniversal::SaveConfig();
				}
				ImGui::SameLine();
				if (ImGui::Button("Load Config")) {
					AegisUniversal::LoadConfig();
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(CheatMenuVariables::LastConfigStatus);
				ImGui::Checkbox("Auto-save On Detach", &CheatMenuVariables::AutoSaveConfig);
				if (ImGui::Button("Copy Diagnostics")) {
					AegisUniversal::CopyDiagnosticsToClipboard();
				}
				ImGui::SameLine();
				if (ImGui::Button("Export External Profile")) {
					AegisUniversal::ExportExternalProfile();
				}
				ImGui::SameLine();
				if (ImGui::Button("Clamp HUD")) {
					AegisUniversal::ClampHudPositions();
				}
				ImGui::SameLine();
				if (ImGui::Button("Preset: Clean Visual")) {
					AegisUniversal::ApplyQuickPreset(0);
				}
				ImGui::SameLine();
				if (ImGui::Button("Preset: Scout")) {
					AegisUniversal::ApplyQuickPreset(1);
				}
				ImGui::SameLine();
				if (ImGui::Button("Preset: Diagnostics")) {
					AegisUniversal::ApplyQuickPreset(2);
				}
				ImGui::SameLine();
				if (ImGui::Button("Preset: Party")) {
					AegisUniversal::ApplyQuickPreset(3);
				}
				ImGui::TextWrapped("%s", AegisUniversal::ConfigPath().c_str());

				ImGui::Separator();
				ImGui::Text("Hotkeys");
				AegisUniversal::HotkeyControl("Show / hide menu", KEYS::SHOWMENU_KEY, 1);
				AegisUniversal::HotkeyControl("Panic disable", KEYS::PANIC_KEY, 2);
				AegisUniversal::HotkeyControl("Detach", KEYS::DEATTACH_KEY, 3);
				AegisUniversal::HotkeyControl("Toggle boxes", KEYS::PLAYERS_BOX_KEY, 4);
				AegisUniversal::HotkeyControl("Toggle snaplines", KEYS::PLAYERS_SNAPLINE_KEY, 5);
				AegisUniversal::HotkeyControl("Toggle aimbot", KEYS::AIMBOT_KEY, 6);
				AegisUniversal::HotkeyControl("Toggle crosshair", KEYS::CROSSHAIR_KEY, 7);
				AegisUniversal::HotkeyControl("Aimbot hold key", KEYS::AIMBOT_ACTIVATION_KEY, 8);

				ImGui::Separator();
				ImGui::Text("Object Explorer");
				ImGui::Checkbox("Enable Object Explorer", &CheatMenuVariables::EnableDeveloperOptions);
				ImGui::SameLine();
				if (ImGui::Button("Refresh Object Search")) {
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				ImGui::InputTextWithHint("Object Component", "Name of a component...", CheatVariables::TestObjects::Name, sizeof(CheatVariables::TestObjects::Name));
				if (ImGui::Button("Use Search As Player Cache")) {
					AegisUniversal::CopyText(CheatVariables::PlayerComponentName, sizeof(CheatVariables::PlayerComponentName), CheatVariables::TestObjects::Name);
					CheatVariables::ForcePlayerCacheRefresh.store(true);
				}
				ImGui::SameLine();
				ImGui::Checkbox("Pinned Only", &CheatVariables::TestObjects::UsePinnedOnly);
				ImGui::SameLine();
				ImGui::Text("Objects: %d", CheatVariables::TestObjects::CacheCount.load());
				ImGui::Text("Rows");
				ImGui::SameLine();
				ImGui::SliderInt("##ObjectExplorerRows", &CheatVariables::TestObjects::MaxRows, 10, 250);
				if (CheatVariables::TestObjects::PinnedObject) {
					ImGui::Text("Pinned: %s", CheatVariables::TestObjects::PinnedObjectName);
				}

				ImGui::Checkbox("Explorer Snapline", &CheatVariables::TestObjects::Snapline);
				ImGui::SameLine();
				ImGui::Checkbox("Explorer Box", &CheatVariables::TestObjects::Box);
				ImGui::SameLine();
				ImGui::Checkbox("Explorer Aimbot", &CheatVariables::TestObjects::Aimbot);
				ImGui::SameLine();
				ImGui::Checkbox("Explorer Chams", &CheatVariables::TestObjects::Chams);

				std::vector<Unity::CGameObject*> objects;
				{
					std::scoped_lock lock(CheatVariables::TestObjects::ListMutex);
					objects = CheatVariables::TestObjects::List;
				}

				if (ImGui::BeginChild("##ObjectExplorerList", ImVec2(0, 145), true))
				{
					int visibleIndex = 0;
					for (Unity::CGameObject* object : objects)
					{
						if (!object)
							continue;

						ImGui::PushID(object);
						if (ImGui::SmallButton(CheatVariables::TestObjects::PinnedObject == object ? "Pinned" : "Pin")) {
							CheatVariables::TestObjects::PinnedObject = object;
							const std::string name = AegisUniversal::CachedObjectName(object);
							AegisUniversal::CopyText(CheatVariables::TestObjects::PinnedObjectName, sizeof(CheatVariables::TestObjects::PinnedObjectName), name.c_str());
						}
						ImGui::SameLine();
						const std::string name = AegisUniversal::CachedObjectName(object);
						ImGui::Text("[%02d] %p  %s", visibleIndex, static_cast<void*>(object), name.c_str());

						Unity::CTransform* transform = nullptr;
						try { transform = AegisUniversal::GetObjectTransform(object); }
						catch (...) { transform = nullptr; }
						if (transform) {
							Unity::Vector3 position = {};
							if (AegisUniversal::GetTransformPosition(transform, position)) {
								ImGui::SameLine();
								ImGui::Text("(%.1f, %.1f, %.1f)", position.x, position.y, position.z);
							}
						}
						ImGui::PopID();

						const int maxRows = std::clamp(CheatVariables::TestObjects::MaxRows, 10, 250);
						if (++visibleIndex >= maxRows) {
							ImGui::Text("Showing first %d objects.", maxRows);
							break;
						}
					}
					if (visibleIndex == 0) {
						ImGui::TextUnformatted("No objects cached for this component yet.");
					}
				}
				ImGui::EndChild();

				break;
			}
			case TAB_DEV: {

				const int cacheSource = CheatVariables::PlayersCacheSource.load();
				const char* cacheSourceName = "None";
				if (cacheSource == 1) cacheSourceName = CheatVariables::PlayerComponentName;
				if (cacheSource == 2) cacheSourceName = CheatVariables::PlayerFallbackComponentName;
				ImGui::Text("Cached Players: %d", CheatVariables::PlayersCacheCount.load());
				ImGui::Text("Cache Source: %s", cacheSourceName);
				ImGui::Spacing();

				ImGui::Checkbox("Enable Developer Options", &CheatMenuVariables::EnableDeveloperOptions);

				if (CheatMenuVariables::EnableDeveloperOptions)
				{
					ImGui::Indent();
					ImGui::Checkbox("Show Inspector", &CheatMenuVariables::ShowInspector);
					ImGui::Checkbox("Show Lua Editor", &Lua::ShowEditor);
					ImGui::Spacing();

					{ // test things
						ImGui::Text("Test Objects");
						ImGui::SameLine();
						ImGui::InputTextWithHint("##SearchObject", "Name of a component...", CheatVariables::TestObjects::Name, 200);

						ImGui::Checkbox("Test Objects Chams", &CheatVariables::TestObjects::Chams);
						ImGui::SameLine();
						ImGui::Checkbox("Test Objects Snapline", &CheatVariables::TestObjects::Snapline);
						ImGui::Checkbox("Test Objects Box", &CheatVariables::TestObjects::Box);
						ImGui::SameLine();
						ImGui::Checkbox("Test Objects Aimbot", &CheatVariables::TestObjects::Aimbot);
					}
					ImGui::Unindent();

				}
				break;
			}
		}
		ImGui::End();
	}
}

void CheatsLoop()
{
	const ULONGLONG currentTime = GetTickCount64();

	if (Variables::System::RuntimeBackend == UnityRuntimeBackend::Unknown) return;

	AegisUniversal::ClampRuntimeSettings();

	AegisUniversal::ApplyCameraFov();

	if (CheatMenuVariables::Spinbot) {
		Fns::Spinbot();
	}

	Unity::CCamera* frameCamera = AegisUniversal::GetFrameCamera();
	const bool allowRiskyRuntimeFeatures = !CheatMenuVariables::StabilityMode;
	const bool canRunChamsThisFrame = allowRiskyRuntimeFeatures &&
		currentTime - CheatVariables::LastChamsTick > static_cast<ULONGLONG>(CheatMenuVariables::StabilityMode ? 1000 : 500);
	bool chamsRequestedThisFrame = false;

	if (CheatMenuVariables::EnableDeveloperOptions)	{
		std::vector<Unity::CGameObject*> testObjects;
		{
			std::scoped_lock lock(CheatVariables::TestObjects::ListMutex);
			testObjects = CheatVariables::TestObjects::List;
		}

		int processedTestObjects = 0;
		const int maxTestObjects = std::clamp(CheatMenuVariables::MaxTestObjectsPerFrame, 8, 250);
		for (Unity::CGameObject* curObject : testObjects) {
			if (!curObject) continue;
			if (++processedTestObjects > maxTestObjects) break;
			if (CheatVariables::TestObjects::UsePinnedOnly && CheatVariables::TestObjects::PinnedObject != curObject) continue;

			Unity::CTransform* objectTransform = AegisUniversal::GetObjectTransform(curObject);
			if(!objectTransform) continue;

			Unity::Vector3 objectPos = {};
			if (!AegisUniversal::GetTransformPosition(objectTransform, objectPos)) continue;

			Unity::Vector3 headPos = objectPos; // HEAD OF THE PLAYER <-- THIS TRICK IS USELESS IF YOU KNOW THE HEAD POSITION
			headPos.y += CheatMenuVariables::FakeHeadPosDiff;
			Unity::Vector3 feetPos = objectPos; // FEET OF THE PLAYER <-- THIS TRICK IS USELESS IF YOU KNOW THE FEET POSITION
			feetPos.y -= CheatMenuVariables::FakeFeetPosDiff;

			Vector2 top, bottom;
			if (!AegisUniversal::WorldToScreen(frameCamera, feetPos, bottom)) continue;

			if (CheatVariables::TestObjects::Snapline)
			{
				ImColor color = CheatVariables::TestObjects::SnaplineColor;

				Fns::RenderESPSnapline(color, bottom);
			}

			if (CheatVariables::TestObjects::Box) {
				ImColor color = CheatVariables::TestObjects::BoxColor;

				if (AegisUniversal::WorldToScreen(frameCamera, headPos, top)) {
					Fns::RenderESPBox(color, bottom, top);
				}
			}

			if (CheatVariables::TestObjects::Aimbot) {
				if (AegisUniversal::WorldToScreen(frameCamera, headPos, top)) {
					Fns::ExecAimbot(curObject, top);
				}
			}

			if (CheatVariables::TestObjects::Chams) {
				chamsRequestedThisFrame = true;
			}

			if (CheatVariables::TestObjects::Chams && canRunChamsThisFrame) {
				Fns::RenderChams(curObject);
			}

		}
	}

	std::vector<Unity::CGameObject*> players;
	{
		std::scoped_lock lock(CheatVariables::PlayersListMutex);
		players = CheatVariables::PlayersList;
	}

	Unity::Vector3 cameraPosition = {};
	const bool wantsPlayerDistance = CheatMenuVariables::PlayersDistance || CheatMenuVariables::PlayersMaxDistance ||
		CheatMenuVariables::Radar || CheatMenuVariables::PlayersDistanceFade || CheatMenuVariables::PlayersListOverlay ||
		CheatMenuVariables::OffscreenArrowLabels || CheatMenuVariables::PlayersTargetInfoOverlay;
	const bool hasCameraPosition = wantsPlayerDistance && AegisUniversal::TryGetMainCameraPosition(cameraPosition);
	if (CheatMenuVariables::Radar && hasCameraPosition) {
		AegisUniversal::DrawRadarBase();
	}
	Unity::CGameObject* bestAimbotTarget = nullptr;
	Vector2 bestAimbotScreen = {};
	float bestAimbotDistance = 999999999.0f;
	Vector2 bestMarkerTop = {};
	Vector2 bestMarkerBottom = {};
	float bestMarkerDistance = 999999999.0f;
	float bestMarkerPlayerDistance = -1.0f;
	std::string bestMarkerName;
	Unity::CGameObject* bestMarkerObject = nullptr;
	bool hasBestMarker = false;
	int drawnPlayers = 0;
	int visiblePlayers = 0;
	int playerIndex = 0;
	int processedPlayers = 0;
	const int maxPlayersThisFrame = std::clamp(CheatMenuVariables::MaxPlayersPerFrame, 16, 250);
	std::vector<AegisUniversal::PlayerOverlayRow> playerRows;

	for (Unity::CGameObject* curPlayer : players)
	{
		if (!curPlayer) continue;
		if (++processedPlayers > maxPlayersThisFrame) break;

		Unity::CTransform* playerTransform = AegisUniversal::GetObjectTransform(curPlayer);
		if (!playerTransform) continue;

		Unity::Vector3 playerPos = {};
		if (!AegisUniversal::GetTransformPosition(playerTransform, playerPos)) continue;
		float playerDistance = -1.0f;
		if (hasCameraPosition) {
			playerDistance = Utils::GetDistance(cameraPosition, playerPos);
			if (CheatMenuVariables::PlayersMaxDistance && playerDistance > CheatMenuVariables::PlayersMaxDistanceValue)
				continue;
		}
		++playerIndex;

		std::string playerName;
		if (CheatMenuVariables::PlayersName || CheatMenuVariables::RadarLabels || CheatMenuVariables::PlayersListOverlay ||
			CheatMenuVariables::OffscreenArrowLabels || CheatMenuVariables::PlayersTargetInfoOverlay) {
			playerName = AegisUniversal::PlayerDisplayName(curPlayer, playerIndex);
		}

		size_t playerRowIndex = static_cast<size_t>(-1);
		if (CheatMenuVariables::PlayersListOverlay) {
			playerRowIndex = playerRows.size();
			playerRows.push_back({ curPlayer, playerName, playerDistance, false });
		}

		Unity::Vector3 headPos = playerPos; // HEAD OF THE PLAYER <-- THIS TRICK IS USELESS IF YOU KNOW THE HEAD POSITION
		headPos.y += CheatMenuVariables::FakeHeadPosDiff;
		Unity::Vector3 feetPos = playerPos; // FEET OF THE PLAYER <-- THIS TRICK IS USELESS IF YOU KNOW THE FEET POSITION
		feetPos.y -= CheatMenuVariables::FakeFeetPosDiff;

		if (CheatMenuVariables::PlayersOffscreenArrows) {
			ImColor arrowColor = CheatMenuVariables::RainbowOffscreenArrows ? CheatVariables::RainbowColor : CheatMenuVariables::OffscreenArrowColor;
			arrowColor = CheatVariables::TargetPlayer == curPlayer ? CheatVariables::TargetPlayerColor : arrowColor;
			arrowColor = AegisUniversal::ApplyDistanceFade(arrowColor, playerDistance);
			AegisUniversal::DrawOffscreenArrow(headPos, arrowColor, playerName.c_str(), playerDistance);
		}

		if (CheatMenuVariables::Radar && hasCameraPosition) {
			ImColor radarColor = CheatVariables::TargetPlayer == curPlayer ? CheatVariables::TargetPlayerColor : CheatMenuVariables::RadarColor;
			radarColor = AegisUniversal::ApplyDistanceFade(radarColor, playerDistance);
			AegisUniversal::DrawRadarPoint(cameraPosition, playerPos, radarColor, playerName.c_str());
		}

		Vector2 top, bottom;
		if (!AegisUniversal::WorldToScreen(frameCamera, feetPos, bottom)) continue;
		++visiblePlayers;
		if (playerRowIndex != static_cast<size_t>(-1) && playerRowIndex < playerRows.size()) {
			playerRows[playerRowIndex].visible = true;
		}
		if (CheatMenuVariables::PlayersDrawLimit && drawnPlayers >= std::clamp(CheatMenuVariables::PlayersMaxDrawCount, 5, 250)) {
			continue;
		}
		++drawnPlayers;
		const bool hasHeadScreen = AegisUniversal::WorldToScreen(frameCamera, headPos, top);

		if (CheatMenuVariables::PlayersSnapline)
		{
			ImColor color = CheatMenuVariables::RainbowPlayersSnapline ? CheatVariables::RainbowColor : CheatMenuVariables::PlayersSnaplineColor;

			if (CheatMenuVariables::BotChecker && allowRiskyRuntimeFeatures) {
				bool isBot = false;
				if (AegisUniversal::MonoUnity::Active()) {
					Unity::CComponent* CPlayer = AegisUniversal::MonoUnity::GetComponentByName(curPlayer, "Player");
					isBot = CPlayer && AegisUniversal::MonoUnity::ReadMemberValue<bool>(CPlayer, "Player", "isbot", isBot) && isBot;
				}
				else if (System::RuntimeBackend == UnityRuntimeBackend::IL2CPP) {
					Unity::CComponent* CPlayer = curPlayer->GetComponent("Player");
					isBot = CPlayer && CPlayer->GetMemberValue<bool>("isbot");
				}

				if (isBot) {
					color = CheatMenuVariables::RainbowBotChecker ? CheatVariables::RainbowColor : CheatMenuVariables::BotCheckerColor;

					if (CheatMenuVariables::BotCheckerText) {
						Utils::DrawOutlinedTextForeground(gameFont, ImVec2(bottom.x, bottom.y + 10), CheatMenuVariables::PlayersLabelSize, color, false, "Bot");
					}
				}
			}

			// another color if target of aimbot
			color = CheatVariables::TargetPlayer == curPlayer ? CheatVariables::TargetPlayerColor : color;
			color = AegisUniversal::ApplyDistanceFade(color, playerDistance);

			Fns::RenderESPSnapline(color, bottom);
		}

		if (CheatMenuVariables::PlayersBox) {
			ImColor color = CheatMenuVariables::RainbowPlayersBox ? CheatVariables::RainbowColor : CheatMenuVariables::PlayersBoxColor;

			color = CheatVariables::TargetPlayer == curPlayer ? CheatVariables::TargetPlayerColor : color;
			color = AegisUniversal::ApplyDistanceFade(color, playerDistance);

			if (hasHeadScreen) {
				Fns::RenderESPBox(color, bottom, top);
			}
		}

		if (CheatMenuVariables::PlayersName || CheatMenuVariables::PlayersDistance) {
			std::string label;
			if (CheatMenuVariables::PlayersName) {
				label = playerName;
			}

			if (CheatMenuVariables::PlayersDistance && playerDistance >= 0.0f) {
				char distanceText[48] = {};
				AegisUniversal::FormatDistance(playerDistance, distanceText, sizeof(distanceText));
				if (!label.empty())
					label += "  ";
				label += distanceText;
			}

			if (!label.empty()) {
				ImColor textColor = AegisUniversal::ApplyDistanceFade(CheatMenuVariables::PlayersTextColor, playerDistance);
				Utils::DrawOutlinedTextForeground(gameFont, ImVec2(bottom.x, bottom.y + 6.0f), CheatMenuVariables::PlayersLabelSize, textColor, true, "%s", label.c_str());
			}
		}

		if (CheatMenuVariables::PlayerSkeleton && allowRiskyRuntimeFeatures)
		{
			ImColor color = CheatMenuVariables::RainbowPlayerSkeleton ? CheatVariables::RainbowColor : CheatMenuVariables::PlayerSkeletonColor;
			color = AegisUniversal::ApplyDistanceFade(color, playerDistance);

			Fns::RenderESPSkeleton(color, curPlayer, frameCamera);
		}

		if (CheatMenuVariables::PlayersHealth && allowRiskyRuntimeFeatures) {
			Fns::RenderHealthBar(curPlayer, bottom);
		}

		if ((CheatMenuVariables::EnableAimbot || CheatMenuVariables::PlayersTargetMarker || CheatMenuVariables::PlayersTargetInfoOverlay) && hasHeadScreen) {
			const float dx = top.x - System::ScreenCenter.x;
			const float dy = top.y - System::ScreenCenter.y;
			const float distanceToCrosshair = (dx * dx) + (dy * dy);
			const float maxFov = CheatMenuVariables::AimbotFOV * CheatMenuVariables::AimbotFOV;
			const bool insideAimFov = !CheatMenuVariables::AimbotFOVCheck || distanceToCrosshair <= maxFov;

			if ((CheatMenuVariables::PlayersTargetMarker || CheatMenuVariables::PlayersTargetInfoOverlay) && insideAimFov && distanceToCrosshair < bestMarkerDistance) {
				bestMarkerDistance = distanceToCrosshair;
				bestMarkerPlayerDistance = playerDistance;
				bestMarkerName = playerName;
				bestMarkerObject = curPlayer;
				bestMarkerTop = top;
				bestMarkerBottom = bottom;
				hasBestMarker = true;
			}

			if (CheatMenuVariables::EnableAimbot) {
				if (CheatMenuVariables::AimbotTargetMode == 1) {
					if (insideAimFov && distanceToCrosshair < bestAimbotDistance) {
						bestAimbotDistance = distanceToCrosshair;
						bestAimbotTarget = curPlayer;
						bestAimbotScreen = top;
					}
				}
				else {
					Fns::ExecAimbot(curPlayer, top);
				}
			}
		}

		if (CheatMenuVariables::PlayerChams) {
			chamsRequestedThisFrame = true;
		}

		if (CheatMenuVariables::PlayerChams && canRunChamsThisFrame) {
			Fns::RenderChams(curPlayer);
		}
	}

	if (chamsRequestedThisFrame && canRunChamsThisFrame) {
		CheatVariables::LastChamsTick = currentTime;
	}

	if (CheatMenuVariables::EnableAimbot && CheatMenuVariables::AimbotTargetMode == 1 && bestAimbotTarget) {
		Fns::ExecAimbot(bestAimbotTarget, bestAimbotScreen);
	}
	else if (KEYS::AIMBOT_ACTIVATION_KEY <= 0 || !GetAsyncKeyState(KEYS::AIMBOT_ACTIVATION_KEY)) {
		CheatVariables::TargetPlayer = nullptr;
	}

	if (hasBestMarker) {
		ImColor markerColor = CheatMenuVariables::RainbowTargetMarker ? CheatVariables::RainbowColor : CheatMenuVariables::TargetMarkerColor;
		markerColor = AegisUniversal::ApplyDistanceFade(markerColor, bestMarkerPlayerDistance);
		AegisUniversal::DrawTargetMarker(bestMarkerTop, bestMarkerBottom, markerColor);
		AegisUniversal::DrawTargetInfoOverlay(bestMarkerName.c_str(), bestMarkerPlayerDistance, std::sqrt(bestMarkerDistance), CheatVariables::TargetPlayer == bestMarkerObject, static_cast<int>(playerRows.size()));
	}

	AegisUniversal::DrawPlayerListOverlay(playerRows);

	if (CheatMenuVariables::PlayersCountOverlay) {
		const ImVec2 countPos = AegisUniversal::HudPlayerCountPos();
		if (CheatMenuVariables::PlayersDrawLimit) {
			Utils::DrawOutlinedTextForeground(gameFont, countPos, CheatMenuVariables::PlayersLabelSize, CheatVariables::RainbowColor, false, "Players: %d visible / %d drawn / %zu cached", visiblePlayers, drawnPlayers, players.size());
		}
		else {
			Utils::DrawOutlinedTextForeground(gameFont, countPos, CheatMenuVariables::PlayersLabelSize, CheatVariables::RainbowColor, false, "Players: %d visible / %zu cached", visiblePlayers, players.size());
		}
	}

	if (currentTime - CheatVariables::LastTick > 5)
	{
		CheatVariables::LastTick = currentTime;
	}
}

void CacheManager()
{
	while (Variables::System::Running.load())
	{
		if (Variables::System::RuntimeBackend == UnityRuntimeBackend::Unknown) {
			Sleep(100);
			continue;
		}

		AegisUniversal::ClampRuntimeSettings();
		UnityRuntime::ScopedThread runtimeThread;
		if (!runtimeThread.IsAttached()) {
			Sleep(100);
			continue;
		}

		// here maybe you can check for a localplayer, or look the Objects Cache method

		try {
			if (CheatMenuVariables::EnableDeveloperOptions) {
				std::vector<Unity::CGameObject*> testObjects;
				const int maxCachedObjects = std::clamp(CheatMenuVariables::MaxTestObjectsPerFrame * 4, 64, 512);
				if (System::RuntimeBackend == UnityRuntimeBackend::Mono)
					AegisUniversal::MonoUnity::ObjectsCache(&testObjects, CheatVariables::TestObjects::Name, maxCachedObjects);
				else
					Utils::ObjectsCache(&testObjects, CheatVariables::TestObjects::Name, nullptr, maxCachedObjects);
				CheatVariables::TestObjects::CacheCount.store(static_cast<int>(testObjects.size()));
				{
					std::scoped_lock lock(CheatVariables::TestObjects::ListMutex);
					CheatVariables::TestObjects::List.swap(testObjects);
				}
			}

			std::vector<Unity::CGameObject*> players;
			const char* primaryComponent = CheatVariables::PlayerComponentName[0] ? CheatVariables::PlayerComponentName : "PlayerController";
			const char* fallbackComponent = CheatVariables::PlayerFallbackComponentName[0] ? CheatVariables::PlayerFallbackComponentName : "UnityEngine.CharacterController";
			int cacheSource = 1;
			const int maxCachedPlayers = std::clamp(CheatMenuVariables::MaxPlayersPerFrame * 4, 64, 512);
			if (System::RuntimeBackend == UnityRuntimeBackend::Mono)
				AegisUniversal::MonoUnity::ObjectsCache(&players, primaryComponent, maxCachedPlayers);
			else
				Utils::ObjectsCache(&players, primaryComponent, nullptr, maxCachedPlayers);
			if (players.empty() && CheatVariables::UsePlayerFallbackComponent) {
				cacheSource = 2;
				if (System::RuntimeBackend == UnityRuntimeBackend::Mono)
					AegisUniversal::MonoUnity::ObjectsCache(&players, fallbackComponent, maxCachedPlayers);
				else
					Utils::ObjectsCache(&players, fallbackComponent, nullptr, maxCachedPlayers);
			}
			if (players.empty()) {
				cacheSource = 0;
			}
			CheatVariables::PlayersCacheCount.store(static_cast<int>(players.size()));
			CheatVariables::PlayersCacheSource.store(cacheSource);
			{
				std::scoped_lock lock(CheatVariables::PlayersListMutex);
				CheatVariables::PlayersList.swap(players);
			}
		}
		catch (const std::exception& e) {
			if (DEBUG)
				std::cout << e.what() << std::endl;
		}

		const int refreshMs = std::clamp(CheatVariables::CacheRefreshMs, CheatMenuVariables::StabilityMode ? 750 : 250, 10000);
		int sleptMs = 0;
		while (sleptMs < refreshMs && Variables::System::Running.load()) {
			if (CheatVariables::ForcePlayerCacheRefresh.exchange(false))
				break;
			Sleep(100);
			sleptMs += 100;
		}
	}
}
