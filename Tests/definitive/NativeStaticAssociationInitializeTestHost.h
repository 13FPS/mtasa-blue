/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observation and original-call injection for owned initializer tests
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeStaticAssociationInitializeCalls.h"
#include <windows.h>
namespace NativeStaticAssociationInitializeTestHost
{
    HMODULE WINAPI            Module(LPCWSTR);
    BOOL WINAPI               Read(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
    mta::game_de::ClockResult Invoke(mta::game_de::NativeStaticAssociationInitializeCalls::NativeFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                     std::uint32_t&) noexcept;
}
