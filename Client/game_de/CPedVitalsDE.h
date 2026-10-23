/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CPed vitals wiring through the registered native owner
 ****************************************************************************/
#pragma once
#include "NativePedVitalsAccess.h"
#include <windows.h>
// The existing SDK forward-declares its unscoped bone enum before CPed
// defines it. Keep that legacy MSVC contract local to this include instead
// of weakening warnings for the new backend or changing the shared ABI.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPed.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <type_traits>

namespace mta::game_de
{
    // Only these four genuine SDK methods are implemented. The enclosing
    // future ped/pool owner must supply every remaining CPed method and own
    // registration retirement before this layer can be a usable SDK object.
    class CPedVitalsDE : public CPed
    {
    public:
        float GetHealth() const override;
        void  SetHealth(float health) override;
        float GetArmor() const override;
        void  SetArmor(float armor) override;

    protected:
        CPedVitalsDE(NativePedVitalsAccess& access, const NativePedReferenceToken& token) noexcept : native(access), reference(token) {}
        // Additional qualified SDK slices share this one immutable CPU token;
        // borrowing it adds no native pointer or reference-retirement claim.
        const NativePedReferenceToken& ReferenceToken() const noexcept { return reference; }

    private:
        // The abstract pool lookup slice may validate this immutable binding
        // for a factory-owned borrowed registration. It cannot acquire/release
        // native cells or turn this abstract ped layer into a complete factory.
        friend class CPoolsDE;
        static void Require(ClockResult, const char* operation);
        float       Read(NativePedVitalField, const char* operation) const;
        // Copy only immutable CPU identity, never a raw native pointer or a
        // registration ownership claim. This layer neither acquires/releases
        // the token nor frees native-visible storage from its destructor.
        NativePedVitalsAccess&        native;
        const NativePedReferenceToken reference;
    };
    static_assert(std::is_abstract_v<CPedVitalsDE> && std::is_base_of_v<CPed, CPedVitalsDE>);
}
