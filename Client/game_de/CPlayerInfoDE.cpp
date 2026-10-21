/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's signed money, instant display and ability semantics
 *****************************************************************************/
#include "CPlayerInfoDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CPlayerInfoDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE player-info owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    long CPlayerInfoDE::GetPlayerMoney()
    {
        std::int32_t value = 0;
        Require(native.ReadPlayerMoney(value), "GetPlayerMoney");
        return value;
    }
    void CPlayerInfoDE::SetPlayerMoney(long value, bool instant)
    {
        Require(native.SetPlayerMoney(value, instant), "SetPlayerMoney");
    }
    void CPlayerInfoDE::SetLastTimeEaten(short value)
    {
        Require(native.SetPlayerLastTimeEaten(value), "SetLastTimeEaten");
    }
    void CPlayerInfoDE::SetDoesNotGetTired(bool value)
    {
        Require(native.SetPlayerDoesNotGetTired(value), "SetDoesNotGetTired");
    }
    CWanted* CPlayerInfoDE::GetWanted()
    {
        CWanted* value = nullptr;
        Require(native.GetWanted(value), "GetWanted");
        if (!value)
            throw std::runtime_error("DE player-info owner returned no qualified wanted object");
        return value;
    }
    float CPlayerInfoDE::ReadFloat(NativePlayerInfoFloatField field, const char* operation) const
    {
        float value = 0;
        Require(native.ReadPlayerFloat(field, value), operation);
        return value;
    }
    DWORD CPlayerInfoDE::ReadDword(NativePlayerInfoDwordField field, const char* operation) const
    {
        std::uint32_t value = 0;
        Require(native.ReadPlayerDword(field, value), operation);
        return value;
    }
    std::uint8_t CPlayerInfoDE::ReadByte(NativePlayerInfoByteField field, const char* operation) const
    {
        std::uint8_t value = 0;
        Require(native.ReadPlayerByte(field, value), operation);
        return value;
    }
    float CPlayerInfoDE::GetFPSMoveHeading()
    {
        return ReadFloat(NativePlayerInfoFloatField::FpsMoveHeading, "GetFPSMoveHeading");
    }
    byte CPlayerInfoDE::GetCamDrunkLevel()
    {
        return ReadByte(NativePlayerInfoByteField::CamDrunkLevel, "GetCamDrunkLevel");
    }
    void CPlayerInfoDE::SetCamDrunkLevel(byte value)
    {
        Require(native.SetPlayerCamDrunkLevel(value), "SetCamDrunkLevel");
    }
    DWORD CPlayerInfoDE::GetCarTwoWheelCounter()
    {
        return ReadDword(NativePlayerInfoDwordField::CarTwoWheelCounter, "GetCarTwoWheelCounter");
    }
    float CPlayerInfoDE::GetCarTwoWheelDist()
    {
        return ReadFloat(NativePlayerInfoFloatField::CarTwoWheelDistance, "GetCarTwoWheelDist");
    }
    DWORD CPlayerInfoDE::GetCarLess3WheelCounter()
    {
        return ReadDword(NativePlayerInfoDwordField::CarLess3WheelCounter, "GetCarLess3WheelCounter");
    }
    DWORD CPlayerInfoDE::GetBikeRearWheelCounter()
    {
        return ReadDword(NativePlayerInfoDwordField::BikeRearWheelCounter, "GetBikeRearWheelCounter");
    }
    float CPlayerInfoDE::GetBikeRearWheelDist()
    {
        return ReadFloat(NativePlayerInfoFloatField::BikeRearWheelDistance, "GetBikeRearWheelDist");
    }
    DWORD CPlayerInfoDE::GetBikeFrontWheelCounter()
    {
        return ReadDword(NativePlayerInfoDwordField::BikeFrontWheelCounter, "GetBikeFrontWheelCounter");
    }
    float CPlayerInfoDE::GetBikeFrontWheelDist()
    {
        return ReadFloat(NativePlayerInfoFloatField::BikeFrontWheelDistance, "GetBikeFrontWheelDist");
    }
    std::uint8_t CPlayerInfoDE::GetMaxHealth() const
    {
        return ReadByte(NativePlayerInfoByteField::MaxHealth, "GetMaxHealth");
    }
    std::uint8_t CPlayerInfoDE::GetMaxArmor() const
    {
        return ReadByte(NativePlayerInfoByteField::MaxArmor, "GetMaxArmor");
    }
    bool CPlayerInfoDE::CanDoDriveBy() const
    {
        return ReadByte(NativePlayerInfoByteField::CanDoDriveBy, "CanDoDriveBy") != 0;
    }
}
