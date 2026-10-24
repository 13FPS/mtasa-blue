/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward one copied registration and preserve the native signed group
 ****************************************************************************/
#include "CPlayerPedMoveAnimDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    eMoveAnim CPlayerPedMoveAnimDE::GetMoveAnim()
    {
        // Match actual CPlayerPedSA's signed int field cast, including values
        // outside MTA's setter policy. Do not substitute the custom CPU cache.
        const auto   token = reference;
        std::int32_t value;
        const auto   result = native.ReadPedMoveAnim(token, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE player movement owner refused GetMoveAnim: " + std::to_string(static_cast<unsigned>(result)));
        static_assert(sizeof(int) == sizeof(value));
        return static_cast<eMoveAnim>(value);
    }
}
