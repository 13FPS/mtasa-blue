/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw current registered-ped render observation, without target authority
 ****************************************************************************/
#pragma once
#include "NativePedMetadataAccess.h"
namespace mta::game_de
{
    class NativePedRenderMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        NativePedRenderMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const Value&, std::uintptr_t&) const noexcept;
        // The runtime resolves its registered token before and after this
        // immediate read. No raw target value, including null, is dereferenced
        // or promoted into native initialization, node or release authority.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
