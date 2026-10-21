/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One typed native teleport transaction without effect rollback
 ****************************************************************************/
#pragma once
#include "NativePedTransformReader.h"
#include "NativePedTeleportAccess.h"

namespace mta::game_de
{
    class NativePedTeleportMemory final
    {
    public:
        using Binding = NativePedTransformReader::Binding;
        using CheckOwner = NativePedTransformReader::CheckOwner;
        using NativeFunction = void (*)(void*, const CVector*, bool);
        using Invoke = ClockResult (*)(NativeFunction, void*, const CVector*, bool, std::uint32_t& exception) noexcept;
        struct Receipt
        {
            std::uint32_t  handle = 0, exceptionCode = 0;
            std::uintptr_t nativePed = 0, intelligence = 0, entry = 0;
            std::uintptr_t matrixBefore = 0, matrixAfter = 0;
            CVector        requested;
            bool           entered = false, returned = false, afterObserved = false;
        };
        // Owner must admit the actual current loaded-world GameThread callback
        // with matching image/generation/depth. Binding or PLAYING9 alone never
        // supplies that scope. The primitive rechecks immediate native pool,
        // receiver and owned-dependency identities before entry and afterward.
        NativePedTeleportMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        ClockResult Apply(std::uint32_t handle, const CVector&, Receipt* receipt = nullptr, Invoke invoke = nullptr) const noexcept;
        // After receipt.entered, callers must never retry/rewind this intent.
        // A failed post-call check may follow real tasks, lists and placement
        // effects; the existing owner must stop its lease and preserve them.

    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
