/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed player-info intents for the existing qualified engine owner
 *****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>

class CWanted;

namespace mta::game_de
{
    enum class NativePlayerInfoFloatField : std::uint32_t
    {
        FpsMoveHeading,
        CarTwoWheelDistance,
        BikeRearWheelDistance,
        BikeFrontWheelDistance
    };
    enum class NativePlayerInfoDwordField : std::uint32_t
    {
        CarTwoWheelCounter,
        CarLess3WheelCounter,
        BikeRearWheelCounter,
        BikeFrontWheelCounter
    };
    enum class NativePlayerInfoByteField : std::uint32_t
    {
        CamDrunkLevel,
        MaxHealth,
        MaxArmor,
        CanDoDriveBy
    };
    // Publishing the actual SDK object never grants a later engine lease.
    // A timing-only owner must refuse the additional player operations.
    class NativePlayerInfoAccess
    {
    public:
        virtual ClockResult ReadPlayerMoney(std::int32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPlayerMoney(std::int32_t, bool) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPlayerLastTimeEaten(std::int16_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPlayerDoesNotGetTired(bool) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult GetWanted(CWanted*&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadPlayerFloat(NativePlayerInfoFloatField, float&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadPlayerDword(NativePlayerInfoDwordField, std::uint32_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadPlayerByte(NativePlayerInfoByteField, std::uint8_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetPlayerCamDrunkLevel(std::uint8_t) noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativePlayerInfoAccess() = default;
    };
}
