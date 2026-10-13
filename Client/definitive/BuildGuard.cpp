/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "Bridge.h"
#include <array>
#include <windows.h>
#include <bcrypt.h>

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

int DE_CheckExecutableBuild(const wchar_t* path)
{
    if (!path || !*path)
        return 0;
    HashResources resources;
    // Read-only verification fails closed when Steam updates the executable.
    // Denying shared writes also prevents a concurrent updater changing its bytes
    // during this check. Hashing never changes the commercial executable.
    resources.file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (resources.file == INVALID_HANDLE_VALUE || BCryptOpenAlgorithmProvider(&resources.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(resources.algorithm, &resources.hash, nullptr, 0, nullptr, 0, 0) < 0)
        return 0;
    std::array<unsigned char, 65536> buffer;
    for (;;)
    {
        DWORD count = 0;
        if (!ReadFile(resources.file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
            return 0;
        if (!count)
            break;
        if (BCryptHashData(resources.hash, buffer.data(), count, 0) < 0)
            return 0;
    }
    std::array<unsigned char, 32> digest;
    return BCryptFinishHash(resources.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0 && digest == testedBuild ? 1 : 0;
}

int DE_CheckGameBuild()
{
    // The process executable does not change across script resets. Cache this
    // read-only startup check so binding does not hash it twice on the game thread.
    static int verified = -1;
    if (verified >= 0)
        return verified;
    std::array<wchar_t, 32768> path;
    const auto                 length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        return 0;
    verified = DE_CheckExecutableBuild(path.data());
    return verified;
}
