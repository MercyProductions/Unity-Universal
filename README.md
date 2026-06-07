# Aegis Unity Universal

Aegis Unity Universal is a Windows x64 Unity runtime overlay and diagnostics workspace for authorized Unity projects, internal research builds, and games you own or control. The solution contains two projects:

- `Aegis Unity Universal Internal`: an injected/in-process DLL with the original overlay and Unity feature surface.
- `Aegis Unity Universal External`: an out-of-process desktop GUI and console diagnostics tool. This is still a prototype.

The internal and external builds share the same goal: make Unity runtime inspection easier across IL2CPP and Mono projects. They do it in very different ways, so read the support matrix before using either one.

## Support Matrix

| Capability | Internal DLL | External Prototype |
| --- | --- | --- |
| Build target | `AegisUnityUniversalInternal.dll` | `AegisUnityUniversalExternal.exe` |
| Runtime support | IL2CPP and Mono | IL2CPP and Mono detection; IL2CPP method-map support; Mono assembly metadata maps |
| Runs inside game process | Yes | No |
| Injection required | Yes, by design | No |
| Game render hook | D3D11/OpenGL ImGui overlay | No game hook; own transparent ImGui desktop window |
| Unity managed calls | Yes, in-process | No direct managed calls |
| Runtime export resolver | Yes | Yes |
| IL2CPP method resolving | Yes, plus generated/internal SDK data when available | Yes, from maps/dumps or auto-generated metadata map |
| Mono method resolving | Yes, through the internal Mono runtime helpers | Yes for metadata identity from `_Data\Managed` assemblies; no target-process invocation |
| Object/GameObject cache | Yes | Offset-driven visuals plus a read-only Mono/IL2CPP runtime pointer candidate cache |
| ESP/radar/crosshair | In-game overlay path | Offset-driven external overlay path |
| Lua | Yes, internal process scripting surface | No |
| Memory writes/patches | Internal features can modify in-process state | No; external memory access is read-only |

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

The internal DLL is produced at:

```text
build\internal\AegisUnityUniversalInternal.dll
```

The external diagnostics executable is produced at:

```text
build\external\AegisUnityUniversalExternal.exe
```

## Internal Usage

The internal project is the original in-process Unity Universal implementation. Build the solution, then load `AegisUnityUniversalInternal.dll` into an authorized Unity game or your own Unity development build using your normal local test loader. Once it initializes, the ImGui menu appears through the D3D11 or OpenGL render path.

Because the internal DLL runs inside the Unity process, it can use the Unity runtime directly. That is why the internal project can maintain a live object cache, call Unity methods, use IL2CPP/Mono resolver helpers, run feature loops, and expose Lua inside the target process.

Internal runtime setup currently includes:

- Unity runtime detection for IL2CPP and Mono.
- D3D11 and OpenGL ImGui initialization.
- Runtime method/signature setup in `main.cpp`.
- Object caching through `CacheManager`.
- Config save/load support.
- Visual, aim, exploit, misc, universal, and developer menu panels.
- Developer diagnostics for runtime status, object cache status, FOV state, and config path.
- Lua scripting surface for local experimentation.

## Changing The Internal Component Search

The internal object cache searches for a component name, then turns matching components into GameObjects. The default player component is configured in:

```text
Aegis Unity Universal\Core\Variables.h
```

The important values are:

```cpp
inline char PlayerComponentName[128] = "PlayerController";
inline char PlayerFallbackComponentName[128] = "UnityEngine.CharacterController";
inline bool UsePlayerFallbackComponent = true;
```

Change `PlayerComponentName` to the component used by your game. For example, if your Unity project has a player script called `MyGame.PlayerController`, set:

```cpp
inline char PlayerComponentName[128] = "MyGame.PlayerController";
```

If the game has more than one possible player-style component, use the fallback:

```cpp
inline char PlayerComponentName[128] = "MyGame.PlayerController";
inline char PlayerFallbackComponentName[128] = "MyGame.NetworkPlayer";
inline bool UsePlayerFallbackComponent = true;
```

Use the full namespace when the short name does not resolve. Common examples are:

```text
PlayerController
Game.PlayerController
MyGame.Runtime.PlayerController
UnityEngine.CharacterController
```

The cache path is:

1. `main.cpp` starts `CacheThread`.
2. `CacheThread` runs `CacheManager`.
3. `CacheManager` reads `CheatVariables::PlayerComponentName`.
4. For IL2CPP targets, it calls the IL2CPP object cache helper.
5. For Mono targets, it calls the Mono object cache helper.
6. Matching components become cached GameObjects for the menu and feature loops.

If nothing shows up:

- Check spelling and namespace first.
- Try the fallback component.
- Use the internal developer/object/component viewer to inspect component names in the live game.
- Prefer the most specific component you own, such as `MyGame.PlayerController`, instead of broad Unity built-ins.
- Make sure the objects are already spawned when the cache runs.
- If your game uses pooled or disabled objects, test after the scene has fully loaded.

The cache can also be changed at runtime in the internal menu through the Universal/developer controls that edit the primary and fallback component fields. Saving the config preserves those choices for the next run.

## External Prototype Usage

The external project is designed to be clicked directly. It does not need startup arguments.

1. Start your Unity game.
2. Run `AegisUnityUniversalExternal.exe`.
3. The console asks for a target executable name or process id.
4. Type a value like `MyUnityGame.exe`, `Stumble Guys.exe`, or a pid.
5. The tool attaches with read-only process access.
6. It scans Unity modules, resolves runtime exports, loads or generates method maps, and prints success/error details to the console.
7. The transparent ImGui desktop window opens even if part of setup failed, so you can inspect diagnostics.

The external tool saves the last target and method map in:

```text
AegisUnityUniversalExternal.ini
```

On the next launch, pressing Enter accepts the saved target.

You can still use command-line options for testing, but they are optional:

```powershell
.\build\external\AegisUnityUniversalExternal.exe
.\build\external\AegisUnityUniversalExternal.exe --console
.\build\external\AegisUnityUniversalExternal.exe --exe MyUnityGame.exe
.\build\external\AegisUnityUniversalExternal.exe --exe MyUnityGame.exe --api il2cpp_class_get_method_from_name
.\build\external\AegisUnityUniversalExternal.exe --exe MyUnityGame.exe --map .\methods.txt --resolve UnityEngine.Time get_timeScale 0
```

## External IL2CPP Method Maps

IL2CPP native runtime exports, such as `il2cpp_domain_get`, are resolved from `GameAssembly.dll`. Managed C# methods, such as `UnityEngine.Time::get_timeScale`, are not normal DLL exports, so the external resolver needs RVAs from a method map or dump.

The external tool auto-checks these locations:

- The external executable folder.
- The current working folder.
- The target executable folder.
- Unity runtime module folders.

It looks for common files:

```text
methods.txt
method_map.txt
il2cpp_methods.txt
aegis_methods.txt
aegis_unity_methods.txt
script.json
dump.cs
```

Method map entries can use any of these forms:

```text
image|Namespace.Type|Method|argc|0xRVA
Namespace.Type|Method|argc|0xRVA
Namespace.Type::Method|argc|0xRVA
```

The loader also accepts Il2CppDumper-style `script.json` and `dump.cs` files. If a dump address is a static VA instead of an RVA, the loader normalizes it against a sibling `GameAssembly.dll` image base when that DLL is beside the dump.

If no compatible file exists for an IL2CPP target, the external tool attempts a read-only auto-generation pass from:

```text
global-metadata.dat
GameAssembly.dll
```

The automatic IL2CPP generator currently targets common fixed-width metadata layouts such as metadata v24-v31, including 84-, 88-, and 92-byte type definition rows seen across Unity variants. It locates codegen modules by image name inside `GameAssembly.dll`, reads each module method pointer table, and builds an in-memory method map. This has been useful for standard Unity IL2CPP builds, but it is still a prototype and may not work on every Unity version or custom build pipeline.

When a method map is available, the external startup resolves the same common Unity method presets the internal bootstrap cares about, including:

```text
UnityEngine.Shader::Find
UnityEngine.Time::get_timeScale
UnityEngine.Time::set_timeScale
UnityEngine.Camera::get_main
UnityEngine.Camera::set_fieldOfView
UnityEngine.Camera::WorldToScreenPoint
UnityEngine.GameObject::Find
UnityEngine.GameObject::GetComponent
UnityEngine.Component::get_transform
UnityEngine.Transform::get_position
```

## External Mono Support

Mono support in the external prototype now has a real read-only metadata path. The tool can detect Mono Unity targets, find Mono runtime modules, resolve Mono exports, run process/module scans, perform read-only memory reads, run module-scoped AOB scans, maintain typed watch rows, and auto-generate a managed method metadata map from the target game's managed assemblies.

When no compatible method map file exists for a Mono target, startup searches for:

```text
<GameName>_Data\Managed
```

It parses the managed `.dll` files directly, reads ECMA-335 metadata tables, and builds an in-memory map of:

```text
Assembly image
Namespace.Type
Method
Argument count
MethodDef metadata token
IL RVA when present
```

Mono method resolution therefore returns a metadata identity, such as `UnityEngine.Time::get_timeScale -> token 0x060014BA`. It does not pretend that token is a callable native address. Normal Mono JIT/native addresses are runtime-dependent and require code running inside the target to compile or invoke methods.

## External Runtime Object Cache

The Developer tab includes a read-only runtime object cache scanner for both Mono and IL2CPP. The current testing default is `UnityEngine.Rigidbody` for both the primary and fallback component fields; you can replace either field with your own component such as `PlayerController`, then refresh the cache. The scanner walks committed readable private/mapped memory with `VirtualQueryEx` and caches addresses whose first pointer-sized field matches a target runtime header pointer.

For Mono, provide the Mono object/vtable pointer for the component when you know it from your own diagnostics, symbols, logs, or internal testing.

For IL2CPP, provide an `Il2CppClass*` or object-header class pointer when you already know it. The external can also attempt a read-only class-name scan: it searches readable process memory for class-name strings, finds candidate `Il2CppClass::name` references, validates the namespace when a fully qualified name is used, and then scans for live objects whose first pointer matches the candidate class pointer. The default x64 `Il2CppClass::name` and `Il2CppClass::namespaze` offsets are `0x10` and `0x18`; both are configurable in the Developer tab for Unity-version/layout differences, and the scanner tries nearby pointer-aligned fallbacks automatically if the preferred offsets do not resolve.

For Mono, the external can also attempt read-only `MonoClass`/`MonoVTable` discovery by class name. This lets common UnityEngine components such as `UnityEngine.Rigidbody` populate the cache without manually pasting a vtable pointer. The default `MonoClass` name/namespace offsets are `0x30` and `0x38`, and the default `MonoVTable` class pointer offset is `0x0`; these are configurable in Developer for layout differences.

Cached runtime objects can feed the Visual tab. Set `Entity Source` to `Object cache`, configure the position extraction mode/offsets, provide the view-projection matrix address or enable `Auto Find ViewProjection`, and enable ESP boxes/snaplines. The overlay re-reads cached object positions every frame before world-to-screen, so boxes can move with the objects once the offsets are correct.

If object cache entries exist but no view-projection matrix is configured, the overlay draws clearly labeled debug snaplines. Those lines only prove that the cache is active; real boxes/snaplines at the object's game position still require a valid matrix plus correct position extraction mode/offsets.

The `Auto probe object/native Vec3` position mode is a read-only helper for early testing. It starts from the managed object's `m_CachedPtr`, rejects nearby metadata-looking pointers, and prioritizes one-hop Transform-like native pointers before accepting plausible `Vector3` values. This is useful for confirming the cache can drive overlay lines, but a game-specific Transform/position offset is still the most reliable setup for exact world placement.

`Auto Find ViewProjection` is also best-effort. It samples cached object positions, scans readable private/mapped memory for plausible 4x4 float matrices, scores row-major and column-major layouts, and prefers a strict multi-sample match when enough unique objects are visible. For small scenes, `Few-Sample Matrix Guess` allows a lower-confidence fallback with one to four unique positions by adding heavier view-projection structure checks plus projected height validation. If that fallback selects the wrong matrix, disable it and use the internal tool, your own game symbols/logs, or a known camera/matrix address to finish that target-specific setup.

Object-cache position modes:

```text
Direct Vec3: object + position offset
Pointer field -> Vec3
m_CachedPtr -> native Vec3
m_CachedPtr -> native Transform ptr -> Vec3
Reference field -> m_CachedPtr -> native Vec3
Auto probe object/native Vec3
```

The default `m_CachedPtr` offset is `0x10`, which matches the common x64 managed object header plus the first inherited `UnityEngine.Object` field layout. Native Rigidbody/Transform position offsets are Unity-version and game-layout dependent, so those remain configurable.

The default object-cache scan target is 32 results so startup and diagnostics stay responsive. Increase `Max Results` in Developer only when you need a deeper scan and are comfortable with a slower full-process pass.

You can test object-cache discovery from the console without opening the GUI:

```powershell
.\build\external\AegisUnityUniversalExternal.exe --exe Muck.exe --object-cache UnityEngine.Rigidbody
.\build\external\AegisUnityUniversalExternal.exe --exe "MyGame.exe" --object-cache CharacterVisualController --fallback PlayerVisualController
```

Use the exact component class name from your build. If a requested IL2CPP name is not present in metadata, try the name shown by your own dump/internal tools or use the fallback field. For Stumble Guys-style builds where `CharacterVisualController` is not present by that exact name, the external also tries `PlayerVisualController` and `FallGirlVisualController` as read-only IL2CPP visual-controller aliases.

Use the internal DLL when you need full runtime interaction or actual in-process IL2CPP/Mono calls. Use the external tool when you want read-only process diagnostics without running code inside the game process.

## External Visuals, ESP, And Radar

The external GUI has a transparent borderless desktop window, not a game render hook. It draws its own ImGui overlay over the desktop. The game is never hooked or patched by this path.

External ESP/radar is offset-driven. You must provide the game-specific data layout:

- Entity list address.
- Entity count or entity count address.
- Entity pointer layout and stride.
- Position offset inside the entity or component structure.
- View-projection matrix address.
- Matrix layout.
- Optional local-player/local-position address for radar.

Once those values are configured, the external overlay can draw boxes, snaplines, and radar using its own world-to-screen math. It does not yet automatically discover every GameObject or component from outside the process. For unknown games, use your own symbols, debug builds, dumps, logs, or the internal object/component tools to identify the right component and offsets first.

## How The External Works

The external tool stays out-of-process:

1. It enumerates Windows processes and modules.
2. It opens the chosen target with query/read access.
3. It detects Unity runtime modules such as `UnityPlayer.dll`, `GameAssembly.dll`, and Mono DLLs.
4. It resolves native runtime exports by parsing module export tables.
5. It loads method RVAs from maps/dumps, generates an IL2CPP map from metadata, or generates a Mono metadata map from managed assemblies.
6. For IL2CPP/native maps, it turns RVAs into live addresses using the target module base.
7. For Mono metadata maps, it reports MethodDef tokens and IL RVAs as metadata identities.
8. It exposes read-only diagnostics, AOB scanning, typed memory reads, method browsing, a runtime pointer-scanned object cache, and object-cache/manual offset-driven visuals in ImGui.

The external project intentionally does not inject, hook the game renderer, call managed methods, patch code, or write memory. It is meant for authorized read-only inspection and prototype overlay work.

## Using This With Your Own Unity Games

For a game you own or are authorized to inspect:

1. Build the internal and external projects in `Release|x64`.
2. For internal testing, configure `PlayerComponentName` to your real player component.
3. For IL2CPP projects, keep `global-metadata.dat` and `GameAssembly.dll` available beside the target, or export `script.json`/`dump.cs` with your preferred Unity dump tool.
4. For Mono projects, keep the normal `<GameName>_Data\Managed` folder beside the executable so the external can auto-generate the metadata map.
5. Start the game, then launch the external executable and type the process exe name.
6. Confirm the console reports the expected runtime, module addresses, and method-map status.
7. Use the external Developer and Universal tabs to inspect exports, method maps, process modules, AOB scans, object-cache candidates, and read-only memory values.
8. For runtime object-cache ESP, start with the default `UnityEngine.Rigidbody` test component or enter your own component name such as `PlayerController`.
9. On Mono, provide the matching object/vtable pointer(s). On IL2CPP, leave `Auto Resolve IL2CPP Class Pointers` enabled or provide the matching `Il2CppClass*`/header pointer(s) manually.
10. Refresh the cache, then click `Draw Cache In Visual`.
11. In Visual, set `Entity Source` to `Object cache`, choose the correct position mode/offsets, and provide the view-projection matrix address for world-to-screen. If there are fewer than five unique objects, leave `Few-Sample Matrix Guess` enabled to let the external try the lower-confidence matrix scan.
12. Use internal component/object diagnostics when you need to discover the exact component name, GameObject relationship, Mono object/vtable pointer, IL2CPP class pointer, or native position offsets.
13. Use manual external ESP/radar only after you know the target entity list, position offsets, and camera matrix address for your build.

For best results, start with a small Unity test scene that has a known `UnityEngine.Rigidbody` or `PlayerController`, one camera, and a few spawned test objects. Confirm the internal cache sees the component, then move to the external prototype once you know which memory structures you want to read.

## Current External Prototype Limits

- No injection.
- No anti-cheat bypass or stealth layer.
- No D3D/OpenGL game render interception.
- No direct IL2CPP/Mono managed calls.
- No automatic live Unity GameObject explorer externally yet.
- No Lua execution externally.
- No memory writes or patching.
- Direct Mono invocation through `mono_runtime_invoke` remains internal-only because a real call must execute inside the target process.
- External object caching resolves component metadata names and caches live candidates from supplied Mono object/vtable pointers or IL2CPP class/header pointers; it is not a full automatic managed heap walker yet.
- External IL2CPP class-pointer auto-resolution is best-effort and depends on readable metadata strings plus the configured `Il2CppClass` name/namespace offsets.
- External object-cache ESP needs a correct position extraction mode, position offsets, and view-projection matrix address.
- IL2CPP auto-map generation depends on metadata layout and may fail on custom or unsupported Unity builds.
- Mono metadata-map generation depends on standard managed assembly metadata in `_Data\Managed`.
- Offset-driven ESP/radar requires game-specific addresses and structure offsets.

## Repository Notes

- `build/` and `vcpkg_installed/` are generated locally and intentionally ignored.
- Vendored internal libraries live under `Aegis Unity Universal\Libraries`.
- Runtime-generated or dumped metadata is marked as generated for GitHub language stats.
- Generated Unity dump headers are not required for the current checked-in build path.

## Usage Notice

Use this only with software you own, control, or are explicitly authorized to inspect or modify. The external prototype is intended for read-only diagnostics and authorized development/testing workflows.
