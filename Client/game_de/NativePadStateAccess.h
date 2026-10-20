/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reusable checked access to the two verified native pad state blocks
 *****************************************************************************/
#pragma once

#include "NativeInputRouting.h"
#include <limits>
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
            Bytes next{};
            if (!ReadRange(0, next.data(), next.size()))
                return false;
            // A page change or partial OS copy cannot turn an earlier caller
            // snapshot into a mixture of physical and replacement input.
            output = next;
            return true;
        }

        bool Write(const Bytes& input) const noexcept
        {
            // Exactly the adjacent current/previous states are writable. The
            // physical source queues and all enclosing pad fields stay outside
            // this range, including during a failed partial application.
            return WriteRange(0, input.data(), input.size());
        }

        bool ReadCurrent(CControllerState& output) const noexcept { return ReadState(0, output); }

        bool ReadPrevious(CControllerState& output) const noexcept { return ReadState(sizeof(CControllerState), output); }

        bool WriteCurrent(const CControllerState& input) const noexcept
        {
            // MTA shifts history explicitly in its pre-frame pulse. Updating
            // current input must never shift or replay the previous block.
            return WriteRange(0, &input, sizeof(input));
        }

        bool WritePrevious(const CControllerState& input) const noexcept { return WriteRange(sizeof(CControllerState), &input, sizeof(input)); }

        bool Clear() const noexcept
        {
            // SDK Clear only clears the two states. Native broad Clear also
            // resets queues and timers outside this qualified write range.
            return Write(Bytes{});
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
        bool Range(std::size_t offset, std::size_t size, bool writable, std::uintptr_t& target) const noexcept
        {
            // Refuse a malformed borrowed address before arithmetic or the
            // supplied page policy; no state operation may escape 96 bytes.
            if (!address || offset > sizeof(Bytes) || size > sizeof(Bytes) - offset || address > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(Bytes))
                return false;
            target = address + offset;
            return pages && pages(target, size, writable, false);
        }

        bool ReadRange(std::size_t offset, void* output, std::size_t size) const noexcept
        {
            std::uintptr_t target = 0;
            SIZE_T         copied = 0;
            return Range(offset, size, false, target) && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(target), output, size, &copied) &&
                   copied == size;
        }

        bool WriteRange(std::size_t offset, const void* input, std::size_t size) const noexcept
        {
            std::uintptr_t target = 0;
            SIZE_T         copied = 0;
            return Range(offset, size, true, target) && WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(target), input, size, &copied) &&
                   copied == size;
        }

        bool ReadState(std::size_t offset, CControllerState& output) const noexcept
        {
            CControllerState next{};
            if (!ReadRange(offset, &next, sizeof(next)))
                return false;
            output = next;
            return true;
        }

        const std::uintptr_t address;
        const PageCheck      pages;
    };
}
