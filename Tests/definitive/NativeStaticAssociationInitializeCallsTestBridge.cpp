/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compile the actual primitive with only observation/call injection
 ****************************************************************************/
#include "NativeStaticAssociationInitializeTestHost.h"
#define GetModuleHandleW                                   NativeStaticAssociationInitializeTestHost::Module
#define ReadProcessMemory                                  NativeStaticAssociationInitializeTestHost::Read
#define MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE ::NativeStaticAssociationInitializeTestHost::Invoke
#include "../../Client/game_de/NativeStaticAssociationInitializeCalls.cpp"
#undef MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
