#pragma once

// includes
#include <string>
#include <intrin.h>

// my includes
#include <Core/Variables.h>
#include <Libraries/Il2cpp_Resolver/il2cpp_resolver.hpp>

using namespace Variables;

namespace HooksFunctions
{
	using CameraSetFieldOfViewFn = void(UNITY_CALLING_CONVENTION)(Unity::CCamera*, float);

	inline CameraSetFieldOfViewFn UnityEngine_Camera__set_fieldOfView = nullptr;
	inline thread_local bool CameraFovInternalCall = false;

	inline float ClampCameraFov(float value)
	{
		if (value < 1.0f)
			return 1.0f;
		if (value > 300.0f)
			return 300.0f;
		return value;
	}

	inline float ResolveCameraFov(float requestedFov)
	{
		float desiredFov = requestedFov;

		if (CheatMenuVariables::CameraFovChanger) {
			if (CheatMenuVariables::CameraFovAdditive) {
				CheatVariables::CameraFovBaseline = requestedFov;
				CheatVariables::CameraFovBaselineCaptured = true;
				desiredFov = requestedFov + CheatMenuVariables::CameraFovOffset;
			}
			else {
				CheatVariables::CameraFovBaselineCaptured = false;
				desiredFov = CheatMenuVariables::CameraCustomFOV;
			}
		}

		desiredFov = ClampCameraFov(desiredFov);
		CheatVariables::CameraFovLastDesired = desiredFov;
		return desiredFov;
	}

	inline void SetCameraFovOriginal(Unity::CCamera* camera, float fov)
	{
		if (!camera)
			return;

		if (UnityEngine_Camera__set_fieldOfView) {
			CameraFovInternalCall = true;
			UnityEngine_Camera__set_fieldOfView(camera, ClampCameraFov(fov));
			CameraFovInternalCall = false;
			return;
		}

		if (Unity::CameraFunctions.m_pSetFieldOfView) {
			camera->SetFieldOfView(ClampCameraFov(fov));
		}
	}

	inline void __fastcall UnityEngine_Camera__set_fieldOfView_hook(Unity::CCamera* camera, float value)
	{
		if (!UnityEngine_Camera__set_fieldOfView)
			return;

		if (CameraFovInternalCall || !CheatMenuVariables::CameraFovChanger) {
			UnityEngine_Camera__set_fieldOfView(camera, value);
			return;
		}

		UnityEngine_Camera__set_fieldOfView(camera, ResolveCameraFov(value));
	}

	// EXAMPLE
	//void(UNITY_CALLING_CONVENTION Health__TakeDamage)(Health_o*, int32_t, UnityEngine_Vector3_o, System_String_o*, uint8_t, System_String_o*);
	//void Health__TakeDamage_hook(Health_o* _this, int32_t _damage, UnityEngine_Vector3_o _position, System_String_o* _name, uint8_t _weaponSpriteIndex, System_String_o* steamIdData)
	//{
	//	if (_this != nullptr) {

	//		if(CheatMenuVariables::GodMode) _damage = 0;
	//	}

	//	return Health__TakeDamage(_this, _damage, _position, _name, _weaponSpriteIndex, steamIdData);
	//}
}
