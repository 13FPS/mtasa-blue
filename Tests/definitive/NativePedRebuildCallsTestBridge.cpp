/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compile an actual native body with only owned observation shims
 ****************************************************************************/
#include "NativePedRebuildTestHost.h"
// Separate translation units preserve each real reader's private helpers;
// no aggregate namespace alias replaces any guard or production control flow.
#define GetModuleHandleW                 NativePedRebuildTestHost::Module
#define ReadProcessMemory                NativePedRebuildTestHost::Read
#define MTA_DE_PED_REBUILD_NATIVE_INVOKE ::NativePedRebuildTestHost::Invoke
#include "../../Client/game_de/NativePedRebuildCalls.cpp"
#undef MTA_DE_PED_REBUILD_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
