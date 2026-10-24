/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real saved hierarchy pins and owned row40 data for current block hosts
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeAnimationBlockHierarchyProfile.h"
#include "../../Client/game_de/NativeAnimationManagerProfile.h"
#include <cstring>
#include <windows.h>
namespace NativeAnimationBlockFactoryFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        (void)image;
        for (const auto& pin : mta::game_de::NativeAnimationManagerProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto& fold = mta::game_de::NativeAnimationManagerProfile::NameFold;
        writer(host, mta::game_de::NativeAnimationManagerProfile::NameFoldRva, fold.data(), fold.size(), PAGE_READONLY);
        for (const auto& pin : mta::game_de::NativeAnimationBlockHierarchyProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    inline void SeedHierarchy(std::uintptr_t image, Writer writer, void* host, std::uint32_t globalIndex, std::uint32_t blockIndex)
    {
        (void)image;
        namespace Profile = mta::game_de::NativeAnimationBlockHierarchyProfile;
        std::array<unsigned char, Profile::RowBytes> row{};
        const std::uint32_t                          hash = 0x13579bdfu;
        std::memcpy(row.data(), &hash, sizeof(hash));
        std::memcpy(row.data() + Profile::OwningBlockOffset, &blockIndex, sizeof(blockIndex));
        writer(host, Profile::BankRva + static_cast<std::uintptr_t>(globalIndex) * Profile::RowBytes, row.data(), row.size(), PAGE_READWRITE);
    }
}
