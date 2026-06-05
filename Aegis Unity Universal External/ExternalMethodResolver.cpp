#include "ExternalMethodResolver.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace Aegis::UnityExternal
{
    namespace
    {
        struct PeSection
        {
            std::uint32_t virtualAddress = 0;
            std::uint32_t virtualSize = 0;
            std::uint32_t rawPointer = 0;
            std::uint32_t rawSize = 0;
        };

        std::string Trim(std::string value)
        {
            if (value.rfind("\xEF\xBB\xBF", 0) == 0)
            {
                value.erase(0, 3);
            }

            const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
                return std::isspace(ch) != 0;
            });
            const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
                return std::isspace(ch) != 0;
            }).base();

            if (first >= last)
            {
                return {};
            }

            return std::string(first, last);
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        bool EqualsInsensitive(std::string left, std::string right)
        {
            return ToLowerAscii(std::move(left)) == ToLowerAscii(std::move(right));
        }

        std::string NormalizeImageName(std::string value)
        {
            value = ToLowerAscii(Trim(std::move(value)));
            if (value.size() > 4 && value.substr(value.size() - 4) == ".dll")
            {
                value.resize(value.size() - 4);
            }
            return value;
        }

        std::wstring WidenAscii(const std::string& value)
        {
            return std::wstring(value.begin(), value.end());
        }

        std::vector<std::string> Split(const std::string& line, char delimiter)
        {
            std::vector<std::string> parts;
            std::stringstream stream(line);
            std::string part;

            while (std::getline(stream, part, delimiter))
            {
                parts.push_back(Trim(part));
            }

            return parts;
        }

        bool ParseInteger64(const std::string& text, std::uint64_t* value)
        {
            if (!value || text.empty())
            {
                return false;
            }

            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(text.c_str(), &end, 0);
            if (!end || *end != '\0')
            {
                return false;
            }

            *value = static_cast<std::uint64_t>(parsed);
            return true;
        }

        bool ParseInteger(const std::string& text, std::uint32_t* value)
        {
            std::uint64_t parsed = 0;
            if (!ParseInteger64(text, &parsed) || parsed > std::numeric_limits<std::uint32_t>::max())
            {
                return false;
            }

            *value = static_cast<std::uint32_t>(parsed);
            return true;
        }

        bool ParseArgumentCount(const std::string& text, int* value)
        {
            if (!value || text.empty() || text == "*")
            {
                if (value)
                {
                    *value = -1;
                }
                return true;
            }

            char* end = nullptr;
            const long parsed = std::strtol(text.c_str(), &end, 10);
            if (!end || *end != '\0')
            {
                return false;
            }

            *value = static_cast<int>(parsed);
            return true;
        }

        bool ReadFileBytes(const std::wstring& path, std::vector<std::uint8_t>* bytes)
        {
            if (!bytes)
            {
                return false;
            }

            std::ifstream file{ std::filesystem::path(path), std::ios::binary };
            if (!file)
            {
                return false;
            }

            bytes->assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            return !bytes->empty();
        }

        bool ReadFileText(const std::wstring& path, std::string* text)
        {
            if (!text)
            {
                return false;
            }

            std::ifstream file{ std::filesystem::path(path), std::ios::binary };
            if (!file)
            {
                return false;
            }

            text->assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            return !text->empty();
        }

        template <typename T>
        const T* ViewAs(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
            {
                return nullptr;
            }

            return reinterpret_cast<const T*>(bytes.data() + offset);
        }

        bool RvaToFileOffset(
            const std::vector<PeSection>& sections,
            std::uint32_t rva,
            std::uint32_t* fileOffset)
        {
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

        const char* ReadCStringAtRva(
            const std::vector<std::uint8_t>& bytes,
            const std::vector<PeSection>& sections,
            std::uint32_t rva)
        {
            std::uint32_t fileOffset = 0;
            if (!RvaToFileOffset(sections, rva, &fileOffset) || fileOffset >= bytes.size())
            {
                return nullptr;
            }

            return reinterpret_cast<const char*>(bytes.data() + fileOffset);
        }

        std::optional<std::uint32_t> ResolveExportRvaFromImage(
            const std::wstring& imagePath,
            const std::string& exportName)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadFileBytes(imagePath, &bytes))
            {
                return std::nullopt;
            }

            const auto* dos = ViewAs<IMAGE_DOS_HEADER>(bytes, 0);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            {
                return std::nullopt;
            }

            const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
            const auto* signature = ViewAs<DWORD>(bytes, ntOffset);
            if (!signature || *signature != IMAGE_NT_SIGNATURE)
            {
                return std::nullopt;
            }

            const auto* fileHeader = ViewAs<IMAGE_FILE_HEADER>(bytes, ntOffset + sizeof(DWORD));
            if (!fileHeader)
            {
                return std::nullopt;
            }

            const auto optionalHeaderOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
            const auto* optionalMagic = ViewAs<WORD>(bytes, optionalHeaderOffset);
            if (!optionalMagic)
            {
                return std::nullopt;
            }

            IMAGE_DATA_DIRECTORY exportDirectory = {};
            std::size_t sectionOffset = 0;
            if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                const auto* optionalHeader = ViewAs<IMAGE_OPTIONAL_HEADER64>(bytes, optionalHeaderOffset);
                if (!optionalHeader)
                {
                    return std::nullopt;
                }

                exportDirectory = optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                sectionOffset = optionalHeaderOffset + fileHeader->SizeOfOptionalHeader;
            }
            else if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            {
                const auto* optionalHeader = ViewAs<IMAGE_OPTIONAL_HEADER32>(bytes, optionalHeaderOffset);
                if (!optionalHeader)
                {
                    return std::nullopt;
                }

                exportDirectory = optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                sectionOffset = optionalHeaderOffset + fileHeader->SizeOfOptionalHeader;
            }
            else
            {
                return std::nullopt;
            }

            if (!exportDirectory.VirtualAddress || !exportDirectory.Size)
            {
                return std::nullopt;
            }

            std::vector<PeSection> sections;
            sections.reserve(fileHeader->NumberOfSections);

            for (WORD index = 0; index < fileHeader->NumberOfSections; ++index)
            {
                const auto* header = ViewAs<IMAGE_SECTION_HEADER>(bytes, sectionOffset + (sizeof(IMAGE_SECTION_HEADER) * index));
                if (!header)
                {
                    return std::nullopt;
                }

                sections.push_back(PeSection{
                    header->VirtualAddress,
                    header->Misc.VirtualSize,
                    header->PointerToRawData,
                    header->SizeOfRawData
                });
            }

            std::uint32_t exportFileOffset = 0;
            if (!RvaToFileOffset(sections, exportDirectory.VirtualAddress, &exportFileOffset))
            {
                return std::nullopt;
            }

            const auto* exportTable = ViewAs<IMAGE_EXPORT_DIRECTORY>(bytes, exportFileOffset);
            if (!exportTable)
            {
                return std::nullopt;
            }

            std::uint32_t namesOffset = 0;
            std::uint32_t ordinalsOffset = 0;
            std::uint32_t functionsOffset = 0;
            if (!RvaToFileOffset(sections, exportTable->AddressOfNames, &namesOffset) ||
                !RvaToFileOffset(sections, exportTable->AddressOfNameOrdinals, &ordinalsOffset) ||
                !RvaToFileOffset(sections, exportTable->AddressOfFunctions, &functionsOffset))
            {
                return std::nullopt;
            }

            for (DWORD index = 0; index < exportTable->NumberOfNames; ++index)
            {
                const auto* nameRva = ViewAs<DWORD>(bytes, namesOffset + (index * sizeof(DWORD)));
                const auto* ordinal = ViewAs<WORD>(bytes, ordinalsOffset + (index * sizeof(WORD)));
                if (!nameRva || !ordinal)
                {
                    return std::nullopt;
                }

                const char* name = ReadCStringAtRva(bytes, sections, *nameRva);
                if (!name || exportName != name)
                {
                    continue;
                }

                const auto* functionRva = ViewAs<DWORD>(bytes, functionsOffset + ((*ordinal) * sizeof(DWORD)));
                if (!functionRva)
                {
                    return std::nullopt;
                }

                return *functionRva;
            }

            return std::nullopt;
        }

        bool ReadExportRvasFromImage(
            const std::wstring& imagePath,
            std::unordered_map<std::string, std::uint32_t>* exports)
        {
            if (!exports)
            {
                return false;
            }

            exports->clear();

            std::vector<std::uint8_t> bytes;
            if (!ReadFileBytes(imagePath, &bytes))
            {
                return false;
            }

            const auto* dos = ViewAs<IMAGE_DOS_HEADER>(bytes, 0);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            {
                return false;
            }

            const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
            const auto* signature = ViewAs<DWORD>(bytes, ntOffset);
            if (!signature || *signature != IMAGE_NT_SIGNATURE)
            {
                return false;
            }

            const auto* fileHeader = ViewAs<IMAGE_FILE_HEADER>(bytes, ntOffset + sizeof(DWORD));
            if (!fileHeader)
            {
                return false;
            }

            const auto optionalHeaderOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
            const auto* optionalMagic = ViewAs<WORD>(bytes, optionalHeaderOffset);
            if (!optionalMagic)
            {
                return false;
            }

            IMAGE_DATA_DIRECTORY exportDirectory = {};
            std::size_t sectionOffset = 0;
            if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                const auto* optionalHeader = ViewAs<IMAGE_OPTIONAL_HEADER64>(bytes, optionalHeaderOffset);
                if (!optionalHeader)
                {
                    return false;
                }

                exportDirectory = optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                sectionOffset = optionalHeaderOffset + fileHeader->SizeOfOptionalHeader;
            }
            else if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            {
                const auto* optionalHeader = ViewAs<IMAGE_OPTIONAL_HEADER32>(bytes, optionalHeaderOffset);
                if (!optionalHeader)
                {
                    return false;
                }

                exportDirectory = optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                sectionOffset = optionalHeaderOffset + fileHeader->SizeOfOptionalHeader;
            }
            else
            {
                return false;
            }

            if (!exportDirectory.VirtualAddress || !exportDirectory.Size)
            {
                return false;
            }

            std::vector<PeSection> sections;
            sections.reserve(fileHeader->NumberOfSections);

            for (WORD index = 0; index < fileHeader->NumberOfSections; ++index)
            {
                const auto* header = ViewAs<IMAGE_SECTION_HEADER>(bytes, sectionOffset + (sizeof(IMAGE_SECTION_HEADER) * index));
                if (!header)
                {
                    return false;
                }

                sections.push_back(PeSection{
                    header->VirtualAddress,
                    header->Misc.VirtualSize,
                    header->PointerToRawData,
                    header->SizeOfRawData
                });
            }

            std::uint32_t exportFileOffset = 0;
            if (!RvaToFileOffset(sections, exportDirectory.VirtualAddress, &exportFileOffset))
            {
                return false;
            }

            const auto* exportTable = ViewAs<IMAGE_EXPORT_DIRECTORY>(bytes, exportFileOffset);
            if (!exportTable)
            {
                return false;
            }

            std::uint32_t namesOffset = 0;
            std::uint32_t ordinalsOffset = 0;
            std::uint32_t functionsOffset = 0;
            if (!RvaToFileOffset(sections, exportTable->AddressOfNames, &namesOffset) ||
                !RvaToFileOffset(sections, exportTable->AddressOfNameOrdinals, &ordinalsOffset) ||
                !RvaToFileOffset(sections, exportTable->AddressOfFunctions, &functionsOffset))
            {
                return false;
            }

            for (DWORD index = 0; index < exportTable->NumberOfNames; ++index)
            {
                const auto* nameRva = ViewAs<DWORD>(bytes, namesOffset + (index * sizeof(DWORD)));
                const auto* ordinal = ViewAs<WORD>(bytes, ordinalsOffset + (index * sizeof(WORD)));
                if (!nameRva || !ordinal)
                {
                    return false;
                }

                const char* name = ReadCStringAtRva(bytes, sections, *nameRva);
                if (!name)
                {
                    continue;
                }

                const auto* functionRva = ViewAs<DWORD>(bytes, functionsOffset + ((*ordinal) * sizeof(DWORD)));
                if (!functionRva)
                {
                    return false;
                }

                exports->emplace(name, *functionRva);
            }

            return true;
        }

        std::optional<std::uint64_t> ReadImageBaseFromImage(const std::wstring& imagePath)
        {
            std::vector<std::uint8_t> bytes;
            if (!ReadFileBytes(imagePath, &bytes))
            {
                return std::nullopt;
            }

            const auto* dos = ViewAs<IMAGE_DOS_HEADER>(bytes, 0);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            {
                return std::nullopt;
            }

            const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
            const auto* signature = ViewAs<DWORD>(bytes, ntOffset);
            if (!signature || *signature != IMAGE_NT_SIGNATURE)
            {
                return std::nullopt;
            }

            const auto optionalHeaderOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
            const auto* optionalMagic = ViewAs<WORD>(bytes, optionalHeaderOffset);
            if (!optionalMagic)
            {
                return std::nullopt;
            }

            if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                const auto* optionalHeader = ViewAs<IMAGE_OPTIONAL_HEADER64>(bytes, optionalHeaderOffset);
                return optionalHeader ? std::optional<std::uint64_t>(optionalHeader->ImageBase) : std::nullopt;
            }

            if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            {
                const auto* optionalHeader = ViewAs<IMAGE_OPTIONAL_HEADER32>(bytes, optionalHeaderOffset);
                return optionalHeader ? std::optional<std::uint64_t>(optionalHeader->ImageBase) : std::nullopt;
            }

            return std::nullopt;
        }

        std::optional<std::uint64_t> FindSiblingGameAssemblyImageBase(const std::wstring& mapPath)
        {
            const std::filesystem::path directory = std::filesystem::path(mapPath).parent_path();
            if (directory.empty())
            {
                return std::nullopt;
            }

            return ReadImageBaseFromImage((directory / L"GameAssembly.dll").wstring());
        }

        bool NormalizeDumpAddress(
            std::uint64_t address,
            std::optional<std::uint64_t> imageBase,
            std::uint32_t* rva)
        {
            if (!rva)
            {
                return false;
            }

            if (address <= std::numeric_limits<std::uint32_t>::max())
            {
                *rva = static_cast<std::uint32_t>(address);
                return true;
            }

            if (imageBase && address >= *imageBase)
            {
                const std::uint64_t normalized = address - *imageBase;
                if (normalized <= std::numeric_limits<std::uint32_t>::max())
                {
                    *rva = static_cast<std::uint32_t>(normalized);
                    return true;
                }
            }

            constexpr std::uint64_t commonX64ImageBase = 0x180000000ull;
            if (address >= commonX64ImageBase)
            {
                const std::uint64_t normalized = address - commonX64ImageBase;
                if (normalized <= std::numeric_limits<std::uint32_t>::max())
                {
                    *rva = static_cast<std::uint32_t>(normalized);
                    return true;
                }
            }

            return false;
        }

        std::string UnescapeJsonString(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());

            bool escaping = false;
            for (char ch : value)
            {
                if (!escaping)
                {
                    if (ch == '\\')
                    {
                        escaping = true;
                    }
                    else
                    {
                        result.push_back(ch);
                    }
                    continue;
                }

                switch (ch)
                {
                case '"':
                case '\\':
                case '/':
                    result.push_back(ch);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    result.push_back(ch);
                    break;
                }

                escaping = false;
            }

            return result;
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

        bool MakeEntryFromQualifiedName(
            const std::string& qualifiedName,
            std::uint32_t rva,
            MethodMap::Entry* entry)
        {
            if (!entry)
            {
                return false;
            }

            std::string name = Trim(qualifiedName);
            const std::size_t parameterStart = name.find('(');
            if (parameterStart != std::string::npos)
            {
                name = Trim(name.substr(0, parameterStart));
            }

            std::size_t separator = name.rfind("$$");
            std::size_t separatorSize = 2;
            if (separator == std::string::npos)
            {
                separator = name.rfind("::");
                separatorSize = 2;
            }

            if (separator == std::string::npos)
            {
                return false;
            }

            MethodMap::Entry parsed;
            parsed.className = Trim(name.substr(0, separator));
            parsed.methodName = Trim(name.substr(separator + separatorSize));
            parsed.argumentCount = -1;
            parsed.rva = rva;
            NormalizeManagedName(&parsed.className);

            if (parsed.className.empty() || parsed.methodName.empty())
            {
                return false;
            }

            *entry = std::move(parsed);
            return true;
        }

        int CountSignatureArguments(const std::string& arguments)
        {
            const std::string text = Trim(arguments);
            if (text.empty())
            {
                return 0;
            }

            int count = 1;
            int angleDepth = 0;
            int parenDepth = 0;
            int bracketDepth = 0;
            for (char ch : text)
            {
                switch (ch)
                {
                case '<':
                    ++angleDepth;
                    break;
                case '>':
                    if (angleDepth > 0)
                    {
                        --angleDepth;
                    }
                    break;
                case '(':
                    ++parenDepth;
                    break;
                case ')':
                    if (parenDepth > 0)
                    {
                        --parenDepth;
                    }
                    break;
                case '[':
                    ++bracketDepth;
                    break;
                case ']':
                    if (bracketDepth > 0)
                    {
                        --bracketDepth;
                    }
                    break;
                case ',':
                    if (angleDepth == 0 && parenDepth == 0 && bracketDepth == 0)
                    {
                        ++count;
                    }
                    break;
                default:
                    break;
                }
            }

            return count;
        }

        bool ParseDumpMethodSignature(
            const std::string& rawLine,
            const std::string& className,
            std::uint32_t rva,
            MethodMap::Entry* entry)
        {
            if (!entry || className.empty())
            {
                return false;
            }

            std::string line = Trim(rawLine);
            if (line.empty() || line[0] == '[' || line.rfind("//", 0) == 0)
            {
                return false;
            }

            const std::size_t open = line.find('(');
            const std::size_t close = line.find(')', open == std::string::npos ? 0 : open);
            if (open == std::string::npos || close == std::string::npos || open == 0)
            {
                return false;
            }

            const std::string before = Trim(line.substr(0, open));
            const std::size_t methodStart = before.find_last_of(" \t");
            std::string methodName = methodStart == std::string::npos ? before : before.substr(methodStart + 1);
            methodName = Trim(methodName);
            const std::size_t genericStart = methodName.find('<');
            if (genericStart != std::string::npos)
            {
                methodName = methodName.substr(0, genericStart);
            }

            const std::string lowerMethodName = ToLowerAscii(methodName);
            if (methodName.empty() ||
                lowerMethodName == "if" ||
                lowerMethodName == "for" ||
                lowerMethodName == "while" ||
                lowerMethodName == "switch" ||
                lowerMethodName == "catch" ||
                lowerMethodName == "using")
            {
                return false;
            }

            MethodMap::Entry parsed;
            parsed.className = className;
            parsed.methodName = methodName;
            parsed.argumentCount = CountSignatureArguments(line.substr(open + 1, close - open - 1));
            parsed.rva = rva;
            NormalizeManagedName(&parsed.className);
            *entry = std::move(parsed);
            return true;
        }

        std::optional<MethodMap::Entry> ParseMapEntry(const std::string& rawLine);

        bool LoadTextMethodMap(const std::wstring& path, std::vector<MethodMap::Entry>* entries)
        {
            if (!entries)
            {
                return false;
            }

            std::ifstream file{ std::filesystem::path(path) };
            if (!file)
            {
                return false;
            }

            std::string line;
            while (std::getline(file, line))
            {
                std::optional<MethodMap::Entry> entry = ParseMapEntry(line);
                if (entry)
                {
                    entries->push_back(*entry);
                }
            }

            return true;
        }

        bool LoadScriptJsonMethodMap(const std::wstring& path, std::vector<MethodMap::Entry>* entries)
        {
            if (!entries)
            {
                return false;
            }

            std::string text;
            if (!ReadFileText(path, &text))
            {
                return false;
            }

            const std::optional<std::uint64_t> imageBase = FindSiblingGameAssemblyImageBase(path);
            const std::regex objectPattern("\\{[^{}]*\\}");
            const std::regex addressPattern("\"Address\"\\s*:\\s*\"?(0x[0-9A-Fa-f]+|[0-9]+)\"?");
            const std::regex namePattern("\"Name\"\\s*:\\s*\"((\\\\.|[^\"\\\\])*)\"");

            for (std::sregex_iterator it(text.begin(), text.end(), objectPattern), end; it != end; ++it)
            {
                const std::string objectText = it->str();
                std::smatch addressMatch;
                std::smatch nameMatch;
                if (!std::regex_search(objectText, addressMatch, addressPattern) ||
                    !std::regex_search(objectText, nameMatch, namePattern))
                {
                    continue;
                }

                std::uint64_t rawAddress = 0;
                std::uint32_t rva = 0;
                if (!ParseInteger64(addressMatch[1].str(), &rawAddress) ||
                    !NormalizeDumpAddress(rawAddress, imageBase, &rva))
                {
                    continue;
                }

                MethodMap::Entry entry;
                if (MakeEntryFromQualifiedName(UnescapeJsonString(nameMatch[1].str()), rva, &entry))
                {
                    entries->push_back(std::move(entry));
                }
            }

            return true;
        }

        bool LoadDumpCsMethodMap(const std::wstring& path, std::vector<MethodMap::Entry>* entries)
        {
            if (!entries)
            {
                return false;
            }

            std::ifstream file{ std::filesystem::path(path) };
            if (!file)
            {
                return false;
            }

            const std::optional<std::uint64_t> imageBase = FindSiblingGameAssemblyImageBase(path);
            const std::regex namespacePattern("^\\s*namespace\\s+([A-Za-z_][A-Za-z0-9_\\.]*)");
            const std::regex typePattern("\\b(class|struct|interface)\\s+([A-Za-z_][A-Za-z0-9_`]*)");
            const std::regex rvaPattern("RVA:\\s*(0x[0-9A-Fa-f]+|[0-9]+)");

            std::string currentNamespace;
            std::string currentClass;
            std::optional<std::uint32_t> pendingRva;

            std::string line;
            while (std::getline(file, line))
            {
                line = Trim(line);

                std::smatch match;
                if (std::regex_search(line, match, namespacePattern))
                {
                    currentNamespace = match[1].str();
                }

                if (std::regex_search(line, match, typePattern))
                {
                    currentClass = match[2].str();
                    if (!currentNamespace.empty())
                    {
                        currentClass = currentNamespace + "." + currentClass;
                    }
                    NormalizeManagedName(&currentClass);
                }

                if (std::regex_search(line, match, rvaPattern))
                {
                    std::uint64_t rawAddress = 0;
                    std::uint32_t rva = 0;
                    if (ParseInteger64(match[1].str(), &rawAddress) &&
                        NormalizeDumpAddress(rawAddress, imageBase, &rva))
                    {
                        pendingRva = rva;
                    }
                    continue;
                }

                if (pendingRva)
                {
                    MethodMap::Entry entry;
                    if (ParseDumpMethodSignature(line, currentClass, *pendingRva, &entry))
                    {
                        entries->push_back(std::move(entry));
                        pendingRva.reset();
                    }
                }
            }

            return true;
        }

        std::wstring ToLowerWide(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            return value;
        }

        std::optional<MethodMap::Entry> ParseMapEntry(const std::string& rawLine)
        {
            std::string line = Trim(rawLine);
            if (line.empty() || line[0] == '#')
            {
                return std::nullopt;
            }

            if (line.rfind("//", 0) == 0)
            {
                return std::nullopt;
            }

            const char delimiter = line.find('|') != std::string::npos ? '|' : ',';
            std::vector<std::string> parts = Split(line, delimiter);
            if (parts.size() < 2)
            {
                return std::nullopt;
            }

            MethodMap::Entry entry;
            std::string rvaText;
            std::string argsText = "*";

            if (parts.size() >= 5)
            {
                entry.imageName = parts[0];
                entry.className = parts[1];
                entry.methodName = parts[2];
                argsText = parts[3];
                rvaText = parts[4];
            }
            else if (parts.size() == 4)
            {
                entry.className = parts[0];
                entry.methodName = parts[1];
                argsText = parts[2];
                rvaText = parts[3];
            }
            else
            {
                const std::size_t separator = parts[0].rfind("::");
                if (separator == std::string::npos)
                {
                    return std::nullopt;
                }

                entry.className = parts[0].substr(0, separator);
                entry.methodName = parts[0].substr(separator + 2);

                if (parts.size() == 3)
                {
                    argsText = parts[1];
                    rvaText = parts[2];
                }
                else
                {
                    rvaText = parts[1];
                }
            }

            if (!ParseArgumentCount(argsText, &entry.argumentCount) ||
                !ParseInteger(rvaText, &entry.rva) ||
                entry.className.empty() ||
                entry.methodName.empty())
            {
                return std::nullopt;
            }

            return entry;
        }
    }

    ResolveResult ResolveResult::Success(ResolvedAddress resolved)
    {
        ResolveResult result;
        result.value = std::move(resolved);
        return result;
    }

    ResolveResult ResolveResult::Failure(std::string message)
    {
        ResolveResult result;
        result.error = ResolveError{ std::move(message) };
        return result;
    }

    bool MethodMap::Load(const std::wstring& path, std::string* errorMessage)
    {
        entries_.clear();

        std::error_code fileError;
        if (path.empty() ||
            !std::filesystem::exists(std::filesystem::path(path), fileError) ||
            !std::filesystem::is_regular_file(std::filesystem::path(path), fileError))
        {
            if (errorMessage)
            {
                *errorMessage = "Unable to open method map.";
            }
            return false;
        }

        const std::wstring extension = ToLowerWide(std::filesystem::path(path).extension().wstring());
        bool loaded = false;
        if (extension == L".json")
        {
            loaded = LoadScriptJsonMethodMap(path, &entries_);
        }
        else if (extension == L".cs")
        {
            loaded = LoadDumpCsMethodMap(path, &entries_);
        }
        else
        {
            loaded = LoadTextMethodMap(path, &entries_);
        }

        if (!loaded)
        {
            if (errorMessage)
            {
                *errorMessage = "Unable to read method map.";
            }
            return false;
        }

        if (entries_.empty())
        {
            if (errorMessage)
            {
                *errorMessage =
                    "No method map entries were loaded. Supported inputs: "
                    "methods.txt style entries, Il2CppDumper script.json, or Il2CppDumper dump.cs.";
            }
            return false;
        }

        return true;
    }

    bool MethodMap::IsLoaded() const
    {
        return !entries_.empty();
    }

    void MethodMap::SetEntries(std::vector<Entry> entries)
    {
        entries_ = std::move(entries);
    }

    std::size_t MethodMap::Count() const
    {
        return entries_.size();
    }

    const std::vector<MethodMap::Entry>& MethodMap::Entries() const
    {
        return entries_;
    }

    std::optional<ResolvedAddress> MethodMap::Find(const MethodQuery& query, const ModuleInfo* module) const
    {
        for (const Entry& entry : entries_)
        {
            const bool imageMatches = query.imageName.empty() ||
                entry.imageName.empty() ||
                NormalizeImageName(entry.imageName) == NormalizeImageName(query.imageName);
            const bool classMatches = EqualsInsensitive(entry.className, query.className);
            const bool methodMatches = EqualsInsensitive(entry.methodName, query.methodName);
            const bool argsMatch = query.argumentCount < 0 ||
                entry.argumentCount < 0 ||
                entry.argumentCount == query.argumentCount;

            if (!imageMatches || !classMatches || !methodMatches || !argsMatch)
            {
                continue;
            }

            if (entry.kind == MethodMapEntryKind::MonoMetadataToken)
            {
                return ResolvedAddress{
                    0,
                    entry.rva,
                    entry.metadataToken,
                    false,
                    entry.rva != 0,
                    true,
                    WidenAscii(entry.imageName),
                    "mono-metadata",
                    entry.sourcePath
                };
            }

            if (!module)
            {
                return ResolvedAddress{
                    0,
                    entry.rva,
                    entry.metadataToken,
                    false,
                    true,
                    entry.metadataToken != 0,
                    WidenAscii(entry.imageName),
                    "method-map-rva",
                    entry.sourcePath
                };
            }

            return ResolvedAddress{
                module->base + entry.rva,
                entry.rva,
                entry.metadataToken,
                true,
                true,
                entry.metadataToken != 0,
                module->name,
                "method-map",
                entry.sourcePath
            };
        }

        return std::nullopt;
    }

    ExternalMethodResolver::ExternalMethodResolver(UnityRuntimeModules modules)
        : modules_(std::move(modules))
    {
    }

    void ExternalMethodResolver::SetMethodMap(MethodMap methodMap)
    {
        methodMap_ = std::move(methodMap);
    }

    RuntimeBackend ExternalMethodResolver::Backend() const
    {
        return modules_.Backend();
    }

    bool ExternalMethodResolver::HasMethodMap() const
    {
        return methodMap_ && methodMap_->IsLoaded();
    }

    const ModuleInfo* ExternalMethodResolver::RuntimeModule() const
    {
        if (modules_.HasGameAssembly())
        {
            return &*modules_.gameAssembly;
        }

        if (modules_.HasMono())
        {
            return &*modules_.mono;
        }

        return nullptr;
    }

    ResolveResult ExternalMethodResolver::ResolveRuntimeExport(const std::string& exportName) const
    {
        const ModuleInfo* module = RuntimeModule();
        if (!module)
        {
            return ResolveResult::Failure("No Unity runtime module was found for this process.");
        }

        return ResolveExportFromModule(*module, exportName);
    }

    ResolveResult ExternalMethodResolver::ResolveExportFromModule(const ModuleInfo& module, const std::string& exportName) const
    {
        if (!exportCache_ || exportCachePath_ != module.path)
        {
            std::unordered_map<std::string, std::uint32_t> exports;
            if (!ReadExportRvasFromImage(module.path, &exports))
            {
                return ResolveResult::Failure("Unable to read runtime module export table.");
            }

            exportCachePath_ = module.path;
            exportCache_ = std::move(exports);
        }

        const auto found = exportCache_->find(exportName);
        if (found == exportCache_->end())
        {
            return ResolveResult::Failure("Export was not found in the runtime module.");
        }

        return ResolveResult::Success(ResolvedAddress{
            module.base + found->second,
            found->second,
            0,
            true,
            true,
            false,
            module.name,
            "pe-export",
            {}
        });
    }

    ResolveResult ExternalMethodResolver::ResolveMethod(const MethodQuery& query) const
    {
        if (query.className.empty() || query.methodName.empty())
        {
            return ResolveResult::Failure("Both class and method are required.");
        }

        if (Backend() == RuntimeBackend::IL2CPP)
        {
            if (!modules_.gameAssembly)
            {
                return ResolveResult::Failure("GameAssembly.dll was not found.");
            }

            if (methodMap_ && methodMap_->IsLoaded())
            {
                std::optional<ResolvedAddress> resolved = methodMap_->Find(query, &*modules_.gameAssembly);
                if (resolved)
                {
                    return ResolveResult::Success(*resolved);
                }

                return ResolveResult::Failure("Method was not found in the loaded method map.");
            }

            return ResolveResult::Failure(
                "External IL2CPP managed method resolution needs a method map. "
                "Use --map with entries like image|Namespace.Type|Method|argc|0xRVA.");
        }

        if (Backend() == RuntimeBackend::Mono)
        {
            if (methodMap_ && methodMap_->IsLoaded())
            {
                std::optional<ResolvedAddress> resolved = methodMap_->Find(query, nullptr);
                if (resolved)
                {
                    return ResolveResult::Success(*resolved);
                }

                return ResolveResult::Failure("Mono method was not found in the loaded metadata map.");
            }

            return ResolveResult::Failure(
                "External Mono managed method resolution needs a metadata map. "
                "The startup flow can auto-generate one from the target _Data\\Managed assemblies.");
        }

        return ResolveResult::Failure("Unity runtime backend is unknown.");
    }
}
