/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One test host across separately compiled actual native readers
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativePedRebuildCalls.h"
#include <windows.h>
namespace NativePedRebuildTestHost
{
    HMODULE WINAPI            Module(LPCWSTR);
    BOOL WINAPI               Read(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
    mta::game_de::ClockResult Invoke(mta::game_de::NativePedRebuildCalls::NativeFunction, std::uintptr_t, std::uint32_t&) noexcept;
}
