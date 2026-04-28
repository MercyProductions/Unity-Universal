#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

namespace Mono
{
	struct Domain;
	struct Assembly;
	struct Image;
	struct Class;
	struct Method;
	struct Object;
	struct String;
	struct Thread;
	struct Field;
	struct VTable;
	struct Type;
	struct Array;
	struct MethodDesc;

	struct Api
	{
		using mono_get_root_domain_t = Domain* (__cdecl*)();
		using mono_domain_get_t = Domain* (__cdecl*)();
		using mono_thread_attach_t = Thread* (__cdecl*)(Domain*);
		using mono_thread_detach_t = void(__cdecl*)(Thread*);
		using mono_domain_assembly_open_t = Assembly* (__cdecl*)(Domain*, const char*);
		using mono_assembly_foreach_t = void(__cdecl*)(void(__cdecl*)(Assembly*, void*), void*);
		using mono_assembly_get_image_t = Image* (__cdecl*)(Assembly*);
		using mono_image_get_name_t = const char* (__cdecl*)(Image*);
		using mono_image_get_filename_t = const char* (__cdecl*)(Image*);
		using mono_class_from_name_t = Class* (__cdecl*)(Image*, const char*, const char*);
		using mono_class_get_method_from_name_t = Method* (__cdecl*)(Class*, const char*, int);
		using mono_compile_method_t = void* (__cdecl*)(Method*);
		using mono_runtime_invoke_t = Object* (__cdecl*)(Method*, void*, void**, Object**);
		using mono_string_new_t = String* (__cdecl*)(Domain*, const char*);
		using mono_object_unbox_t = void* (__cdecl*)(Object*);
		using mono_class_get_field_from_name_t = Field* (__cdecl*)(Class*, const char*);
		using mono_field_get_offset_t = int(__cdecl*)(Field*);
		using mono_class_vtable_t = VTable* (__cdecl*)(Domain*, Class*);
		using mono_field_static_get_value_t = void(__cdecl*)(VTable*, Field*, void*);
		using mono_field_static_set_value_t = void(__cdecl*)(VTable*, Field*, void*);
		using mono_class_get_type_t = Type* (__cdecl*)(Class*);
		using mono_type_get_object_t = Object* (__cdecl*)(Domain*, Type*);
		using mono_array_length_t = uintptr_t(__cdecl*)(Array*);
		using mono_array_addr_with_size_t = char* (__cdecl*)(Array*, int, uintptr_t);
		using mono_string_to_utf8_t = char* (__cdecl*)(String*);
		using mono_free_t = void(__cdecl*)(void*);
		using mono_method_desc_new_t = MethodDesc* (__cdecl*)(const char*, int);
		using mono_method_desc_search_in_class_t = Method* (__cdecl*)(MethodDesc*, Class*);
		using mono_method_desc_free_t = void(__cdecl*)(MethodDesc*);

		mono_get_root_domain_t get_root_domain = nullptr;
		mono_domain_get_t domain_get = nullptr;
		mono_thread_attach_t thread_attach = nullptr;
		mono_thread_detach_t thread_detach = nullptr;
		mono_domain_assembly_open_t domain_assembly_open = nullptr;
		mono_assembly_foreach_t assembly_foreach = nullptr;
		mono_assembly_get_image_t assembly_get_image = nullptr;
		mono_image_get_name_t image_get_name = nullptr;
		mono_image_get_filename_t image_get_filename = nullptr;
		mono_class_from_name_t class_from_name = nullptr;
		mono_class_get_method_from_name_t class_get_method_from_name = nullptr;
		mono_compile_method_t compile_method = nullptr;
		mono_runtime_invoke_t runtime_invoke = nullptr;
		mono_string_new_t string_new = nullptr;
		mono_object_unbox_t object_unbox = nullptr;
		mono_class_get_field_from_name_t class_get_field_from_name = nullptr;
		mono_field_get_offset_t field_get_offset = nullptr;
		mono_class_vtable_t class_vtable = nullptr;
		mono_field_static_get_value_t field_static_get_value = nullptr;
		mono_field_static_set_value_t field_static_set_value = nullptr;
		mono_class_get_type_t class_get_type = nullptr;
		mono_type_get_object_t type_get_object = nullptr;
		mono_array_length_t array_length = nullptr;
		mono_array_addr_with_size_t array_addr_with_size = nullptr;
		mono_string_to_utf8_t string_to_utf8 = nullptr;
		mono_free_t free = nullptr;
		mono_method_desc_new_t method_desc_new = nullptr;
		mono_method_desc_search_in_class_t method_desc_search_in_class = nullptr;
		mono_method_desc_free_t method_desc_free = nullptr;
	};

	enum class InitializeMode
	{
		LoadedModuleOnly,
		AllowSystemMonoFallback
	};

	extern Api Functions;
	extern HMODULE Module;

	bool Initialize(InitializeMode mode = InitializeMode::LoadedModuleOnly);
	bool IsInitialized();

	Domain* GetDomain();
	Thread* Attach();
	void Detach(Thread* thread);

	Assembly* OpenAssembly(const char* assemblyPath);
	void ForEachAssembly(void(__cdecl* callback)(Assembly*, void*), void* userData);
	Image* GetImage(Assembly* assembly);
	const char* GetImageName(Image* image);
	const char* GetImageFilename(Image* image);
	Class* FindClass(Image* image, const char* nameSpace, const char* className);
	Method* FindMethod(Class* klass, const char* methodName, int argumentCount = -1);
	void* CompileMethod(Method* method);
	Object* Invoke(Method* method, void* instance = nullptr, void** params = nullptr, Object** exception = nullptr);
	String* NewString(const char* value);
	Field* FindField(Class* klass, const char* fieldName);
	Method* FindMethodByDesc(Class* klass, const char* description);
	int GetFieldOffset(Field* field);
	bool GetStaticFieldValue(Class* klass, Field* field, void* outValue);
	bool SetStaticFieldValue(Class* klass, Field* field, void* value);
	Type* GetClassType(Class* klass);
	Object* GetTypeObject(Type* type);
	uintptr_t GetArrayLength(Array* array);
	Object* GetArrayObject(Array* array, uintptr_t index);
	std::string StringToUtf8(String* value);
}
