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
    bool CModelInfoReferenceDE::DoIsLoaded()
    {
        NativeModelBackingReferenceState state;
        Require(referenceOwner.ReadModelBackingReference(reference, state), "Read");
        if (state.reference.ownerGeneration != reference.ownerGeneration || state.reference.serial != reference.serial ||
            state.reference.modelIndex != reference.modelIndex ||
            (state.phase != NativeModelBackingReferencePhase::Bound && state.phase != NativeModelBackingReferencePhase::Held))
            throw std::runtime_error("DE model backing reference is not a current usable issued capability");
        // Preserve the existing streaming/category/render policy after the
        // facade lifetime gate. CPU GetRefCount needs no native observation.
        return CModelInfoRequestDE::DoIsLoaded();
    }
    void CModelInfoReferenceDE::IncrementNativeModelInterfaceReference()
    {
        // Source counter/pending acknowledgement commits only after this
        // returns. The durable owner, not those unchanged CPU counters, records
        // accepted/lost-ACK effects and prevents acquisition in a later epoch.
        Require(referenceOwner.AcquireModelBackingReference(reference), "Acquire");
    }
}
