/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep native reference receipts when source wrapper counters cannot commit
 *****************************************************************************/
#include "CModelInfoReferenceDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CModelInfoReferenceDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE model backing owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    CModelInfoReferenceDE::CModelInfoReferenceDE(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game,
                                                 NativeModelBackingReferenceAccess& owner, const NativeModelBackingReferenceToken& issued)
        : CModelInfoRequestDE(model, classification, game), referenceOwner(owner), reference(issued)
    {
        if (!reference.ownerGeneration || !reference.serial || reference.modelIndex != model ||
            dynamic_cast<void*>(&classification) != dynamic_cast<void*>(&owner))
            throw std::invalid_argument("DE model backing reference requires the exact issuing complete owner and model token");
        // The owner reserved this token before partial facade construction.
        // Bind consumes only its CPU claim; failure performs no native cleanup.
        Require(referenceOwner.BindModelBackingReference(reference), "Bind");
    }
    NativeModelBackingReferenceState CModelInfoReferenceDE::ReadReferenceState()
    {
        NativeModelBackingReferenceState state;
        Require(referenceOwner.ReadModelBackingReference(reference, state), "Read");
        if (state.reference.ownerGeneration != reference.ownerGeneration || state.reference.serial != reference.serial ||
            state.reference.modelIndex != reference.modelIndex)
            throw std::runtime_error("DE model backing owner returned a different capability");
        return state;
    }
    bool CModelInfoReferenceDE::DoIsLoaded()
    {
        const auto state = ReadReferenceState();
        if (state.phase != NativeModelBackingReferencePhase::Bound && state.phase != NativeModelBackingReferencePhase::Held &&
            state.phase != NativeModelBackingReferencePhase::Released)
            throw std::runtime_error("DE model backing reference is not a current usable issued capability");
        // Readiness alone needs no backing acquire. A consumed serial stays
        // consumed; the actual Increment hook reserves the next cycle only
        // when the unchanged source policy is ready to acquire it.
        return CModelInfoRequestDE::DoIsLoaded();
    }
    void CModelInfoReferenceDE::IncrementNativeModelInterfaceReference()
    {
        auto state = ReadReferenceState();
        if (state.phase == NativeModelBackingReferencePhase::Released)
        {
            NativeModelBackingReferenceToken next;
            Require(referenceOwner.RenewModelBackingReferenceCycle(reference, next), "RenewCycle");
            if (!next.serial || next.serial == reference.serial || next.ownerGeneration != reference.ownerGeneration || next.modelIndex != reference.modelIndex)
                throw std::runtime_error("DE model backing renewal returned an invalid cycle");
            NativeModelBackingReferenceState prepared;
            Require(referenceOwner.ReadModelBackingReference(next, prepared), "ReadRenewedCycle");
            if (prepared.reference.ownerGeneration != next.ownerGeneration || prepared.reference.serial != next.serial ||
                prepared.reference.modelIndex != next.modelIndex || !state.familySerial || prepared.familySerial != state.familySerial ||
                prepared.phase != NativeModelBackingReferencePhase::Bound)
                throw std::runtime_error("DE model backing renewal crossed its issued family");
            reference = next;
            state = prepared;
        }
        if (state.phase != NativeModelBackingReferencePhase::Bound)
            throw std::runtime_error("DE model backing cycle is unreleased or uncertain; acquisition cannot replay");
        // Source CPU refs/pending commit only after the durable owner returns.
        Require(referenceOwner.AcquireModelBackingReference(reference), "Acquire");
    }
    void CModelInfoReferenceDE::ClaimUnmodifiedFamily()
    {
        Require(referenceOwner.ClaimUnmodifiedModelFamily(reference), "ClaimUnmodifiedFamily");
        const auto state = ReadReferenceState();
        if (!state.familySerial || !state.unmodifiedFamily || state.phase != NativeModelBackingReferencePhase::Bound)
            throw std::runtime_error("DE model owner did not issue an unmodified family");
    }
    void CModelInfoReferenceDE::ReleaseUnmodifiedBackingReference()
    {
        const auto state = ReadReferenceState();
        if (!state.familySerial || !state.unmodifiedFamily || state.phase != NativeModelBackingReferencePhase::Held)
            throw std::runtime_error("DE unmodified model has no acknowledged held cycle to release");
        Require(referenceOwner.ReleaseModelBackingReference(reference, NativeModelReferenceRelease::ZeroPolicy::RemoveAtZero), "ReleaseUnmodified");
    }
}
