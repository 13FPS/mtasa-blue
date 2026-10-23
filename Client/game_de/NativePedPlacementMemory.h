/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified raw ped placement with persistent component receipts
 ****************************************************************************/
#pragma once
#include "NativePedTransformReader.h"
#include "NativePedMetadataMemory.h"
#include <array>
namespace mta::game_de
{
    class NativePedPlacementMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Ped = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t                handle = 0, exceptionCode = 0;
            std::uint16_t                modelIndex = 0;
            std::uintptr_t               nativePed = 0, matrix = 0, address = 0;
            std::array<std::uint32_t, 3> original{}, target{}, observed{};
            std::uint8_t                 attemptedMask = 0, appliedMask = 0, unchangedMask = 0, observedMask = 0, foreignMask = 0;
            bool                         entered = false, verified = false;
        };
        NativePedPlacementMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Set(const Ped&, const CVector&, Receipt* = nullptr) const noexcept;
        // Only original inline/matrix XYZ storage is owned. Basis/padding,
        // heading, tasks, world lists and actor/RW refresh are not modified.
        // appliedMask is acknowledgement, not rollback authority. Any entered
        // failure must stop the existing owner without replay or restoration.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
