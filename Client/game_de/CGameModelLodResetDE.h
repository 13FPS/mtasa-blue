/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reset actual model distance defaults through the existing game owner
 ****************************************************************************/
#pragma once
#include "CGameFileAudioDE.h"
#include "CGameModelTimeResetDE.h"
namespace mta::game_de
{
    class CGameModelLodResetDE : public CGameFileAudioDE<CGameModelTimeResetDE>
    {
    public:
        void ResetModelLodDistances() override;

    protected:
        using CGameFileAudioDE<CGameModelTimeResetDE>::CGameFileAudioDE;
    };
    static_assert(std::is_abstract_v<CGameModelLodResetDE> && std::is_convertible_v<CGameModelLodResetDE*, CGame*>);
}
