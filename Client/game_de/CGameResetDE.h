/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual reset orchestration with mandatory backend restoration gates
 *****************************************************************************/
#pragma once
#include "CGameInitializationDE.h"
namespace mta::game_de
{
    // Parameterizing the base reuses this exact policy on the qualified
    // startup chain without creating a second CGame/clock/service owner.
    template <class GameBase>
    class CGameResetMixinDE : public GameBase
    {
    public:
        void Reset() override;

    protected:
        using GameBase::GameBase;
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
    // Keep the injected legacy class name used by existing derived ctor
    // initializers. This empty shell owns no second state or virtual policy.
    class CGameResetDE : public CGameResetMixinDE<CGameInitializationDE>
    {
    protected:
        using CGameResetMixinDE<CGameInitializationDE>::CGameResetMixinDE;
    };
    static_assert(std::is_abstract_v<CGameResetDE> && std::is_base_of_v<CGameInitializationDE, CGameResetDE>);
}

#include "CGameResetMixinDE.inl"
