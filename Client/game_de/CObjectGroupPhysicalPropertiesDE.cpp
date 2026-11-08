/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA property dispatch without facade-local fake defaults
 ****************************************************************************/
#include "CObjectGroupPhysicalPropertiesDE.h"
#include <cstring>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace ObjectPropertiesSdkDetail
    {
        void Require(ClockResult result, const char* method)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string(method) + ": " + std::to_string(static_cast<std::uint32_t>(result)));
        }
    }
    CObjectGroupPhysicalPropertiesDE::CObjectGroupPhysicalPropertiesDE(std::uint8_t group, NativeObjectGroupPropertiesAccess& owner)
        : group(group), owner(owner)
    {
        // Classic SetGroup initializes this wrapper's own flag from shared-map
        // membership. Two facades may subsequently have different flags.
        ObjectPropertiesSdkDetail::Require(owner.HasOriginalObjectGroupProperties(group, modified), "SetGroup");
    }
    float CObjectGroupPhysicalPropertiesDE::Read(NativeObjectGroupProperty field, const char* method) const
    {
        float value = 0;
        ObjectPropertiesSdkDetail::Require(owner.ReadObjectGroupProperty(group, field, value), method);
        return value;
    }
    void CObjectGroupPhysicalPropertiesDE::Set(NativeObjectGroupProperty field, float value, const char* method)
    {
        // First-change/epsilon/default state belongs to the same native owner,
        // so dropping or recreating this partial facade cannot erase originals.
        ObjectPropertiesSdkDetail::Require(owner.SetObjectGroupProperty(group, field, value, modified), method);
    }
    std::uint32_t CObjectGroupPhysicalPropertiesDE::ReadRaw(NativeObjectGroupProperty field, const char* method) const
    {
        std::uint32_t value = 0;
        ObjectPropertiesSdkDetail::Require(owner.ReadObjectGroupPropertyRaw(group, field, value), method);
        return value;
    }
    void CObjectGroupPhysicalPropertiesDE::SetRaw(NativeObjectGroupProperty field, std::uint32_t value, const char* method)
    {
        // The owner compares full SDK enums before a native byte conversion
        // and shares the same first defaults and this facade's modified flag.
        ObjectPropertiesSdkDetail::Require(owner.SetObjectGroupPropertyRaw(group, field, value, modified), method);
    }
    CVector CObjectGroupPhysicalPropertiesDE::ReadVector(NativeObjectGroupProperty first, const char* method) const
    {
        NativeObjectGroupVectorBits bits{};
        ObjectPropertiesSdkDetail::Require(owner.ReadObjectGroupVector(group, first, bits), method);
        CVector value;
        std::memcpy(&value.fX, &bits[0], sizeof(value.fX));
        std::memcpy(&value.fY, &bits[1], sizeof(value.fY));
        std::memcpy(&value.fZ, &bits[2], sizeof(value.fZ));
        return value;
    }
    void CObjectGroupPhysicalPropertiesDE::SetVector(NativeObjectGroupProperty first, const CVector& value, const char* method)
    {
        // One vector call retains the owner's three-field native prefix.
        // Preserve raw component bits while its actual CVector comparison
        // decides whether this SDK change needs the shared first default.
        NativeObjectGroupVectorBits bits{};
        std::memcpy(&bits[0], &value.fX, sizeof(value.fX));
        std::memcpy(&bits[1], &value.fY, sizeof(value.fY));
        std::memcpy(&bits[2], &value.fZ, sizeof(value.fZ));
        ObjectPropertiesSdkDetail::Require(owner.SetObjectGroupVector(group, first, bits, modified), method);
    }
    void CObjectGroupPhysicalPropertiesDE::RestoreDefault()
    {
        ObjectPropertiesSdkDetail::Require(owner.RestoreObjectGroupProperties(group, modified), "RestoreDefault");
    }
    void CObjectGroupPhysicalPropertiesDE::SetMass(float value)
    {
        Set(NativeObjectGroupProperty::Mass, value, "SetMass");
    }
    float CObjectGroupPhysicalPropertiesDE::GetMass() const
    {
        return Read(NativeObjectGroupProperty::Mass, "GetMass");
    }
    void CObjectGroupPhysicalPropertiesDE::SetTurnMass(float value)
    {
        Set(NativeObjectGroupProperty::TurnMass, value, "SetTurnMass");
    }
    float CObjectGroupPhysicalPropertiesDE::GetTurnMass() const
    {
        return Read(NativeObjectGroupProperty::TurnMass, "GetTurnMass");
    }
    void CObjectGroupPhysicalPropertiesDE::SetAirResistance(float value)
    {
        Set(NativeObjectGroupProperty::AirResistance, value, "SetAirResistance");
    }
    float CObjectGroupPhysicalPropertiesDE::GetAirResistance() const
    {
        return Read(NativeObjectGroupProperty::AirResistance, "GetAirResistance");
    }
    void CObjectGroupPhysicalPropertiesDE::SetElasticity(float value)
    {
        Set(NativeObjectGroupProperty::Elasticity, value, "SetElasticity");
    }
    float CObjectGroupPhysicalPropertiesDE::GetElasticity() const
    {
        return Read(NativeObjectGroupProperty::Elasticity, "GetElasticity");
    }
    void CObjectGroupPhysicalPropertiesDE::SetBuoyancy(float value)
    {
        Set(NativeObjectGroupProperty::Buoyancy, value, "SetBuoyancy");
    }
    float CObjectGroupPhysicalPropertiesDE::GetBuoyancy() const
    {
        return Read(NativeObjectGroupProperty::Buoyancy, "GetBuoyancy");
    }
    void CObjectGroupPhysicalPropertiesDE::SetUprootLimit(float value)
    {
        Set(NativeObjectGroupProperty::UprootLimit, value, "SetUprootLimit");
    }
    float CObjectGroupPhysicalPropertiesDE::GetUprootLimit() const
    {
        return Read(NativeObjectGroupProperty::UprootLimit, "GetUprootLimit");
    }
    void CObjectGroupPhysicalPropertiesDE::SetCollisionDamageMultiplier(float value)
    {
        Set(NativeObjectGroupProperty::CollisionDamageMultiplier, value, "SetCollisionDamageMultiplier");
    }
    float CObjectGroupPhysicalPropertiesDE::GetCollisionDamageMultiplier() const
    {
        return Read(NativeObjectGroupProperty::CollisionDamageMultiplier, "GetCollisionDamageMultiplier");
    }
    void CObjectGroupPhysicalPropertiesDE::SetCollisionDamageEffect(ObjectGroupPhysicalProperties::DamageEffect value)
    {
        SetRaw(NativeObjectGroupProperty::CollisionDamageEffect, static_cast<std::uint32_t>(value), "SetCollisionDamageEffect");
    }
    ObjectGroupPhysicalProperties::DamageEffect CObjectGroupPhysicalPropertiesDE::GetCollisionDamageEffect() const
    {
        return static_cast<ObjectGroupPhysicalProperties::DamageEffect>(ReadRaw(NativeObjectGroupProperty::CollisionDamageEffect, "GetCollisionDamageEffect"));
    }
    void CObjectGroupPhysicalPropertiesDE::SetCollisionSpecialResponseCase(ObjectGroupPhysicalProperties::CollisionResponse value)
    {
        SetRaw(NativeObjectGroupProperty::CollisionSpecialResponseCase, static_cast<std::uint32_t>(value), "SetCollisionSpecialResponseCase");
    }
    ObjectGroupPhysicalProperties::CollisionResponse CObjectGroupPhysicalPropertiesDE::GetCollisionSpecialResponseCase() const
    {
        return static_cast<ObjectGroupPhysicalProperties::CollisionResponse>(
            ReadRaw(NativeObjectGroupProperty::CollisionSpecialResponseCase, "GetCollisionSpecialResponseCase"));
    }
    void CObjectGroupPhysicalPropertiesDE::SetCameraAvoidObject(bool value)
    {
        SetRaw(NativeObjectGroupProperty::CameraAvoidObject, static_cast<std::uint32_t>(value), "SetCameraAvoidObject");
    }
    bool CObjectGroupPhysicalPropertiesDE::GetCameraAvoidObject() const
    {
        return ReadRaw(NativeObjectGroupProperty::CameraAvoidObject, "GetCameraAvoidObject") != 0;
    }
    void CObjectGroupPhysicalPropertiesDE::SetCausesExplosion(bool value)
    {
        SetRaw(NativeObjectGroupProperty::CausesExplosion, static_cast<std::uint32_t>(value), "SetCausesExplosion");
    }
    bool CObjectGroupPhysicalPropertiesDE::GetCausesExplosion() const
    {
        return ReadRaw(NativeObjectGroupProperty::CausesExplosion, "GetCausesExplosion") != 0;
    }
    void CObjectGroupPhysicalPropertiesDE::SetFxType(ObjectGroupPhysicalProperties::FxType value)
    {
        SetRaw(NativeObjectGroupProperty::FxType, static_cast<std::uint32_t>(value), "SetFxType");
    }
    ObjectGroupPhysicalProperties::FxType CObjectGroupPhysicalPropertiesDE::GetFxType() const
    {
        return static_cast<ObjectGroupPhysicalProperties::FxType>(ReadRaw(NativeObjectGroupProperty::FxType, "GetFxType"));
    }
    void CObjectGroupPhysicalPropertiesDE::SetFxOffset(CVector value)
    {
        SetVector(NativeObjectGroupProperty::FxOffsetX, value, "SetFxOffset");
    }
    CVector CObjectGroupPhysicalPropertiesDE::GetFxOffset() const
    {
        return ReadVector(NativeObjectGroupProperty::FxOffsetX, "GetFxOffset");
    }
    void CObjectGroupPhysicalPropertiesDE::SetSmashMultiplier(float value)
    {
        Set(NativeObjectGroupProperty::SmashMultiplier, value, "SetSmashMultiplier");
    }
    float CObjectGroupPhysicalPropertiesDE::GetSmashMultiplier() const
    {
        return Read(NativeObjectGroupProperty::SmashMultiplier, "GetSmashMultiplier");
    }
    void CObjectGroupPhysicalPropertiesDE::SetBreakVelocity(CVector value)
    {
        SetVector(NativeObjectGroupProperty::BreakVelocityX, value, "SetBreakVelocity");
    }
    CVector CObjectGroupPhysicalPropertiesDE::GetBreakVelocity() const
    {
        return ReadVector(NativeObjectGroupProperty::BreakVelocityX, "GetBreakVelocity");
    }
    void CObjectGroupPhysicalPropertiesDE::SetBreakVelocityRandomness(float value)
    {
        Set(NativeObjectGroupProperty::BreakVelocityRandomness, value, "SetBreakVelocityRandomness");
    }
    float CObjectGroupPhysicalPropertiesDE::GetBreakVelocityRandomness() const
    {
        return Read(NativeObjectGroupProperty::BreakVelocityRandomness, "GetBreakVelocityRandomness");
    }
    void CObjectGroupPhysicalPropertiesDE::SetBreakMode(ObjectGroupPhysicalProperties::BreakMode value)
    {
        SetRaw(NativeObjectGroupProperty::BreakMode, static_cast<std::uint32_t>(value), "SetBreakMode");
    }
    ObjectGroupPhysicalProperties::BreakMode CObjectGroupPhysicalPropertiesDE::GetBreakMode() const
    {
        return static_cast<ObjectGroupPhysicalProperties::BreakMode>(ReadRaw(NativeObjectGroupProperty::BreakMode, "GetBreakMode"));
    }
    void CObjectGroupPhysicalPropertiesDE::SetSparksOnImpact(bool value)
    {
        // Its qualified native field is a byte. The owner's raw API preserves
        // that width and its neighbours while retaining the SDK bool meaning.
        SetRaw(NativeObjectGroupProperty::SparksOnImpact, static_cast<std::uint32_t>(value), "SetSparksOnImpact");
    }
    bool CObjectGroupPhysicalPropertiesDE::GetSparksOnImpact() const
    {
        return ReadRaw(NativeObjectGroupProperty::SparksOnImpact, "GetSparksOnImpact") != 0;
    }
}
