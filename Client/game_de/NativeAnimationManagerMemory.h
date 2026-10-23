/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate current native counts and bounded first-match name lookup
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerAccess.h"
namespace mta::game_de
{
    class NativeAnimationManagerMemory
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        NativeAnimationManagerMemory(Binding value, CheckOwner callback) noexcept : binding(value), owner(callback) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCounts(NativeAnimationManagerCounts&) const noexcept;
        ClockResult FindBlockIndex(const NativeAnimationBlockName&, std::int32_t&) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner;
    };
}
