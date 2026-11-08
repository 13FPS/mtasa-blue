/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute supported real SDK methods while every other method aborts
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/CObjectGroupPhysicalPropertiesDE.h"
#include <CVector.h>
#include <cstdlib>
#pragma warning(push)
#pragma warning(disable : 4100)
namespace ObjectPropertiesOwnerFixture
{
    class Properties final : public mta::game_de::CObjectGroupPhysicalPropertiesDE
    {
    public:
        Properties(std::uint8_t group, mta::game_de::NativeObjectGroupPropertiesAccess& owner) : CObjectGroupPhysicalPropertiesDE(group, owner) {}
        CObjectGroupPhysicalPropertiesSAInterface*  GetInterface() const override { std::abort(); }
        void                                        SetUprootLimit(float) override { std::abort(); }
        float                                       GetUprootLimit() const override { std::abort(); }
        void                                        SetCollisionDamageMultiplier(float) override { std::abort(); }
        float                                       GetCollisionDamageMultiplier() const override { std::abort(); }
        void                                        SetCollisionDamageEffect(ObjectGroupPhysicalProperties::DamageEffect) override { std::abort(); }
        ObjectGroupPhysicalProperties::DamageEffect GetCollisionDamageEffect() const override { std::abort(); }
        void                                        SetCollisionSpecialResponseCase(ObjectGroupPhysicalProperties::CollisionResponse) override { std::abort(); }
        ObjectGroupPhysicalProperties::CollisionResponse GetCollisionSpecialResponseCase() const override { std::abort(); }
        void                                             SetCameraAvoidObject(bool) override { std::abort(); }
        bool                                             GetCameraAvoidObject() const override { std::abort(); }
        void                                             SetCausesExplosion(bool) override { std::abort(); }
        bool                                             GetCausesExplosion() const override { std::abort(); }
        void                                             SetFxType(ObjectGroupPhysicalProperties::FxType) override { std::abort(); }
        ObjectGroupPhysicalProperties::FxType            GetFxType() const override { std::abort(); }
        void                                             SetFxOffset(CVector) override { std::abort(); }
        CVector                                          GetFxOffset() const override { std::abort(); }
        bool                                             SetFxParticleSystem(CFxSystemBPSAInterface*) override { std::abort(); }
        void                                             RemoveFxParticleSystem() override { std::abort(); }
        void                                             SetSmashMultiplier(float) override { std::abort(); }
        float                                            GetSmashMultiplier() const override { std::abort(); }
        void                                             SetBreakVelocity(CVector) override { std::abort(); }
        CVector                                          GetBreakVelocity() const override { std::abort(); }
        void                                             SetBreakVelocityRandomness(float) override { std::abort(); }
        float                                            GetBreakVelocityRandomness() const override { std::abort(); }
        void                                             SetBreakMode(ObjectGroupPhysicalProperties::BreakMode) override { std::abort(); }
        ObjectGroupPhysicalProperties::BreakMode         GetBreakMode() const override { std::abort(); }
        void                                             SetSparksOnImpact(bool) override { std::abort(); }
        bool                                             GetSparksOnImpact() const override { std::abort(); }
    };
    static_assert(!std::is_abstract_v<Properties> && std::is_abstract_v<mta::game_de::CObjectGroupPhysicalPropertiesDE>);
}
#pragma warning(pop)
