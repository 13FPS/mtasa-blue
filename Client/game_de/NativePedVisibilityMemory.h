/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh named visibility-bit observations over registered generic/player peds
 ****************************************************************************/
#pragma once
#include "NativePedVisibilityAccess.h"
namespace mta::game_de
{
    class NativePedVisibilityMemory final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using Value = NativePedPoolReader::Value;
        struct Receipt
        {
            std::uint32_t  handle = 0, original = 0, target = 0, observed = 0, exceptionCode = 0, attempts = 0;
            std::uintptr_t nativePed = 0, address = 0;
            bool           entered = false, accepted = false, observedValid = false, verified = false;
        };
        NativePedVisibilityMemory(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadVisible(const Value&, bool&) const noexcept;
        // Reuse existing metadata/pool/native GT/GC/state owner admission for
        // the supplied registered ped. Only the named boolean flag is returned;
        // no task query, native category, entity factory or lifetime
        // lease is inferred. Any failure preserves the caller's prior bool.
        ClockResult SetVisible(const Value&, bool, Receipt* = nullptr) const noexcept;
        // Actual CEntitySA changes only this bool. The script command
        // additionally changes a local-player global; that is not SDK policy. One masked CAS retains all
        // neighboring flags and never retries a competing engine word. The
        // retained owner Stops on entered failure; accepted or unknown effects
        // persist without render/component updates, repair, rollback or replay.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
