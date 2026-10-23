/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Install exact filter consumers and one owned native-shaped global
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldIgnoredEntityProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <windows.h>
namespace NativeWorldIgnoredEntityFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t, Writer writer, void* host)
    {
        namespace P = mta::game_de::NativeWorldIgnoredEntityProfile;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePedMetadataProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const std::uintptr_t empty = 0;
        writer(host, P::GlobalRva, &empty, sizeof(empty), PAGE_READWRITE);
    }
}
