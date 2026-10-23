/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe each genuine orientation phase without replay or compensation
 ****************************************************************************/
#pragma once
#include "NativePedTransformReader.h"
#include <array>
namespace mta::game_de
{
    class NativePedOrientationMemory final
    {
    public:
        using Binding = NativePedTransformReader::Binding;
        using CheckOwner = NativePedTransformReader::CheckOwner;
        enum class Phase : std::uint8_t
        {
            Remove,
            Rotate,
            Add
        };
        using WorldFunction = void (*)(void*);
        using RotateFunction = void (*)(void*, float, float, float);
        struct Entries
        {
            WorldFunction  remove;
            RotateFunction rotate;
            WorldFunction  add;
        };
        using Invoke = ClockResult (*)(Phase, const Entries&, void*, const CVector&, std::uint32_t&) noexcept;
        struct Receipt
        {
            std::uint32_t  handle = 0, exceptionCode = 0;
            std::uintptr_t nativePed = 0, matrixBefore = 0, matrixAfter = 0;
            CVector        requested;
            std::uint8_t   enteredMask = 0, returnedMask = 0;
            bool           entered = false, afterObserved = false;
        };
        NativePedOrientationMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Apply(std::uint32_t handle, const CVector& radians, Receipt* = nullptr, Invoke = nullptr) const noexcept;
        // Every entered phase is persistent. A failed native acknowledgement
        // or fresh post-call relation requires the original owner to Stop;
        // neither Add compensation nor replay of an accepted prefix is safe.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
