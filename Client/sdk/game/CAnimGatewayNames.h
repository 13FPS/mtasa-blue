/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's authored gateway names across its engine backends
 ****************************************************************************/
#pragma once
namespace MtaGame::CAnimGatewayNames
{
    // These are MTA's exact authored gateway names, not native animation
    // observations. Returning their literals needs no engine owner or cache;
    // hierarchy hash checks and factories still require their real services.
    constexpr const char* GetBlockName() noexcept
    {
        return "ped";
    }
    constexpr const char* GetAnimationName() noexcept
    {
        return "run_wuzi";
    }
}
