/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Assemble completed DE services behind the existing MTA CGame SDK
 *****************************************************************************/
#pragma once

#include "CGameTimingDE.h"
#include "CClockDE.h"
#include "CKeyGenDE.h"
#include "NativeStreamingFileIdsReader.h"
#include "NativeGameStateReader.h"

namespace mta::game_de
{
    // Keep the actual SDK object graph in the backend. The clock remains the
    // engine callback owner's existing instance, so a future complete CGame
    // cannot accidentally create a second native clock or cleanup owner.
    // Unimplemented SDK methods remain pure: this is not yet a factory product.
    class CGameDE : public CGameTimingDE
    {
    public:
        CClock*           GetClock() noexcept override;
        CKeyGen*          GetKeyGen() noexcept override;
        SystemState::Enum GetSystemState() override;
        bool              IsAtMenu() override;

        uint32_t GetBaseIDforDFF() override;
        uint32_t GetBaseIDforTXD() override;
        uint32_t GetBaseIDforCOL() override;
        uint32_t GetBaseIDforIPL() override;
        uint32_t GetBaseIDforDAT() override;
        uint32_t GetBaseIDforIFP() override;
        uint32_t GetBaseIDforRRR() override;
        uint32_t GetBaseIDforSCM() override;
        uint32_t GetCountOfAllFileIDs() override;

        CGameDE(const CGameDE&) = delete;
        CGameDE& operator=(const CGameDE&) = delete;

    protected:
        // Borrowed services must outlive this object. Their qualification
        // and engine-thread rules stay with their existing native owners.
        CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter,
                const NativeStreamingFileIdsReader& streamingReader, const NativeGameStateReader& stateReader) noexcept;

    private:
        NativeStreamingFileIds RequireStreamingLayout() const;

        CClockDE&                           clock;
        CKeyGenDE                           keyGen;
        const NativeStreamingFileIdsReader& streaming;
        const NativeGameStateReader&        gameState;
    };
    static_assert(std::is_abstract_v<CGameDE> && std::is_base_of_v<CGame, CGameDE>);
}
