/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute Core's actual value-only render lifecycle without the game
 *****************************************************************************/

#include "../../Client/core/DefinitiveRenderReference.h"
#include "../../Client/sdk/game/CGameRenderObservation.h"
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace DefinitiveRenderReference;
    unsigned int checks = 0;
    void         Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Clock
    {
        std::uint64_t now = 1000;
        std::uint32_t regularWait = 50;
        std::uint64_t Now() const noexcept { return now; }
        void          Wait(std::uint32_t milliseconds) noexcept { now += milliseconds == 50 ? regularWait : milliseconds; }
    };
    struct Observer
    {
        Clock&        clock;
        unsigned int  begins = 0, reads = 0, ends = 0, busyEnds = 0;
        bool          began = false, ended = false, rejectBegin = false, rejectRead = false, rejectFinalRead = false;
        bool          nativeFailure = false, throwRead = false, throwFirstEnd = false;
        std::uint32_t lateEnd = 0;
        std::uint32_t route = 2;
        bool          noCallbacks = false;
        bool          Begin()
        {
            ++begins;
            began = !rejectBegin;
            return began;
        }
        template <typename SnapshotType>
        bool Read(SnapshotType& snapshot)
        {
            ++reads;
            if (!began || rejectRead || (ended && rejectFinalRead))
                return false;
            if (throwRead && !ended)
                throw std::runtime_error("controlled read exception");
            snapshot[Bound] = snapshot[ModulePinned] = snapshot[BackendPinnedExpected] = 1;
            snapshot[Active] = ended ? 0u : 1u;
            snapshot[NativeCleanupPending] = ends && !ended ? 1u : 0u;
            snapshot[GenerationLow] = 7;
            snapshot[GenerationHigh] = 2;
            snapshot[PresentCallbacksLow] = 123;
            snapshot[AcceptedSnapshotsLow] = 120;
            snapshot[Width] = 1280;
            snapshot[Height] = 720;
            snapshot[NativePresence] = 15;
            snapshot[NativeOriginalReturned] = 1;
            snapshot[NativeOriginalResult] = 0;  // A returned false bool is not an observer failure.
            snapshot[FailureReason] = nativeFailure ? 4u : 0u;
            if constexpr (std::tuple_size<SnapshotType>::value >= FieldCountV2)
            {
                snapshot[RequestedRoute] = route;
                snapshot[ObservedRoute] = noCallbacks ? 0u : route;
                snapshot[PresenceKnownMask] = noCallbacks ? 0u : route == 2 ? 19u : 15u;
                snapshot[QueuePresent] = !noCallbacks && route == 2 ? 1u : 0u;
                snapshot[NativePresence] = noCallbacks ? 0u : route == 2 ? 3u : 15u;
                if (noCallbacks)
                {
                    snapshot[PresentCallbacksLow] = snapshot[AcceptedSnapshotsLow] = 0;
                    snapshot[Width] = snapshot[Height] = snapshot[NativeOriginalReturned] = 0;
                }
            }
            return true;
        }
        bool End()
        {
            ++ends;
            if (throwFirstEnd && ends == 1)
                throw std::runtime_error("controlled End exception");
            if (ends <= busyEnds)
                return false;
            clock.now += lateEnd;
            ended = true;
            return true;
        }
    };
    struct Request
    {
        std::uint32_t                                            durationMs = MaximumDurationMs;
        std::uint32_t                                            renderRoute = 2;
        std::function<bool()>                                    cancelled;
        std::function<void(const std::uint32_t*, std::uint32_t)> publish;
    };
    template <typename SnapshotType>
    struct FixtureT
    {
        Clock                     clock;
        Observer                  observer{clock};
        Request                   request;
        std::vector<SnapshotType> publications;
        bool                      cancel = false, throwPublish = false;
        FixtureT()
        {
            request.cancelled = [&] { return cancel; };
            request.publish = [&](const std::uint32_t* state, std::uint32_t count)
            {
                Check(count == std::tuple_size<SnapshotType>::value, "Core must publish the exact selected protocol length");
                if (throwPublish && state[Phase] == 1)
                    throw std::runtime_error("controlled publication failure");
                SnapshotType copy{};
                std::copy_n(state, count, copy.begin());
                if (!publications.empty())
                    Check(copy[Sequence] > publications.back()[Sequence], "every publication must advance sequence");
                if (copy[Phase] == 3)
                    Check(!observer.ended && copy[CleanupPending] == 1 && copy[CleanupComplete] == 0, "pending snapshots cannot imply a released native owner");
                publications.push_back(copy);
            };
        }
        SnapshotType Run() { return DefinitiveRenderReference::Run<SnapshotType>(observer, clock, request); }
    };
    using Fixture = FixtureT<Snapshot>;
    using FixtureV2 = FixtureT<SnapshotV2>;
    using FixtureV3 = FixtureT<SnapshotV3>;
    template <typename SnapshotType>
    void Terminal(const FixtureT<SnapshotType>& fixture, const SnapshotType& state, StopReason reason, bool clean = true)
    {
        Check(fixture.observer.ended && fixture.observer.ends > 0, "every exit must finish actual observer End");
        Check(state[Version] == (std::tuple_size<SnapshotType>::value == FieldCountV3   ? 3u
                                 : std::tuple_size<SnapshotType>::value == FieldCountV2 ? 2u
                                                                                        : 1u) &&
                  state[Phase] == 2 && state[CleanupPending] == 0 && state[CleanupComplete] == (clean ? 1u : 0u),
              "terminal publication must accurately describe cleanup");
        Check(state[Reason] == static_cast<std::uint32_t>(reason), "stop reason must survive the cleanup path");
        Check(state[Reserved] == 0 && state[ReservedHigh] == 0, "reserved fields must remain neutral");
    }
}

int main()
{
    try
    {
        {
            Fixture    f;
            const auto state = f.Run();
            Terminal(f, state, StopReason::TimeBudget);
            Check(state[SampleCount] == 200 && state[ElapsedMs] == 10000 && f.publications.size() == 201,
                  "ten-second budget must allow exactly two hundred fifty-millisecond observations");
            Check(state[Bound] == 1 && state[Active] == 0 && state[ModulePinned] == 1 && state[GenerationHigh] == 2 && state[PresentCallbacksLow] == 123 &&
                      state[AcceptedSnapshotsLow] == 120 && state[NativeOriginalResult] == 0,
                  "final state must preserve actual ended counters and a valid false native result");
        }
        {
            Fixture f;
            f.cancel = true;
            const auto state = f.Run();
            Terminal(f, state, StopReason::Cancelled);
            Check(f.observer.begins == 0 && state[ObserverStarted] == 0 && state[SampleCount] == 0 && state[Bound] == 0,
                  "known cancellation must not install an observer or invent native state");
        }
        {
            Fixture f;
            f.request.cancelled = [&] { return f.clock.now >= 1150; };
            const auto state = f.Run();
            Terminal(f, state, StopReason::Cancelled);
            Check(state[SampleCount] == 3 && state[ElapsedMs] == 150, "cancellation must stop before another native read/publication");
        }
        {
            Fixture f;
            f.observer.rejectBegin = true;
            Terminal(f, f.Run(), StopReason::BeginError);
            Check(f.observer.ends == 1, "failed Begin must still attempt cleanup of a partial lease");
        }
        {
            Fixture f;
            f.observer.rejectRead = true;
            Terminal(f, f.Run(), StopReason::CleanupError, false);
        }
        {
            Fixture f;
            f.observer.rejectFinalRead = true;
            Terminal(f, f.Run(), StopReason::CleanupError, false);
        }
        {
            Fixture f;
            f.observer.nativeFailure = true;
            Terminal(f, f.Run(), StopReason::ReadError);
        }
        {
            Fixture f;
            f.observer.throwRead = true;
            Terminal(f, f.Run(), StopReason::Exception);
        }
        {
            Fixture f;
            f.throwPublish = true;
            Terminal(f, f.Run(), StopReason::Exception);
        }
        {
            Fixture f;
            f.clock.regularWait = 1001;
            Terminal(f, f.Run(), StopReason::PulseGap);
        }
        {
            Fixture f;
            f.clock.regularWait = 1;
            Terminal(f, f.Run(), StopReason::SampleBudget);
        }
        {
            Fixture f;
            f.observer.busyEnds = 3;
            const auto state = f.Run();
            Terminal(f, state, StopReason::TimeBudget);
            Check(state[CleanupAttempts] == 4 && state[CleanupDeadlineExceeded] == 0 && f.publications.size() == 204,
                  "brief cleanup retries must retain the owner and final actual readback");
        }
        {
            Fixture f;
            f.observer.busyEnds = 42;
            const auto state = f.Run();
            Terminal(f, state, StopReason::CleanupError);
            Check(state[CleanupDeadlineExceeded] == 1 && state[CleanupAttempts] == 43 && state[Bound] == 1 && state[Active] == 0,
                  "deadline must stay failed even after safe eventual End and actual final state");
        }
        {
            Fixture f;
            f.observer.lateEnd = 2001;
            const auto state = f.Run();
            Terminal(f, state, StopReason::CleanupError);
            Check(state[CleanupDeadlineExceeded] == 1, "late successful End must not erase the cleanup deadline");
        }
        {
            Fixture f;
            f.observer.throwFirstEnd = true;
            Terminal(f, f.Run(), StopReason::Exception);
            Check(f.observer.ends == 2, "unexpected End exception must retain the owner for another cleanup attempt");
        }
        for (const auto route : {1u, 2u})
        {
            FixtureV2 f;
            f.request.renderRoute = f.observer.route = route;
            const auto state = f.Run();
            Terminal(f, state, StopReason::TimeBudget);
            Check(state[RequestedRoute] == route && state[ObservedRoute] == route && state[PresenceKnownMask] == (route == 2 ? 19u : 15u) &&
                      state[QueuePresent] == (route == 2 ? 1u : 0u) && state[NativePresence] == (route == 2 ? 3u : 15u),
                  "explicit protocol must preserve the actual selected route and known presence fields");
            Check(state[SampleCount] == 200 && f.publications.size() == 201 && state[NativeOriginalResult] == 0,
                  "API2 must retain the bounded lifecycle and distinguish a returned false native bool");
        }
        {
            FixtureV2 f;
            f.observer.noCallbacks = true;
            const auto state = f.Run();
            Terminal(f, state, StopReason::TimeBudget);
            Check(state[RequestedRoute] == 2 && !state[ObservedRoute] && !state[PresenceKnownMask] && !state[QueuePresent] && !state[AcceptedSnapshotsLow],
                  "requesting D3D12 must not invent a callback, queue, or known presence before publication");
        }
        for (const auto invalid : {0u, 3u, 0xffffffffu})
        {
            FixtureV2 f;
            f.request.renderRoute = invalid;
            const auto state = f.Run();
            Terminal(f, state, StopReason::BeginError);
            Check(!f.observer.begins && !state[ObserverStarted], "unknown API2 route must reject before installing an observer");
        }
        for (unsigned scenario = 0; scenario < 6; ++scenario)
        {
            FixtureV2 f;
            if (scenario == 0)
                f.request.cancelled = [&] { return f.clock.now >= 1150; };
            else if (scenario == 1)
                f.observer.rejectBegin = true;
            else if (scenario == 2)
                f.observer.throwRead = true;
            else if (scenario == 3)
                f.observer.busyEnds = 42;
            else if (scenario == 4)
                f.observer.lateEnd = 2001;
            else
                f.observer.rejectFinalRead = true;
            const auto reason = scenario == 0   ? StopReason::Cancelled
                                : scenario == 1 ? StopReason::BeginError
                                : scenario == 2 ? StopReason::Exception
                                                : StopReason::CleanupError;
            Terminal(f, f.Run(), reason, scenario != 5);
        }
        {
            // Use the actual SDK POD and production copy function. Distinct high
            // halves and unknown device/context fields catch layout or route
            // assumptions which a lifecycle-only fake observer would miss.
            mta::game_de::RenderObservationStateV2 state;
            state.base.bound = state.base.modulePinned = 1;
            state.base.generation = 0x1234567887654321ull;
            state.base.nativePresentCallbacks = 0x1122334455667788ull;
            state.base.foreignThreadCallbacks = 0x9988776655443322ull;
            state.base.publicationMisses = 0xffeeddccbbaa0099ull;
            state.base.windowPresent = state.base.swapchainPresent = 1;
            state.base.lastOriginalReturned = 1;
            state.requestedRoute = state.observedRoute = 2;
            state.presenceKnownMask = 19;
            state.queuePresent = 1;
            SnapshotV2 copied{};
            copied[Version] = 2;
            CopyNativeStateV2(copied, state);
            Check(copied[GenerationLow] == 0x87654321u && copied[GenerationHigh] == 0x12345678u && copied[PresentCallbacksHigh] == 0x11223344u &&
                      copied[ForeignCallbacksHigh] == 0x99887766u && copied[PublicationMissesHigh] == 0xffeeddccu,
                  "production scalar copy must preserve all observed uint64 halves");
            Check(copied[NativePresence] == 3 && copied[PresenceKnownMask] == 19 && copied[QueuePresent] == 1 && copied[ObservedRoute] == 2 &&
                      !state.base.devicePresent && !state.base.contextPresent && copied[NativeOriginalReturned] == 1 && !copied[NativeOriginalResult],
                  "D3D12 copy must carry queue presence without advertising D3D11 device/context knowledge");
            Snapshot legacy{};
            CopyNativeState(legacy, state.base);
            Check(legacy[NativePresence] == 3 && legacy[GenerationHigh] == copied[GenerationHigh] && legacy[Reserved] == 0,
                  "API1 base copy must never append API2 queue presence");
        }
        {
            FixtureV3 f;
            Terminal(f, f.Run(), StopReason::TimeBudget);
            Check(f.publications.size() == 201 && f.publications.back()[RequestedRoute] == 2 && f.publications.back()[NativePresence] == 3,
                  "API3 must retain the actual route/lifecycle without changing historical presence bits");
        }
        for (const auto invalid : {0u, 1u, 3u, 0xffffffffu})
        {
            FixtureV3 f;
            f.request.renderRoute = invalid;
            Terminal(f, f.Run(), StopReason::BeginError);
            Check(!f.observer.begins, "GPU ownership probes require explicit D3D12 before Begin");
        }
        {
            FixtureV3 f;
            f.observer.busyEnds = 42;
            const auto state = f.Run();
            Terminal(f, state, StopReason::CleanupError);
            Check(state[CleanupDeadlineExceeded] && f.observer.ends == 43, "API3 cannot unload its owner after a cleanup deadline");
        }
        {
            // Copy intentionally distinct scalars from the real SDK POD. This
            // catches a shifted wire tail, lost high half or V2 reinterpretation.
            mta::game_de::RenderObservationStateV3 state;
            state.base.requestedRoute = state.base.observedRoute = 2;
            state.base.base.windowPresent = state.base.base.swapchainPresent = 1;
            state.base.presenceKnownMask = 19;
            state.base.queuePresent = 1;
            state.gpu.attempts = 0x1234567887654321ull;
            state.gpu.accepted = 0x1122334455667788ull;
            state.gpu.rejected = 0x9988776655443322ull;
            state.gpu.queryStage = 19;
            state.gpu.hresult = 0x80004002;
            state.gpu.metadataValid = 1;
            state.gpu.supportPredicate = 0;
            state.gpu.queuePriority = 0xffffffffu;
            state.gpu.adapterLuidHigh = 0xabcdef01u;
            state.gpu.backbufferWidth = 0x9876543212345678ull;
            state.gpu.backbufferHeight = 720;
            state.gpu.backbufferFormat = 87;
            state.gpu.backbufferSampleCount = 1;
            state.gpu.queryStatus = 3;
            state.gpu.nativeExplicitGPUCount = 1;
            state.gpu.nativeAFRCount = 2;
            SnapshotV3 copied{};
            CopyNativeStateV3(copied, state);
            Check(copied[ProbeAttemptsLow] == 0x87654321u && copied[ProbeAttemptsHigh] == 0x12345678u && copied[ProbeAcceptedHigh] == 0x11223344u &&
                      copied[ProbeRejectedHigh] == 0x99887766u,
                  "API3 copy must retain all published probe counter halves");
            Check(copied[QueryStage] == 19 && copied[QueryHresult] == 0x80004002u && copied[QueuePriority] == 0xffffffffu &&
                      copied[AdapterLuidHigh] == 0xabcdef01u,
                  "Raw HRESULT, signed priority bits and adapter LUID must remain lossless");
            Check(copied[BackbufferWidthLow] == 0x12345678u && copied[BackbufferWidthHigh] == 0x98765432u && copied[BackbufferHeight] == 720 &&
                      copied[BackbufferFormat] == 87 && copied[NativeAFRCount] == 2,
                  "API3 metadata tail must preserve exact SDK field ordering");
            Check(copied[NativePresence] == 3 && copied[PresenceKnownMask] == 19 && copied[QueuePresent] == 1 && !copied[ReservedV3] && !copied[ReservedHighV3],
                  "API3 must not merge queue into historical presence bits or populate reserved fields");
        }
        std::printf("{\"success\":true,\"checks\":%u,\"game_operations\":false}\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
