#pragma once

#include <Windows.h>
#include <cstdint>

#include <Libraries/Mono_Resolver/MonoResolver.hpp>

namespace UnityRuntime
{
	enum class Backend
	{
		Unknown,
		IL2CPP,
		Mono
	};

	bool Initialize();
	bool IsInitialized();
	void Shutdown();

	Backend GetBackend();
	const char* GetBackendName();
	bool IsIL2CPP();
	bool IsMono();

	class ScopedThread
	{
	public:
		ScopedThread();
		~ScopedThread();

		ScopedThread(const ScopedThread&) = delete;
		ScopedThread& operator=(const ScopedThread&) = delete;

		bool IsAttached() const;

	private:
		void* il2cppThread_ = nullptr;
		Mono::Thread* monoThread_ = nullptr;
	};

	void* ResolveMethod(const char* className, const char* methodName, int argumentCount = -1);
	void* ResolveMethod(const char* imageName, const char* className, const char* methodName, int argumentCount);
	Mono::Class* ResolveMonoClass(const char* imageName, const char* className);
	Mono::Method* ResolveMonoMethod(const char* imageName, const char* className, const char* methodName, int argumentCount = -1);
	Mono::Method* ResolveMonoMethodDesc(const char* imageName, const char* className, const char* description);
}
