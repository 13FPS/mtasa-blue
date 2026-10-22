/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw current angle over an immediately resolved occupied native ped
 ****************************************************************************/
#pragma once
#include "NativePedRotationAccess.h"
namespace mta::game_de
{
    class NativePedRotationMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0;
            std::uintptr_t nativePed = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedRotationMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCurrent(const Value&, float&) const noexcept;
        ClockResult SetCurrent(const Value&, float, Receipt* = nullptr) const noexcept;
        // The caller must pass its fresh registered-token resolution. Both
        // supported ped classes reuse actual metadata/pool/GT/GC/state owner
        // admission; no local-player pointer or retained native lease exists.
        // Preserve every raw binary32 bit as the classic SDK does. This is
        // neither script heading conversion/normalization nor matrix update,
        // and it grants no write to the separately owned adjacent angle.
        // Entered failure can retain persistent or unknown partial effects;
        // the real owner must stop without replay or restoring old ped state.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
