/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep actual independent readers and their private helper scopes
 ****************************************************************************/
#include "NativeStaticAssociationInitializeTestHost.h"
#define GetModuleHandleW  NativeStaticAssociationInitializeTestHost::Module
#define ReadProcessMemory NativeStaticAssociationInitializeTestHost::Read
#include "../../Client/game_de/NativePedRenderMemory.cpp"
#include "../../Client/game_de/NativeAnimationBlockMemory.cpp"
#include "../../Client/game_de/NativeAnimationManagerMemory.cpp"
#include "../../Client/game_de/NativeAnimationHierarchyMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
