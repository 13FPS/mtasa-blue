/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve checked native weather failures behind the existing SDK
 *****************************************************************************/
#include "CWeatherTransitionDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error("DE native weather transition unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        }
    }
    unsigned char CWeatherTransitionDE::Get()
    {
        std::uint16_t forced = 0;
        Require(access.ReadForced(forced));
        // Get's SDK return is a byte even though DE stores a native WORD.
        // Native -1 release therefore retains classic Get's visible0xff.
        return static_cast<unsigned char>(forced);
    }
    void CWeatherTransitionDE::Set(unsigned char primary, unsigned char secondary)
    {
        Require(access.SetTypes(primary, secondary));
    }
    void CWeatherTransitionDE::ResyncInterpolationWithGameClock(unsigned char, unsigned char)
    {
        // Like the classic SDK method this does not set the requested types.
        // DE's owner must resync the integer wrap bookend as well as zeroing
        // float interpolation; a copied classic float-only write is incorrect.
        Require(access.ResyncWithClock());
    }
    void CWeatherTransitionDE::Release()
    {
        Require(access.ReleaseForced());
    }
}
