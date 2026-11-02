/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain three native camera row clients until producer-owned drain
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
namespace mta::game_de
{
    enum class CameraRowVectorKind : std::uint32_t
    {
        Front,
        Up,
        Source,
        FixedModeSource,
        FixedModeVector
    };
    namespace NativeCameraRowBorrowDetail
    {
        // Slot reuse never reuses an identity, including across generations
        // and ledger instances in this retained backend. No valid token may
        // outlive backend unload: its live slot prevents that unload itself.
        inline std::atomic<std::uint64_t> nextTokenId{0};
        inline std::uint64_t              NextId() noexcept
        {
            auto prior = nextTokenId.load();
            for (unsigned attempt = 0; attempt != 16; ++attempt)
            {
                if (prior == (std::numeric_limits<std::uint64_t>::max)())
                    return 0;
                if (nextTokenId.compare_exchange_strong(prior, prior + 1))
                    return prior + 1;
            }
            return 0;
        }
    }
    class NativeCameraRowBorrow final
    {
    public:
        struct Identity
        {
            std::uint64_t  generation = 0, admissionSerial = 0;
            std::uintptr_t image = 0;
            // This is NativeClockService's Begin/End producer, never the
            // engine callback thread on which acquisition is admitted.
            std::uint32_t producerThread = 0;
        };

    private:
        struct Facts
        {
            Identity      owner;
            std::uint64_t id = 0;
            std::uint32_t row = 0;
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
            std::uint32_t   Row() const noexcept { return facts.row; }
            const Identity& Owner() const noexcept { return facts.owner; }

        private:
            friend class NativeCameraRowBorrow;
            // Destruction alone cannot drop retention. A foreign-thread
            // wrapper destructor must leave the row held, even if that means
            // permanent refusal outside the supported producer drain path.
            Facts facts;
        };
        using CheckCurrentCallback = ClockResult (*)() noexcept;
        NativeCameraRowBorrow() noexcept = default;
        ~NativeCameraRowBorrow() = default;
        NativeCameraRowBorrow(const NativeCameraRowBorrow&) = delete;
        NativeCameraRowBorrow& operator=(const NativeCameraRowBorrow&) = delete;

        // The real host must first pass its Begin/End prior-generation and
        // inFlight gates. Zero row holds alone is not callback quiescence;
        // concurrent Open versus an old callback/Stop is not supported.
        ClockResult Open(const Identity& identity) noexcept
        {
            if (!Valid(identity))
                return ClockResult::InvalidArgument;
            if (GetCurrentThreadId() != identity.producerThread)
                return ClockResult::WrongThread;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (IsRetained())
                return ClockResult::Busy;
            if (!IssuanceClosed())
                return ClockResult::OwnerConflict;
            // Begin's producer setup runs only after old callbacks drain.
            // Reopening the same stopped generation would revoke Stop's
            // guarantee, so even a drained identical binding cannot reopen.
            if (binding.generation && (identity.generation <= binding.generation || identity.admissionSerial <= binding.admissionSerial))
                return ClockResult::OwnerConflict;
            for (const auto& slot : slots)
                if (slot.id)
                    return ClockResult::OwnerConflict;
            binding = identity;
            state.store(0);
            return ClockResult::Ok;
        }
        void CloseIssuance() noexcept { state.fetch_or(Closed); }
        bool IsRetained() const noexcept { return (state.load() & CountMask) != 0; }
        bool IssuanceClosed() const noexcept { return (state.load() & Closed) != 0; }

        ClockResult Acquire(const Identity& identity, std::uint32_t row, CheckCurrentCallback check, Token& output) noexcept
        {
            if (!check)
                return ClockResult::NotInitialized;
            return AcquireChecked(identity, row, check, output);
        }
        template <class Check>
        ClockResult AcquireChecked(const Identity& identity, std::uint32_t row, Check check, Token& output) noexcept
        {
            static_assert(std::is_nothrow_invocable_r_v<ClockResult, Check>);
            if (row >= slots.size() || !Valid(identity) || output)
                return ClockResult::InvalidArgument;
            if (IssuanceClosed())
                return ClockResult::Stopped;
            auto result = check();
            if (result != ClockResult::Ok)
                return result;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (!Same(binding, identity))
                return ClockResult::OwnerConflict;
            if (slots[row].id)
                return ClockResult::Busy;
            if ((result = check()) != ClockResult::Ok)
                return result;
            auto observed = state.load();
            if (observed & Closed)
                return ClockResult::Stopped;
            if ((observed & CountMask) >= slots.size())
                return ClockResult::Busy;
            const auto id = NativeCameraRowBorrowDetail::NextId();
            if (!id)
                return ClockResult::OwnerConflict;
            const Facts next{identity, id, row};
            slots[row] = next;
            // Stop and admission share this one atomic state. If Stop wins
            // the CAS, no token is published; if admission wins, End sees a
            // retained hold before that token can escape the callback.
            if (!state.compare_exchange_strong(observed, observed + 1))
            {
                slots[row] = {};
                return observed & Closed ? ClockResult::Stopped : ClockResult::Busy;
            }
            output.facts = next;
            return ClockResult::Ok;
        }
        ClockResult Validate(const Token& token, const Identity& identity) noexcept
        {
            if (!token || token.facts.row >= slots.size())
                return ClockResult::InvalidArgument;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (!Same(binding, identity) || !Same(token.facts.owner, identity) || !Same(slots[token.facts.row], token.facts))
                return ClockResult::OwnerConflict;
            // Identity is CPU-only retention evidence. Current engine/frame,
            // source, storage and writable-field checks are still mandatory.
            return IssuanceClosed() ? ClockResult::Stopped : ClockResult::Ok;
        }
        ClockResult Release(Token& token) noexcept
        {
            if (!token || token.facts.row >= slots.size())
                return ClockResult::InvalidArgument;
            if (GetCurrentThreadId() != token.facts.owner.producerThread)
                return ClockResult::WrongThread;
            Lock lock{busy};
            if (!lock)
                return ClockResult::Busy;
            if (!Same(binding, token.facts.owner) || !Same(slots[token.facts.row], token.facts) || !(state.load() & CountMask))
                return ClockResult::OwnerConflict;
            // Release has no native access, owner callback, repair or resume.
            // Same-producer synchronous destruction serializes this final
            // decrement with End/Shutdown and the loader's later FreeLibrary.
            slots[token.facts.row] = {};
            token.facts = {};
            state.fetch_sub(1);
            return ClockResult::Ok;
        }

    private:
        static constexpr std::uint32_t Closed = 8, CountMask = 7;
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
        static bool Valid(const Identity& value) noexcept { return value.generation && value.admissionSerial && value.image && value.producerThread; }
        static bool Same(const Identity& a, const Identity& b) noexcept
        {
            return a.generation == b.generation && a.admissionSerial == b.admissionSerial && a.image == b.image && a.producerThread == b.producerThread;
        }
        static bool                Same(const Facts& a, const Facts& b) noexcept { return a.id && a.id == b.id && a.row == b.row && Same(a.owner, b.owner); }
        Identity                   binding;
        std::array<Facts, 3>       slots{};
        std::atomic<std::uint32_t> state{Closed};
        std::atomic_flag           busy = ATOMIC_FLAG_INIT;
    };
    static_assert(!std::is_copy_constructible_v<NativeCameraRowBorrow::Token> && !std::is_copy_assignable_v<NativeCameraRowBorrow::Token> &&
                  std::is_nothrow_move_constructible_v<NativeCameraRowBorrow::Token> && !std::is_move_assignable_v<NativeCameraRowBorrow::Token>);
}
