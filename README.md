# Aegis Unity Universal

Aegis Unity Universal is a Windows x64 Unity runtime overlay and diagnostics DLL for authorized Unity projects and internal research builds. It supports both IL2CPP and Mono Unity runtimes, with D3D11 and OpenGL ImGui rendering paths.

## Features

- Automatic Unity runtime detection for IL2CPP and Mono.
- D3D11 and OpenGL overlay initialization.
- ImGui menu with configurable runtime, visual, misc, universal, and developer panels.
- Developer diagnostics for runtime status, object cache status, FOV state, and config path.
- Config save/load support.
- Lua scripting surface for local experimentation.

## Requirements

- Windows x64.
- Visual Studio 2022 with Desktop development with C++.
- Windows 10 SDK.
- DirectX SDK June 2010.
- vcpkg manifest restore enabled. The repo includes `vcpkg.json` for Lua.

## Build

Open `AegisUnityUniversal.sln` in Visual Studio and build `Release|x64`, or run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' '.\AegisUnityUniversal.sln' /p:Configuration=Release /p:Platform=x64 /m
```

The DLL is produced at:

```text
build\AegisUnityUniversal.dll
```

## Repository Notes

- `build/` and `vcpkg_installed/` are generated locally and intentionally ignored.
- Vendored libraries live under `Aegis Unity Universal\Libraries`.
- Runtime-generated or dumped metadata is marked as generated for GitHub language stats.

## Usage Notice

Use this only with software you own, control, or are explicitly authorized to inspect or modify.
