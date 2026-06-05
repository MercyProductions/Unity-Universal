#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Aegis::UnityExternal
{
    class ExternalMemoryReader
    {
    public:
        ExternalMemoryReader() = default;
        ~ExternalMemoryReader();

        ExternalMemoryReader(const ExternalMemoryReader&) = delete;
        ExternalMemoryReader& operator=(const ExternalMemoryReader&) = delete;

        ExternalMemoryReader(ExternalMemoryReader&& other) noexcept;
        ExternalMemoryReader& operator=(ExternalMemoryReader&& other) noexcept;

        bool Open(DWORD pid);
        void Close();
        bool IsOpen() const;
        DWORD ProcessId() const;
        DWORD LastErrorCode() const;
        HANDLE ProcessHandle() const;

        bool ReadRaw(uintptr_t address, void* buffer, std::size_t size, std::size_t* bytesRead = nullptr) const;
        std::vector<std::uint8_t> ReadBytes(uintptr_t address, std::size_t size) const;

        template <typename T>
        std::optional<T> Read(uintptr_t address) const
        {
            T value{};
            return ReadRaw(address, &value, sizeof(T)) ? std::optional<T>(value) : std::nullopt;
        }

    private:
        HANDLE process_ = nullptr;
        DWORD pid_ = 0;
        mutable DWORD lastError_ = ERROR_SUCCESS;
    };
}
