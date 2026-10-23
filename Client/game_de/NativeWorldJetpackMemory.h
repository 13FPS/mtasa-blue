/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the genuine consumed jetpack height float under the current native owner
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de::NativeWorldJetpackMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Receipt
    {
        std::uintptr_t limitRva = 0, root = 0, gameMode = 0, level = 0, world = 0;
        float          value = 0;
        bool           read = false;
    };
    // Read the native float each time. Neither the disk value100 nor a prior
    // snapshot substitutes for a refused read. This admits no engine entry,
    // shared-literal write, operand redirection, retry, rollback or Stop effect.
    ClockResult Read(std::uintptr_t image, float&, CheckOwner, Receipt* = nullptr) noexcept;
}
