/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep ordered garage initialization behind the real garage receiver
 *****************************************************************************/
#include "CGaragesDE.h"
#include <game/CGarage.h>
#include <game/CGarageInitializationPolicy.h>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CGaragesDE::Initialize()
    {
        static_assert(MAX_GARAGES > 47);
        MtaGame::CGarageInitializationPolicy::Initialize(
            [this](std::uint32_t index, std::uint8_t type)
            {
                auto* garage = GetGarage(index);
                // A required missing receiver is unavailable, not a skipped garage
                // or successful partial startup. Earlier accepted types persist.
                if (!garage)
                    throw std::runtime_error("DE CGarages Initialize requires garage " + std::to_string(index));
                garage->SetType(type);
            });
    }
}
