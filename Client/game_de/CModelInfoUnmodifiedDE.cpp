/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Apply real reference release only from a positively owned empty domain
 *****************************************************************************/
#include "CModelInfoUnmodifiedDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    CModelInfoUnmodifiedDE::CModelInfoUnmodifiedDE(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game,
                                                   NativeModelBackingReferenceAccess& owner, const NativeModelBackingReferenceToken& reference,
                                                   CCoreInterface& core)
        : CModelInfoDiagnosticsDE(model, classification, game, owner, reference, core)
    {
        ClaimUnmodifiedFamily();
    }
    void CModelInfoUnmodifiedDE::RemoveNativeModelInterfaceReference()
    {
        // Source Remove tests these wrapper-owned fields after decrement.
        // Here their actual initialized-empty state is immutable and every
        // replacement entry point refuses. This grants no native/global claim.
        if (replacements.customClump || replacements.customCollision)
            throw std::logic_error("DE unmodified model replacement ownership changed");
        ReleaseUnmodifiedBackingReference();
    }
    bool CModelInfoUnmodifiedDE::UnloadUnused()
    {
        // Source UnloadUnused tests its own replacement fields, never the
        // wrapper CPU refcount. The same owner distinguishes a positive native
        // count (false) from a verified zero-count removal (true) or refusal.
        if (replacements.customClump || replacements.customCollision)
            throw std::logic_error("DE unmodified model replacement ownership changed");
        bool removed = false;
        Require(referenceOwner.UnloadUnusedModelFamily(reference, removed), "UnloadUnused");
        return removed;
    }
    [[noreturn]] void CModelInfoUnmodifiedDE::ReplacementUnsupported(const char* operation)
    {
        throw std::runtime_error(std::string("DE unmodified model does not support ") + operation);
    }
    bool CModelInfoUnmodifiedDE::SetCustomModel(RpClump*)
    {
        ReplacementUnsupported("SetCustomModel");
    }
    void CModelInfoUnmodifiedDE::RestoreOriginalModel()
    {
        ReplacementUnsupported("RestoreOriginalModel");
    }
    void CModelInfoUnmodifiedDE::SetColModel(CColModel*)
    {
        ReplacementUnsupported("SetColModel");
    }
    void CModelInfoUnmodifiedDE::RestoreColModel()
    {
        ReplacementUnsupported("RestoreColModel");
    }
    void CModelInfoUnmodifiedDE::MakeCustomModel()
    {
        ReplacementUnsupported("MakeCustomModel");
    }
}
