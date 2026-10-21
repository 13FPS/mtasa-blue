/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA player-info startup methods using their native owner
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>  // The SDK's byte alias normally arrives through the classic PCH.
#include <cstdint>
#include <game/CPlayerInfo.h>
#include "NativePlayerInfoAccess.h"
#include <type_traits>

namespace mta::game_de
{
    // All SDK methods delegate fresh native observations/intents to the same
    // retained owner. The complete object itself never grants call authority.
    class CPlayerInfoDE final : public CPlayerInfo
    {
    public:
        explicit CPlayerInfoDE(NativePlayerInfoAccess& access) noexcept : native(access) {}
        long         GetPlayerMoney() override;
        void         SetPlayerMoney(long value, bool instant = false) override;
        void         SetLastTimeEaten(short value) override;
        void         SetDoesNotGetTired(bool value) override;
        CWanted*     GetWanted() override;
        float        GetFPSMoveHeading() override;
        byte         GetCamDrunkLevel() override;
        void         SetCamDrunkLevel(byte value) override;
        DWORD        GetCarTwoWheelCounter() override;
        float        GetCarTwoWheelDist() override;
        DWORD        GetCarLess3WheelCounter() override;
        DWORD        GetBikeRearWheelCounter() override;
        float        GetBikeRearWheelDist() override;
        DWORD        GetBikeFrontWheelCounter() override;
        float        GetBikeFrontWheelDist() override;
        std::uint8_t GetMaxHealth() const override;
        std::uint8_t GetMaxArmor() const override;
        bool         CanDoDriveBy() const override;

    private:
        static void             Require(ClockResult result, const char* operation);
        float                   ReadFloat(NativePlayerInfoFloatField field, const char* operation) const;
        DWORD                   ReadDword(NativePlayerInfoDwordField field, const char* operation) const;
        std::uint8_t            ReadByte(NativePlayerInfoByteField field, const char* operation) const;
        NativePlayerInfoAccess& native;
    };
    static_assert(!std::is_abstract_v<CPlayerInfoDE> && std::is_base_of_v<CPlayerInfo, CPlayerInfoDE>);
    static_assert(sizeof(long) == sizeof(std::int32_t) && sizeof(short) == sizeof(std::int16_t));
    static_assert(sizeof(float) == sizeof(std::uint32_t) && sizeof(DWORD) == sizeof(std::uint32_t));
}
