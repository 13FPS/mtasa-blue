/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA task query on the existing borrowed ped SDK chain
 ****************************************************************************/
#pragma once
#include "CPedCollisionDE.h"
namespace mta::game_de
{
    class CPedTasksDE : public CPedCollisionDE
    {
    public:
        bool IsDoingGangDriveby() const override;

    protected:
        using CPedCollisionDE::CPedCollisionDE;
        // A future concrete owner must consult its real stored intelligence
        // SDK wrapper before virtual lookup, as classic does. No native task
        // pointer or second wrapper registry is constructed by this layer.
        virtual bool HasPedIntelligence() const noexcept = 0;
    };
    // GetPedIntelligence and all unresolved native ownership/effects remain
    // mandatory. The true availability branch requires its real nonnull
    // intelligence and manager contracts; no null-manager fallback is added.
    static_assert(std::is_abstract_v<CPedTasksDE> && std::is_base_of_v<CPedCollisionDE, CPedTasksDE>);
}
