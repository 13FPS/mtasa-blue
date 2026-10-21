/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked native vector stores with honest persistent partial effects
 ****************************************************************************/
#pragma once
#include "NativePedTransformReader.h"
#include "NativePedVelocityAccess.h"
#include <array>

namespace mta::game_de
{
    class NativePedVelocityMemory final
    {
    public:
        using Binding = NativePedTransformReader::Binding;
        using CheckOwner = NativePedTransformReader::CheckOwner;
        struct Receipt
        {
            std::uint32_t                handle = 0, exceptionCode = 0;
            NativePedVelocityKind        kind = NativePedVelocityKind::Linear;
            std::uintptr_t               nativePed = 0, address = 0;
            std::array<std::uint32_t, 3> original{}, target{}, observed{};
            std::uint8_t                 attemptedMask = 0, appliedMask = 0, unchangedMask = 0, observedMask = 0, foreignMask = 0;
            bool                         entered = false, verified = false;
        };
        // observed is the last qualified whole-vector copy, not a promise of
        // current bytes after a refused CAS or a subsequent native advance.
        NativePedVelocityMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t handle, NativePedVelocityKind, CVector&) const noexcept;
        ClockResult Set(std::uint32_t handle, NativePedVelocityKind, const CVector&, Receipt* = nullptr) const noexcept;
        // A CAS attempt is an entered write even when acknowledgement faults.
        // appliedMask records only confirmed accepted components. Partial
        // effects persist; the actual owner must stop on entered non-Ok and
        // never replay/rewind the vector. A native seven-bit pool generation
        // supplies no later callback lease to repair this row.

    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
