/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compile an actual native body with only owned observation shims
 ****************************************************************************/
#include "NativePedRebuildTestHost.h"
// Separate translation units preserve each real reader's private helpers;
// no aggregate namespace alias replaces any guard or production control flow.
#define GetModuleHandleW  NativePedRebuildTestHost::Module
#define ReadProcessMemory NativePedRebuildTestHost::Read
#include "../../Client/game_de/NativeTaskPoolReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
