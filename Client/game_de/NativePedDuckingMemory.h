/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh named ducking-bit observations over registered generic/player peds
 ****************************************************************************/
#pragma once
#include "NativePedDuckingAccess.h"
namespace mta::game_de
{
    class NativePedDuckingMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedDuckingMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadDucking(const Value&, bool&) const noexcept;
        // Reuse existing metadata/pool/native GT/GC/state owner admission for
        // the supplied registered ped. Only the named boolean flag is returned;
        // no task query, native category, setter, entity factory or lifetime
        // lease is inferred. Any failure preserves the caller's prior bool.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
