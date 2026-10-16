/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Deterministic control-lease ownership and freshness checks
 *****************************************************************************/
#define MTA_TEST_FRONTEND_CONTROL_ACCESS
#include "../../Client/definitive/FrontendControlMailbox.h"
#include <cstdio>
#include <limits>

namespace deonline
{
    struct FrontendControlMailboxTestAccess
    {
        static SRWLOCK& Lock(FrontendControlMailbox& mailbox) { return mailbox.lock; }
        static void     ExhaustGeneration(FrontendControlMailbox& mailbox) { mailbox.generation = UINT32_MAX; }
    };
}

namespace
{
    using deonline::FrontendControlMailbox;
    using namespace DefinitiveGuiFrontendReference;
    constexpr std::uint32_t ScriptThread = 11, WorkerThread = 22;
    int                     checks = 0, failures = 0;
    void                    Check(bool passed, const char* message)
    {
        ++checks;
        if (!passed)
        {
            ++failures;
            std::fprintf(stderr, "FAIL: %s\n", message);
        }
    }
    bool Worker(FrontendControlMailbox& mailbox, ControlAction action, std::uint64_t now, ControlSnapshot& snapshot)
    {
        return mailbox.Worker(static_cast<std::uint32_t>(action), WorkerThread, now, snapshot.data(), static_cast<std::uint32_t>(snapshot.size()));
    }
    bool Script(FrontendControlMailbox& mailbox, FrontendControlMailbox::ScriptAction action, std::uint32_t generation, std::uint64_t now)
    {
        return mailbox.Script(action, ScriptThread, generation, now);
    }
    using Action = FrontendControlMailbox::ScriptAction;

    void TestOwnership()
    {
        FrontendControlMailbox mailbox;
        ControlSnapshot        snapshot{};
        snapshot.fill(71);
        const auto before = snapshot;
        Check(!Worker(mailbox, ControlAction::Request, 100, snapshot) && snapshot == before, "unprepared output preserved");
        Check(!mailbox.PrepareAttempt(0) && mailbox.PrepareAttempt(ScriptThread), "one nonzero script owner");
        Check(!mailbox.PrepareAttempt(33), "foreign prepare rejected");
        Check(!mailbox.Worker(1, ScriptThread, 100, snapshot.data(), static_cast<std::uint32_t>(snapshot.size())) && snapshot == before,
              "script cannot act as worker");
        Check(!mailbox.Worker(1, WorkerThread, 100, snapshot.data(), static_cast<std::uint32_t>(snapshot.size()) - 1) && snapshot == before,
              "count failure preserves output");
        Check(!mailbox.Worker(9, WorkerThread, 100, snapshot.data(), static_cast<std::uint32_t>(snapshot.size())) && snapshot == before,
              "unknown worker action rejected");
        Check(Worker(mailbox, ControlAction::Request, 100, snapshot) && snapshot[Generation] == 1 && snapshot[Requested] == 1 && !snapshot[Acknowledged],
              "request creates generation without permission");
        const auto requested = snapshot;
        Check(!Worker(mailbox, ControlAction::Request, 100, snapshot) && snapshot == requested, "request replay preserves caller sample");
        Check(!mailbox.Worker(2, 33, 101, snapshot.data(), static_cast<std::uint32_t>(snapshot.size())) && snapshot == requested, "foreign worker rejected");
        Check(!mailbox.Capture(33, 101, snapshot) && snapshot == requested, "foreign capture preserves output");
        Check(!mailbox.Script(Action::Acknowledge, 33, 1, 101) && !Script(mailbox, Action::Acknowledge, 2, 101), "foreign and stale acknowledgement rejected");
        Check(!Script(mailbox, Action::Restore, 1, 101), "restore before detached rejected");
        Check(Script(mailbox, Action::Acknowledge, 1, 101) && !Script(mailbox, Action::Acknowledge, 1, 101), "acknowledgement cannot replay");
        Check(mailbox.Capture(ScriptThread, 102, snapshot) && snapshot[Acknowledged] && snapshot[Fresh], "ack grants freshness");
        Check(!mailbox.PrepareAttempt(ScriptThread), "new attempt cannot discard active generation");
        Check(Script(mailbox, Action::Revoke, 1, 103) && Script(mailbox, Action::Revoke, 1, 103), "cleanup revoke idempotent");
        Check(!Script(mailbox, Action::Heartbeat, 1, 104) && !Script(mailbox, Action::Restore, 1, 104), "revoked cannot renew or restore before detach");
        Check(Worker(mailbox, ControlAction::InputDetached, 104, snapshot) && snapshot[Detached] && !snapshot[Fresh], "detach revokes input permission");
        Check(Script(mailbox, Action::Restore, 1, 105) && !Script(mailbox, Action::Restore, 1, 105), "restoration single-use");
        Check(mailbox.PrepareAttempt(ScriptThread) && Worker(mailbox, ControlAction::Request, 106, snapshot) && snapshot[Generation] == 2,
              "generation persists across attempts");
        Check(!Script(mailbox, Action::Acknowledge, 1, 106) && !Script(mailbox, Action::Restore, 1, 106), "old generation never affects new request");
        Check(Worker(mailbox, ControlAction::InputDetached, 107, snapshot) && Script(mailbox, Action::Restore, 2, 108),
              "unacknowledged request can retire only after detach");
    }

    void TestFreshness()
    {
        FrontendControlMailbox mailbox;
        ControlSnapshot        snapshot{};
        Check(mailbox.PrepareAttempt(ScriptThread) && Worker(mailbox, ControlAction::Request, 100, snapshot), "freshness request");
        Check(Script(mailbox, Action::Acknowledge, 1, 5100), "acquire deadline inclusive");
        Check(mailbox.Capture(ScriptThread, 6100, snapshot) && snapshot[Fresh], "heartbeat deadline inclusive");
        Check(Worker(mailbox, ControlAction::Poll, 6101, snapshot) && !snapshot[Fresh] && snapshot[Revoked] && !snapshot[Fault],
              "poll expires lease without unsafe-protocol fault");
        Check(!Script(mailbox, Action::Heartbeat, 1, 6101), "capture and poll never extend heartbeat");
        Check(!mailbox.PrepareAttempt(ScriptThread), "expiry retains restoration obligation");
        Check(Worker(mailbox, ControlAction::InputDetached, 6102, snapshot) && Script(mailbox, Action::Restore, 1, 6103), "expired lease restores cleanly");
        Check(mailbox.PrepareAttempt(ScriptThread) && Worker(mailbox, ControlAction::Request, 7000, snapshot), "next acquire");
        Check(mailbox.Capture(ScriptThread, 12001, snapshot) && snapshot[Revoked] && !snapshot[Fault] && !snapshot[Acknowledged],
              "unacknowledged acquire expires");
        Check(!Script(mailbox, Action::Acknowledge, 2, 12001), "late acknowledgement rejected");
        Check(Worker(mailbox, ControlAction::InputDetached, 12002, snapshot) && Script(mailbox, Action::Restore, 2, 12003),
              "expired no-write attempt can retire");
        Check(
            mailbox.PrepareAttempt(ScriptThread) && Worker(mailbox, ControlAction::Request, 13000, snapshot) && Script(mailbox, Action::Acknowledge, 3, 13001),
            "heartbeat setup");
        Check(Script(mailbox, Action::Heartbeat, 3, 14001) && Worker(mailbox, ControlAction::Poll, 15001, snapshot) && snapshot[Fresh],
              "valid heartbeat extends exactly one expiry interval");
    }

    void TestContentionAndClock()
    {
        FrontendControlMailbox mailbox;
        ControlSnapshot        snapshot{};
        Check(mailbox.PrepareAttempt(ScriptThread) && Worker(mailbox, ControlAction::Request, 200, snapshot), "clock setup");
        const auto before = snapshot;
        auto&      lock = deonline::FrontendControlMailboxTestAccess::Lock(mailbox);
        AcquireSRWLockExclusive(&lock);
        Check(!mailbox.Capture(ScriptThread, 201, snapshot) && snapshot == before, "contended capture preserves output without waiting");
        Check(!Worker(mailbox, ControlAction::Poll, 201, snapshot) && snapshot == before, "contended worker preserves output");
        Check(!Script(mailbox, Action::Acknowledge, 1, 201) && !mailbox.PrepareAttempt(ScriptThread), "contended writes and reset refuse");
        ReleaseSRWLockExclusive(&lock);
        Check(Script(mailbox, Action::Acknowledge, 1, 201), "contention did not alter obligation");
        Check(mailbox.Capture(ScriptThread, 300, snapshot) && Worker(mailbox, ControlAction::Poll, 250, snapshot) && !snapshot[Fault],
              "cross-actor sample ordering is not clock regression");
        Check(Worker(mailbox, ControlAction::Poll, 249, snapshot) && snapshot[Fault] && snapshot[Revoked] && !snapshot[Fresh],
              "same-actor clock rollback fails closed");
        Check(Worker(mailbox, ControlAction::InputDetached, 301, snapshot) && Script(mailbox, Action::Restore, 1, 302),
              "fault may record physical restoration");
        Check(mailbox.Capture(ScriptThread, 303, snapshot) && snapshot[Fault] && snapshot[Restored], "unsafe protocol fault remains visible");

        FrontendControlMailbox neutral;
        Check(neutral.PrepareAttempt(ScriptThread) && Worker(neutral, ControlAction::InputDetached, 0, snapshot) && !snapshot[Requested] &&
                  !snapshot[Generation] && snapshot[Restored],
              "early no-request detach owns no engine obligation");
        Check(!Worker(neutral, ControlAction::Request, 1, snapshot), "request cannot follow detachment in same attempt");
        Check(neutral.PrepareAttempt(ScriptThread), "neutral attempt may reset");
        deonline::FrontendControlMailboxTestAccess::ExhaustGeneration(neutral);
        snapshot.fill(17);
        const auto exhausted = snapshot;
        Check(!Worker(neutral, ControlAction::Request, 2, snapshot) && snapshot == exhausted, "generation never wraps or reuses zero");
    }
}

int main()
{
    static_assert(ControlFieldCount == 8 && FieldCount == 160 && DefinitiveGuiSurfaceReference::FieldCount == 96);
    TestOwnership();
    TestFreshness();
    TestContentionAndClock();
    std::printf("Frontend control mailbox: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
