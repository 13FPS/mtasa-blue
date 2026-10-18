/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the established native-image guards across DE game services
 *****************************************************************************/

#include "NativeImageAccess.h"
#include <array>
#include <limits>
#include <bcrypt.h>

namespace mta::game_de::NativeImageAccess
{
    namespace
    {
        constexpr std::array<unsigned char, 32> testedBuild = {0x63, 0xe6, 0x33, 0xbb, 0x51, 0xc3, 0x5f, 0xe3, 0x5c, 0xa9, 0x8f, 0x50, 0x3b, 0x62, 0xe7, 0xdd,
                                                               0xb8, 0x40, 0x3a, 0xbe, 0x4c, 0x9d, 0x50, 0x1c, 0xfa, 0x91, 0x62, 0x89, 0x24, 0x53, 0x84, 0x6f};
        struct HashResources
        {
            HANDLE             file = INVALID_HANDLE_VALUE;
            BCRYPT_ALG_HANDLE  algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;

            ~HashResources()
            {
                if (hash)
                    BCryptDestroyHash(hash);
                if (algorithm)
                    BCryptCloseAlgorithmProvider(algorithm, 0);
                if (file != INVALID_HANDLE_VALUE)
                    CloseHandle(file);
            }
        };

    }

    bool CheckExecutableBuild() noexcept
    {
        std::array<wchar_t, 32768> path;
        const auto                 length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!length || length >= path.size())
            return false;

        HashResources resources;
        // Use the existing bridge's exact-build hash policy without importing
        // its CLEO or transport lifetime into the MTA-owned backend.
        resources.file = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (resources.file == INVALID_HANDLE_VALUE || BCryptOpenAlgorithmProvider(&resources.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptCreateHash(resources.algorithm, &resources.hash, nullptr, 0, nullptr, 0, 0) < 0)
            return false;
        std::array<unsigned char, 65536> buffer;
        for (;;)
        {
            DWORD count = 0;
            if (!ReadFile(resources.file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
                return false;
            if (!count)
                break;
            if (BCryptHashData(resources.hash, buffer.data(), count, 0) < 0)
                return false;
        }
        std::array<unsigned char, 32> digest;
        return BCryptFinishHash(resources.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0 && digest == testedBuild;
    }

    bool ImagePages(std::uintptr_t image, std::uintptr_t address, std::size_t bytes, bool writable, bool executable) noexcept
    {
        if (!image || address < image || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
            return false;
        const auto end = address + bytes;
        while (address < end)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                info.Type != MEM_IMAGE || reinterpret_cast<std::uintptr_t>(info.AllocationBase) != image || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return false;
            const auto protection = info.Protect & 0xff;
            const bool canRead = protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
                                 protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
            const bool canWrite =
                protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
            const bool canExecute = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
            if (!canRead || (writable && (!canWrite || canExecute)) || (executable && !canExecute))
                return false;
            const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
            if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - region || region + info.RegionSize <= address)
                return false;
            const auto regionEnd = region + info.RegionSize;
            address = regionEnd < end ? regionEnd : end;
        }
        return true;
    }

    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable, bool executable) noexcept
    {
        if (!ImagePages(image, address, bytes, writable, executable))
            return false;
        SIZE_T copied = 0;
        // The OS read also handles a protection change between VirtualQuery
        // and copying; failed reads never publish partially copied states.
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), destination, bytes, &copied) && copied == bytes;
    }

    bool SectionContains(const IMAGE_SECTION_HEADER& section, std::size_t rva, std::size_t bytes, DWORD required, DWORD forbidden) noexcept
    {
        return (section.Characteristics & required) == required && !(section.Characteristics & forbidden) && rva >= section.VirtualAddress &&
               rva - section.VirtualAddress <= section.Misc.VirtualSize && bytes <= section.Misc.VirtualSize - (rva - section.VirtualAddress);
    }

}
