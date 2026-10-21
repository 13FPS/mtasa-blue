/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual SDK argument bits and native rope ID results
 *****************************************************************************/
#include "CRopesDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CRopesDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE CRopes ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                     ")");
    }
    int CRopesDE::CreateRopeForSwatPed(const CVector& position, DWORD duration)
    {
        static_assert(sizeof(int) == sizeof(std::int32_t) && sizeof(DWORD) == sizeof(std::uint32_t));
        std::int32_t id = -1;
        Require(native.CreateSwatRope(position, duration, id), "CreateRopeForSwatPed");
        return id;
    }
    void CRopesDE::RemoveEntityRope(CEntitySAInterface* entity)
    {
        // The existing SDK parameter is an opaque identity key. Native row
        // selection compares only its bits; it is never a classic layout cast
        // or permission to read through a guessed DE entity wrapper.
        Require(native.RemoveEntityRope(reinterpret_cast<std::uintptr_t>(entity)), "RemoveEntityRope");
    }
}
