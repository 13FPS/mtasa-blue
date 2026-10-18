/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the shared frontend cursor owner without native hooks
 *****************************************************************************/
#include "../../Client/core/FrontendCursorLeaseOwner.h"
#include "../../Client/core/FrontendCursorLeasePolicy.h"
#include "../../Client/core/DefinitiveGuiStartupReference.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <new>
#include <type_traits>

namespace
{
    namespace Gui = DefinitiveGuiStartupReference;
    unsigned checks = 0;
    struct Failure
    {
        const char* message;
    };
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw Failure{message};
    }
    enum class Event
    {
        Neutral,
        Construct,
        Begin,
        Admit,
        Renew,
        Draw,
        Disarm,
        Detach,
        NativeRestored,
        End,
        Destroy,
        Release,
    };
    struct Context
    {
        std::array<Event, 2048> events{};
        unsigned                eventCount = 0, constructions = 0, destructions = 0, begins = 0, admits = 0, renewals = 0, draws = 0;
        unsigned                disarms = 0, detachAttempts = 0, nativeEndAttempts = 0, ends = 0, releases = 0, publications = 0;
        unsigned                detachFailures = 0, nativeEndFailures = 0, busyEnds = 0;
        std::uint32_t           thread = 31;
        std::uint64_t           now = 1000, cancelAt = 0, passAt = 0, focusLostAt = 0;
        bool                    throwConstruction = false, failBeginBeforePolicy = false, failBeginAfterHook = false;
        bool                    focused = true, eventOverflow = false, unsafeDestruction = false, releaseBeforeRetirement = false;
        bool                    detachBeforeDisarm = false, publicationReleasedDebt = false;
        void                    Record(Event event) noexcept
        {
            if (eventCount < events.size())
                events[eventCount++] = event;
            else
                eventOverflow = true;
        }
        unsigned First(Event event) const noexcept
        {
            for (unsigned index = 0; index < eventCount; ++index)
                if (events[index] == event)
                    return index;
            return eventCount;
        }
    };
    Context* current = nullptr;

    // Only the OS hook is fake. Renewal, expiry, focus revocation and the owner
    // under test are the production policies, so a draw cannot manufacture a
    // fresh lease or skip the same retained resource's End decision.
    struct Cursor
    {
        inline static Cursor*       singleton = nullptr;
        Context&                    context;
        FrontendCursorLease::Policy policy;
        std::uint64_t               generation = 0;
        bool                        hooked = false;
        Cursor() : context(*current)
        {
            ++context.constructions;
            context.Record(Event::Construct);
            if (context.throwConstruction)
                throw std::bad_alloc();
            singleton = this;
        }
        ~Cursor()
        {
            ++context.destructions;
            context.Record(Event::Destroy);
            const auto state = policy.Read();
            if (hooked || !state.ended || state.inFlight)
                context.unsafeDestruction = true;
            if (singleton == this)
                singleton = nullptr;
        }
        static Cursor* GetSingletonPtr() noexcept { return singleton; }
        bool           BeginFrontendLease(unsigned window) noexcept
        {
            ++context.begins;
            context.Record(Event::Begin);
            if (!window || context.failBeginBeforePolicy || !policy.Begin(context.thread, context.now))
                return false;
            generation = policy.Read().generation;
            hooked = true;
            return !context.failBeginAfterHook;
        }
        bool RenewFrontendLease() noexcept
        {
            ++context.renewals;
            context.Record(Event::Renew);
            return hooked && policy.Renew(context.thread, generation, context.now, context.focused);
        }
        void DisarmFrontendLease() noexcept
        {
            ++context.disarms;
            context.Record(Event::Disarm);
            policy.Disarm();
        }
        bool EndFrontendLease() noexcept
        {
            ++context.ends;
            context.Record(Event::End);
            const auto state = policy.Read();
            if (state.ownerThread && state.ownerThread != context.thread)
                return false;
            policy.Disarm();
            if (state.inFlight || context.ends <= context.busyEnds)
                return false;
            hooked = false;
            return !state.ownerThread || policy.Complete(context.thread);
        }
    };
    using CursorOwner = FrontendCursorLease::Owner<Cursor>;
    static_assert(!std::is_copy_constructible_v<CursorOwner> && !std::is_copy_assignable_v<CursorOwner>);
    static_assert(noexcept(std::declval<CursorOwner&>().EndAfterNative(false)));

    bool Admit(CursorOwner& owner, Context& context, bool neutral = true, bool accepted = true)
    {
        if (neutral)
            context.Record(Event::Neutral);
        return owner.AdmitAfterNeutral(neutral, 7u,
                                       [&]() noexcept
                                       {
                                           ++context.admits;
                                           context.Record(Event::Admit);
                                           return accepted;
                                       });
    }
    void Retire(CursorOwner& owner)
    {
        Check(owner.DisarmBeforeDetach([]() noexcept { return true; }), "retirement detach failed");
        Check(owner.EndAfterNative(true), "retirement did not finish");
    }
    void CheckDestroyed(const Context& context)
    {
        Check(!Cursor::singleton && !context.eventOverflow && !context.unsafeDestruction, "cursor outlived safe retirement or trace capacity");
    }

    void TestAdmissionAndIdentity()
    {
        Context context;
        current = &context;
        {
            CursorOwner owner;
            Check(!Admit(owner, context, false) && !owner.Get() && !owner.Attempted() && owner.Ended() && !owner.Fault(),
                  "neutral input proof must precede cursor allocation");
            Check(!context.constructions && !context.begins && !context.admits, "unverified input admitted cursor or GUI");
            Check(!owner.EndAfterNative(false) && owner.EndAfterNative(true) && !context.ends, "unattempted cleanup touched a cursor");
            Check(Admit(owner, context), "neutral frontend did not acquire cursor");
            auto* const resource = owner.Get();
            Check(resource && owner.Attempted() && !owner.Ended() && !owner.Fault(), "successful admission lost live ownership");
            Check(context.First(Event::Neutral) < context.First(Event::Construct) && context.First(Event::Construct) < context.First(Event::Begin) &&
                      context.First(Event::Begin) < context.First(Event::Admit),
                  "actual owner did not begin cursor between neutral proof and GUI admission");
            Check(!Admit(owner, context) && owner.Get() == resource && context.constructions == 1 && context.admits == 1,
                  "repeated admission replaced a retained cursor");
            CursorOwner other;
            Check(!Admit(other, context) && !other.Get() && !other.Attempted() && other.Ended() && other.Fault() && context.constructions == 1,
                  "existing singleton allowed duplicate cursor construction");
            Retire(owner);
            Check(owner.Get() == resource && owner.Ended() && !context.destructions, "End destroyed the retained diagnostic object");
            const auto ends = context.ends;
            Check(owner.EndAfterNative(true) && context.ends == ends, "completed End retried a retired resource");
        }
        Check(context.destructions == 1, "one successful allocation was not released once");
        CheckDestroyed(context);
    }

    void TestCreationAndBeginFailures()
    {
        for (unsigned kind = 0; kind < 3; ++kind)
        {
            Context context;
            current = &context;
            context.throwConstruction = kind == 0;
            context.failBeginBeforePolicy = kind == 1;
            context.failBeginAfterHook = kind == 2;
            context.busyEnds = kind == 2 ? 1u : 0u;
            {
                CursorOwner owner;
                Check(!Admit(owner, context) && owner.Attempted() && owner.Fault() && !context.admits, "failed cursor startup admitted GUI");
                Check(owner.Ended() == (kind == 0) && (owner.Get() != nullptr) == (kind != 0), "creation/partial Begin ownership was conflated");
                auto* const resource = owner.Get();
                Check(!Admit(owner, context) && context.constructions == 1 && owner.Get() == resource, "failed attempt silently constructed a replacement");
                Check(!owner.EndAfterNative(false) && !context.ends && !context.destructions, "Begin failure released before native restoration");
                if (kind == 2)
                {
                    Check(!owner.EndAfterNative(true) && owner.Get() == resource && !owner.Ended() && resource->hooked && !context.destructions,
                          "partial Begin EndBusy dropped its hook owner");
                }
                Check(owner.EndAfterNative(true) && owner.Ended(), "failed startup resource could not retire");
                Check(!context.admits && context.begins == (kind ? 1u : 0u), "startup failure called unexpected GUI/Begin");
            }
            Check(context.destructions == (kind ? 1u : 0u), "failed construction produced an owned destructor");
            CheckDestroyed(context);
        }
    }

    void TestGuiFailureAndCleanupRetries()
    {
        Context context;
        current = &context;
        context.busyEnds = 2;
        {
            CursorOwner owner;
            Check(!Admit(owner, context, true, false) && owner.Get() && !owner.Ended() && context.admits == 1, "GUI refusal discarded a begun cursor");
            auto* const resource = owner.Get();
            const auto  detach = [&]() noexcept
            {
                ++context.detachAttempts;
                context.Record(Event::Detach);
                context.detachBeforeDisarm = context.detachBeforeDisarm || !resource->policy.Read().revoked;
                return context.detachAttempts != 1;
            };
            Check(!owner.DisarmBeforeDetach(detach) && resource->policy.Read().revoked && !owner.Ended() && !context.ends,
                  "failed GUI detach discarded cursor or tried End");
            Check(!owner.EndAfterNative(false) && !context.ends && !context.destructions, "native debt admitted cursor End");
            Check(owner.DisarmBeforeDetach(detach) && !context.detachBeforeDisarm && context.First(Event::Disarm) < context.First(Event::Detach),
                  "GUI detach ran before independent cursor disarm");
            for (unsigned retry = 0; retry < 2; ++retry)
                Check(!owner.EndAfterNative(true) && owner.Get() == resource && !owner.Ended() && !context.destructions && resource->hooked,
                      "EndBusy released the same cursor resource");
            Check(owner.EndAfterNative(true) && owner.Get() == resource && owner.Ended() && !resource->hooked && context.ends == 3,
                  "cursor End retries did not finish on original resource");
            Check(!context.draws && !context.renewals, "GUI refusal rendered or renewed input");
        }
        CheckDestroyed(context);
    }

    void TestRenewalAndIndependentExpiry()
    {
        for (unsigned kind = 0; kind < 5; ++kind)
        {
            Context context;
            current = &context;
            {
                CursorOwner owner;
                if (kind == 0)
                {
                    Check(!owner.RenewBeforeDraw([&]() noexcept { ++context.draws; }) && owner.Fault() && !context.draws,
                          "draw without owned cursor was admitted");
                    continue;
                }
                Check(Admit(owner, context), "renewal fixture admission failed");
                auto* const resource = owner.Get();
                context.now += 100;
                Check(owner.RenewBeforeDraw(
                          [&]() noexcept
                          {
                              ++context.draws;
                              context.Record(Event::Draw);
                          }),
                      "owner renewal refused a fresh lease");
                Check(context.draws == 1 && resource->policy.Read().renewals == 1 && context.First(Event::Renew) < context.First(Event::Draw),
                      "drawing did not follow the owner's renewal");
                if (kind == 1)
                    context.thread = 32;
                else if (kind == 2)
                    context.focused = false;
                else if (kind == 3)
                    context.now += FrontendCursorLease::LifetimeMs;
                else
                    context.now = 999;
                Check(!owner.RenewBeforeDraw([&]() noexcept { ++context.draws; }) && owner.Fault() && context.draws == 1,
                      "foreign, unfocused, expired or regressed renewal admitted a draw");
                const auto state = resource->policy.Read();
                Check(state.renewals == 1, "failed renewal advanced the production lease");
                if (kind != 1)
                    Check(state.revoked && state.reason == static_cast<std::uint32_t>(kind == 2   ? FrontendCursorLease::Reason::FocusLost
                                                                                      : kind == 3 ? FrontendCursorLease::Reason::Expired
                                                                                                  : FrontendCursorLease::Reason::ClockRollback),
                          "production cursor policy lost its independent stop reason");
                context.thread = 31;
                Retire(owner);
            }
            CheckDestroyed(context);
        }
        Context context;
        current = &context;
        {
            CursorOwner owner;
            Check(Admit(owner, context), "callback fixture admission failed");
            auto* const resource = owner.Get();
            resource->policy.EnterCallback();
            Check(resource->policy.Suppress(context.now + 1, true, true) && !resource->policy.Read().renewals, "cursor callback renewed its own lease");
            Check(!owner.EndAfterNative(true) && !owner.Ended() && !context.destructions, "in-flight cursor callback was destroyed");
            resource->policy.LeaveCallback();
            Check(owner.EndAfterNative(true) && owner.Ended() && context.ends == 2, "drained cursor callback did not retire");
        }
        CheckDestroyed(context);
    }

    struct Clock
    {
        Context&      context;
        std::uint64_t Now() const noexcept { return context.now; }
        void          Wait(std::uint32_t milliseconds) noexcept { context.now += milliseconds; }
    };
    bool Cancelled() noexcept
    {
        return current->cancelAt && current->now >= current->cancelAt;
    }
    void Published(const std::uint32_t* values, std::uint32_t count) noexcept
    {
        ++current->publications;
        if (count != Gui::FieldCount || (values[Gui::Phase] == 3 && current->releases))
            current->publicationReleasedDebt = true;
    }
    struct Request
    {
        std::uint32_t durationMs;
        bool (*cancelled)();
        void (*publish)(const std::uint32_t*, std::uint32_t);
    };
    struct GuiOwner
    {
        Context&    context;
        CursorOwner cursor;
        bool        admitted = false, nativeEnded = false;
        bool        Prepare(Gui::Snapshot&) noexcept { return true; }
        bool        Begin(Gui::Snapshot&) noexcept { return true; }
        bool        ReadNative(Gui::Snapshot&) noexcept { return true; }
        bool        TryDevice(Gui::Snapshot& values) noexcept
        {
            values[Gui::LeaseMetadataValid] = 1;
            return true;
        }
        bool         End(Gui::Snapshot&) noexcept { return true; }
        bool         StartGpu(Gui::Snapshot&) noexcept { return true; }
        Gui::Outcome PollGpu(Gui::Snapshot&) noexcept
        {
            if (!admitted)
            {
                admitted = Admit(cursor, context);
                if (!admitted)
                    return Gui::Outcome::Failed;
            }
            if (context.focusLostAt && context.now >= context.focusLostAt)
                context.focused = false;
            if (!cursor.RenewBeforeDraw(
                    [&]() noexcept
                    {
                        ++context.draws;
                        context.Record(Event::Draw);
                    }))
                return Gui::Outcome::Failed;
            return context.passAt && context.now >= context.passAt ? Gui::Outcome::Passed : Gui::Outcome::Pending;
        }
        bool ShutdownGpu(Gui::Snapshot& values) noexcept
        {
            // The same production cursor owner is composed with the actual
            // shared GUI retry loop. These fake detach/native operations make
            // retained debt observable without detouring or unloading modules.
            if (!cursor.DisarmBeforeDetach(
                    [&]() noexcept
                    {
                        ++context.detachAttempts;
                        context.Record(Event::Detach);
                        context.detachBeforeDisarm = context.detachBeforeDisarm || (cursor.Get() && !cursor.Get()->policy.Read().revoked);
                        return context.detachAttempts > context.detachFailures;
                    }))
                return false;
            if (!nativeEnded)
            {
                ++context.nativeEndAttempts;
                if (context.nativeEndAttempts <= context.nativeEndFailures)
                    return false;
                nativeEnded = true;
                context.Record(Event::NativeRestored);
            }
            if (!cursor.EndAfterNative(nativeEnded))
                return false;
            if ((cursor.Get() && (cursor.Get()->hooked || !cursor.Get()->policy.Read().ended)) || !cursor.Ended())
                context.releaseBeforeRetirement = true;
            ++context.releases;
            context.Record(Event::Release);
            values[Gui::GuiCleanupComplete] = 1;
            return true;
        }
    };

    void TestProductionGuiLifetime()
    {
        // The existing shared lifetime's bounded/renewable argument remains
        // authoritative. No alternate display timer is introduced by cursor
        // ownership; cancellation and failure still use its real retry loop.
        for (unsigned kind = 0; kind < 7; ++kind)
        {
            Context context;
            current = &context;
            context.passAt = kind == 0 || kind >= 5 ? 1150 : 0;
            context.cancelAt = kind == 1 ? 1150 : kind == 4 ? 1650 : 0;
            context.focusLostAt = kind == 2 ? 1100 : 0;
            if (kind == 5)
                context.detachFailures = context.nativeEndFailures = context.busyEnds = 2;
            if (kind == 6)
                context.busyEnds = 43;
            {
                Clock      clock{context};
                GuiOwner   owner{context};
                Request    request{200, &Cancelled, &Published};
                const auto result = Gui::Run(owner, clock, request, kind == 4);
                const auto expected = kind == 0 || kind == 5   ? Gui::StopReason::Completed
                                      : kind == 1 || kind == 4 ? Gui::StopReason::Cancelled
                                      : kind == 6              ? Gui::StopReason::CleanupError
                                                               : Gui::StopReason::GpuError;
                Check(result[Gui::Reason] == static_cast<std::uint32_t>(expected), "cursor composition changed existing GUI stop outcome");
                Check(result[Gui::Phase] == 2 && result[Gui::CleanupComplete] && !result[Gui::CleanupPending] && result[Gui::GuiCleanupComplete],
                      "actual GUI lifetime returned before shared cursor cleanup");
                Check(context.constructions == 1 && context.begins == 1 && owner.cursor.Ended() && owner.cursor.Get() && !context.destructions,
                      "GUI lifetime replaced or destroyed retained cursor diagnostic state");
                Check(context.releases == 1 && !context.releaseBeforeRetirement && !context.detachBeforeDisarm && !context.publicationReleasedDebt,
                      "failed detach/native End/cursor End admitted resource release");
                Check(context.First(Event::Disarm) < context.First(Event::Detach) && context.First(Event::NativeRestored) < context.First(Event::End) &&
                          context.First(Event::End) < context.First(Event::Release),
                      "GUI cleanup changed cursor detach/native restore/End order");
                if (kind == 3)
                    Check(result[Gui::ElapsedMs] >= request.durationMs && !result[Gui::WorkloadSucceeded], "bounded GUI lifetime stopped being bounded");
                if (kind == 4)
                    Check(result[Gui::ElapsedMs] >= 650 && context.draws > 5 && !result[Gui::WorkloadSucceeded], "persistent GUI inherited bounded lifetime");
                if (kind == 5)
                    Check(context.detachAttempts == 7 && context.nativeEndAttempts == 3 && context.ends == 3 && !result[Gui::CleanupDeadlineExceeded],
                          "GUI/native/cursor cleanup retries were skipped or conflated");
                if (kind == 6)
                    Check(context.ends == 44 && result[Gui::CleanupDeadlineExceeded] && context.publications > 40,
                          "cleanup deadline released rather than retained busy cursor");
                if (kind == 2)
                    Check(
                        owner.cursor.Fault() && owner.cursor.Get()->policy.Read().reason == static_cast<std::uint32_t>(FrontendCursorLease::Reason::FocusLost),
                        "focus loss did not stop drawing and retain the revoked cursor");
            }
            CheckDestroyed(context);
        }
    }
}

int main()
{
    const char* error = nullptr;
    try
    {
        TestAdmissionAndIdentity();
        TestCreationAndBeginFailures();
        TestGuiFailureAndCleanupRetries();
        TestRenewalAndIndependentExpiry();
        TestProductionGuiLifetime();
    }
    catch (const Failure& failure)
    {
        error = failure.message;
    }
    catch (...)
    {
        error = "unexpected fixture exception";
    }
    std::printf(
        "{\"success\":%s,\"checks\":%u,\"productionCursorOwner\":true,\"productionLeasePolicy\":true,\"productionGuiLifetime\":true,"
        "\"fakeCursorResource\":true,\"gameLaunched\":false,\"nativeHooksInstalled\":false,\"actualModuleUnload\":false,\"fullMtaReady\":false,\"error\":",
        error ? "false" : "true", checks);
    if (error)
        std::printf("\"%s\"", error);
    else
        std::printf("null");
    std::puts("}");
    return error ? 1 : 0;
}
