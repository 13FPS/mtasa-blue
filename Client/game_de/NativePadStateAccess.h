/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reusable checked access to the two verified native pad state blocks
 *****************************************************************************/
#pragma once

#include "NativeInputRouting.h"
#include <windows.h>

namespace mta::game_de
{
    // This is the verified controller-state portion of a pad, not a CPad
    // implementation. Whole-pad storage, rumble, horn history and updater
    // patches need their own native evidence before the full SDK can use them.
    // The caller supplies its already qualified address/page policy and retains
    // all engine-thread, generation and actor admission around every operation.
    class NativePadStateAccess final
    {
    public:
        using Bytes = NativeInputRouting::PadStateBytes;
        using PageCheck = bool (*)(std::uintptr_t, std::size_t, bool, bool) noexcept;

        NativePadStateAccess(std::uintptr_t stateAddress, PageCheck pageCheck) noexcept : address(stateAddress), pages(pageCheck) {}

        bool Read(Bytes& output) const noexcept
        {
            Bytes  next{};
            SIZE_T copied = 0;
            if (!pages || !pages(address, next.size(), false, false) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), next.data(), next.size(), &copied) || copied != next.size())
                return false;
            // A page change or partial OS copy cannot turn an earlier caller
            // snapshot into a mixture of physical and replacement input.
            output = next;
            return true;
        }

        bool Write(const Bytes& input) const noexcept
        {
            SIZE_T copied = 0;
            // Exactly the adjacent current/previous states are writable. The
            // physical source queues and all enclosing pad fields stay outside
            // this range, including during a failed partial application.
            return pages && pages(address, input.size(), true, false) &&
                   WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), input.data(), input.size(), &copied) && copied == input.size();
        }

        bool Prepare(NativeInputRouting::PadTransaction& transaction, const MtaInputState& input, Bytes& replacement) const noexcept
        {
            Bytes physical{};
            return Read(physical) && transaction.Prepare(physical, input, replacement);
        }

        bool Restore(NativeInputRouting::PadTransaction& transaction) const noexcept
        {
            Bytes original{}, observed{};
            // Write failure and mismatching readback keep the transaction's
            // original snapshot alive for its owner's explicit cleanup retry.
            return transaction.RestoreBytes(original) && Write(original) && Read(observed) && transaction.ConfirmRestored(observed);
        }

    private:
        const std::uintptr_t address;
        const PageCheck      pages;
    };
}
