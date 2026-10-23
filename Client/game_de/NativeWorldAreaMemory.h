/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the actual world area DWORD inside the existing native owner
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de::NativeWorldAreaMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Receipt
    {
        std::uintptr_t areaRva = 0, root = 0, gameMode = 0, level = 0, world = 0;
        std::uint32_t  value = 0;
        bool           read = false;
    };
    // This is a guarded copy, not a script/getter call. A zero value is a real
    // area, never an uninitialized/failed read fallback. There are no native
    // scan, matrix, setter, removal or rollback effects to claim or replay.
    ClockResult Read(std::uintptr_t image, std::uint32_t&, CheckOwner, Receipt* = nullptr) noexcept;
}
