/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve prepared graph retention until explicit producer release
 ****************************************************************************/
#include "CGamePreparedBorrowDE.h"
#include "NativeGameTimingAccess.h"
#include <exception>
#include <type_traits>
namespace mta::game_de
{
    static_assert(std::is_nothrow_copy_constructible_v<NativeGameConstructorServices> && std::is_nothrow_copy_assignable_v<NativeGameConstructorServices>);
    CGamePreparedBorrowDE::CGamePreparedBorrowDE(NativeGameTimingAccess& owner) noexcept : native(owner)
    {
    }
    CGamePreparedBorrowDE::~CGamePreparedBorrowDE() noexcept
    {
        // A destructor may run on a callback or foreign thread. It cannot
        // establish producer quiescence or discard the Runtime's live hold.
        if (token)
            std::terminate();
    }
    ClockResult CGamePreparedBorrowDE::Acquire(const NativeGameConstructorServices& supplied) noexcept
    {
        if (publicationClosed.load())
            return ClockResult::Stopped;
        if (token)
            return ClockResult::Busy;
        const auto selected = supplied;
        if (!selected.Complete())
            return ClockResult::InvalidArgument;
        auto result = native.ValidateConstructorServices(selected);
        if (result != ClockResult::Ok)
            return result;
        if (!selected.Same(supplied))
            return ClockResult::OwnerConflict;
        result = native.AcquirePreparedGameBorrow(selected, token);
        // The issuer performs all fallible checks before commit. This final
        // identity assignment is nonthrowing; later product construction is
        // deliberately outside the envelope's lifetime.
        if (result == ClockResult::Ok)
            services = selected;
        return result;
    }
    ClockResult CGamePreparedBorrowDE::ReadServices(NativeGameConstructorServices& output) const noexcept
    {
        if (publicationClosed.load())
            return ClockResult::Stopped;
        if (!token)
            return ClockResult::NotInitialized;
        const auto selected = services;
        auto       result = native.ValidatePreparedGameBorrow(token);
        if (result != ClockResult::Ok)
            return result;
        result = native.ValidateConstructorServices(selected);
        if (result != ClockResult::Ok)
            return result;
        result = native.ValidatePreparedGameBorrow(token);
        if (result != ClockResult::Ok)
            return result;
        if (publicationClosed.load())
            return ClockResult::Stopped;
        // Only the complete bookended value escapes. It is still a borrowed
        // identity bundle, never independent native call or factory authority.
        output = selected;
        return ClockResult::Ok;
    }
    ClockResult CGamePreparedBorrowDE::CheckProducerDrain() const noexcept
    {
        if (!publicationClosed.load())
            return ClockResult::Busy;
        return token ? native.CheckPreparedGameBorrowDrain(token) : ClockResult::Ok;
    }
    ClockResult CGamePreparedBorrowDE::ReleaseOnProducer() noexcept
    {
        if (!publicationClosed.load())
            return ClockResult::Busy;
        if (!token)
            return ClockResult::Ok;
        auto result = native.CheckPreparedGameBorrowDrain(token);
        if (result != ClockResult::Ok)
            return result;
        // The enclosing host has already quiesced external users and destroyed
        // its typed products after preflight. Release rechecks independently;
        // a refusal retains this token and bundle for the recorded producer.
        result = native.ReleasePreparedGameBorrow(token);
        if (result == ClockResult::Ok)
            services = {};
        return result;
    }
}
