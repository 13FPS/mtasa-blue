/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA property overrides backed by one native registry
 ****************************************************************************/
#pragma once
#include <game/CObjectGroupPhysicalProperties.h>
// Shared vector companion types use MTA's global comparison tolerance.
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "NativeObjectGroupPropertiesAccess.h"
#include <type_traits>

namespace mta::game_de
{
    class CObjectGroupPhysicalPropertiesDE : public CObjectGroupPhysicalProperties
    {
    public:
        unsigned char                                    GetGroup() const override { return group; }
        void                                             RestoreDefault() override;
        void                                             SetMass(float) override;
        float                                            GetMass() const override;
        void                                             SetTurnMass(float) override;
        float                                            GetTurnMass() const override;
        void                                             SetAirResistance(float) override;
        float                                            GetAirResistance() const override;
        void                                             SetElasticity(float) override;
        float                                            GetElasticity() const override;
        void                                             SetBuoyancy(float) override;
        float                                            GetBuoyancy() const override;
        void                                             SetUprootLimit(float) override;
        float                                            GetUprootLimit() const override;
        void                                             SetCollisionDamageMultiplier(float) override;
        float                                            GetCollisionDamageMultiplier() const override;
        void                                             SetCollisionDamageEffect(ObjectGroupPhysicalProperties::DamageEffect) override;
        ObjectGroupPhysicalProperties::DamageEffect      GetCollisionDamageEffect() const override;
        void                                             SetCollisionSpecialResponseCase(ObjectGroupPhysicalProperties::CollisionResponse) override;
        ObjectGroupPhysicalProperties::CollisionResponse GetCollisionSpecialResponseCase() const override;
        void                                             SetCameraAvoidObject(bool) override;
        bool                                             GetCameraAvoidObject() const override;
        void                                             SetCausesExplosion(bool) override;
        bool                                             GetCausesExplosion() const override;
        void                                             SetFxType(ObjectGroupPhysicalProperties::FxType) override;
        ObjectGroupPhysicalProperties::FxType            GetFxType() const override;
        void                                             SetFxOffset(CVector) override;
        CVector                                          GetFxOffset() const override;
        void                                             SetSmashMultiplier(float) override;
        float                                            GetSmashMultiplier() const override;
        void                                             SetBreakVelocity(CVector) override;
        CVector                                          GetBreakVelocity() const override;
        void                                             SetBreakVelocityRandomness(float) override;
        float                                            GetBreakVelocityRandomness() const override;
        void                                             SetBreakMode(ObjectGroupPhysicalProperties::BreakMode) override;
        ObjectGroupPhysicalProperties::BreakMode         GetBreakMode() const override;
        void                                             SetSparksOnImpact(bool) override;
        bool                                             GetSparksOnImpact() const override;

    protected:
        CObjectGroupPhysicalPropertiesDE(std::uint8_t group, NativeObjectGroupPropertiesAccess& owner);

    private:
        const std::uint8_t                 group;
        NativeObjectGroupPropertiesAccess& owner;
        bool                               modified = false;
        float                              Read(NativeObjectGroupProperty, const char*) const;
        void                               Set(NativeObjectGroupProperty, float, const char*);
        std::uint32_t                      ReadRaw(NativeObjectGroupProperty, const char*) const;
        void                               SetRaw(NativeObjectGroupProperty, std::uint32_t, const char*);
        CVector                            ReadVector(NativeObjectGroupProperty, const char*) const;
        void                               SetVector(NativeObjectGroupProperty, const CVector&, const char*);
    };
    // The qualified scalar/enum/bool/vector fields share one builtin-row owner.
    // Raw classic interfaces and FX blueprint lifetime methods stay pure until
    // their native ownership is independently established.
    static_assert(std::is_abstract_v<CObjectGroupPhysicalPropertiesDE> &&
                  std::is_convertible_v<CObjectGroupPhysicalPropertiesDE*, CObjectGroupPhysicalProperties*>);
}
