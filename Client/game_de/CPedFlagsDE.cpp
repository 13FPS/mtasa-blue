/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward raw bike/shot flag intents without adjacent-bit guesses
 ****************************************************************************/
#include "CPedFlagsDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedFlagsDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped flag owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    std::uint32_t CPedFlagsDE::GetCantBeKnockedOffBike() const
    {
        std::uint32_t value;
        Require(bikeFlagsAccess.ReadPedCantBeKnockedOffBike(ReferenceToken(), value), "GetCantBeKnockedOffBike");
        return value;
    }
    void CPedFlagsDE::SetCantBeKnockedOffBike(std::uint32_t value)
    {
        // Classic assigns the full input to a two-bit field. The genuine
        // native mask supplies that storage intent, including raw mode3;
        // this SDK forward does not clamp to documented gameplay modes.
        Require(bikeFlagsAccess.SetPedCantBeKnockedOffBike(ReferenceToken(), value), "SetCantBeKnockedOffBike");
    }
    bool CPedFlagsDE::GetCanBeShotInVehicle() const
    {
        bool value;
        Require(shotFlagsAccess.ReadPedCanBeShotInVehicle(ReferenceToken(), value), "GetCanBeShotInVehicle");
        return value;
    }
    void CPedFlagsDE::SetCanBeShotInVehicle(bool value)
    {
        // Only the independently named flag is owned. Neighbor updates and
        // persistent uncertain effects are the same native owner's concern.
        Require(shotFlagsAccess.SetPedCanBeShotInVehicle(ReferenceToken(), value), "SetCanBeShotInVehicle");
    }
}
