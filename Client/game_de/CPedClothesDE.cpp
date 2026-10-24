/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward MTA's clothing edit once to its registered native owner
 ****************************************************************************/
#include "CPedClothesDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CPedClothesDE::SetClothesTextureAndModel(const char* texture, const char* model, int textureType)
    {
        // Classic CPedSA narrows the caller's int to an unsigned byte before
        // the genuine descriptor method. Preserve that conversion and the
        // nullable name pointers; the owner qualifies/copies them before entry.
        const auto selected = ReferenceToken();
        const auto part = static_cast<std::uint8_t>(textureType);
        const auto result = nativeClothesAccess.SetPedClothes(selected, texture, model, part);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped clothes owner refused SetClothesTextureAndModel: " + std::to_string(static_cast<unsigned>(result)));
    }
}
