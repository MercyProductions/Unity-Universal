#include "ExternalMonoMetadataGenerator.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace Aegis::UnityExternal
{
    namespace
    {
        constexpr int kTableModule = 0;
        constexpr int kTableTypeRef = 1;
        constexpr int kTableTypeDef = 2;
        constexpr int kTableField = 4;
        constexpr int kTableMethodDef = 6;
        constexpr int kTableParam = 8;
        constexpr int kTableInterfaceImpl = 9;
        constexpr int kTableMemberRef = 10;
        constexpr int kTableDeclSecurity = 14;
        constexpr int kTableEvent = 20;
        constexpr int kTableProperty = 23;
        constexpr int kTableModuleRef = 26;
        constexpr int kTableTypeSpec = 27;
        constexpr int kTableAssembly = 32;
        constexpr int kTableAssemblyRef = 35;
        constexpr int kTableFile = 38;
        constexpr int kTableExportedType = 39;
        constexpr int kTableManifestResource = 40;
        constexpr int kTableGenericParam = 42;
        constexpr int kTableMethodSpec = 43;
        constexpr int kTableGenericParamConstraint = 44;

        struct PeSection
        {
            std::uint32_t virtualAddress = 0;
            std::uint32_t virtualSize = 0;
            std::uint32_t rawPointer = 0;
            std::uint32_t rawSize = 0;
        };

        struct MetadataStreams
        {
            std::size_t metadataOffset = 0;
            std::size_t tablesOffset = 0;
            std::size_t tablesSize = 0;
            std::size_t stringsOffset = 0;
            std::size_t stringsSize = 0;
            std::size_t blobOffset = 0;
            std::size_t blobSize = 0;
        };

        struct TablesContext
        {
            std::array<std::uint32_t, 64> rows{};
            std::array<std::size_t, 64> offsets{};
            std::uint8_t heapSizes = 0;
            std::size_t stringIndexSize = 2;
            std::size_t guidIndexSize = 2;
            std::size_t blobIndexSize = 2;
        };

        struct TypeDefInfo
        {
            std::string fullName;
            std::uint32_t methodList = 0;
        };

        struct MethodDefInfo
        {
            std::string name;
            std::uint32_t rva = 0;
            std::uint32_t signature = 0;
            std::uint32_t token = 0;
            int argumentCount = -1;
        };

        std::size_t Align4(std::size_t value)
        {
            return (value + 3u) & ~std::size_t(3u);
        }

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

        template <typename T>
        bool ReadAt(const std::vector<std::uint8_t>& bytes, std::size_t offset, T* value)
        {
            if (!value || offset > bytes.size() || sizeof(T) > bytes.size() - offset)
            {
                return false;
            }

            std::memcpy(value, bytes.data() + offset, sizeof(T));
            return true;
        }

        bool ReadUInt(
            const std::vector<std::uint8_t>& bytes,
            std::size_t* offset,
            std::size_t width,
            std::uint32_t* value)
        {
            if (!offset || !value || (*offset > bytes.size()) || width > bytes.size() - *offset)
            {
                return false;
            }

            if (width == 2)
            {
                std::uint16_t raw = 0;
                std::memcpy(&raw, bytes.data() + *offset, sizeof(raw));
                *value = raw;
            }
            else if (width == 4)
            {
                std::uint32_t raw = 0;
                std::memcpy(&raw, bytes.data() + *offset, sizeof(raw));
                *value = raw;
            }
            else
            {
                return false;
            }

            *offset += width;
            return true;
        }

        bool RvaToFileOffset(
            const std::vector<PeSection>& sections,
            std::uint32_t rva,
            std::uint32_t* fileOffset)
        {
            if (!fileOffset)
            {
                return false;
            }

            for (const PeSection& section : sections)
            {
                const std::uint32_t span = std::max(section.virtualSize, section.rawSize);
                if (rva >= section.virtualAddress && rva < section.virtualAddress + span)
                {
                    *fileOffset = section.rawPointer + (rva - section.virtualAddress);
                    return true;
                }
            }

            *fileOffset = rva;
            return true;
        }

        std::string ToUtf8Path(const std::filesystem::path& path)
        {
            return path.string();
        }

        void NormalizeManagedName(std::string* value)
        {
            if (!value)
            {
                return;
            }

            std::replace(value->begin(), value->end(), '/', '.');
            std::replace(value->begin(), value->end(), '+', '.');
        }

        bool OpenCliMetadata(
            const std::filesystem::path& path,
            std::vector<std::uint8_t>* bytes,
            MetadataStreams* streams)
        {
            if (!bytes || !streams || !ReadFileBytes(path, bytes))
            {
                return false;
            }

            IMAGE_DOS_HEADER dos{};
            if (!ReadAt(*bytes, 0, &dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
            {
                return false;
            }

            const auto ntOffset = static_cast<std::size_t>(dos.e_lfanew);
            DWORD signature = 0;
            IMAGE_FILE_HEADER fileHeader{};
            if (!ReadAt(*bytes, ntOffset, &signature) ||
                signature != IMAGE_NT_SIGNATURE ||
                !ReadAt(*bytes, ntOffset + sizeof(DWORD), &fileHeader))
            {
                return false;
            }

            const std::size_t optionalHeaderOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
            WORD optionalMagic = 0;
            if (!ReadAt(*bytes, optionalHeaderOffset, &optionalMagic))
            {
                return false;
            }

            IMAGE_DATA_DIRECTORY cliDirectory{};
            std::size_t sectionOffset = optionalHeaderOffset + fileHeader.SizeOfOptionalHeader;
            if (optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                IMAGE_OPTIONAL_HEADER64 optionalHeader{};
                if (!ReadAt(*bytes, optionalHeaderOffset, &optionalHeader))
                {
                    return false;
                }
                cliDirectory = optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR];
            }
            else if (optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            {
                IMAGE_OPTIONAL_HEADER32 optionalHeader{};
                if (!ReadAt(*bytes, optionalHeaderOffset, &optionalHeader))
                {
                    return false;
                }
                cliDirectory = optionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR];
            }
            else
            {
                return false;
            }

            if (!cliDirectory.VirtualAddress || !cliDirectory.Size)
            {
                return false;
            }

            std::vector<PeSection> sections;
            sections.reserve(fileHeader.NumberOfSections);
            for (WORD index = 0; index < fileHeader.NumberOfSections; ++index)
            {
                IMAGE_SECTION_HEADER section{};
                if (!ReadAt(*bytes, sectionOffset + (sizeof(IMAGE_SECTION_HEADER) * index), &section))
                {
                    return false;
                }

                sections.push_back(PeSection{
                    section.VirtualAddress,
                    section.Misc.VirtualSize,
                    section.PointerToRawData,
                    section.SizeOfRawData
                });
            }

            std::uint32_t cliOffset = 0;
            if (!RvaToFileOffset(sections, cliDirectory.VirtualAddress, &cliOffset))
            {
                return false;
            }

            IMAGE_COR20_HEADER cliHeader{};
            if (!ReadAt(*bytes, cliOffset, &cliHeader) ||
                !cliHeader.MetaData.VirtualAddress ||
                !cliHeader.MetaData.Size)
            {
                return false;
            }

            std::uint32_t metadataOffset = 0;
            if (!RvaToFileOffset(sections, cliHeader.MetaData.VirtualAddress, &metadataOffset) ||
                metadataOffset >= bytes->size())
            {
                return false;
            }

            std::size_t cursor = metadataOffset;
            std::uint32_t metadataSignature = 0;
            std::uint16_t major = 0;
            std::uint16_t minor = 0;
            std::uint32_t reserved = 0;
            std::uint32_t versionLength = 0;
            if (!ReadAt(*bytes, cursor, &metadataSignature) || metadataSignature != 0x424A5342u)
            {
                return false;
            }
            cursor += sizeof(metadataSignature);

            if (!ReadAt(*bytes, cursor, &major) || !ReadAt(*bytes, cursor + sizeof(major), &minor))
            {
                return false;
            }
            cursor += sizeof(major) + sizeof(minor);

            if (!ReadAt(*bytes, cursor, &reserved))
            {
                return false;
            }
            cursor += sizeof(reserved);

            if (!ReadAt(*bytes, cursor, &versionLength))
            {
                return false;
            }
            cursor += sizeof(versionLength);
            cursor += Align4(versionLength);

            std::uint16_t flags = 0;
            std::uint16_t streamCount = 0;
            if (!ReadAt(*bytes, cursor, &flags) || !ReadAt(*bytes, cursor + sizeof(flags), &streamCount))
            {
                return false;
            }
            cursor += sizeof(flags) + sizeof(streamCount);

            streams->metadataOffset = metadataOffset;
            for (std::uint16_t index = 0; index < streamCount; ++index)
            {
                std::uint32_t streamOffset = 0;
                std::uint32_t streamSize = 0;
                if (!ReadAt(*bytes, cursor, &streamOffset) || !ReadAt(*bytes, cursor + sizeof(streamOffset), &streamSize))
                {
                    return false;
                }
                cursor += sizeof(streamOffset) + sizeof(streamSize);

                std::string name;
                while (cursor < bytes->size() && (*bytes)[cursor] != 0)
                {
                    name.push_back(static_cast<char>((*bytes)[cursor++]));
                }
                if (cursor >= bytes->size())
                {
                    return false;
                }
                ++cursor;
                cursor = Align4(cursor);

                const std::size_t fileOffset = static_cast<std::size_t>(metadataOffset) + streamOffset;
                if (fileOffset > bytes->size() || streamSize > bytes->size() - fileOffset)
                {
                    continue;
                }

                if (name == "#~" || name == "#-")
                {
                    streams->tablesOffset = fileOffset;
                    streams->tablesSize = streamSize;
                }
                else if (name == "#Strings")
                {
                    streams->stringsOffset = fileOffset;
                    streams->stringsSize = streamSize;
                }
                else if (name == "#Blob")
                {
                    streams->blobOffset = fileOffset;
                    streams->blobSize = streamSize;
                }
            }

            return streams->tablesOffset != 0 && streams->stringsOffset != 0 && streams->blobOffset != 0;
        }

        std::size_t TableIndexSize(const TablesContext& context, int table)
        {
            if (table < 0 || table >= static_cast<int>(context.rows.size()))
            {
                return 4;
            }

            return context.rows[table] < 0x10000u ? 2u : 4u;
        }

        std::size_t CodedIndexSize(const TablesContext& context, std::initializer_list<int> tables, int tagBits)
        {
            std::uint32_t maxRows = 0;
            for (int table : tables)
            {
                if (table >= 0 && table < static_cast<int>(context.rows.size()))
                {
                    maxRows = std::max(maxRows, context.rows[table]);
                }
            }

            const std::uint32_t twoByteLimit = 1u << (16 - tagBits);
            return maxRows < twoByteLimit ? 2u : 4u;
        }

        std::size_t RowSize(const TablesContext& context, int table)
        {
            const std::size_t str = context.stringIndexSize;
            const std::size_t guid = context.guidIndexSize;
            const std::size_t blob = context.blobIndexSize;
            const auto idx = [&context](int tableId) { return TableIndexSize(context, tableId); };
            const auto coded = [&context](std::initializer_list<int> tables, int bits) {
                return CodedIndexSize(context, tables, bits);
            };

            switch (table)
            {
            case 0: return 2 + str + guid + guid + guid;
            case 1: return coded({ kTableModule, kTableModuleRef, kTableAssemblyRef, kTableTypeRef }, 2) + str + str;
            case 2: return 4 + str + str + coded({ kTableTypeDef, kTableTypeRef, kTableTypeSpec }, 2) + idx(kTableField) + idx(kTableMethodDef);
            case 3: return idx(kTableField);
            case 4: return 2 + str + blob;
            case 5: return idx(kTableMethodDef);
            case 6: return 4 + 2 + 2 + str + blob + idx(kTableParam);
            case 7: return idx(kTableParam);
            case 8: return 2 + 2 + str;
            case 9: return idx(kTableTypeDef) + coded({ kTableTypeDef, kTableTypeRef, kTableTypeSpec }, 2);
            case 10: return coded({ kTableTypeDef, kTableTypeRef, kTableModuleRef, kTableMethodDef, kTableTypeSpec }, 3) + str + blob;
            case 11: return 2 + coded({ kTableField, kTableParam, kTableProperty }, 2) + blob;
            case 12: return coded({ kTableMethodDef, kTableField, kTableTypeRef, kTableTypeDef, kTableParam, kTableInterfaceImpl, kTableMemberRef, kTableModule, kTableDeclSecurity, kTableProperty, kTableEvent, 17, kTableModuleRef, kTableTypeSpec, kTableAssembly, kTableAssemblyRef, kTableFile, kTableExportedType, kTableManifestResource, kTableGenericParam, kTableGenericParamConstraint, kTableMethodSpec }, 5) + coded({ kTableMethodDef, kTableMemberRef }, 3) + blob;
            case 13: return coded({ kTableField, kTableParam }, 1) + blob;
            case 14: return 2 + coded({ kTableTypeDef, kTableMethodDef, kTableAssembly }, 2) + blob;
            case 15: return 2 + 4 + idx(kTableTypeDef);
            case 16: return 4 + idx(kTableField);
            case 17: return blob;
            case 18: return idx(kTableTypeDef) + idx(kTableEvent);
            case 19: return idx(kTableEvent);
            case 20: return 2 + str + coded({ kTableTypeDef, kTableTypeRef, kTableTypeSpec }, 2);
            case 21: return idx(kTableTypeDef) + idx(kTableProperty);
            case 22: return idx(kTableProperty);
            case 23: return 2 + str + blob;
            case 24: return 2 + idx(kTableMethodDef) + coded({ kTableEvent, kTableProperty }, 1);
            case 25: return idx(kTableTypeDef) + coded({ kTableMethodDef, kTableMemberRef }, 1) + coded({ kTableMethodDef, kTableMemberRef }, 1);
            case 26: return str;
            case 27: return blob;
            case 28: return 2 + coded({ kTableField, kTableMethodDef }, 1) + str + idx(kTableModuleRef);
            case 29: return 4 + idx(kTableField);
            case 30: return 4 + 4;
            case 31: return 4;
            case 32: return 4 + 2 + 2 + 2 + 2 + 4 + blob + str + str;
            case 33: return 4;
            case 34: return 4 + 4 + 4;
            case 35: return 2 + 2 + 2 + 2 + 4 + blob + str + str + blob;
            case 36: return 4 + idx(kTableAssemblyRef);
            case 37: return 4 + 4 + 4 + idx(kTableAssemblyRef);
            case 38: return 4 + str + blob;
            case 39: return 4 + 4 + str + str + coded({ kTableFile, kTableAssemblyRef, kTableExportedType }, 2);
            case 40: return 4 + 4 + str + coded({ kTableFile, kTableAssemblyRef, kTableExportedType }, 2);
            case 41: return idx(kTableTypeDef) + idx(kTableTypeDef);
            case 42: return 2 + 2 + coded({ kTableTypeDef, kTableMethodDef }, 1) + str;
            case 43: return coded({ kTableMethodDef, kTableMemberRef }, 1) + blob;
            case 44: return idx(kTableGenericParam) + coded({ kTableTypeDef, kTableTypeRef, kTableTypeSpec }, 2);
            default: return 0;
            }
        }

        bool ParseTablesContext(
            const std::vector<std::uint8_t>& bytes,
            const MetadataStreams& streams,
            TablesContext* context)
        {
            if (!context || streams.tablesOffset == 0)
            {
                return false;
            }

            std::size_t cursor = streams.tablesOffset;
            std::uint32_t reserved = 0;
            if (!ReadAt(bytes, cursor, &reserved))
            {
                return false;
            }
            cursor += sizeof(reserved);

            if (cursor + 4 > bytes.size())
            {
                return false;
            }
            cursor += 2;
            context->heapSizes = bytes[cursor++];
            cursor += 1;
            context->stringIndexSize = (context->heapSizes & 0x01) ? 4 : 2;
            context->guidIndexSize = (context->heapSizes & 0x02) ? 4 : 2;
            context->blobIndexSize = (context->heapSizes & 0x04) ? 4 : 2;

            std::uint64_t validMask = 0;
            std::uint64_t sortedMask = 0;
            if (!ReadAt(bytes, cursor, &validMask) || !ReadAt(bytes, cursor + sizeof(validMask), &sortedMask))
            {
                return false;
            }
            cursor += sizeof(validMask) + sizeof(sortedMask);

            for (int table = 0; table < 64; ++table)
            {
                if ((validMask & (1ull << table)) == 0)
                {
                    continue;
                }

                std::uint32_t rowCount = 0;
                if (!ReadAt(bytes, cursor, &rowCount))
                {
                    return false;
                }
                context->rows[table] = rowCount;
                cursor += sizeof(rowCount);
            }

            for (int table = 0; table < 64; ++table)
            {
                if (context->rows[table] == 0)
                {
                    continue;
                }

                const std::size_t rowSize = RowSize(*context, table);
                if (rowSize == 0)
                {
                    return false;
                }

                context->offsets[table] = cursor;
                const std::uint64_t byteCount = static_cast<std::uint64_t>(rowSize) * context->rows[table];
                if (byteCount > std::numeric_limits<std::size_t>::max() || byteCount > bytes.size() - cursor)
                {
                    return false;
                }
                cursor += static_cast<std::size_t>(byteCount);
            }

            return true;
        }

        std::string ReadHeapString(
            const std::vector<std::uint8_t>& bytes,
            const MetadataStreams& streams,
            std::uint32_t index)
        {
            if (index == 0 || index >= streams.stringsSize)
            {
                return {};
            }

            const std::size_t offset = streams.stringsOffset + index;
            if (offset >= bytes.size())
            {
                return {};
            }

            std::string value;
            for (std::size_t cursor = offset; cursor < streams.stringsOffset + streams.stringsSize && cursor < bytes.size(); ++cursor)
            {
                const char ch = static_cast<char>(bytes[cursor]);
                if (ch == '\0')
                {
                    break;
                }
                value.push_back(ch);
            }
            return value;
        }

        bool DecodeCompressedUInt(
            const std::uint8_t* data,
            std::size_t size,
            std::size_t* offset,
            std::uint32_t* value)
        {
            if (!data || !offset || !value || *offset >= size)
            {
                return false;
            }

            const std::uint8_t first = data[(*offset)++];
            if ((first & 0x80u) == 0)
            {
                *value = first;
                return true;
            }

            if ((first & 0xC0u) == 0x80u)
            {
                if (*offset >= size)
                {
                    return false;
                }
                *value = (static_cast<std::uint32_t>(first & 0x3Fu) << 8)
                    | data[(*offset)++];
                return true;
            }

            if ((first & 0xE0u) == 0xC0u)
            {
                if (*offset + 2 >= size)
                {
                    return false;
                }
                *value = (static_cast<std::uint32_t>(first & 0x1Fu) << 24)
                    | (static_cast<std::uint32_t>(data[(*offset)++]) << 16)
                    | (static_cast<std::uint32_t>(data[(*offset)++]) << 8)
                    | data[(*offset)++];
                return true;
            }

            return false;
        }

        int ReadMethodArgumentCount(
            const std::vector<std::uint8_t>& bytes,
            const MetadataStreams& streams,
            std::uint32_t blobIndex)
        {
            if (blobIndex == 0 || blobIndex >= streams.blobSize)
            {
                return -1;
            }

            const std::size_t blobStart = streams.blobOffset + blobIndex;
            if (blobStart >= bytes.size())
            {
                return -1;
            }

            std::size_t offset = 0;
            std::uint32_t blobLength = 0;
            const std::size_t available = std::min<std::size_t>(streams.blobSize - blobIndex, bytes.size() - blobStart);
            if (!DecodeCompressedUInt(bytes.data() + blobStart, available, &offset, &blobLength) ||
                offset >= available ||
                blobLength > available - offset)
            {
                return -1;
            }

            const std::uint8_t* signature = bytes.data() + blobStart + offset;
            std::size_t sigOffset = 0;
            if (blobLength == 0)
            {
                return -1;
            }

            const std::uint8_t callConv = signature[sigOffset++];
            constexpr std::uint8_t kGeneric = 0x10;
            if ((callConv & kGeneric) != 0)
            {
                std::uint32_t genericParamCount = 0;
                if (!DecodeCompressedUInt(signature, blobLength, &sigOffset, &genericParamCount))
                {
                    return -1;
                }
            }

            std::uint32_t paramCount = 0;
            if (!DecodeCompressedUInt(signature, blobLength, &sigOffset, &paramCount) ||
                paramCount > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
            {
                return -1;
            }

            return static_cast<int>(paramCount);
        }

        bool ReadTypeDefRow(
            const std::vector<std::uint8_t>& bytes,
            const MetadataStreams& streams,
            const TablesContext& context,
            std::uint32_t row,
            TypeDefInfo* type)
        {
            if (!type || row == 0 || row > context.rows[kTableTypeDef])
            {
                return false;
            }

            std::size_t cursor = context.offsets[kTableTypeDef] + ((row - 1) * RowSize(context, kTableTypeDef));
            cursor += 4;

            std::uint32_t nameIndex = 0;
            std::uint32_t namespaceIndex = 0;
            std::uint32_t unused = 0;
            std::uint32_t methodList = 0;
            if (!ReadUInt(bytes, &cursor, context.stringIndexSize, &nameIndex) ||
                !ReadUInt(bytes, &cursor, context.stringIndexSize, &namespaceIndex) ||
                !ReadUInt(bytes, &cursor, CodedIndexSize(context, { kTableTypeDef, kTableTypeRef, kTableTypeSpec }, 2), &unused) ||
                !ReadUInt(bytes, &cursor, TableIndexSize(context, kTableField), &unused) ||
                !ReadUInt(bytes, &cursor, TableIndexSize(context, kTableMethodDef), &methodList))
            {
                return false;
            }

            const std::string typeName = ReadHeapString(bytes, streams, nameIndex);
            const std::string namespaceName = ReadHeapString(bytes, streams, namespaceIndex);
            if (typeName.empty())
            {
                return false;
            }

            type->fullName = namespaceName.empty() ? typeName : namespaceName + "." + typeName;
            NormalizeManagedName(&type->fullName);
            type->methodList = methodList;
            return true;
        }

        bool ReadMethodDefRow(
            const std::vector<std::uint8_t>& bytes,
            const MetadataStreams& streams,
            const TablesContext& context,
            std::uint32_t row,
            MethodDefInfo* method)
        {
            if (!method || row == 0 || row > context.rows[kTableMethodDef])
            {
                return false;
            }

            std::size_t cursor = context.offsets[kTableMethodDef] + ((row - 1) * RowSize(context, kTableMethodDef));
            std::uint32_t rva = 0;
            std::uint32_t nameIndex = 0;
            std::uint32_t signatureIndex = 0;
            std::uint32_t unused = 0;

            if (!ReadUInt(bytes, &cursor, 4, &rva) ||
                !ReadUInt(bytes, &cursor, 2, &unused) ||
                !ReadUInt(bytes, &cursor, 2, &unused) ||
                !ReadUInt(bytes, &cursor, context.stringIndexSize, &nameIndex) ||
                !ReadUInt(bytes, &cursor, context.blobIndexSize, &signatureIndex) ||
                !ReadUInt(bytes, &cursor, TableIndexSize(context, kTableParam), &unused))
            {
                return false;
            }

            method->name = ReadHeapString(bytes, streams, nameIndex);
            method->rva = rva;
            method->signature = signatureIndex;
            method->token = 0x06000000u | row;
            method->argumentCount = ReadMethodArgumentCount(bytes, streams, signatureIndex);
            return !method->name.empty();
        }

        bool ParseManagedAssembly(
            const std::filesystem::path& path,
            std::vector<MethodMap::Entry>* entries,
            std::size_t* typeCount,
            std::size_t* methodCount)
        {
            if (!entries || !typeCount || !methodCount)
            {
                return false;
            }

            std::vector<std::uint8_t> bytes;
            MetadataStreams streams;
            if (!OpenCliMetadata(path, &bytes, &streams))
            {
                return false;
            }

            TablesContext context;
            if (!ParseTablesContext(bytes, streams, &context) ||
                context.rows[kTableTypeDef] == 0 ||
                context.rows[kTableMethodDef] == 0)
            {
                return false;
            }

            std::vector<TypeDefInfo> types(context.rows[kTableTypeDef] + 1);
            for (std::uint32_t row = 1; row <= context.rows[kTableTypeDef]; ++row)
            {
                TypeDefInfo type;
                if (ReadTypeDefRow(bytes, streams, context, row, &type))
                {
                    types[row] = std::move(type);
                }
            }

            const std::string imageName = path.stem().string();
            const std::string sourcePath = ToUtf8Path(path);
            std::size_t parsedTypes = 0;
            std::size_t parsedMethods = 0;

            for (std::uint32_t row = 1; row <= context.rows[kTableTypeDef]; ++row)
            {
                const TypeDefInfo& type = types[row];
                if (type.fullName.empty() || type.fullName == "<Module>" || type.methodList == 0)
                {
                    continue;
                }

                const std::uint32_t nextMethodList =
                    (row < context.rows[kTableTypeDef] && types[row + 1].methodList != 0)
                    ? types[row + 1].methodList
                    : context.rows[kTableMethodDef] + 1;

                if (nextMethodList <= type.methodList)
                {
                    continue;
                }

                ++parsedTypes;
                for (std::uint32_t methodRow = type.methodList; methodRow < nextMethodList; ++methodRow)
                {
                    MethodDefInfo method;
                    if (!ReadMethodDefRow(bytes, streams, context, methodRow, &method))
                    {
                        continue;
                    }

                    MethodMap::Entry entry;
                    entry.imageName = imageName;
                    entry.className = type.fullName;
                    entry.methodName = method.name;
                    entry.argumentCount = method.argumentCount;
                    entry.rva = method.rva;
                    entry.metadataToken = method.token;
                    entry.kind = MethodMapEntryKind::MonoMetadataToken;
                    entry.sourcePath = sourcePath;
                    entries->push_back(std::move(entry));
                    ++parsedMethods;
                }
            }

            *typeCount += parsedTypes;
            *methodCount += parsedMethods;
            return parsedMethods > 0;
        }
    }

    std::optional<std::filesystem::path> FindMonoManagedDirectory(const UnityProcess& process)
    {
        std::vector<std::filesystem::path> candidates;

        if (!process.executablePath.empty())
        {
            const std::filesystem::path exePath(process.executablePath);
            const std::filesystem::path parent = exePath.parent_path();
            const std::filesystem::path dataFolder = parent / (exePath.stem().wstring() + L"_Data");
            candidates.push_back(dataFolder / L"Managed");
            candidates.push_back(parent / L"Managed");
        }

        candidates.push_back(std::filesystem::current_path() / L"Managed");

        for (const std::filesystem::path& candidate : candidates)
        {
            std::error_code error;
            if (std::filesystem::exists(candidate, error) && std::filesystem::is_directory(candidate, error))
            {
                return candidate;
            }
        }

        return std::nullopt;
    }

    GeneratedMonoMethodMap GenerateMonoMethodMap(const UnityProcess& process)
    {
        GeneratedMonoMethodMap result;
        if (process.modules.Backend() != RuntimeBackend::Mono)
        {
            result.message = "Target is not a Mono Unity process.";
            return result;
        }

        const std::optional<std::filesystem::path> managedDirectory = FindMonoManagedDirectory(process);
        if (!managedDirectory)
        {
            result.message = "Could not find the target _Data\\Managed assembly folder.";
            return result;
        }

        std::vector<MethodMap::Entry> entries;
        std::size_t assemblyCount = 0;
        std::size_t typeCount = 0;
        std::size_t methodCount = 0;

        std::error_code error;
        for (const std::filesystem::directory_entry& file : std::filesystem::directory_iterator(*managedDirectory, error))
        {
            if (error)
            {
                break;
            }

            if (!file.is_regular_file(error) || error)
            {
                error.clear();
                continue;
            }

            const std::filesystem::path path = file.path();
            if (_wcsicmp(path.extension().wstring().c_str(), L".dll") != 0)
            {
                continue;
            }

            const std::size_t before = entries.size();
            if (ParseManagedAssembly(path, &entries, &typeCount, &methodCount) && entries.size() > before)
            {
                ++assemblyCount;
            }
        }

        if (entries.empty())
        {
            result.message = "No managed methods were parsed from Mono assemblies.";
            result.managedDirectory = *managedDirectory;
            return result;
        }

        std::sort(entries.begin(), entries.end(), [](const MethodMap::Entry& left, const MethodMap::Entry& right) {
            if (left.imageName != right.imageName) return left.imageName < right.imageName;
            if (left.className != right.className) return left.className < right.className;
            if (left.methodName != right.methodName) return left.methodName < right.methodName;
            return left.argumentCount < right.argumentCount;
        });

        result.success = true;
        result.message = "Generated Mono metadata method map.";
        result.managedDirectory = *managedDirectory;
        result.assemblyCount = assemblyCount;
        result.typeCount = typeCount;
        result.methodCount = methodCount;
        result.methodMap.SetEntries(std::move(entries));
        return result;
    }
}
