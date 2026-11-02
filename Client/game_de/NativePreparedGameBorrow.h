/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain one prepared CPU graph until its producer explicitly drains it
 ****************************************************************************/
#pragma once
// The genuine weapon SDK uses Win32 scalar types. Keep this standalone
// header usable before any project PCH or another Windows-aware header.
#include <windows.h>
#include "NativeGameConstructorServices.h"
#include <game/CGameClockService.h>
#include <atomic>
#include <cstdint>
#include <limits>
#include <type_traits>
namespace mta::game_de
{
    namespace NativePreparedGameBorrowDetail
    {
        // A retained slot prevents backend retirement. Identities never recycle
        // across ledger instances or later generations in that live backend.
        inline std::atomic<std::uint64_t> nextId{0};
        inline std::uint64_t              NextId() noexcept
        {
            auto prior = nextId.load();
            for (unsigned attempt = 0; attempt != 16; ++attempt)
            {
                if (prior == (std::numeric_limits<std::uint64_t>::max)())
                    return 0;
                if (nextId.compare_exchange_strong(prior, prior + 1))
                    return prior + 1;
            }
            return 0;
        }
    }
    class NativePreparedGameBorrow final
    {
    public:
        struct Identity
        {
            std::uint64_t  generation = 0, admissionSerial = 0;
            std::uintptr_t image = 0;
            std::uint32_t  producerThread = 0;
        };

    private:
        struct Facts
        {
            Identity      owner;
            std::uint64_t id = 0;
        };

    public:
        class Token final
        {
        public:
            Token() noexcept = default;
            ~Token() = default;
            Token(const Token&) = delete;
            Token& operator=(const Token&) = delete;
            Token& operator=(Token&&) = delete;
            Token(Token&& other) noexcept : facts(other.facts) { other.facts = {}; }
            explicit        operator bool() const noexcept { return facts.id != 0; }
            std::uint64_t   Id() const noexcept { return facts.id; }
            const Identity& Owner() const noexcept { return facts.owner; }

        private:
            friend class NativePreparedGameBorrow;
            // Token destruction is deliberately inert. Losing a token cannot
            // silently release a graph that another SDK borrower still uses.
            Facts facts;
        };
        bool        IsRetained() const noexcept { return (state.load() & Held) != 0; }
        bool        IssuanceClosed() const noexcept { return (state.load() & Closed) != 0; }
        void        CloseIssuance() noexcept { state.fetch_or(Closed); }
        ClockResult Open(const Identity& identity) noexcept
        {
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (IsRetained())
                return ClockResult::Busy;
            if (!Valid(identity))
                return ClockResult::InvalidArgument;
            // Runtime calls this only at genuinely quiescent qualified Begin.
            // An old live slot is never cleared to admit a new generation.
            state.store(Closed);
            binding = identity;
            slot = {};
            services = {};
            state.store(0);
            return ClockResult::Ok;
        }
        template <class Qualification>
        ClockResult AcquireChecked(const Identity& identity, const NativeGameConstructorServices& bundle, Token& output, Qualification&& qualify) noexcept
        {
            if (output || !bundle.Complete())
                return ClockResult::InvalidArgument;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            auto observed = state.load();
            if (observed & Closed)
                return ClockResult::Stopped;
            if (observed & Held)
                return ClockResult::Busy;
            if (!Same(binding, identity) || bundle.generation != identity.generation)
                return ClockResult::OwnerConflict;
            const auto result = qualify();
            if (result != ClockResult::Ok)
                return result;
            const auto id = NativePreparedGameBorrowDetail::NextId();
            if (!id)
                return ClockResult::OwnerConflict;
            const Facts next{identity, id};
            slot = next;
            services = bundle;
            // Stop and issuance share this atomic decision. No fallible native
            // checks follow the commit, so a callback cannot strand its token.
            if (!state.compare_exchange_strong(observed, observed | Held))
            {
                slot = {};
                services = {};
                return observed & Closed ? ClockResult::Stopped : ClockResult::Busy;
            }
            output.facts = next;
            return ClockResult::Ok;
        }
        ClockResult Validate(const Token& token, const Identity& identity) noexcept
        {
            if (!token)
                return ClockResult::InvalidArgument;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (!Matches(token, identity))
                return ClockResult::OwnerConflict;
            // This is CPU hold identity only; the Runtime must still freshly
            // qualify the real current callback and prepared native services.
            return IssuanceClosed() ? ClockResult::Stopped : ClockResult::Ok;
        }
        ClockResult ReadServices(const Token& token, const Identity& identity, NativeGameConstructorServices& output) noexcept
        {
            if (!token)
                return ClockResult::InvalidArgument;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (!Matches(token, identity))
                return ClockResult::OwnerConflict;
            if (IssuanceClosed())
                return ClockResult::Stopped;
            const auto next = services;
            if (IssuanceClosed())
                return ClockResult::Stopped;
            output = next;
            return ClockResult::Ok;
        }
        ClockResult Release(Token& token, const Identity& identity) noexcept
        {
            if (!token)
                return ClockResult::InvalidArgument;
            if (GetCurrentThreadId() != token.facts.owner.producerThread)
                return ClockResult::WrongThread;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (!Matches(token, identity))
                return ClockResult::OwnerConflict;
            if (!IssuanceClosed())
                return ClockResult::Busy;
            // Runtime independently proves stopped/quiescent producer drain
            // before this CPU-only decrement. No native cleanup is inferred.
            slot = {};
            services = {};
            token.facts = {};
            state.fetch_sub(Held);
            return ClockResult::Ok;
        }

    private:
        static constexpr std::uint32_t Held = 1, Closed = 2;
        struct Lock
        {
            std::atomic_flag& flag;
            bool              held;
            explicit Lock(std::atomic_flag& value) noexcept : flag(value), held(!value.test_and_set()) {}
            ~Lock()
            {
                if (held)
                    flag.clear();
            }
            explicit operator bool() const noexcept { return held; }
        };
        static bool Valid(const Identity& identity) noexcept
        {
            return identity.generation && identity.admissionSerial && identity.image && identity.producerThread;
        }
        static bool Same(const Identity& first, const Identity& second) noexcept
        {
            return first.generation == second.generation && first.admissionSerial == second.admissionSerial && first.image == second.image &&
                   first.producerThread == second.producerThread;
        }
        bool Matches(const Token& token, const Identity& identity) const noexcept
        {
            return IsRetained() && Same(binding, identity) && Same(token.facts.owner, identity) && slot.id && slot.id == token.facts.id &&
                   Same(slot.owner, identity);
        }
        Identity                      binding;
        Facts                         slot;
        NativeGameConstructorServices services;
        std::atomic<std::uint32_t>    state{Closed};
        std::atomic_flag              busy = ATOMIC_FLAG_INIT;
    };
    static_assert(!std::is_copy_constructible_v<NativePreparedGameBorrow::Token> && !std::is_copy_assignable_v<NativePreparedGameBorrow::Token> &&
                  std::is_nothrow_move_constructible_v<NativePreparedGameBorrow::Token> && !std::is_move_assignable_v<NativePreparedGameBorrow::Token>);
}
