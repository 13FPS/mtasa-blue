/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise prepared graph CPU retention without a public game factory
 ****************************************************************************/
#include "../../Client/game_de/CGamePreparedBorrowDE.h"
#include <SharedUtil.Defines.h>
#pragma warning(push)
#pragma warning(disable : 4201 4456)
#include <SharedUtil.Misc.h>
#pragma warning(pop)
#include "../../Client/game_de/CGameBuildingPoolDefaultDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include "../../Client/game_de/CClockDE.h"
#include "../../Client/game_de/CGaragesStartupDE.h"
#include "../../Client/game_de/CStreamingStartupDE.h"
#include <game/CFireManager.h>
#include <game/CCamera.h>
#include <game/CHud.h>
#include <game/CPools.h>
#include <game/CDummyPool.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include "PreparedGameBorrowFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Prepared borrow check %u failed: %s\n", checks, reason);
            std::fflush(stderr);
            throw std::runtime_error(reason);
        }
    }
    class ClockOwner final : public NativeClockAccess
    {
        ClockResult Read(ClockValues&) noexcept override { std::abort(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { std::abort(); }
        ClockResult Freeze(bool) noexcept override { std::abort(); }
        ClockResult Frozen(bool&) const noexcept override { std::abort(); }
    };
    class MissingOwner final : public NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { std::abort(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { std::abort(); }
        ClockResult Pause(bool) noexcept override { std::abort(); }
    };
    class Owner final : public NativeGameTimingAccess
    {
    public:
        enum class StartupEvent
        {
            Validate,
            Garages,
            Streaming
        };
        enum class LookupMutation
        {
            None,
            Generation,
            Bundle
        };
        struct StartupLookup
        {
            ClockResult    result = ClockResult::Ok;
            bool           absent = false;
            LookupMutation mutation = LookupMutation::None;
            unsigned       calls = 0;
        };
        NativePreparedGameBorrow           ledger;
        NativePreparedGameBorrow::Identity identity{7, 19, 0x10000000u, GetCurrentThreadId()};
        NativeGameConstructorServices      bundle;
        ClockResult validationResult = ClockResult::Ok, acquisitionResult = ClockResult::Ok, drainResult = ClockResult::Ok, releaseResult = ClockResult::Ok;
        bool        stopped = false, inFlight = false, heldPeers = false;
        unsigned    acquisitions = 0, tokenValidations = 0, bundleValidations = 0, releases = 0;
        unsigned    closeAtTokenValidation = 0;
        CGamePreparedBorrowDE* closeEnvelope = nullptr;
        std::uint64_t          issuedId = 0;
        // Real complete SDK receivers keep these getter tests on the actual
        // startup graph. Their constructors perform no native operation; the
        // provider double below alone selects and faults their publication.
        CGaragesStartupDE           garagesSdk{*this};
        CStreamingStartupDE         streamingSdk{*this};
        StartupLookup               garagesLookup, streamingLookup;
        std::array<StartupEvent, 8> startupEvents{};
        unsigned                    startupEventCount = 0;
        bool                        recordStartupEvents = false;
        std::uintptr_t              foreignServiceTag = 0;
        Owner()
        {
            if (ledger.Open(identity) != ClockResult::Ok)
                throw std::runtime_error("test CPU issuer failed to open");
        }
        ClockResult SetTimeScale(float) noexcept override { std::abort(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { std::abort(); }
        ClockResult Pause(bool) noexcept override { std::abort(); }
        ClockResult ValidateConstructorServices(const NativeGameConstructorServices& selected) noexcept override
        {
            ++bundleValidations;
            RecordStartupEvent(StartupEvent::Validate);
            if (validationResult != ClockResult::Ok)
                return validationResult;
            return selected.Same(bundle) ? ClockResult::Ok : ClockResult::OwnerConflict;
        }
        void RecordStartupEvent(StartupEvent event) noexcept
        {
            if (recordStartupEvents)
            {
                if (startupEventCount == startupEvents.size())
                    std::abort();
                startupEvents[startupEventCount++] = event;
            }
        }
        void BeginStartupTrace() noexcept
        {
            startupEventCount = 0;
            recordStartupEvents = true;
            garagesLookup.calls = 0;
            streamingLookup.calls = 0;
        }
        template <class Receiver, class Base>
        ClockResult SelectStartup(StartupLookup& lookup, StartupEvent event, Receiver& receiver, Base*& output) noexcept
        {
            ++lookup.calls;
            RecordStartupEvent(event);
            if (lookup.result != ClockResult::Ok)
                return lookup.result;
            output = lookup.absent ? nullptr : &receiver;
            // A successful lookup can race a generation or any other captured
            // service identity. Mutation after assigning the selected receiver
            // proves the actual CGame getter needs its second bundle bookend.
            if (lookup.mutation == LookupMutation::Generation)
                ++bundle.generation;
            else if (lookup.mutation == LookupMutation::Bundle)
                bundle.stats = reinterpret_cast<CStats*>(&foreignServiceTag);
            return ClockResult::Ok;
        }
        ClockResult GetStartupNumericGarages(CGarages*& output) noexcept override
        {
            return SelectStartup(garagesLookup, StartupEvent::Garages, garagesSdk, output);
        }
        ClockResult GetStartupStreaming(CStreaming*& output) noexcept override
        {
            return SelectStartup(streamingLookup, StartupEvent::Streaming, streamingSdk, output);
        }
        ClockResult CaptureConstructorServices(const CClock* clock, NativeGameConstructorServices& output) noexcept override
        {
            if (clock != bundle.clock)
                return ClockResult::OwnerConflict;
            const auto result = ValidateConstructorServices(bundle);
            if (result == ClockResult::Ok)
                output = bundle;
            return result;
        }
        ClockResult AcquirePreparedGameBorrow(const NativeGameConstructorServices& selected, NativePreparedGameBorrow::Token& output) noexcept override
        {
            ++acquisitions;
            if (acquisitionResult != ClockResult::Ok)
                return acquisitionResult;
            if (stopped)
                return ClockResult::Stopped;
            const auto result = ledger.AcquireChecked(identity, selected, output, [&]() noexcept { return ValidateConstructorServices(selected); });
            if (result == ClockResult::Ok)
                issuedId = output.Id();
            return result;
        }
        ClockResult ValidatePreparedGameBorrow(const NativePreparedGameBorrow::Token& token) noexcept override
        {
            ++tokenValidations;
            auto result = ledger.Validate(token, identity);
            if (result != ClockResult::Ok)
                return result;
            NativeGameConstructorServices selected;
            result = ledger.ReadServices(token, identity, selected);
            if (result != ClockResult::Ok)
                return result;
            result = ValidateConstructorServices(selected);
            // Model an outer close racing the final publication boundary.
            // This does not manufacture a native token or change its owner.
            if (result == ClockResult::Ok && closeEnvelope && tokenValidations == closeAtTokenValidation)
                closeEnvelope->ClosePublication();
            return result;
        }
        ClockResult CheckPreparedGameBorrowDrain(const NativePreparedGameBorrow::Token& token) noexcept override
        {
            if (GetCurrentThreadId() != identity.producerThread)
                return ClockResult::WrongThread;
            if (!stopped || inFlight || heldPeers || !ledger.IssuanceClosed())
                return ClockResult::Busy;
            if (drainResult != ClockResult::Ok)
                return drainResult;
            const auto result = ledger.Validate(token, identity);
            return result == ClockResult::Stopped ? ClockResult::Ok : result;
        }
        ClockResult ReleasePreparedGameBorrow(NativePreparedGameBorrow::Token& token) noexcept override
        {
            ++releases;
            const auto result = CheckPreparedGameBorrowDrain(token);
            if (result != ClockResult::Ok)
                return result;
            if (releaseResult != ClockResult::Ok)
                return releaseResult;
            return ledger.Release(token, identity);
        }
        void Stop() noexcept
        {
            stopped = true;
            ledger.CloseIssuance();
        }
    };
    struct Context
    {
        NativeGameTimingReader          reader;
        NativeStreamingFileIdsReader    streaming;
        NativeGameStateReader           state;
        ClockOwner                      clockOwner;
        CClockDE                        clock{clockOwner};
        Owner                           owner;
        std::array<std::uintptr_t, 128> tags{};
        Context()
        {
            // These are opaque portable test-service identities. The fixture
            // compares them but never dereferences them as native services.
            auto& b = owner.bundle;
            b.generation = owner.identity.generation;
            b.clock = &clock;
            b.stats = reinterpret_cast<CStats*>(&tags[0]);
            b.playerInfo = reinterpret_cast<CPlayerInfo*>(&tags[1]);
            b.wanted = reinterpret_cast<CWanted*>(&tags[2]);
            b.ropes = reinterpret_cast<CRopes*>(&tags[3]);
            b.weather = reinterpret_cast<CWeather*>(&tags[4]);
            b.weaponManager = reinterpret_cast<CWeaponStatManager*>(&tags[5]);
            for (std::size_t i = 0; i < b.weapons.size(); ++i)
                b.weapons[i] = reinterpret_cast<CWeaponInfo*>(&tags[6 + i]);
        }
    };
    static_assert(!std::is_copy_constructible_v<CGamePreparedBorrowDE> && !std::is_move_constructible_v<CGamePreparedBorrowDE> &&
                  !std::is_copy_assignable_v<CGamePreparedBorrowDE> && !std::is_move_assignable_v<CGamePreparedBorrowDE> &&
                  std::is_nothrow_constructible_v<CGamePreparedBorrowDE, NativeGameTimingAccess&> && std::is_nothrow_destructible_v<CGamePreparedBorrowDE>);
    static_assert(std::is_abstract_v<CGameBuildingPoolDefaultDE> && std::is_abstract_v<CGamePresentationPolicyMixinDE<CGameBuildingPoolDefaultDE>> &&
                  std::is_convertible_v<CGamePresentationPolicyMixinDE<CGameBuildingPoolDefaultDE>*, CGame*>);
    // The strict fixture must inherit the production startup getters. A future
    // generated replacement could otherwise hide the exact binding regression.
    static_assert(std::is_same_v<decltype(&prepared_game_fixture::Game::GetGarages), decltype(&CGameNumericGaragesDE::GetGarages)> &&
                  std::is_same_v<decltype(&prepared_game_fixture::Game::GetStreaming), decltype(&CGameStartupStreamingDE::GetStreaming)>);
    void Lifetime()
    {
        Context               c;
        CGamePreparedBorrowDE envelope(c.owner);
        Check(!c.owner.ledger.IsRetained() && !c.owner.acquisitions, "CPU-only envelope construction grants no service hold");
        auto sentinel = c.owner.bundle;
        sentinel.generation = 999;
        auto output = sentinel;
        Check(envelope.ReadServices(output) == ClockResult::NotInitialized && output.Same(sentinel), "missing token preserves the caller service output");
        Check(envelope.CheckProducerDrain() == ClockResult::Busy, "open publication refuses drain even before acquisition");
        const auto copy = c.owner.bundle;
        Check(!c.owner.ledger.IsRetained(), "a copied complete bundle remains identity data without a hold");
        Check(envelope.Acquire(copy) == ClockResult::Ok && c.owner.ledger.IsRetained() && c.owner.issuedId,
              "qualified acquisition issues a real unique CPU token");
        Check(envelope.Acquire(copy) == ClockResult::Busy && c.owner.acquisitions == 1, "one envelope cannot acquire or replace a second graph hold");
        Check(envelope.ReadServices(output) == ClockResult::Ok && output.Same(copy) && c.owner.tokenValidations == 2,
              "current services publish only through token and bundle bookends");
        auto     graph = std::make_unique<prepared_game_fixture::Game>(c.reader, c.clock, c.owner, c.streaming, c.state, output);
        CGame*   common = graph.get();
        CGameDE* actual = graph.get();
        Check(common == static_cast<CGame*>(actual) && common->GetClock() == &c.clock,
              "the existing prepared partial graph retains one actual CGame and clock");
        Check(envelope.ReleaseOnProducer() == ClockResult::Busy && c.owner.ledger.IsRetained() && !c.owner.releases,
              "an open graph cannot be released beneath its typed product");
        envelope.ClosePublication();
        output = sentinel;
        Check(envelope.ReadServices(output) == ClockResult::Stopped && output.Same(sentinel),
              "closed publication preserves outputs and hides retained services");
        Check(envelope.Acquire(copy) == ClockResult::Stopped && c.owner.acquisitions == 1, "closed publication cannot acquire or reopen a graph");
        Check(envelope.CheckProducerDrain() == ClockResult::Busy && c.owner.ledger.IsRetained(), "local close does not substitute for owner Stop");
        c.owner.Stop();
        c.owner.inFlight = true;
        Check(envelope.CheckProducerDrain() == ClockResult::Busy && c.owner.ledger.IsRetained() && graph.get() == actual,
              "in-flight refusal preserves both typed product and token");
        c.owner.inFlight = false;
        c.owner.heldPeers = true;
        Check(envelope.ReleaseOnProducer() == ClockResult::Busy && c.owner.ledger.IsRetained() && !c.owner.releases,
              "retained peers prevent release before any native release call");
        c.owner.heldPeers = false;
        ClockResult foreignCheck = ClockResult::Ok, foreignRelease = ClockResult::Ok;
        std::thread worker(
            [&]
            {
                foreignCheck = envelope.CheckProducerDrain();
                foreignRelease = envelope.ReleaseOnProducer();
            });
        worker.join();
        Check(foreignCheck == ClockResult::WrongThread && foreignRelease == ClockResult::WrongThread && c.owner.ledger.IsRetained() && !c.owner.releases,
              "foreign drain and release leave the actual token and hold for the joined producer");
        Check(envelope.CheckProducerDrain() == ClockResult::Ok && c.owner.ledger.IsRetained(), "producer preflight does not itself consume the hold");
        graph.reset();
        Check(c.owner.ledger.IsRetained() && !c.owner.releases, "typed graph destruction precedes explicit final token release");
        c.owner.releaseResult = ClockResult::Busy;
        Check(envelope.ReleaseOnProducer() == ClockResult::Busy && c.owner.ledger.IsRetained() && c.owner.releases == 1,
              "a refused actual release preserves the token and services for producer retry");
        c.owner.releaseResult = ClockResult::Ok;
        Check(envelope.ReleaseOnProducer() == ClockResult::Ok && !c.owner.ledger.IsRetained() && c.owner.releases == 2,
              "the recorded producer explicitly consumes the held token after typed destruction");
        Check(envelope.CheckProducerDrain() == ClockResult::Ok && envelope.ReleaseOnProducer() == ClockResult::Ok && c.owner.releases == 2,
              "closed drained retirement is idempotent without a second native release");
        std::puts("Prepared game borrow SDK lifetime fixture completed");
        std::puts("Prepared game borrow current graph identity fixture completed");
        std::puts("Prepared game borrow producer drain fixture completed");
    }
    void Refusals()
    {
        Context               c;
        MissingOwner          missing;
        CGamePreparedBorrowDE absent(missing);
        Check(absent.Acquire(c.owner.bundle) == ClockResult::NotInitialized, "missing constructor service authority explicitly refuses without a hold");
        absent.ClosePublication();
        Check(absent.CheckProducerDrain() == ClockResult::Ok && absent.ReleaseOnProducer() == ClockResult::Ok,
              "unused closed envelope needs no invented release authority");
        CGamePreparedBorrowDE envelope(c.owner);
        auto                  invalid = c.owner.bundle;
        invalid.clock = nullptr;
        Check(envelope.Acquire(invalid) == ClockResult::InvalidArgument && !c.owner.ledger.IsRetained() && !c.owner.acquisitions,
              "incomplete input refuses before invoking the token issuer");
        auto wrong = c.owner.bundle;
        wrong.stats = reinterpret_cast<CStats*>(&c.tags[100]);
        Check(wrong.Complete() && envelope.Acquire(wrong) == ClockResult::OwnerConflict && !c.owner.ledger.IsRetained() && !c.owner.acquisitions,
              "a complete foreign service identity cannot stand in for the genuine prepared bundle");
        c.owner.validationResult = ClockResult::OwnerConflict;
        Check(envelope.Acquire(c.owner.bundle) == ClockResult::OwnerConflict && !c.owner.ledger.IsRetained() && !c.owner.acquisitions,
              "wrong bundle owner refuses before token commitment");
        c.owner.validationResult = ClockResult::Ok;
        c.owner.acquisitionResult = ClockResult::WrongThread;
        Check(envelope.Acquire(c.owner.bundle) == ClockResult::WrongThread && !c.owner.ledger.IsRetained(),
              "issuer refusal does not leave or replace a hidden token");
        c.owner.acquisitionResult = ClockResult::Ok;
        Check(envelope.Acquire(c.owner.bundle) == ClockResult::Ok && c.owner.ledger.IsRetained(),
              "the same surviving envelope may acquire after a no-effect refusal");
        auto sentinel = c.owner.bundle;
        sentinel.generation = 999;
        auto output = sentinel;
        c.owner.validationResult = ClockResult::BindingChanged;
        Check(envelope.ReadServices(output) == ClockResult::BindingChanged && output.Same(sentinel) && c.owner.ledger.IsRetained(),
              "current bundle refusal preserves output and retains the graph token");
        c.owner.validationResult = ClockResult::Ok;
        c.owner.closeEnvelope = &envelope;
        c.owner.closeAtTokenValidation = c.owner.tokenValidations + 2;
        Check(envelope.ReadServices(output) == ClockResult::Stopped && output.Same(sentinel) && c.owner.ledger.IsRetained(),
              "close at the final native bookend prevents stale publication without dropping retention");
        c.owner.closeEnvelope = nullptr;
        c.owner.Stop();
        c.owner.drainResult = ClockResult::OwnerConflict;
        Check(envelope.CheckProducerDrain() == ClockResult::OwnerConflict && envelope.ReleaseOnProducer() == ClockResult::OwnerConflict &&
                  c.owner.ledger.IsRetained() && !c.owner.releases,
              "stale-owner drain refusal preserves the issued token and skips release");
        c.owner.drainResult = ClockResult::Ok;
        Check(envelope.ReleaseOnProducer() == ClockResult::Ok && !c.owner.ledger.IsRetained(),
              "only the unchanged genuine issuer's recorded producer finishes retirement");
        Check(envelope.Acquire(c.owner.bundle) == ClockResult::Stopped, "a drained envelope never reopens publication");
        std::puts("Prepared game borrow publication and output preservation fixture completed");
    }
    void ConstructorFailure()
    {
        Context               c;
        CGamePreparedBorrowDE envelope(c.owner);
        Check(envelope.Acquire(c.owner.bundle) == ClockResult::Ok, "enclosing envelope acquires before any later partial product construction");
        NativeGameConstructorServices selected;
        Check(envelope.ReadServices(selected) == ClockResult::Ok, "constructor input is freshly admitted before the separate product attempt");
        c.owner.validationResult = ClockResult::OwnerConflict;
        bool refused = false;
        try
        {
            prepared_game_fixture::Game graph(c.reader, c.clock, c.owner, c.streaming, c.state, selected);
            (void)graph;
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused && c.owner.ledger.IsRetained() && !c.owner.releases,
              "actual prepared CGame constructor refusal leaves the external envelope and real CPU token intact");
        c.owner.validationResult = ClockResult::Ok;
        envelope.ClosePublication();
        c.owner.Stop();
        Check(envelope.CheckProducerDrain() == ClockResult::Ok, "failed construction leaves a reachable token for the same producer drain");
        Check(envelope.ReleaseOnProducer() == ClockResult::Ok && !c.owner.ledger.IsRetained(), "constructor failure causes no implicit or foreign release");
        std::puts("All prepared graph SDK obligations and thirteen lifecycle hooks remain abstract");
    }
    bool StartupTrace(const Owner& owner, std::initializer_list<Owner::StartupEvent> expected)
    {
        if (owner.startupEventCount != expected.size())
            return false;
        unsigned index = 0;
        for (const auto event : expected)
            if (owner.startupEvents[index++] != event)
                return false;
        return true;
    }
    void* ReadStartupService(CGame& game, Owner::StartupEvent service)
    {
        // Invoke through the real common SDK. The strict generated lifecycle
        // receiver inherits both production getters instead of replacing them.
        return service == Owner::StartupEvent::Garages ? static_cast<void*>(game.GetGarages()) : static_cast<void*>(game.GetStreaming());
    }
    bool StartupRefuses(CGame& game, Owner::StartupEvent service, ClockResult expected, void*& published)
    {
        try
        {
            published = ReadStartupService(game, service);
        }
        catch (const std::runtime_error& error)
        {
            const std::string status = "(status " + std::to_string(static_cast<std::uint32_t>(expected)) + ")";
            return std::string(error.what()).find(status) != std::string::npos;
        }
        return false;
    }
    void StartupServiceBinding()
    {
        using Event = Owner::StartupEvent;
        using Mutation = Owner::LookupMutation;
        for (const auto service : {Event::Garages, Event::Streaming})
        {
            Context     c;
            auto        graph = std::make_unique<prepared_game_fixture::Game>(c.reader, c.clock, c.owner, c.streaming, c.state, c.owner.bundle);
            CGame&      common = *graph;
            auto&       lookup = service == Event::Garages ? c.owner.garagesLookup : c.owner.streamingLookup;
            auto&       unrelated = service == Event::Garages ? c.owner.streamingLookup : c.owner.garagesLookup;
            void* const expected = service == Event::Garages ? static_cast<void*>(static_cast<CGarages*>(&c.owner.garagesSdk))
                                                             : static_cast<void*>(static_cast<CStreaming*>(&c.owner.streamingSdk));
            void* const sentinel = &c.tags[127];
            void*       published = sentinel;
            const auto  original = c.owner.bundle;

            c.owner.BeginStartupTrace();
            auto validations = c.owner.bundleValidations;
            Check(ReadStartupService(common, service) == expected && lookup.calls == 1 && !unrelated.calls,
                  "prepared startup getter returns its real same-owner SDK receiver after one late lookup");
            Check(StartupTrace(c.owner, {Event::Validate, service, Event::Validate}),
                  "prepared startup getter bookends the late lookup with current bundle validation");
            Check(c.owner.bundleValidations == validations + 2, "healthy prepared getter performs exactly two bundle checks");

            ++c.owner.bundle.generation;
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(common, service, ClockResult::OwnerConflict, published) && published == sentinel,
                  "stale prepared generation refuses without publishing a receiver");
            Check(!lookup.calls && !unrelated.calls && StartupTrace(c.owner, {Event::Validate}),
                  "stale generation refuses before either startup provider runs");
            c.owner.bundle = original;

            c.owner.bundle.stats = reinterpret_cast<CStats*>(&c.owner.foreignServiceTag);
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(common, service, ClockResult::OwnerConflict, published) && published == sentinel,
                  "foreign captured bundle member refuses without publishing a receiver");
            Check(!lookup.calls && !unrelated.calls && StartupTrace(c.owner, {Event::Validate}),
                  "changed captured service identity refuses before the startup provider");
            c.owner.bundle = original;

            c.owner.validationResult = ClockResult::BindingChanged;
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(common, service, ClockResult::BindingChanged, published) && published == sentinel,
                  "explicit current owner refusal preserves the exact validation status and caller publication");
            Check(!lookup.calls && StartupTrace(c.owner, {Event::Validate}), "owner validation refusal skips provider selection entirely");
            c.owner.validationResult = ClockResult::Ok;

            for (const auto mutation : {Mutation::Generation, Mutation::Bundle})
            {
                lookup.mutation = mutation;
                c.owner.BeginStartupTrace();
                Check(StartupRefuses(common, service, ClockResult::OwnerConflict, published) && published == sentinel,
                      "bundle drift after assigning a successful provider result withholds the selected pointer");
                Check(lookup.calls == 1 && !unrelated.calls && StartupTrace(c.owner, {Event::Validate, service, Event::Validate}),
                      "post-selection generation and member drift are caught by the final current bundle check");
                c.owner.bundle = original;
            }
            lookup.mutation = Mutation::None;

            lookup.absent = true;
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(common, service, ClockResult::NotInitialized, published) && published == sentinel,
                  "successful absent startup receiver becomes NotInitialized without successful-null publication");
            Check(lookup.calls == 1 && StartupTrace(c.owner, {Event::Validate, service}),
                  "null provider refusal does not run the successful-publication bookend");
            lookup.absent = false;

            lookup.result = ClockResult::WrongThread;
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(common, service, ClockResult::WrongThread, published) && published == sentinel,
                  "startup provider failure preserves its exact status and caller output");
            Check(lookup.calls == 1 && StartupTrace(c.owner, {Event::Validate, service}), "provider failure cannot be hidden by an additional validator call");
            lookup.result = ClockResult::Ok;

            c.owner.BeginStartupTrace();
            Check(ReadStartupService(common, service) == expected && lookup.calls == 1,
                  "fresh successful lookup remains possible after no-publication refusals");
            Check(StartupTrace(c.owner, {Event::Validate, service, Event::Validate}), "retry retains both real prepared getter bookends");

            auto   unprepared = std::make_unique<prepared_game_fixture::Game>(c.reader, c.clock, c.owner, c.streaming, c.state);
            CGame& legacy = *unprepared;
            // Deliberately reject constructor-bundle validation. The original
            // unprepared route has no such bundle, and must keep its existing
            // native provider lookup rather than gain a new readiness gate.
            c.owner.validationResult = ClockResult::BindingChanged;
            validations = c.owner.bundleValidations;
            c.owner.BeginStartupTrace();
            Check(ReadStartupService(legacy, service) == expected && lookup.calls == 1,
                  "unprepared SDK route still returns its freshly selected real receiver");
            Check(c.owner.bundleValidations == validations && StartupTrace(c.owner, {service}), "unprepared startup getter adds no captured-bundle validation");

            lookup.result = ClockResult::WrongThread;
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(legacy, service, ClockResult::WrongThread, published) && published == sentinel,
                  "unprepared route still observes later provider failures instead of caching its receiver");
            Check(c.owner.bundleValidations == validations && lookup.calls == 1 && StartupTrace(c.owner, {service}),
                  "unprepared failure keeps exactly the existing late provider call");
            lookup.result = ClockResult::Ok;

            lookup.absent = true;
            c.owner.BeginStartupTrace();
            Check(StartupRefuses(legacy, service, ClockResult::NotInitialized, published) && published == sentinel,
                  "unprepared successful-null provider result retains its NotInitialized behavior");
            Check(c.owner.bundleValidations == validations && StartupTrace(c.owner, {service}), "unprepared null refusal does not gain a bundle check");
            lookup.absent = false;

            lookup.mutation = Mutation::Generation;
            c.owner.BeginStartupTrace();
            Check(ReadStartupService(legacy, service) == expected && lookup.calls == 1,
                  "unprepared publication remains independent of an uncaptured constructor generation");
            Check(c.owner.bundleValidations == validations && StartupTrace(c.owner, {service}),
                  "unprepared selection retains no new validator or cached binding");
        }
        std::puts("Production prepared startup service binding SDK fixture completed");
    }
    Owner*            terminationOwner = nullptr;
    [[noreturn]] void ExpectedDestructorError() noexcept
    {
        if (terminationOwner && terminationOwner->ledger.IsRetained() && !terminationOwner->releases)
        {
            std::puts("Live prepared token destructor refused without implicit release");
            std::fflush(stdout);
            std::_Exit(86);
        }
        std::_Exit(87);
    }
    int LiveDestructor()
    {
        Context c;
        auto*   envelope = new CGamePreparedBorrowDE(c.owner);
        if (envelope->Acquire(c.owner.bundle) != ClockResult::Ok)
            return 87;
        terminationOwner = &c.owner;
        std::set_terminate(&ExpectedDestructorError);
        delete envelope;
        return 88;
    }
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--live-token-destructor") == 0)
        return LiveDestructor();
    try
    {
        Lifetime();
        Refusals();
        ConstructorFailure();
        StartupServiceBinding();
        std::printf("Passed %u prepared game borrow SDK checks (%u-bit; CPU provider doubles only)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Prepared game borrow SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
