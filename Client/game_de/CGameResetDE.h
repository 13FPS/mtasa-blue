/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual reset orchestration with mandatory backend restoration gates
 *****************************************************************************/
#pragma once
#include "CGameInitializationDE.h"
namespace mta::game_de
{
    class CGameResetDE : public CGameInitializationDE
    {
    public:
        void Reset() override;

    protected:
        using CGameInitializationDE::CGameInitializationDE;
        // After the same playing-state gate, the existing enclosing owner must
        // freshly admit every receiver and restoration before the first effect.
        // Individual services retain their own current checks after this gate.
        virtual void RequireResetReady() = 0;
        virtual void ResetVehicleModelDummies() = 0;
        virtual void RestoreObjectModelGroups() = 0;
        virtual void RestoreDefaultObjectGroupProperties() = 0;
        virtual void ResetVehicleModelWheelSizes() = 0;
        virtual void ResetModelTextureDictionaries() = 0;
        // The backend must supply its genuine default capacity. Classic's
        // MAX_BUILDINGS number is not a qualified DE allocation/bank fact.
        virtual int GetResetBuildingPoolCapacity() const = 0;
    };
    static_assert(std::is_abstract_v<CGameResetDE> && std::is_base_of_v<CGameInitializationDE, CGameResetDE>);
}
