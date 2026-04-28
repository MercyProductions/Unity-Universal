#include "UnityRuntime.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <Libraries/Il2cpp_Resolver/il2cpp_resolver.hpp>
#include <Utils/SDK.h>

namespace UnityRuntime
{
	namespace
	{
		Backend g_backend = Backend::Unknown;
		bool g_initialized = false;

		struct MonoClassName
		{
			std::string nameSpace;
			std::string className;
		};

		std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
				return static_cast<char>(std::tolower(ch));
			});
			return value;
		}

		bool ContainsIgnoreCase(const char* text, const char* needle)
		{
			if (!text || !needle || !needle[0])
				return true;

			return ToLower(text).find(ToLower(needle)) != std::string::npos;
		}

		MonoClassName SplitMonoClassName(const char* fullName)
		{
			if (!fullName)
				return {};

			std::string value = fullName;
			const size_t separator = value.rfind('.');
			if (separator == std::string::npos) {
				return { "", value };
			}

			return { value.substr(0, separator), value.substr(separator + 1) };
		}

		struct MonoImageSearch
		{
			const char* imageName = nullptr;
			const char* className = nullptr;
			Mono::Image* image = nullptr;
			Mono::Class* klass = nullptr;
		};

		void __cdecl FindMonoImageCallback(Mono::Assembly* assembly, void* userData)
		{
			auto* search = static_cast<MonoImageSearch*>(userData);
			if (!search || search->image)
				return;

			Mono::Image* image = Mono::GetImage(assembly);
			if (!image)
				return;

			if (!search->imageName ||
				ContainsIgnoreCase(Mono::GetImageName(image), search->imageName) ||
				ContainsIgnoreCase(Mono::GetImageFilename(image), search->imageName)) {
				search->image = image;
			}
		}

		void __cdecl FindMonoClassCallback(Mono::Assembly* assembly, void* userData)
		{
			auto* search = static_cast<MonoImageSearch*>(userData);
			if (!search || search->klass)
				return;

			Mono::Image* image = Mono::GetImage(assembly);
			if (!image)
				return;

			if (search->imageName &&
				!ContainsIgnoreCase(Mono::GetImageName(image), search->imageName) &&
				!ContainsIgnoreCase(Mono::GetImageFilename(image), search->imageName)) {
				return;
			}

			const MonoClassName className = SplitMonoClassName(search->className);
			search->klass = Mono::FindClass(image, className.nameSpace.c_str(), className.className.c_str());
		}
	}

	bool Initialize()
	{
		if (g_initialized)
			return true;

		SDK::Base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
		SDK::UnityPlayer = reinterpret_cast<uintptr_t>(GetModuleHandleA("UnityPlayer.dll"));
		SDK::GameAssembly = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));

		if (SDK::GameAssembly && IL2CPP::Initialize(false)) {
			g_backend = Backend::IL2CPP;
			g_initialized = true;
			return true;
		}

		if (Mono::Initialize(Mono::InitializeMode::LoadedModuleOnly)) {
			SDK::MonoModule = reinterpret_cast<uintptr_t>(Mono::Module);
			g_backend = Backend::Mono;
			g_initialized = true;
			return true;
		}

		return false;
	}

	bool IsInitialized()
	{
		return g_initialized;
	}

	void Shutdown()
	{
		g_backend = Backend::Unknown;
		g_initialized = false;
	}

	Backend GetBackend()
	{
		return g_backend;
	}

	const char* GetBackendName()
	{
		switch (g_backend) {
		case Backend::IL2CPP:
			return "IL2CPP";
		case Backend::Mono:
			return "Mono";
		default:
			return "Unknown";
		}
	}

	bool IsIL2CPP()
	{
		return g_backend == Backend::IL2CPP;
	}

	bool IsMono()
	{
		return g_backend == Backend::Mono;
	}

	ScopedThread::ScopedThread()
	{
		if (IsIL2CPP()) {
			il2cppThread_ = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
		}
		else if (IsMono()) {
			monoThread_ = Mono::Attach();
		}
	}

	ScopedThread::~ScopedThread()
	{
		if (il2cppThread_) {
			IL2CPP::Thread::Detach(il2cppThread_);
		}

		if (monoThread_) {
			Mono::Detach(monoThread_);
		}
	}

	bool ScopedThread::IsAttached() const
	{
		return IsIL2CPP() ? il2cppThread_ != nullptr : !IsMono() || monoThread_ != nullptr;
	}

	void* ResolveMethod(const char* className, const char* methodName, int argumentCount)
	{
		return ResolveMethod(nullptr, className, methodName, argumentCount);
	}

	void* ResolveMethod(const char* imageName, const char* className, const char* methodName, int argumentCount)
	{
		if (!className || !methodName)
			return nullptr;

		if (IsIL2CPP()) {
			Unity::il2cppClass* klass = IL2CPP::Class::Find(className);
			return klass ? IL2CPP::Class::Utils::GetMethodPointer(klass, methodName, argumentCount) : nullptr;
		}

		return nullptr;
	}

	Mono::Method* ResolveMonoMethod(const char* imageName, const char* className, const char* methodName, int argumentCount)
	{
		if (!IsMono() || !className || !methodName)
			return nullptr;

		Mono::Class* klass = ResolveMonoClass(imageName, className);
		if (!klass)
			return nullptr;

		return Mono::FindMethod(klass, methodName, argumentCount);
	}

	Mono::Class* ResolveMonoClass(const char* imageName, const char* className)
	{
		if (!IsMono() || !className)
			return nullptr;

		MonoImageSearch search = {};
		search.imageName = imageName;
		search.className = className;
		Mono::ForEachAssembly(FindMonoClassCallback, &search);
		return search.klass;
	}

	Mono::Method* ResolveMonoMethodDesc(const char* imageName, const char* className, const char* description)
	{
		Mono::Class* klass = ResolveMonoClass(imageName, className);
		return klass ? Mono::FindMethodByDesc(klass, description) : nullptr;
	}
}
