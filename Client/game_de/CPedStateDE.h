/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA wrapper identity and qualified selected-ped state
 ****************************************************************************/
#pragma once
#include "CPedRotationDE.h"
#include "NativePedOxygenAccess.h"
#include "NativePedDuckingAccess.h"

namespace mta::game_de
{
    // The CPU wrapper type drives the classic pool's wrapper destruction; it
    // is unrelated to native ped categories or the entity type. Oxygen and
    // ducking use the same registered native lifetime as the existing chain.
    // Target rotation, SetDucking, repair and the remaining SDK stay pure.
    class CPedStateDE : public CPedRotationDE
    {
    public:
        float         GetOxygenLevel() const override;
        void          SetOxygenLevel(float oxygen) override;
        bool          IsDucking() const override;
        std::uint32_t GetType() const noexcept override;
        int           GetCustomMoveAnim() const noexcept override;
        // Preserve the real CPedSA bookkeeping operation without translating
        // it into native memory or changing this wrapper's registered token.
        void SetType(std::uint32_t type) noexcept { wrapperTypeValue = type; }

    protected:
        CPedStateDE(NativePedVitalsAccess& vitals, NativePedMetadataAccess& metadata, NativePedMutationAccess& mutation,
                    NativePedObservationAccess& observation, NativePedRotationAccess& rotation, NativePedOxygenAccess& oxygen, NativePedDuckingAccess& ducking,
                    const NativePedReferenceToken& token, std::uint32_t wrapperType, NativePedPhysicalScalarsAccess* scalars = nullptr) noexcept
            : CPedRotationDE(vitals, metadata, mutation, observation, rotation, token, scalars),
              oxygenAccess(oxygen),
              duckingAccess(ducking),
              wrapperTypeValue(wrapperType)
        {
        }

        // Classic CPlayerPed's producer manages block references and applies
        // native animation separately. Preserve its protected signed default
        // and bookkeeping without pretending to perform those engine effects.
        int m_iCustomMoveAnim{0};

    private:
        static void             Require(ClockResult, const char* operation);
        NativePedOxygenAccess&  oxygenAccess;
        NativePedDuckingAccess& duckingAccess;
        std::uint32_t           wrapperTypeValue;
    };
    static_assert(std::is_abstract_v<CPedStateDE> && std::is_base_of_v<CPed, CPedStateDE>);
}
