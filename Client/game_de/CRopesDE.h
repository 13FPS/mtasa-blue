/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete existing two-method MTA rope SDK without an entity cast
 *****************************************************************************/
#pragma once
// The existing SDK signature uses DWORD; this backend is also compiled as a
// standalone translation unit without the classic game's Windows PCH.
#include <windows.h>
#include <game/Common.h>
#include <game/CRopes.h>
#include "NativeRopesAccess.h"
#include <type_traits>

namespace mta::game_de
{
    class CRopesDE final : public CRopes
    {
    public:
        explicit CRopesDE(NativeRopesAccess& access) noexcept : native(access) {}
        int  CreateRopeForSwatPed(const CVector& position, DWORD duration = 4000) override;
        void RemoveEntityRope(CEntitySAInterface* entity) override;

    private:
        static void        Require(ClockResult, const char*);
        NativeRopesAccess& native;
    };
    static_assert(!std::is_abstract_v<CRopesDE> && std::is_base_of_v<CRopes, CRopesDE>);
}
