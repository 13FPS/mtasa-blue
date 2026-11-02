/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain a prepared startup graph without publishing a game factory
 ****************************************************************************/
#pragma once
// The genuine weapon SDK uses Win32 scalar types. Keep this standalone
// header usable before any project PCH or another Windows-aware header.
#include <windows.h>
#include "NativeGameConstructorServices.h"
#include "NativePreparedGameBorrow.h"
#include <atomic>
namespace mta::game_de
{
    class NativeGameTimingAccess;
    // The enclosing host serializes acquisition/read/destruction, closes
    // publication and quiesces all external borrows before producer drain.
    // Keep this envelope outside any later typed product: constructor failure
    // must leave the acquired token reachable by that same enclosing owner.
    class CGamePreparedBorrowDE final
    {
    public:
        explicit CGamePreparedBorrowDE(NativeGameTimingAccess&) noexcept;
        ~CGamePreparedBorrowDE() noexcept;
        CGamePreparedBorrowDE(const CGamePreparedBorrowDE&) = delete;
        CGamePreparedBorrowDE& operator=(const CGamePreparedBorrowDE&) = delete;
        CGamePreparedBorrowDE(CGamePreparedBorrowDE&&) = delete;
        CGamePreparedBorrowDE& operator=(CGamePreparedBorrowDE&&) = delete;

        ClockResult Acquire(const NativeGameConstructorServices&) noexcept;
        ClockResult ReadServices(NativeGameConstructorServices&) const noexcept;
        void        ClosePublication() noexcept { publicationClosed.store(true); }
        ClockResult CheckProducerDrain() const noexcept;
        ClockResult ReleaseOnProducer() noexcept;

    private:
        NativeGameTimingAccess&         native;
        NativeGameConstructorServices   services;
        NativePreparedGameBorrow::Token token;
        std::atomic<bool>               publicationClosed{false};
    };
}
