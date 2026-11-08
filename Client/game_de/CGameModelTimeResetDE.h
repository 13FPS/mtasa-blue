/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Restore actual timed-model defaults through the existing game owner
 ****************************************************************************/
#pragma once
#include "CGameModelDummiesResetDE.h"
namespace mta::game_de
{
    class CGameModelTimeResetDE : public CGameModelDummiesResetDE
    {
    public:
        void ResetModelTimes() override;

    protected:
        using CGameModelDummiesResetDE::CGameModelDummiesResetDE;
    };
    // This closes the actual named SDK reset method. Overall Reset/World
    // admission and the complete game factory still require their own effects.
    static_assert(std::is_abstract_v<CGameModelTimeResetDE> && std::is_convertible_v<CGameModelTimeResetDE*, CGame*>);
}
