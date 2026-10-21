/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate native ped placement facts without an entity wrapper
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
// The shared vector headers require MTA's epsilon definition even when this
// reader is compiled without a game precompiled header.
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <array>

namespace mta::game_de
{
    class NativePedTransformReader final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        struct Value
        {
            NativePedPoolReader::Value ped;
            std::uintptr_t             matrix = 0, positionAddress = 0;
            CVector                    position, velocity;
            // Native matrices use padded rows at0/10/20/30. This is a copy of
            // the twelve real components, not an invented native CMatrix cast.
            std::array<CVector, 4> matrixRows;
        };
        NativePedTransformReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t handle, Value& output) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;
        // Preserve the SDK's actual CVector address. The borrow is restricted
        // to the current admitted callback, before another native operation can
        // replace/recycle the matrix. It is never a deferred lifetime lease.
        ClockResult BorrowPosition(std::uint32_t handle, CVector*& output) const noexcept;

    private:
        ClockResult Capture(std::uint32_t, Value&) const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
    static_assert(sizeof(CVector) == 12 && alignof(CVector) == alignof(float));
}
