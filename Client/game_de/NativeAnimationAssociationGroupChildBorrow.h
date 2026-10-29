/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Expire group-owned CPU child borrows at native replacement entry
 ****************************************************************************/
#pragma once
namespace mta::game_de
{
    // This callback grants no native authority. The exact group replacement
    // ledger admits its row before invoking the already-bound CPU owner.
    struct NativeAnimationAssociationGroupChildBorrowInvalidator
    {
        void* context = nullptr;
        void (*invalidate)(void*) noexcept = nullptr;
        void operator()() const noexcept
        {
            if (invalidate)
                invalidate(context);
        }
    };
}
