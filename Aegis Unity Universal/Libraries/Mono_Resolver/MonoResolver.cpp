#include "MonoResolver.hpp"

#include <array>

namespace Mono
{
	Api Functions;
	HMODULE Module = nullptr;

	namespace
	{
		template <typename T>
		bool Resolve(T& out, const char* name)
		{
			out = reinterpret_cast<T>(GetProcAddress(Module, name));
			return out != nullptr;
		}

		HMODULE FindLoadedMonoModule()
		{
			constexpr std::array<const char*, 3> moduleNames = {
				"mono-2.0-bdwgc.dll",
				"mono-2.0-sgen.dll",
				"mono.dll",
			};

			for (const char* moduleName : moduleNames) {
				if (HMODULE module = GetModuleHandleA(moduleName)) {
					return module;
				}
			}

			return nullptr;
		}
	}

	bool Initialize(InitializeMode mode)
	{
		if (IsInitialized())
			return true;

		Module = FindLoadedMonoModule();
		if (!Module && mode == InitializeMode::AllowSystemMonoFallback) {
			Module = LoadLibraryA("C:\\Program Files\\Mono\\bin\\mono-2.0-sgen.dll");
		}

		if (!Module)
			return false;

		bool ok = true;
		ok &= Resolve(Functions.get_root_domain, "mono_get_root_domain");
		ok &= Resolve(Functions.domain_get, "mono_domain_get");
		ok &= Resolve(Functions.thread_attach, "mono_thread_attach");
		ok &= Resolve(Functions.thread_detach, "mono_thread_detach");
		ok &= Resolve(Functions.domain_assembly_open, "mono_domain_assembly_open");
		ok &= Resolve(Functions.assembly_foreach, "mono_assembly_foreach");
		ok &= Resolve(Functions.assembly_get_image, "mono_assembly_get_image");
		ok &= Resolve(Functions.image_get_name, "mono_image_get_name");
		ok &= Resolve(Functions.image_get_filename, "mono_image_get_filename");
		ok &= Resolve(Functions.class_from_name, "mono_class_from_name");
		ok &= Resolve(Functions.class_get_method_from_name, "mono_class_get_method_from_name");
		ok &= Resolve(Functions.compile_method, "mono_compile_method");
		ok &= Resolve(Functions.runtime_invoke, "mono_runtime_invoke");
		ok &= Resolve(Functions.string_new, "mono_string_new");
		ok &= Resolve(Functions.object_unbox, "mono_object_unbox");
		ok &= Resolve(Functions.class_get_field_from_name, "mono_class_get_field_from_name");
		ok &= Resolve(Functions.field_get_offset, "mono_field_get_offset");
		ok &= Resolve(Functions.class_vtable, "mono_class_vtable");
		ok &= Resolve(Functions.field_static_get_value, "mono_field_static_get_value");
		ok &= Resolve(Functions.class_get_type, "mono_class_get_type");
		ok &= Resolve(Functions.type_get_object, "mono_type_get_object");
		ok &= Resolve(Functions.array_length, "mono_array_length");
		ok &= Resolve(Functions.array_addr_with_size, "mono_array_addr_with_size");
		ok &= Resolve(Functions.string_to_utf8, "mono_string_to_utf8");

		Resolve(Functions.field_static_set_value, "mono_field_static_set_value");
		Resolve(Functions.free, "mono_free");
		Resolve(Functions.method_desc_new, "mono_method_desc_new");
		Resolve(Functions.method_desc_search_in_class, "mono_method_desc_search_in_class");
		Resolve(Functions.method_desc_free, "mono_method_desc_free");

		if (!ok) {
			Functions = {};
			Module = nullptr;
			return false;
		}

		return true;
	}

	bool IsInitialized()
	{
		return Module != nullptr && Functions.get_root_domain != nullptr;
	}

	Domain* GetDomain()
	{
		if (!IsInitialized())
			return nullptr;

		if (Domain* currentDomain = Functions.domain_get()) {
			return currentDomain;
		}

		return Functions.get_root_domain();
	}

	Thread* Attach()
	{
		Domain* domain = GetDomain();
		return domain ? Functions.thread_attach(domain) : nullptr;
	}

	void Detach(Thread* thread)
	{
		if (thread && Functions.thread_detach) {
			Functions.thread_detach(thread);
		}
	}

	Assembly* OpenAssembly(const char* assemblyPath)
	{
		Domain* domain = GetDomain();
		return domain && assemblyPath ? Functions.domain_assembly_open(domain, assemblyPath) : nullptr;
	}

	void ForEachAssembly(void(__cdecl* callback)(Assembly*, void*), void* userData)
	{
		if (callback && Functions.assembly_foreach) {
			Functions.assembly_foreach(callback, userData);
		}
	}

	Image* GetImage(Assembly* assembly)
	{
		return assembly ? Functions.assembly_get_image(assembly) : nullptr;
	}

	const char* GetImageName(Image* image)
	{
		return image && Functions.image_get_name ? Functions.image_get_name(image) : nullptr;
	}

	const char* GetImageFilename(Image* image)
	{
		return image && Functions.image_get_filename ? Functions.image_get_filename(image) : nullptr;
	}

	Class* FindClass(Image* image, const char* nameSpace, const char* className)
	{
		return image && className ? Functions.class_from_name(image, nameSpace ? nameSpace : "", className) : nullptr;
	}

	Method* FindMethod(Class* klass, const char* methodName, int argumentCount)
	{
		return klass && methodName ? Functions.class_get_method_from_name(klass, methodName, argumentCount) : nullptr;
	}

	void* CompileMethod(Method* method)
	{
		return method ? Functions.compile_method(method) : nullptr;
	}

	Object* Invoke(Method* method, void* instance, void** params, Object** exception)
	{
		return method ? Functions.runtime_invoke(method, instance, params, exception) : nullptr;
	}

	String* NewString(const char* value)
	{
		Domain* domain = GetDomain();
		return domain && value ? Functions.string_new(domain, value) : nullptr;
	}

	Field* FindField(Class* klass, const char* fieldName)
	{
		return klass && fieldName ? Functions.class_get_field_from_name(klass, fieldName) : nullptr;
	}

	Method* FindMethodByDesc(Class* klass, const char* description)
	{
		if (!klass || !description || !Functions.method_desc_new || !Functions.method_desc_search_in_class)
			return nullptr;

		MethodDesc* desc = Functions.method_desc_new(description, 1);
		if (!desc)
			return nullptr;

		Method* method = Functions.method_desc_search_in_class(desc, klass);
		if (Functions.method_desc_free)
			Functions.method_desc_free(desc);
		return method;
	}

	int GetFieldOffset(Field* field)
	{
		return field ? Functions.field_get_offset(field) : -1;
	}

	bool GetStaticFieldValue(Class* klass, Field* field, void* outValue)
	{
		Domain* domain = GetDomain();
		if (!domain || !klass || !field || !outValue)
			return false;

		VTable* vtable = Functions.class_vtable(domain, klass);
		if (!vtable)
			return false;

		Functions.field_static_get_value(vtable, field, outValue);
		return true;
	}

	bool SetStaticFieldValue(Class* klass, Field* field, void* value)
	{
		Domain* domain = GetDomain();
		if (!domain || !klass || !field || !value || !Functions.field_static_set_value)
			return false;

		VTable* vtable = Functions.class_vtable(domain, klass);
		if (!vtable)
			return false;

		Functions.field_static_set_value(vtable, field, value);
		return true;
	}

	Type* GetClassType(Class* klass)
	{
		return klass && Functions.class_get_type ? Functions.class_get_type(klass) : nullptr;
	}

	Object* GetTypeObject(Type* type)
	{
		Domain* domain = GetDomain();
		return domain && type && Functions.type_get_object ? Functions.type_get_object(domain, type) : nullptr;
	}

	uintptr_t GetArrayLength(Array* array)
	{
		return array && Functions.array_length ? Functions.array_length(array) : 0;
	}

	Object* GetArrayObject(Array* array, uintptr_t index)
	{
		if (!array || !Functions.array_addr_with_size)
			return nullptr;

		char* slot = Functions.array_addr_with_size(array, static_cast<int>(sizeof(void*)), index);
		return slot ? *reinterpret_cast<Object**>(slot) : nullptr;
	}

	std::string StringToUtf8(String* value)
	{
		if (!value || !Functions.string_to_utf8)
			return {};

		char* text = Functions.string_to_utf8(value);
		if (!text)
			return {};

		std::string result = text;
		if (Functions.free)
			Functions.free(text);
		return result;
	}
}
