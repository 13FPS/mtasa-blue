/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA BYTE property without script interior effects
 ****************************************************************************/
#include "CPedAreaDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    BYTE CPedAreaDE::GetAreaCode()
    {
        std::uint8_t area = 0;
        const auto   result = nativeAreaAccess.ReadPedAreaCode(ReferenceToken(), area);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped area owner refused GetAreaCode: " + std::to_string(static_cast<unsigned>(result)));
        return area;
    }
    void CPedAreaDE::SetAreaCode(BYTE area)
    {
        // Actual CEntitySA simply stores its BYTE. Calling the similarly named
        // DE script command would additionally clear interior/player state and
        // change MTA's contract. The qualified owner performs only this store.
        const auto result = nativeAreaAccess.SetPedAreaCode(ReferenceToken(), area);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped area owner refused SetAreaCode: " + std::to_string(static_cast<unsigned>(result)));
    }
}
