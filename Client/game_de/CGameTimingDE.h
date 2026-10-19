/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real MTA CGame timing overrides while other DE subsystems are absent
 *****************************************************************************/
#pragma once

#include <windows.h>
#include <cstdint>
#include <type_traits>
#include <game/CGame.h>
#include "NativeGameTimingReader.h"
#include "NativeGameTimingAccess.h"

namespace mta::game_de
{
    // Implement the actual SDK methods now, while the compiler continues to
    // prohibit constructing/exporting an incomplete CGame. Remaining native
    // subsystems must supply their genuine overrides before this can be a game
    // interface; neither an FPS estimate nor unavailable no-op setters belong
    // in this base. GameAdapter::GetGameInterface therefore remains unchanged.
    class CGameTimingDE : public CGame
    {
    public:
        DWORD         GetSystemTime() override;
        int           GetSystemFrameCounter() const override;
        float         GetTimeStep() override;
        float         GetOldTimeStep() override;
        float         GetTimeScale() override;
        float         GetGameSpeed() override;
        unsigned long GetMinuteDuration() override;
        void          SetTimeScale(float value) override;
        void          SetGameSpeed(float value) override;
        void          SetMinuteDuration(unsigned long value) override;

        // The SDK declares these defaults outside this module. Keep them pure
        // until a real DE weapon subsystem exists instead of linking classic
        // definitions or silently inheriting an unavailable implementation.
        void SetJetpackWeaponEnabled(eWeaponType weaponType, bool enabled) override = 0;
        bool GetJetpackWeaponEnabled(eWeaponType weaponType) override = 0;

    protected:
        CGameTimingDE(const NativeGameTimingReader& reader, NativeGameTimingAccess& writer) noexcept : timing(reader), timingWriter(writer) {}
        static NativeGameTimingValues RequireTiming(const NativeGameTimingReader& reader);

    private:
        const NativeGameTimingReader& timing;
        NativeGameTimingAccess&       timingWriter;
    };
    static_assert(std::is_abstract_v<CGameTimingDE> && std::is_base_of_v<CGame, CGameTimingDE>);
}
