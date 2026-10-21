/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete actual three-method MTA wanted object without native ownership
 *****************************************************************************/
#pragma once
#include <windows.h>  // Standalone backend units need the SDK's DWORD definition.
#include <game/Common.h>
#include <game/CWanted.h>
#include "NativeWantedAccess.h"
#include <type_traits>

namespace mta::game_de
{
    class CWantedDE final : public CWanted
    {
    public:
        explicit CWantedDE(NativeWantedAccess& access) noexcept : native(access) {}
        void SetWantedLevel(DWORD level) override;
        char GetWantedLevel() override;
        void SetWantedLevelNoFlash(DWORD level) override;

    private:
        static void Require(ClockResult result, const char* operation);
        // This stable MTA object borrows the retained service. Every method
        // obtains a fresh loaded native wanted object; no pointer outlives it.
        NativeWantedAccess& native;
    };
    static_assert(!std::is_abstract_v<CWantedDE> && std::is_base_of_v<CWanted, CWantedDE>);
}
