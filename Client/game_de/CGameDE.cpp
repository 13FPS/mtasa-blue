/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse completed SDK services in the future complete DE game object
 *****************************************************************************/
#include "CGameDE.h"

#include <stdexcept>
#include <string>

namespace mta::game_de
{
    CGameDE::CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter,
                     const NativeStreamingFileIdsReader& streamingReader, const NativeGameStateReader& stateReader) noexcept
        : CGameTimingDE(reader, timingWriter), clock(nativeClock), streaming(streamingReader), gameState(stateReader)
    {
    }

    CClock* CGameDE::GetClock() noexcept
    {
        return &clock;
    }

    CKeyGen* CGameDE::GetKeyGen() noexcept
    {
        return &keyGen;
    }

    SystemState::Enum CGameDE::GetSystemState()
    {
        SystemState::Enum state;
        const auto        result = gameState.Read(state);
        // DE startup indices are not a portable enum contract. Unknown early
        // states must refuse rather than admit MTA's gameplay/loading paths
        // using an unproved cast or an old snapshot from another native state.
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame system state unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return state;
    }

    bool CGameDE::IsAtMenu()
    {
        bool       visible;
        const auto result = gameState.ReadMenu(visible);
        // DE pause menus have a managed widget owner as well as the legacy
        // frontend stack. An unavailable owner cannot silently admit MTA's
        // gameplay or chat paths by returning a guessed false menu state.
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame menu state unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return visible;
    }

    NativeStreamingFileIds CGameDE::RequireStreamingLayout() const
    {
        NativeStreamingFileIds ids;
        const auto             result = streaming.Read(ids);
        // File-ID ranges are part of the SDK contract, not a reason to reuse
        // classic CStreamingInfo storage. A changed DE layout must refuse
        // explicitly before a client can index an unrelated native structure.
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame streaming IDs unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return ids;
    }

    uint32_t CGameDE::GetBaseIDforDFF()
    {
        return RequireStreamingLayout().dff;
    }

    uint32_t CGameDE::GetBaseIDforTXD()
    {
        return RequireStreamingLayout().txd;
    }

    uint32_t CGameDE::GetBaseIDforCOL()
    {
        return RequireStreamingLayout().col;
    }

    uint32_t CGameDE::GetBaseIDforIPL()
    {
        return RequireStreamingLayout().ipl;
    }

    uint32_t CGameDE::GetBaseIDforDAT()
    {
        return RequireStreamingLayout().dat;
    }

    uint32_t CGameDE::GetBaseIDforIFP()
    {
        return RequireStreamingLayout().ifp;
    }

    uint32_t CGameDE::GetBaseIDforRRR()
    {
        return RequireStreamingLayout().rrr;
    }

    uint32_t CGameDE::GetBaseIDforSCM()
    {
        return RequireStreamingLayout().scm;
    }

    uint32_t CGameDE::GetCountOfAllFileIDs()
    {
        // This SDK count includes the native array's four sentinel rows;
        // neither count nor category bases imply a classic row layout.
        return RequireStreamingLayout().count;
    }
}
