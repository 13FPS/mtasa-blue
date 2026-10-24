/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact initializer/literal image pins for existing native name hosts
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeClothesDescriptorProfile.h"
#include <windows.h>
namespace NativeClothesDescriptorFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        (void)image;
        for (const auto& pin : mta::game_de::NativeClothesDescriptorProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeClothesDescriptorProfile::Names)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_READONLY);
    }
}
