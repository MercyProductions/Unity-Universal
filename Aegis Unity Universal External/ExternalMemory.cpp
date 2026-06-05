#include "ExternalMemory.hpp"

#include <utility>

namespace Aegis::UnityExternal
{
    ExternalMemoryReader::~ExternalMemoryReader()
    {
        Close();
    }

    ExternalMemoryReader::ExternalMemoryReader(ExternalMemoryReader&& other) noexcept
        : process_(std::exchange(other.process_, nullptr)),
          pid_(std::exchange(other.pid_, 0)),
          lastError_(std::exchange(other.lastError_, ERROR_SUCCESS))
    {
    }

    ExternalMemoryReader& ExternalMemoryReader::operator=(ExternalMemoryReader&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            process_ = std::exchange(other.process_, nullptr);
            pid_ = std::exchange(other.pid_, 0);
            lastError_ = std::exchange(other.lastError_, ERROR_SUCCESS);
        }

        return *this;
    }

    bool ExternalMemoryReader::Open(DWORD pid)
    {
        Close();
        process_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!process_)
        {
            lastError_ = GetLastError();
            return false;
        }

        pid_ = pid;
        lastError_ = ERROR_SUCCESS;
        return true;
    }

    void ExternalMemoryReader::Close()
    {
        if (process_)
        {
            CloseHandle(process_);
            process_ = nullptr;
        }

        pid_ = 0;
    }

    bool ExternalMemoryReader::IsOpen() const
    {
        return process_ != nullptr;
    }

    DWORD ExternalMemoryReader::ProcessId() const
    {
        return pid_;
    }

    DWORD ExternalMemoryReader::LastErrorCode() const
    {
        return lastError_;
    }

    bool ExternalMemoryReader::ReadRaw(uintptr_t address, void* buffer, std::size_t size, std::size_t* bytesRead) const
    {
        if (!process_ || !buffer || size == 0)
        {
            lastError_ = ERROR_INVALID_PARAMETER;
            return false;
        }

        SIZE_T localBytesRead = 0;
        const BOOL ok = ReadProcessMemory(
            process_,
            reinterpret_cast<LPCVOID>(address),
            buffer,
            size,
            &localBytesRead);

        if (bytesRead)
        {
            *bytesRead = static_cast<std::size_t>(localBytesRead);
        }

        if (!ok)
        {
            lastError_ = GetLastError();
            return false;
        }

        lastError_ = ERROR_SUCCESS;
        return localBytesRead == size;
    }

    std::vector<std::uint8_t> ExternalMemoryReader::ReadBytes(uintptr_t address, std::size_t size) const
    {
        std::vector<std::uint8_t> bytes(size);
        std::size_t bytesRead = 0;
        if (!ReadRaw(address, bytes.data(), bytes.size(), &bytesRead))
        {
            bytes.resize(bytesRead);
        }

        return bytes;
    }
}
