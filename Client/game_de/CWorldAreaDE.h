/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's current-world area getter without a false setter contract
 *****************************************************************************/
#pragma once
#include "CWorldLineDE.h"
#include "NativeWorldAreaAccess.h"
namespace mta::game_de
{
    class CWorldAreaDE : public CWorldLineDE
    {
    public:
        DWORD GetCurrentArea() override;

    protected:
        CWorldAreaDE(NativeWorldGroundAccess& ground, NativeWorldLineAccess& line, NativeWorldAreaAccess& area) noexcept
            : CWorldLineDE(ground, line), areaOwner(area)
        {
        }

    private:
        NativeWorldAreaAccess& areaOwner;
    };
    // The world property is the actual raw DWORD, distinct from a ped's BYTE
    // area code. SetCurrentArea's native building-removal effects remain pure.
    static_assert(sizeof(DWORD) == sizeof(std::uint32_t));
    static_assert(std::is_abstract_v<CWorldAreaDE> && std::is_base_of_v<CWorldLineDE, CWorldAreaDE>);
}
