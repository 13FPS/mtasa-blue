/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind model policy diagnostics to the existing borrowed MTA Core
 ****************************************************************************/
#pragma once
#include "CModelInfoReferenceDE.h"

class CCoreInterface;

namespace mta::game_de
{
    // Core must outlive this partial facade. Its existing reporting service
    // grants no native model rights: the issued reference token still owns
    // admission, and removal/custom replacement remain separate obligations.
    class CModelInfoDiagnosticsDE : public CModelInfoReferenceDE
    {
    protected:
        CModelInfoDiagnosticsDE(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game,
                                NativeModelBackingReferenceAccess& referenceOwner, const NativeModelBackingReferenceToken& reference, CCoreInterface& core)
            : CModelInfoReferenceDE(model, classification, game, referenceOwner, reference), core(core)
        {
        }
        void ReportMissingAnimationModel(std::uint32_t animation) final override;
#ifdef MTA_DEBUG
        void ReportRequestingAnimationModel(std::uint32_t animation) final override;
#endif
        void ReportBlockingModelLoad(bool loaded, int attempts, const char* tag) final override;

    private:
        CCoreInterface& core;
    };
    static_assert(std::is_abstract_v<CModelInfoDiagnosticsDE> && std::is_base_of_v<CModelInfoReferenceDE, CModelInfoDiagnosticsDE>);
}
