#pragma once

// includes
#include <vector>
#include <utility> // for std::pair

// my includes
#include <DumpedFiles/il2cpp.h>
#include <Libraries/Il2cpp_Resolver/il2cpp_resolver.hpp>
#include <Libraries/Vectors/vec2.h>
#include <Libraries/Vectors/vec.h>



namespace mem
{
	template<typename T> T read(uintptr_t address) {
		try { return *(T*)address; }
		catch (...) { return T(); }
	}

	template<typename T> void write(uintptr_t address, T value) {
		try { *(T*)address = value; }
		catch (...) { return; }
	}
}

namespace SDK
{
	inline uintptr_t Base = 0;
	inline uintptr_t GameAssembly = 0;
	inline uintptr_t UnityPlayer = 0;
	inline uintptr_t MonoModule = 0;

}
