/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw SDK hour bytes and visible native mutation refusals
 ****************************************************************************/
#include "CModelInfoTimeDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de::ModelTimeSdk
{
    NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess& classification, NativeModelTimeAccess& time)
    {
        // Getter and setter must be sibling views of one retained owner. A
        // second writer cannot redirect an already bound model facade.
        if (dynamic_cast<NativeModelTimeAccess*>(&classification) != &time)
            throw std::runtime_error("DE model time methods require the same classification owner");
        return classification;
    }
    bool Set(NativeModelTimeAccess& owner, std::uint32_t model, char on, char off)
    {
        bool applied = false;
        // Existing script validation is outside this SDK method. Preserve the
        // supplied char representations, including high-bit values, just as
        // classic SetTime writes both bytes without a new range or clamp.
        const auto result = owner.SetModelTime(model, static_cast<std::uint8_t>(on), static_cast<std::uint8_t>(off), applied);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE SetTime refused: " + std::to_string(static_cast<unsigned>(result)));
        return applied;
    }
}
