/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact reference-consumer pins for the existing block image fixture
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeAnimationBlockRefProfile.h"
#include <windows.h>
namespace NativeAnimationBlockRefFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        (void)image;
        for (const auto& pin : mta::game_de::NativeAnimationBlockRefProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
}
