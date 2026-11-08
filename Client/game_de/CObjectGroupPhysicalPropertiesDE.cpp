/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA property dispatch without facade-local fake defaults
 ****************************************************************************/
#include "CObjectGroupPhysicalPropertiesDE.h"
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
}
