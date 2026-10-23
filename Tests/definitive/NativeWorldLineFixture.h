/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the exact new LOS profile in primitive and Runtime fixtures
 ****************************************************************************/
#pragma once
#include "NativeWorldGroundFixture.h"
#include "../../Client/game_de/NativeWorldLineProfile.h"
namespace NativeWorldLineFixture
{
    inline void Install(NativeWorldGroundFixture::Roots::Writer writer, void* host)
    {
        for (const auto& pin : mta::game_de::NativeWorldLineProfile::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
    }
}
