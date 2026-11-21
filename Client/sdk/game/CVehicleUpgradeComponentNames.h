/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Map vehicle attachment names to MTA's supported upgrade flags
 ****************************************************************************/
#pragma once

#include "CModelInfo.h"
#include <string_view>

namespace MtaGame
{
    // Classic frames and DE attachment points use the same case-sensitive names.
    // The caller marks the cache initialised only after traversing the whole model.
    inline void ApplyVehicleUpgradeComponent(SVehicleSupportedUpgrades& upgrades, std::string_view name) noexcept
    {
        struct Component
        {
            std::string_view name;
            bool SVehicleSupportedUpgrades::* flag;
        };
        static constexpr Component components[] = {
            {"ug_bonnet", &SVehicleSupportedUpgrades::m_bBonnet},
            {"ug_bonnet_left", &SVehicleSupportedUpgrades::m_bBonnet_Left},
            {"ug_bonnet_left_dam", &SVehicleSupportedUpgrades::m_bBonnet_Left_dam},
            {"ug_bonnet_right", &SVehicleSupportedUpgrades::m_bBonnet_Right},
            {"ug_bonnet_right_dam", &SVehicleSupportedUpgrades::m_bBonnet_Right_dam},
            {"ug_spoiler", &SVehicleSupportedUpgrades::m_bSpoiler},
            {"ug_spoiler_dam", &SVehicleSupportedUpgrades::m_bSpoiler_dam},
            {"ug_lights", &SVehicleSupportedUpgrades::m_bLamps},
            {"ug_lights_dam", &SVehicleSupportedUpgrades::m_bLamps_dam},
            {"ug_roof", &SVehicleSupportedUpgrades::m_bRoof},
            {"ug_wing_right", &SVehicleSupportedUpgrades::m_bSideSkirt_Right},
            {"ug_wing_left", &SVehicleSupportedUpgrades::m_bSideSkirt_Left},
            {"exhaust_ok", &SVehicleSupportedUpgrades::m_bExhaust},
            {"ug_frontbullbar", &SVehicleSupportedUpgrades::m_bFrontBullbars},
            {"ug_backbullbar", &SVehicleSupportedUpgrades::m_bRearBullbars},
            {"bump_front_dummy", &SVehicleSupportedUpgrades::m_bFrontBumper},
            {"bump_rear_dummy", &SVehicleSupportedUpgrades::m_bRearBumper},
            {"misc_c", &SVehicleSupportedUpgrades::m_bMisc},
        };

        for (const auto& component : components)
        {
            if (component.name == name)
            {
                upgrades.*component.flag = true;
                return;
            }
        }
    }
}
