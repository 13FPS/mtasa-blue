/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the existing engine callback for atomic MTA weather overrides
 *****************************************************************************/
#pragma once
#include "NativeWeatherOverrideAtomic.h"
#include <atomic>
#include <optional>

namespace mta::game_de
{
    class NativeWeatherOverrideHost
    {
    public:
        using Window = NativeWeatherOverrideAtomicProfile::Window;
        // recovery accepts this same generation's natural callback while End
        // is pending. It never authorizes another thread or a stale binding.
        virtual ClockResult Check(std::uint64_t generation, bool recovery) noexcept = 0;
        // Prepare every handle/allocation before suspending. Reject a RIP in
        // each full Window exclusion range, then retain all handles on failure.
        virtual ClockResult ParkPeers(const Window*, std::size_t count) noexcept = 0;
        virtual bool        PeersHeld() const noexcept = 0;
        virtual bool        FinishPeers(bool safeToResume) noexcept = 0;

    protected:
        ~NativeWeatherOverrideHost() = default;
    };
    class NativeWeatherOverridePlatform
    {
    public:
        using Binding = NativeWeatherScalarProfile::Binding;
        using Field = NativeWeatherScalarProfile::Field;
        using CodeRead = NativeWeatherOverrideProfile::CodeRead;
        using Bytes = std::array<unsigned char, 16>;
        enum class Exchange
        {
            Unavailable,
            Conflict,
            Exchanged
        };
        virtual CodeRead       Reader() const noexcept = 0;
        virtual bool           Revalidate(const Binding&, const NativeWeatherOverrideCodeView&) noexcept = 0;
        virtual bool           CompareExchange128() const noexcept = 0;
        virtual std::uintptr_t AllocateNear(const NativeWeatherOverrideProfile::Range&) noexcept = 0;
        virtual bool           ScratchValid(std::uintptr_t) const noexcept = 0;
        virtual bool           ReleaseScratch(std::uintptr_t) noexcept = 0;
        virtual bool           OpenCode(const Binding&, std::uintptr_t, std::size_t, std::uint32_t& previous) noexcept = 0;
        virtual bool           CloseCode(const Binding&, std::uintptr_t, std::size_t, std::uint32_t previous) noexcept = 0;
        // Exchanged/Conflict/Unavailable all describe one indivisible aligned
        // hardware exchange. No platform implementation may copy an operand
        // partially, report an ambiguous write, or introduce torn instruction
        // bytes which the current callback could execute after returning.
        virtual Exchange ExchangeCode(const Binding&, std::uintptr_t, std::size_t, const Bytes& expected, const Bytes& desired) noexcept = 0;
        virtual bool     Flush(std::uintptr_t, std::size_t) noexcept = 0;
        virtual bool     ReadScalar(const Binding&, Field, std::uint32_t&) noexcept = 0;
        virtual Exchange ExchangeScalar(const Binding&, Field, std::uint32_t expected, std::uint32_t desired) noexcept = 0;

    protected:
        ~NativeWeatherOverridePlatform() = default;
    };
    class NativeWeatherOverrideOwner final
    {
    public:
        using Binding = NativeWeatherScalarProfile::Binding;
        using Field = NativeWeatherScalarProfile::Field;
        NativeWeatherOverrideOwner(NativeWeatherOverrideHost& host, NativeWeatherOverridePlatform& platform) noexcept : host(host), platform(platform) {}
        NativeWeatherOverrideOwner(const NativeWeatherOverrideOwner&) = delete;
        NativeWeatherOverrideOwner& operator=(const NativeWeatherOverrideOwner&) = delete;
        ClockResult                 Bind(const Binding&, std::uint64_t generation) noexcept;
        bool                        Admits(Field field) const noexcept;
        ClockResult                 Set(Field, float) noexcept;
        ClockResult                 Reset(Field) noexcept;
        ClockResult                 RestoreAll() noexcept;
        ClockResult                 Recover() noexcept { return RestoreAll(); }
        bool                        HasReferences() const noexcept { return references.load(); }
        bool                        NeedsRecovery() const noexcept { return recovery.load(); }
        // Atomic windows exclude torn code by construction. Unreadable or
        // foreign observations still retain scratch/module debt; they cannot
        // turn a partial-copy operation into a safe continuation.
        bool                          UnsafeCode() const noexcept { return false; }
        std::uint32_t                 SupersededCount() const noexcept { return superseded.load(); }
        NativeWeatherOverrideCodeView CodeView() const noexcept;

    private:
        struct Site
        {
            bool          protectionPending = false, cachePending = false;
            std::uint32_t protection = 0;
        };
        struct Group
        {
            std::optional<NativeWeatherOverrideProfile::Ledger> ledger;
            std::array<Site, 3>                                 sites{};
            std::uintptr_t                                      scratch = 0;
            bool                                                active = false, scalarPending = false;
            std::uint32_t                                       original = 0, replacement = 0;
        };
        ClockResult                    Admit(bool recovery) noexcept;
        ClockResult                    Prepare(Field, Group&) noexcept;
        ClockResult                    Patch(Group&, bool redirected) noexcept;
        ClockResult                    RestoreScalar(Field, Group&) noexcept;
        ClockResult                    Retire(Field, Group&) noexcept;
        ClockResult                    Finish(ClockResult) noexcept;
        void                           Publish(bool failed) noexcept;
        NativeWeatherOverrideHost&     host;
        NativeWeatherOverridePlatform& platform;
        Binding                        binding{};
        std::uint64_t                  generation = 0;
        std::array<Group, 8>           groups{};
        std::atomic<bool>              references{false}, recovery{false};
        std::atomic<std::uint32_t>     superseded{0};
    };
}
