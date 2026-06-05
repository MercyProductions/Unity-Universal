#include "ExternalIl2CppMapGenerator.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Aegis::UnityExternal
{
    namespace
    {
        struct ByteView
        {
            const std::vector<std::uint8_t>* bytes = nullptr;

            template <typename T>
            std::optional<T> Read(std::size_t offset) const
            {
                if (!bytes || offset > bytes->size() || sizeof(T) > bytes->size() - offset)
                {
                    return std::nullopt;
                }

                T value{};
                std::memcpy(&value, bytes->data() + offset, sizeof(T));
                return value;
            }
        };

        struct MetadataRange
        {
            std::uint32_t offset = 0;
            std::uint32_t size = 0;
            std::uint32_t count = 0;
        };

        struct MetadataLayout
        {
            std::int32_t version = 0;
            MetadataRange strings;
            MetadataRange methods;
            MetadataRange typeDefinitions;
            MetadataRange images;
            std::uint32_t methodStride = 0;
            std::uint32_t typeDefinitionStride = 0;
            std::uint32_t imageStride = 0;
            std::uint32_t methodTokenOffset = 0;
            std::uint32_t methodParameterCountOffset = 0;
            std::uint32_t typeMethodStartOffset = 0;
            std::uint32_t typeMethodCountOffset = 0;
        };

        struct MetadataImage
        {
            std::string name;
            std::uint32_t typeStart = 0;
            std::uint32_t typeCount = 0;
            std::uint32_t maxMethodRid = 0;
        };

        struct MetadataType
        {
            std::string className;
            std::uint32_t methodStart = 0;
            std::uint16_t methodCount = 0;
        };

        struct MetadataMethod
        {
            std::string name;
            std::uint32_t token = 0;
            std::uint16_t parameterCount = 0;
        };

        struct PeSection
        {
            std::string name;
            std::uint32_t virtualAddress = 0;
            std::uint32_t virtualSize = 0;
            std::uint32_t rawPointer = 0;
            std::uint32_t rawSize = 0;
            std::uint32_t characteristics = 0;
        };

        struct PeImage
        {
            std::vector<std::uint8_t> bytes;
            std::uint64_t imageBase = 0;
            std::vector<PeSection> sections;

            std::optional<std::size_t> VaToFileOffset(std::uint64_t va) const
            {
                if (va < imageBase)
                {
                    return std::nullopt;
                }

                const std::uint64_t rva64 = va - imageBase;
                if (rva64 > std::numeric_limits<std::uint32_t>::max())
                {
                    return std::nullopt;
                }

                const std::uint32_t rva = static_cast<std::uint32_t>(rva64);
                for (const PeSection& section : sections)
                {
                    const std::uint32_t span = std::max(section.virtualSize, section.rawSize);
                    if (rva >= section.virtualAddress && rva < section.virtualAddress + span)
                    {
                        const std::uint64_t fileOffset =
                            static_cast<std::uint64_t>(section.rawPointer) + (rva - section.virtualAddress);
                        if (fileOffset < bytes.size())
                        {
                            return static_cast<std::size_t>(fileOffset);
                        }
                    }
                }

                return std::nullopt;
            }

            std::optional<std::uint64_t> FileOffsetToVa(std::size_t fileOffset) const
            {
                for (const PeSection& section : sections)
                {
                    if (fileOffset >= section.rawPointer &&
                        fileOffset < static_cast<std::size_t>(section.rawPointer) + section.rawSize)
                    {
                        return imageBase + section.virtualAddress + (fileOffset - section.rawPointer);
                    }
                }

                return std::nullopt;
            }

            bool IsExecutableVa(std::uint64_t va) const
            {
                if (va < imageBase)
                {
                    return false;
                }

                const std::uint64_t rva64 = va - imageBase;
                if (rva64 > std::numeric_limits<std::uint32_t>::max())
                {
                    return false;
                }

                const std::uint32_t rva = static_cast<std::uint32_t>(rva64);
                for (const PeSection& section : sections)
                {
                    const std::uint32_t span = std::max(section.virtualSize, section.rawSize);
                    if (rva >= section.virtualAddress && rva < section.virtualAddress + span)
                    {
                        return (section.characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
                    }
                }

                return false;
            }
        };

        struct CodeGenModuleInfo
        {
            std::uint64_t address = 0;
            std::uint32_t methodPointerCount = 0;
            std::uint64_t methodPointers = 0;
        };

        bool ReadFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>* bytes)
        {
            if (!bytes)
            {
                return false;
            }

            std::ifstream file{ path, std::ios::binary };
            if (!file)
            {
                return false;
            }

            bytes->assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            return !bytes->empty();
        }

        bool FileExists(const std::filesystem::path& path)
        {
            std::error_code error;
            return std::filesystem::exists(path, error) && std::filesystem::is_regular_file(path, error);
        }

        std::string WideToUtf8(const std::wstring& value)
        {
            if (value.empty())
            {
                return {};
            }

            const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (size <= 1)
            {
                return {};
            }

            std::string result(static_cast<std::size_t>(size - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
            return result;
        }

        std::string StripDllExtension(std::string value)
        {
            if (value.size() > 4)
            {
                std::string suffix = value.substr(value.size() - 4);
                std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
                if (suffix == ".dll")
                {
                    value.resize(value.size() - 4);
                }
            }
            return value;
        }

        std::string MakeClassName(std::string namespaceName, std::string typeName)
        {
            std::replace(typeName.begin(), typeName.end(), '/', '.');
            std::replace(typeName.begin(), typeName.end(), '+', '.');
            if (namespaceName.empty())
            {
                return typeName;
            }
            return namespaceName + "." + typeName;
        }

        std::optional<std::string> ReadMetadataString(
            const std::vector<std::uint8_t>& metadata,
            const MetadataLayout& layout,
            std::uint32_t stringIndex)
        {
            if (stringIndex >= layout.strings.size)
            {
                return std::nullopt;
            }

            const std::size_t start = static_cast<std::size_t>(layout.strings.offset) + stringIndex;
            if (start >= metadata.size())
            {
                return std::nullopt;
            }

            std::size_t end = start;
            while (end < metadata.size() && metadata[end] != 0)
            {
                ++end;
            }

            return std::string(
                reinterpret_cast<const char*>(metadata.data() + start),
                reinterpret_cast<const char*>(metadata.data() + end));
        }

        bool RangeIsValid(const std::vector<std::uint8_t>& bytes, const MetadataRange& range)
        {
            return range.offset <= bytes.size() && range.size <= bytes.size() - range.offset;
        }

        MetadataRange ReadOldRange(const ByteView& view, std::size_t pairIndex)
        {
            const std::size_t offset = 8 + (pairIndex * 8);
            MetadataRange range;
            range.offset = view.Read<std::uint32_t>(offset).value_or(0);
            range.size = view.Read<std::uint32_t>(offset + 4).value_or(0);
            return range;
        }

        bool ParseMetadataLayout(const std::vector<std::uint8_t>& bytes, MetadataLayout* layout, std::string* error)
        {
            if (!layout || bytes.size() < 0xB0)
            {
                if (error)
                {
                    *error = "global-metadata.dat is too small.";
                }
                return false;
            }

            const ByteView view{ &bytes };
            const std::uint32_t sanity = view.Read<std::uint32_t>(0).value_or(0);
            if (sanity != 0xFAB11BAF)
            {
                if (error)
                {
                    *error = "global-metadata.dat has an invalid IL2CPP sanity value.";
                }
                return false;
            }

            layout->version = view.Read<std::int32_t>(4).value_or(0);
            if (layout->version < 24 || layout->version > 39)
            {
                if (error)
                {
                    *error = "Unsupported IL2CPP metadata version.";
                }
                return false;
            }

            // Unity metadata versions through the common v24-v31 range use offset/size pairs.
            layout->strings = ReadOldRange(view, 2);
            layout->methods = ReadOldRange(view, 5);
            layout->typeDefinitions = ReadOldRange(view, 19);
            layout->images = ReadOldRange(view, 20);

            if (!RangeIsValid(bytes, layout->strings) ||
                !RangeIsValid(bytes, layout->methods) ||
                !RangeIsValid(bytes, layout->typeDefinitions) ||
                !RangeIsValid(bytes, layout->images))
            {
                if (error)
                {
                    *error = "IL2CPP metadata table ranges are invalid.";
                }
                return false;
            }

            if (layout->methods.size % 36 == 0)
            {
                layout->methodStride = 36;
                layout->methodTokenOffset = 24;
                layout->methodParameterCountOffset = 34;
            }
            else if (layout->methods.size % 32 == 0)
            {
                layout->methodStride = 32;
                layout->methodTokenOffset = 20;
                layout->methodParameterCountOffset = 30;
            }
            else
            {
                if (error)
                {
                    *error = "Unsupported IL2CPP method definition row size.";
                }
                return false;
            }

            if (layout->typeDefinitions.size % 88 == 0)
            {
                layout->typeDefinitionStride = 88;
                layout->typeMethodStartOffset = 36;
                layout->typeMethodCountOffset = 64;
            }
            else if (layout->typeDefinitions.size % 84 == 0)
            {
                layout->typeDefinitionStride = 84;
                layout->typeMethodStartOffset = 32;
                layout->typeMethodCountOffset = 60;
            }
            else
            {
                if (error)
                {
                    *error = "Unsupported IL2CPP type definition row size.";
                }
                return false;
            }

            if (layout->images.size % 40 != 0)
            {
                if (error)
                {
                    *error = "Unsupported IL2CPP image definition row size.";
                }
                return false;
            }

            layout->imageStride = 40;
            layout->methods.count = layout->methods.size / layout->methodStride;
            layout->typeDefinitions.count = layout->typeDefinitions.size / layout->typeDefinitionStride;
            layout->images.count = layout->images.size / layout->imageStride;
            return true;
        }

        std::optional<MetadataImage> ReadImage(
            const std::vector<std::uint8_t>& bytes,
            const MetadataLayout& layout,
            std::uint32_t imageIndex)
        {
            if (imageIndex >= layout.images.count)
            {
                return std::nullopt;
            }

            const ByteView view{ &bytes };
            const std::size_t offset =
                static_cast<std::size_t>(layout.images.offset) + (static_cast<std::size_t>(imageIndex) * layout.imageStride);
            const std::uint32_t nameIndex = view.Read<std::uint32_t>(offset).value_or(0);
            const std::optional<std::string> name = ReadMetadataString(bytes, layout, nameIndex);
            if (!name)
            {
                return std::nullopt;
            }

            MetadataImage image;
            image.name = *name;
            image.typeStart = view.Read<std::uint32_t>(offset + 8).value_or(0);
            image.typeCount = view.Read<std::uint32_t>(offset + 12).value_or(0);
            return image;
        }

        std::optional<MetadataType> ReadType(
            const std::vector<std::uint8_t>& bytes,
            const MetadataLayout& layout,
            std::uint32_t typeIndex)
        {
            if (typeIndex >= layout.typeDefinitions.count)
            {
                return std::nullopt;
            }

            const ByteView view{ &bytes };
            const std::size_t offset =
                static_cast<std::size_t>(layout.typeDefinitions.offset) +
                (static_cast<std::size_t>(typeIndex) * layout.typeDefinitionStride);

            const std::uint32_t nameIndex = view.Read<std::uint32_t>(offset).value_or(0);
            const std::uint32_t namespaceIndex = view.Read<std::uint32_t>(offset + 4).value_or(0);
            const std::optional<std::string> name = ReadMetadataString(bytes, layout, nameIndex);
            const std::optional<std::string> namespaceName = ReadMetadataString(bytes, layout, namespaceIndex);
            if (!name || !namespaceName)
            {
                return std::nullopt;
            }

            MetadataType type;
            type.className = MakeClassName(*namespaceName, *name);
            type.methodStart = view.Read<std::uint32_t>(offset + layout.typeMethodStartOffset).value_or(0xFFFFFFFFu);
            type.methodCount = view.Read<std::uint16_t>(offset + layout.typeMethodCountOffset).value_or(0);
            return type;
        }

        std::optional<MetadataMethod> ReadMethod(
            const std::vector<std::uint8_t>& bytes,
            const MetadataLayout& layout,
            std::uint32_t methodIndex)
        {
            if (methodIndex >= layout.methods.count)
            {
                return std::nullopt;
            }

            const ByteView view{ &bytes };
            const std::size_t offset =
                static_cast<std::size_t>(layout.methods.offset) +
                (static_cast<std::size_t>(methodIndex) * layout.methodStride);

            const std::uint32_t nameIndex = view.Read<std::uint32_t>(offset).value_or(0);
            const std::optional<std::string> name = ReadMetadataString(bytes, layout, nameIndex);
            if (!name)
            {
                return std::nullopt;
            }

            MetadataMethod method;
            method.name = *name;
            method.token = view.Read<std::uint32_t>(offset + layout.methodTokenOffset).value_or(0);
            method.parameterCount = view.Read<std::uint16_t>(offset + layout.methodParameterCountOffset).value_or(0);
            return method;
        }

        bool LoadPeImage(const std::filesystem::path& path, PeImage* image, std::string* error)
        {
            if (!image || !ReadFileBytes(path, &image->bytes))
            {
                if (error)
                {
                    *error = "Unable to read GameAssembly.dll.";
                }
                return false;
            }

            const ByteView view{ &image->bytes };
            const std::optional<IMAGE_DOS_HEADER> dos = view.Read<IMAGE_DOS_HEADER>(0);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            {
                if (error)
                {
                    *error = "GameAssembly.dll has an invalid DOS header.";
                }
                return false;
            }

            const std::size_t ntOffset = static_cast<std::size_t>(dos->e_lfanew);
            const std::optional<DWORD> signature = view.Read<DWORD>(ntOffset);
            if (!signature || *signature != IMAGE_NT_SIGNATURE)
            {
                if (error)
                {
                    *error = "GameAssembly.dll has an invalid NT header.";
                }
                return false;
            }

            const std::optional<IMAGE_FILE_HEADER> fileHeader = view.Read<IMAGE_FILE_HEADER>(ntOffset + sizeof(DWORD));
            if (!fileHeader)
            {
                if (error)
                {
                    *error = "GameAssembly.dll is missing a PE file header.";
                }
                return false;
            }

            const std::size_t optionalHeaderOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
            const std::optional<WORD> optionalMagic = view.Read<WORD>(optionalHeaderOffset);
            if (!optionalMagic || *optionalMagic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                if (error)
                {
                    *error = "Only x64 GameAssembly.dll images are supported by auto-generation.";
                }
                return false;
            }

            const std::optional<IMAGE_OPTIONAL_HEADER64> optionalHeader =
                view.Read<IMAGE_OPTIONAL_HEADER64>(optionalHeaderOffset);
            if (!optionalHeader)
            {
                if (error)
                {
                    *error = "GameAssembly.dll is missing an x64 optional header.";
                }
                return false;
            }

            image->imageBase = optionalHeader->ImageBase;
            const std::size_t sectionOffset = optionalHeaderOffset + fileHeader->SizeOfOptionalHeader;
            image->sections.clear();
            image->sections.reserve(fileHeader->NumberOfSections);

            for (WORD index = 0; index < fileHeader->NumberOfSections; ++index)
            {
                const std::optional<IMAGE_SECTION_HEADER> header =
                    view.Read<IMAGE_SECTION_HEADER>(sectionOffset + (sizeof(IMAGE_SECTION_HEADER) * index));
                if (!header)
                {
                    if (error)
                    {
                        *error = "GameAssembly.dll has a truncated section table.";
                    }
                    return false;
                }

                char name[9] = {};
                std::memcpy(name, header->Name, 8);
                image->sections.push_back(PeSection{
                    name,
                    header->VirtualAddress,
                    header->Misc.VirtualSize,
                    header->PointerToRawData,
                    header->SizeOfRawData,
                    header->Characteristics
                });
            }

            return true;
        }

        std::unordered_map<std::string, std::vector<std::uint64_t>> CollectImageNameStrings(
            const PeImage& image,
            const std::unordered_set<std::string>& wantedNames)
        {
            std::unordered_map<std::string, std::vector<std::uint64_t>> strings;
            for (const PeSection& section : image.sections)
            {
                if ((section.characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 ||
                    section.rawPointer >= image.bytes.size())
                {
                    continue;
                }

                const std::size_t rawEnd = std::min<std::size_t>(
                    image.bytes.size(),
                    static_cast<std::size_t>(section.rawPointer) + section.rawSize);
                std::size_t offset = section.rawPointer;
                while (offset < rawEnd)
                {
                    if (image.bytes[offset] < 0x20 || image.bytes[offset] >= 0x7F)
                    {
                        ++offset;
                        continue;
                    }

                    const std::size_t start = offset;
                    while (offset < rawEnd &&
                        image.bytes[offset] >= 0x20 &&
                        image.bytes[offset] < 0x7F &&
                        offset - start < 300)
                    {
                        ++offset;
                    }

                    if (offset < rawEnd && image.bytes[offset] == 0 && offset - start >= 4)
                    {
                        const std::string value(
                            reinterpret_cast<const char*>(image.bytes.data() + start),
                            reinterpret_cast<const char*>(image.bytes.data() + offset));
                        if (wantedNames.find(value) != wantedNames.end())
                        {
                            const std::optional<std::uint64_t> va = image.FileOffsetToVa(start);
                            if (va)
                            {
                                strings[value].push_back(*va);
                            }
                        }
                        ++offset;
                    }
                    else
                    {
                        ++offset;
                    }
                }
            }

            return strings;
        }

        std::optional<std::uint32_t> ReadU32AtFile(const PeImage& image, std::size_t offset)
        {
            return ByteView{ &image.bytes }.Read<std::uint32_t>(offset);
        }

        std::optional<std::uint64_t> ReadU64AtFile(const PeImage& image, std::size_t offset)
        {
            return ByteView{ &image.bytes }.Read<std::uint64_t>(offset);
        }

        std::optional<std::uint64_t> ReadU64AtVa(const PeImage& image, std::uint64_t va)
        {
            const std::optional<std::size_t> offset = image.VaToFileOffset(va);
            if (!offset)
            {
                return std::nullopt;
            }
            return ReadU64AtFile(image, *offset);
        }

        bool MethodPointerArrayLooksValid(
            const PeImage& image,
            std::uint64_t methodPointers,
            std::uint32_t methodPointerCount)
        {
            if (methodPointerCount == 0)
            {
                return true;
            }

            const std::optional<std::size_t> arrayOffset = image.VaToFileOffset(methodPointers);
            if (!arrayOffset)
            {
                return false;
            }

            const std::uint32_t checkedCount = std::min<std::uint32_t>(methodPointerCount, 64);
            if (*arrayOffset > image.bytes.size() || checkedCount * sizeof(std::uint64_t) > image.bytes.size() - *arrayOffset)
            {
                return false;
            }

            std::uint32_t good = 0;
            for (std::uint32_t index = 0; index < checkedCount; ++index)
            {
                const std::uint64_t pointer =
                    ReadU64AtFile(image, *arrayOffset + (static_cast<std::size_t>(index) * sizeof(std::uint64_t))).value_or(0);
                if (pointer == 0 || image.IsExecutableVa(pointer))
                {
                    ++good;
                }
            }

            return good >= std::max<std::uint32_t>(1, checkedCount / 2);
        }

        std::unordered_map<std::string, CodeGenModuleInfo> FindCodeGenModules(
            const PeImage& image,
            const std::vector<MetadataImage>& metadataImages,
            const std::unordered_map<std::string, std::vector<std::uint64_t>>& stringVas)
        {
            std::unordered_map<std::uint64_t, std::string> nameByVa;
            for (const auto& [name, addresses] : stringVas)
            {
                for (std::uint64_t address : addresses)
                {
                    nameByVa[address] = name;
                }
            }

            std::unordered_map<std::string, std::uint32_t> requiredRidByName;
            for (const MetadataImage& metadataImage : metadataImages)
            {
                requiredRidByName[metadataImage.name] = metadataImage.maxMethodRid;
            }

            std::unordered_map<std::string, CodeGenModuleInfo> modules;
            for (const PeSection& section : image.sections)
            {
                if ((section.characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 ||
                    section.rawPointer >= image.bytes.size())
                {
                    continue;
                }

                const std::size_t rawEnd = std::min<std::size_t>(
                    image.bytes.size(),
                    static_cast<std::size_t>(section.rawPointer) + section.rawSize);
                for (std::size_t offset = section.rawPointer; offset + sizeof(std::uint64_t) <= rawEnd; offset += sizeof(std::uint64_t))
                {
                    const std::uint64_t value = ReadU64AtFile(image, offset).value_or(0);
                    const auto nameIt = nameByVa.find(value);
                    if (nameIt == nameByVa.end() || modules.find(nameIt->second) != modules.end())
                    {
                        continue;
                    }

                    const std::optional<std::uint64_t> candidateVa = image.FileOffsetToVa(offset);
                    const std::optional<std::uint32_t> methodPointerCount = ReadU32AtFile(image, offset + 8);
                    const std::optional<std::uint64_t> methodPointers = ReadU64AtFile(image, offset + 16);
                    if (!candidateVa || !methodPointerCount || !methodPointers)
                    {
                        continue;
                    }

                    const std::uint32_t requiredRid = requiredRidByName[nameIt->second];
                    if (*methodPointerCount < requiredRid || *methodPointerCount > 2'000'000)
                    {
                        continue;
                    }

                    if (!MethodPointerArrayLooksValid(image, *methodPointers, *methodPointerCount))
                    {
                        continue;
                    }

                    modules.emplace(nameIt->second, CodeGenModuleInfo{
                        *candidateVa,
                        *methodPointerCount,
                        *methodPointers
                    });
                }
            }

            return modules;
        }

        void AddMetadataCandidates(std::vector<std::filesystem::path>* candidates, const std::filesystem::path& baseDirectory, const std::wstring& executableStem)
        {
            if (!candidates || baseDirectory.empty())
            {
                return;
            }

            if (!executableStem.empty())
            {
                candidates->push_back(baseDirectory / (executableStem + L"_Data") / L"il2cpp_data" / L"Metadata" / L"global-metadata.dat");
            }

            std::error_code error;
            if (std::filesystem::exists(baseDirectory, error) && std::filesystem::is_directory(baseDirectory, error))
            {
                for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(baseDirectory, error))
                {
                    if (error)
                    {
                        break;
                    }
                    if (!entry.is_directory(error))
                    {
                        continue;
                    }

                    const std::wstring name = entry.path().filename().wstring();
                    if (name.size() > 5 && name.substr(name.size() - 5) == L"_Data")
                    {
                        candidates->push_back(entry.path() / L"il2cpp_data" / L"Metadata" / L"global-metadata.dat");
                    }
                }
            }
        }
    }

    std::optional<std::filesystem::path> FindIl2CppMetadataPath(const UnityProcess& process)
    {
        std::vector<std::filesystem::path> candidates;
        const std::filesystem::path executablePath(process.executablePath);
        const std::filesystem::path executableDirectory = executablePath.parent_path();
        const std::wstring executableStem = executablePath.stem().wstring();

        AddMetadataCandidates(&candidates, executableDirectory, executableStem);
        if (process.modules.gameAssembly)
        {
            AddMetadataCandidates(&candidates, std::filesystem::path(process.modules.gameAssembly->path).parent_path(), executableStem);
        }

        std::error_code error;
        for (const std::filesystem::path& candidate : candidates)
        {
            if (std::filesystem::exists(candidate, error) && std::filesystem::is_regular_file(candidate, error))
            {
                return candidate;
            }
        }

        return std::nullopt;
    }

    GeneratedIl2CppMethodMap GenerateIl2CppMethodMap(const UnityProcess& process)
    {
        GeneratedIl2CppMethodMap result;
        if (process.modules.Backend() != RuntimeBackend::IL2CPP || !process.modules.gameAssembly)
        {
            result.message = "Target is not an IL2CPP process with GameAssembly.dll.";
            return result;
        }

        const std::optional<std::filesystem::path> metadataPath = FindIl2CppMetadataPath(process);
        if (!metadataPath)
        {
            result.message = "global-metadata.dat was not found beside the target.";
            return result;
        }
        result.metadataPath = *metadataPath;

        std::vector<std::uint8_t> metadataBytes;
        if (!ReadFileBytes(*metadataPath, &metadataBytes))
        {
            result.message = "Unable to read global-metadata.dat.";
            return result;
        }

        MetadataLayout layout;
        std::string error;
        if (!ParseMetadataLayout(metadataBytes, &layout, &error))
        {
            result.message = error;
            return result;
        }

        PeImage gameAssembly;
        if (!LoadPeImage(process.modules.gameAssembly->path, &gameAssembly, &error))
        {
            result.message = error;
            return result;
        }

        std::vector<MetadataImage> images;
        images.reserve(layout.images.count);
        std::unordered_set<std::string> wantedImageNames;
        for (std::uint32_t imageIndex = 0; imageIndex < layout.images.count; ++imageIndex)
        {
            std::optional<MetadataImage> image = ReadImage(metadataBytes, layout, imageIndex);
            if (!image || image->name.empty())
            {
                continue;
            }

            for (std::uint32_t typeOffset = 0; typeOffset < image->typeCount; ++typeOffset)
            {
                std::optional<MetadataType> type = ReadType(metadataBytes, layout, image->typeStart + typeOffset);
                if (!type || type->methodStart == 0xFFFFFFFFu)
                {
                    continue;
                }

                for (std::uint16_t methodOffset = 0; methodOffset < type->methodCount; ++methodOffset)
                {
                    std::optional<MetadataMethod> method = ReadMethod(metadataBytes, layout, type->methodStart + methodOffset);
                    if (!method)
                    {
                        continue;
                    }

                    const std::uint32_t rid = method->token & 0x00FFFFFFu;
                    image->maxMethodRid = std::max(image->maxMethodRid, rid);
                }
            }

            wantedImageNames.insert(image->name);
            images.push_back(std::move(*image));
        }

        result.imageCount = images.size();
        if (images.empty())
        {
            result.message = "No IL2CPP metadata images were parsed.";
            return result;
        }

        const auto imageNameStrings = CollectImageNameStrings(gameAssembly, wantedImageNames);
        const auto codeGenModules = FindCodeGenModules(gameAssembly, images, imageNameStrings);
        result.matchedModules = codeGenModules.size();
        if (codeGenModules.empty())
        {
            result.message = "No IL2CPP codegen modules were found in GameAssembly.dll.";
            return result;
        }

        std::vector<MethodMap::Entry> entries;
        entries.reserve(layout.methods.count);

        for (const MetadataImage& image : images)
        {
            const auto moduleIt = codeGenModules.find(image.name);
            if (moduleIt == codeGenModules.end())
            {
                continue;
            }

            const CodeGenModuleInfo& module = moduleIt->second;
            for (std::uint32_t typeOffset = 0; typeOffset < image.typeCount; ++typeOffset)
            {
                std::optional<MetadataType> type = ReadType(metadataBytes, layout, image.typeStart + typeOffset);
                if (!type || type->methodStart == 0xFFFFFFFFu || type->className.empty())
                {
                    continue;
                }

                for (std::uint16_t methodOffset = 0; methodOffset < type->methodCount; ++methodOffset)
                {
                    std::optional<MetadataMethod> method = ReadMethod(metadataBytes, layout, type->methodStart + methodOffset);
                    if (!method || method->name.empty())
                    {
                        continue;
                    }

                    ++result.candidateMethods;
                    const std::uint32_t rid = method->token & 0x00FFFFFFu;
                    if (rid == 0 || rid > module.methodPointerCount)
                    {
                        continue;
                    }

                    const std::optional<std::uint64_t> pointer = ReadU64AtVa(
                        gameAssembly,
                        module.methodPointers + ((static_cast<std::uint64_t>(rid) - 1) * sizeof(std::uint64_t)));
                    if (!pointer || *pointer == 0 || !gameAssembly.IsExecutableVa(*pointer) || *pointer < gameAssembly.imageBase)
                    {
                        continue;
                    }

                    const std::uint64_t rva64 = *pointer - gameAssembly.imageBase;
                    if (rva64 > std::numeric_limits<std::uint32_t>::max())
                    {
                        continue;
                    }

                    MethodMap::Entry entry;
                    entry.imageName = StripDllExtension(image.name);
                    entry.className = type->className;
                    entry.methodName = method->name;
                    entry.argumentCount = method->parameterCount;
                    entry.rva = static_cast<std::uint32_t>(rva64);
                    entries.push_back(std::move(entry));
                    ++result.resolvedMethods;
                }
            }
        }

        if (entries.empty())
        {
            result.message = "IL2CPP metadata was parsed, but no method pointers resolved.";
            return result;
        }

        result.methodMap.SetEntries(std::move(entries));
        result.success = true;
        result.message = "Generated IL2CPP method map from global-metadata.dat and GameAssembly.dll.";
        return result;
    }
}
