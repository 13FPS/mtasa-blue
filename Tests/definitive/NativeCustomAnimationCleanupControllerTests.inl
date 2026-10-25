/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual same-owner cleanup claims and per-original release receipts
 ****************************************************************************/
namespace CustomAnimationCleanupCpuAllocation
{
    std::atomic<bool> failNext{false};
}
#if defined(MTA_TEST_CUSTOM_ANIMATION_CLEANUP)
void* operator new(std::size_t size)
{
    if (CustomAnimationCleanupCpuAllocation::failNext.exchange(false))
        throw std::bad_alloc();
    if (void* pointer = std::malloc(size ? size : 1))
        return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}
#endif
namespace CustomAnimationCleanupControllerFixture
{
    using Children = NativeCustomAnimSequenceController;
    using Parent = NativePrivateAnimationHierarchyController;
    using HToken = NativeAnimationHierarchyReferenceToken;
    using QToken = NativeAnimationSequenceReferenceToken;
    using Request = NativeCustomAnimationCleanupRequest;
    using BeginReceipt = NativeCustomAnimationCleanupBeginReceipt;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    ClockResult forcedOwner = ClockResult::Ok;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (result != ClockResult::Ok)
            return result;
        return forcedOwner;
    }
    struct Prepared
    {
        std::uintptr_t array = 0, frameA = 0, frameB = 0;
        HToken         hierarchy;
    };
    template <class Invoke>
    Prepared Create(ClockImageFixture& fixture, std::uint64_t generation, Children& children, Parent& parent, WorldGroundOwnerFixture::Callback& callback,
                    const Invoke& invoke)
    {
        Prepared value;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(children.Allocate(fixture.image, generation, 3, &Owner, value.array) == ClockResult::Ok &&
                      children.AllocateFrames(fixture.image, generation, 10, &Owner, value.frameA) == ClockResult::Ok &&
                      children.AllocateFrames(fixture.image, generation, 16, &Owner, value.frameB) == ClockResult::Ok,
                  "cleanup fixture owns original array and frame records");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(children.PrepareFreshCompressedSequence(fixture.image, {generation, value.array, 0}, 1, false, value.frameA, &Owner) == ClockResult::Ok &&
                      children.PrepareFreshCompressedSequence(fixture.image, {generation, value.array, 1}, 1, true, value.frameB, &Owner) == ClockResult::Ok &&
                      children.CopySequenceProperties(fixture.image, {generation, value.array, 2}, value.array, &Owner) == ClockResult::Ok,
                  "full capacity includes an accepted duplicate frame alias outside logical count");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(parent.Create(fixture.image, generation, &Owner, value.hierarchy) == ClockResult::Ok &&
                      parent.SetSequenceCount(fixture.image, value.hierarchy, 1, &Owner) == ClockResult::Ok &&
                      parent.SetSequences(fixture.image, value.hierarchy, value.array, &Owner) == ClockResult::Ok,
                  "actual private parent command holds the prepared resources");
        };
        invoke();
        return value;
    }
    void LoseFreeAcknowledgement(Host::State&) noexcept
    {
        forcedOwner = ClockResult::OwnerConflict;
    }
    template <class Invoke>
    void Run(ClockImageFixture& fixture, std::uint64_t generation, Host::State& host, WorldGroundOwnerFixture::Callback& callback, const Invoke& invoke)
    {
        Children children;
        Parent   parent(&children);
        forcedOwner = ClockResult::Ok;
        const auto     a = Create(fixture, generation, children, parent, callback, invoke);
        std::uintptr_t otherArray = 0, loose = 0;
        HToken         otherParent;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(children.Allocate(fixture.image, generation, 1, &Owner, otherArray) == ClockResult::Ok &&
                      children.AllocateFrames(fixture.image, generation, 7, &Owner, loose) == ClockResult::Ok &&
                      parent.Create(fixture.image, generation, &Owner, otherParent) == ClockResult::Ok,
                  "unrelated hierarchy and loose allocation coexist on the actual owner");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(children.CopySequenceProperties(fixture.image, {generation, otherArray, 0}, a.array, &Owner) == ClockResult::Ok &&
                      parent.SetSequenceCount(fixture.image, otherParent, 1, &Owner) == ClockResult::Ok &&
                      parent.SetSequences(fixture.image, otherParent, otherArray, &Owner) == ClockResult::Ok,
                  "another genuine parent holds the copied original frame");
        };
        invoke();
        BeginReceipt first;
        Request      firstRequest{generation, a.hierarchy, a.array, 3};
        const auto   freesBefore = host.freeCalls;
        callback.action = [&](NativeGameTimingAccess&)
        {
            BeginReceipt wrong;
            auto         foreign = firstRequest;
            ++foreign.generation;
            const auto refused = children.BeginCleanup(fixture.image, foreign, parent, &Owner, wrong);
            Check(refused != ClockResult::Ok && refused != ClockResult::Expired && !wrong.accepted && !wrong.view.IsIssued(),
                  "foreign generation cannot reserve actual issued resources or receive a cleanup view");
            auto copied = firstRequest;
            forcedOwner = ClockResult::OwnerConflict;
            const auto beginResult = children.BeginCleanup(fixture.image, copied, parent, &Owner, first);
            copied = {};
            forcedOwner = ClockResult::Ok;
            Check(beginResult == ClockResult::Ok && first.accepted && first.view.IsIssued() && first.view.Token().generation == generation &&
                      first.view.Token().command,
                  "CPU-only Begin captures intent and issues its view before native admission becomes available");
            Check(host.freeCalls == freesBefore, "claim acceptance performs no native free");
        };
        invoke();
        const auto firstCommand = first.view.Token();
        first.view = {};
        Check(children.HasPendingCleanup() && children.IsRetained(),
              "dropping an accepted CPU view before admission cannot discard the same backend cleanup command");
        callback.action = [&](NativeGameTimingAccess&)
        {
            BeginReceipt recovered;
            Check(children.BeginCleanup(fixture.image, firstRequest, parent, &Owner, recovered) == ClockResult::Ok && recovered.accepted &&
                      recovered.view.IsIssued() && recovered.view.Token().generation == firstCommand.generation &&
                      recovered.view.Token().command == firstCommand.command && host.freeCalls == freesBefore,
                  "late matching Begin recovers the accepted journal and its exact handle without duplicate resource claims");
            first.view = std::move(recovered.view);
            first.accepted = recovered.accepted;
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            const auto                               arrayFree = children.FreeStorage(fixture.image, generation, a.array, &Owner);
            const auto                               frameFree = children.FreeFrames(fixture.image, generation, a.frameA, &Owner);
            NativeAnimationSequenceRowCalls::Receipt row;
            const auto                               mutation = children.InitializeSequence(fixture.image, {generation, a.array, 0}, &Owner, &row);
            Check(arrayFree != ClockResult::Ok && arrayFree != ClockResult::Expired && frameFree != ClockResult::Ok && frameFree != ClockResult::Expired &&
                      mutation != ClockResult::Ok && mutation != ClockResult::Expired && !row.entered && host.freeCalls == freesBefore,
                  "actual cleanup claims freeze ordinary frees and participant stores before detach ACK");
        };
        invoke();
        SCustomAnimationCleanupResult firstResult;
        callback.action = [&](NativeGameTimingAccess&)
        {
            forcedOwner = ClockResult::OwnerConflict;
            const auto result = children.ResumeCleanup(fixture.image, first.view.Token(), parent, 1, &Owner, firstResult);
            forcedOwner = ClockResult::Ok;
            Check(result == ClockResult::OwnerConflict && firstResult.status != ECustomAnimationCleanupStatus::Complete && first.view.IsIssued() &&
                      host.freeCalls == freesBefore,
                  "unavailable native admission preserves an accepted CPU view without native entry or false completion");
        };
        invoke();
        for (unsigned step = 0; step != 8; ++step)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(children.ResumeCleanup(fixture.image, first.view.Token(), parent, 1, &Owner, firstResult) == ClockResult::Ok &&
                          firstResult.status == ECustomAnimationCleanupStatus::Pending,
                      "bounded cleanup waits only for the resource held by another real parent");
            };
            invoke();
        }
        Check(!host.Contains(a.frameB) && host.Contains(a.frameA) && host.Contains(loose) && host.Contains(otherArray - 8),
              "fair resource progress preserves unrelated holds while releasing an independent frame");
        BeginReceipt second;
        callback.action = [&](NativeGameTimingAccess&)
        {
            const Request request{generation, otherParent, otherArray, 1};
            Check(children.BeginCleanup(fixture.image, request, parent, &Owner, second) == ClockResult::Ok && second.accepted && second.view.IsIssued() &&
                      second.view.Token().command != first.view.Token().command,
                  "second participant receives a distinct command sharing actual per-resource receipts");
        };
        invoke();
        SCustomAnimationCleanupResult secondResult;
        for (unsigned step = 0;
             step != 12 && (firstResult.status != ECustomAnimationCleanupStatus::Complete || secondResult.status != ECustomAnimationCleanupStatus::Complete);
             ++step)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(children.ResumeCleanup(fixture.image, second.view.Token(), parent, 2, &Owner, secondResult) == ClockResult::Ok &&
                          children.ResumeCleanup(fixture.image, first.view.Token(), parent, 2, &Owner, firstResult) == ClockResult::Ok,
                      "actual matching detach acknowledgements advance both declared cleanup groups");
            };
            invoke();
        }
        Check(firstResult.status == ECustomAnimationCleanupStatus::Complete && secondResult.status == ECustomAnimationCleanupStatus::Complete &&
                  firstResult.disposition == ECustomAnimationCleanupDisposition::PrivateDetached &&
                  secondResult.disposition == ECustomAnimationCleanupDisposition::PrivateDetached && host.freeCalls == freesBefore + 4 &&
                  !parent.Owns(a.hierarchy) && !parent.Owns(otherParent) && host.Contains(loose),
              "shared original frame is freed once, arrays and outers acknowledged, unrelated allocation untouched");
        Check(children.HasPendingCleanup(), "completed live CPU views still retain backend code");
        first.view = {};
        second.view = {};
        Check(children.HasPendingCleanup(), "view destruction alone cannot perform a qualified journal prune");
        callback.action = [&](NativeGameTimingAccess&)
        {
            Children::CleanupProgress progress;
            Check(children.ResumePendingCleanups(fixture.image, generation, parent, 8, &Owner, progress) == ClockResult::Ok && !progress.pending &&
                      !progress.unknown && children.IsRetained() && children.HasPendingCleanup() &&
                      children.FreeFrames(fixture.image, generation, loose, &Owner) == ClockResult::Ok && !children.IsRetained() &&
                      !children.HasPendingCleanup(),
                  "qualified prune removes only completed no-view commands; loose resource retirement stays explicit");
        };
        invoke();
        const auto   recovered = Create(fixture, generation, children, parent, callback, invoke);
        BeginReceipt known;
        callback.action = [&](NativeGameTimingAccess&)
        {
            const Request request{generation, recovered.hierarchy, recovered.array, 3};
            Check(children.BeginCleanup(fixture.image, request, parent, &Owner, known) == ClockResult::Ok && known.accepted,
                  "acknowledgement-loss setup accepts a new actual cleanup command");
        };
        invoke();
        const auto                    beforeLoss = host.freeCalls;
        SCustomAnimationCleanupResult knownResult;
        for (unsigned attempt = 0; attempt != 8 && host.freeCalls == beforeLoss; ++attempt)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                host.afterFree = &LoseFreeAcknowledgement;
                const auto result = children.ResumeCleanup(fixture.image, known.view.Token(), parent, 1, &Owner, knownResult);
                host.afterFree = nullptr;
                forcedOwner = ClockResult::Ok;
                Check(result != ClockResult::Expired && knownResult.status != ECustomAnimationCleanupStatus::Complete && host.freeCalls <= beforeLoss + 1,
                      "bounded progress preserves an actual returned free even when its owner acknowledgement is lost");
            };
            invoke();
        }
        Check(host.freeCalls == beforeLoss + 1, "fault timing reached exactly one genuine native free");
        for (unsigned step = 0; step != 8 && knownResult.status != ECustomAnimationCleanupStatus::Complete; ++step)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(children.ResumeCleanup(fixture.image, known.view.Token(), parent, 2, &Owner, knownResult) == ClockResult::Ok,
                      "known-returned cleanup resumes from saved receipts without re-observing freed memory");
            };
            invoke();
        }
        Check(knownResult.status == ECustomAnimationCleanupStatus::Complete && host.freeCalls == beforeLoss + 3,
              "returned-free acknowledgement loss causes no destructive replay");
        known.view = {};
        callback.action = [&](NativeGameTimingAccess&)
        {
            Children::CleanupProgress progress;
            Check(children.ResumePendingCleanups(fixture.image, generation, parent, 8, &Owner, progress) == ClockResult::Ok && !children.HasPendingCleanup() &&
                      !children.IsRetained() && !parent.IsRetained(),
                  "qualified final view prune retires local groups using real release receipts only");
        };
        invoke();
        std::puts("Production actual private cleanup controller acknowledgements fixture completed");
    }
}
