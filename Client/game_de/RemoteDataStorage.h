/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA per-player storage for DE's canonical controller history
 *****************************************************************************/

#pragma once

#include <game/CGameAdapter.h>
#include <multiplayer/CRemoteDataStorageImpl.h>
#include <cstddef>
#include <type_traits>

namespace mta::game_de
{
    // Only the canonical states belong here. The classic full CPad layout is
    // neither the DE engine pad nor a prerequisite for MTA's storage contract.
    struct RemotePadStorage
    {
        CControllerState NewState;
        CControllerState OldState;
    };
    static_assert(sizeof(RemotePadStorage) == 96 && sizeof(RemotePadStorage) == sizeof(MtaInputState));
    static_assert(std::is_standard_layout_v<RemotePadStorage> && std::is_trivially_copyable_v<RemotePadStorage>);
    static_assert(offsetof(RemotePadStorage, NewState) == 0 && offsetof(RemotePadStorage, OldState) == 48);

    class RemoteDataStorage final : public CRemoteDataStorageImpl<RemotePadStorage>
    {
    public:
        RemoteDataStorage()
        {
            // Classic initialization remains unchanged in the shared base.
            // This new owner gives every additional CPU-only shot field a
            // defined initial value without advertising native shot support.
            m_shotSyncData = CShotSyncData{};
        }

        MtaInputState ReadInput() const noexcept { return {m_pad.NewState, m_pad.OldState}; }
        void          WriteInput(const MtaInputState& input) noexcept
        {
            m_pad.NewState = input.current;
            m_pad.OldState = input.previous;
        }
    };
}
