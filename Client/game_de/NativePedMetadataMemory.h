/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh native ped model/type observations without model lookup
 ****************************************************************************/
#pragma once
#include "NativePedMetadataAccess.h"

namespace mta::game_de
{
    class NativePedMetadataMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedMetadataMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const Value&, NativePedMetadata&) const noexcept;

        // Input is the actual owner's fresh registered-token resolution, not
        // a retained raw pointer. The primitive independently bookends the same
        // occupied generic/player pool identity, native thread/state and owner.
        // A raw model WORD, including ffff, grants no model-bank dereference,
        // native model setter, factory or complete CPed ownership contract.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
