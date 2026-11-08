/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA scalar property overrides backed by one native registry
 ****************************************************************************/
#pragma once
#include <game/CObjectGroupPhysicalProperties.h>
#include "NativeObjectGroupPropertiesAccess.h"
#include <type_traits>

namespace mta::game_de
{
    class CObjectGroupPhysicalPropertiesDE : public CObjectGroupPhysicalProperties
    {
    public:
        unsigned char GetGroup() const override { return group; }
        void          RestoreDefault() override;
        void          SetMass(float) override;
        float         GetMass() const override;
        void          SetTurnMass(float) override;
        float         GetTurnMass() const override;
        void          SetAirResistance(float) override;
        float         GetAirResistance() const override;
        void          SetElasticity(float) override;
        float         GetElasticity() const override;
        void          SetBuoyancy(float) override;
        float         GetBuoyancy() const override;

    protected:
        CObjectGroupPhysicalPropertiesDE(std::uint8_t group, NativeObjectGroupPropertiesAccess& owner);

    private:
        const std::uint8_t                 group;
        NativeObjectGroupPropertiesAccess& owner;
        bool                               modified = false;
        float                              Read(NativeObjectGroupProperty, const char*) const;
        void                               Set(NativeObjectGroupProperty, float, const char*);
    };
    // This admits five builtin scalar fields only. The classic raw interface,
    // FX pointer and every other property stay pure until independently owned.
    static_assert(std::is_abstract_v<CObjectGroupPhysicalPropertiesDE> &&
                  std::is_convertible_v<CObjectGroupPhysicalPropertiesDE*, CObjectGroupPhysicalProperties*>);
}
