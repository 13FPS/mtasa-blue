/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Supply the actual CPU renderer initialization stage to CGame
 ****************************************************************************/
#pragma once
#include "CGameInitializationDE.h"
#include "CRenderWarePreparationDE.h"
#include <type_traits>

namespace mta::game_de
{
    // This owned lifetime supplies the exact SA Initialize stage. Native
    // renderer operations and all other CGame factory/admission obligations
    // remain mandatory; registry membership is not a GPU readiness signal.
    class CGameRenderPreparationDE : public CGameInitializationDE
    {
    public:
        CRenderWarePreparationDE& GetRenderPreparation() noexcept { return renderPreparation; }

    protected:
        using CGameInitializationDE::CGameInitializationDE;
        void InitializeRenderer() override { renderPreparation.Initialize(); }

    private:
        CRenderWarePreparationDE renderPreparation;
    };
    static_assert(std::is_abstract_v<CGameRenderPreparationDE> && std::is_base_of_v<CGame, CGameRenderPreparationDE>);
}
