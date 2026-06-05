#pragma once

#include "ExternalProcess.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aegis::UnityExternal
{
    enum class MethodMapEntryKind
    {
        NativeRva,
        MonoMetadataToken
    };

    struct ResolvedAddress
    {
        uintptr_t address = 0;
        std::uint32_t rva = 0;
        std::uint32_t metadataToken = 0;
        bool hasAddress = true;
        bool hasRva = true;
        bool hasMetadataToken = false;
        std::wstring moduleName;
        std::string source;
        std::string detail;
    };

    struct ResolveError
    {
        std::string message;
    };

    struct ResolveResult
    {
        std::optional<ResolvedAddress> value;
        std::optional<ResolveError> error;

        static ResolveResult Success(ResolvedAddress resolved);
        static ResolveResult Failure(std::string message);
    };

    struct MethodQuery
    {
        std::string imageName;
        std::string className;
        std::string methodName;
        int argumentCount = -1;
    };

    class MethodMap
    {
    public:
        struct Entry
        {
            std::string imageName;
            std::string className;
            std::string methodName;
            int argumentCount = -1;
            std::uint32_t rva = 0;
            std::uint32_t metadataToken = 0;
            MethodMapEntryKind kind = MethodMapEntryKind::NativeRva;
            std::string sourcePath;
        };

        bool Load(const std::wstring& path, std::string* errorMessage = nullptr);
        void SetEntries(std::vector<Entry> entries);
        bool IsLoaded() const;
        std::size_t Count() const;
        const std::vector<Entry>& Entries() const;
        std::optional<ResolvedAddress> Find(const MethodQuery& query, const ModuleInfo* module) const;

    private:
        std::vector<Entry> entries_;
    };

    class ExternalMethodResolver
    {
    public:
        explicit ExternalMethodResolver(UnityRuntimeModules modules);

        void SetMethodMap(MethodMap methodMap);
        RuntimeBackend Backend() const;
        bool HasMethodMap() const;

        ResolveResult ResolveRuntimeExport(const std::string& exportName) const;
        ResolveResult ResolveMethod(const MethodQuery& query) const;

    private:
        const ModuleInfo* RuntimeModule() const;
        ResolveResult ResolveExportFromModule(const ModuleInfo& module, const std::string& exportName) const;

        UnityRuntimeModules modules_;
        std::optional<MethodMap> methodMap_;
        mutable std::wstring exportCachePath_;
        mutable std::optional<std::unordered_map<std::string, std::uint32_t>> exportCache_;
    };
}
