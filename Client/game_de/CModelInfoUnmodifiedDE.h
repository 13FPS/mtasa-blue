/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-owned empty replacement state for an unmodified model family
 *****************************************************************************/
#pragma once
#include "CModelInfoDiagnosticsDE.h"

namespace mta::game_de
{
    // A deliberately closed replacement domain, not a complete model factory.
    // Generic Reference/Diagnostics layers retain their independent removal
    // obligation and may later bind a genuine replacement-owning provider.
    // Reuse the real Core diagnostics base so native removal and reporting
    // compose once, without parallel Reference bases or another logger.
    class CModelInfoUnmodifiedDE : public CModelInfoDiagnosticsDE
    {
    public:
        bool SetCustomModel(RpClump*) final override;
        void RestoreOriginalModel() final override;
        void SetColModel(CColModel*) final override;
        void RestoreColModel() final override;
        void MakeCustomModel() final override;
        bool UnloadUnused() final override;

    protected:
        CModelInfoUnmodifiedDE(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game, NativeModelBackingReferenceAccess& owner,
                               const NativeModelBackingReferenceToken& reference, CCoreInterface& core);
        void RemoveNativeModelInterfaceReference() final override;

    private:
        // Match the actual SA constructor's two CPU-owned null fields, not
        // the ordinary engine RW/collision pointers. Final refusal routes
        // prevent derived implementations from silently invalidating this proof.
        struct ReplacementState
        {
            RpClump*   customClump = nullptr;
            CColModel* customCollision = nullptr;
        };
        const ReplacementState   replacements{};
        [[noreturn]] static void ReplacementUnsupported(const char* operation);
    };
    static_assert(std::is_abstract_v<CModelInfoUnmodifiedDE> && std::is_base_of_v<CModelInfoDiagnosticsDE, CModelInfoUnmodifiedDE>);
}
