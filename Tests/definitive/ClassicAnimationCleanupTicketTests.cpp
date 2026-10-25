/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute the actual classic journal without native engine entry
 ****************************************************************************/
#include <windows.h>
// Supply the fixed-width prerequisite normally provided by the classic PCH
// before the actual SA sequence declarations are parsed standalone.
#include <cstdint>
#include <SharedUtil.Defines.h>
#include "Client/game_sa/CCustomAnimationCleanupSA.h"
#include "Client/game_sa/CCustomAnimationSequenceOperationSA.h"
#include "ClassicCleanupFixture.generated.h"
#include "ClassicAnimationOperationEntries.generated.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <thread>

namespace AllocationFault
{
    bool ticket = false;
}
void* operator new(std::size_t size)
{
    if (AllocationFault::ticket && size == sizeof(MtaGame::CustomAnimationCleanupSA::Ticket))
    {
        AllocationFault::ticket = false;
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size ? size : 1))
        return memory;
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

namespace
{
    using namespace MtaGame::CustomAnimationCleanupSA;
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::printf("Classic cleanup check %u failed\n", checks);
            std::exit(1);
        }
    }
    struct Event
    {
        enum Kind
        {
            Cache,
            Frame,
            Array,
            Outer
        } kind;
        void* pointer;
    };
    void WriteClassicFrames(CAnimBlendSequenceSAInterface* row, std::size_t count, bool root, bool compressed, void* frames)
    {
        // Only the native field-write boundary is substituted. The real SA
        // operation owner creates and retires all ownership/ACK records.
        row->sNumKeyFrames = static_cast<std::uint16_t>(count);
        row->sFlags = static_cast<unsigned short>((row->sFlags & ~7u) | 1u | (root ? 2u : 0u) | (compressed ? 4u : 0u));
        row->pKeyFrames = static_cast<BYTE*>(frames);
    }
    class Manager final : public ClassicCleanupRejectingManager
    {
    public:
        Manager() : owner(std::make_shared<Owner>(*this)), operations(std::make_shared<MtaGame::CustomAnimationSequenceSA::Owner>(*this, &WriteClassicFrames))
        {
            events.reserve(32);
        }
        ~Manager()
        {
            owner->liveOrigin = nullptr;
            // These are only this fixture's unconsumed CPU buffers. Production
            // entered debt is never retried or discharged through this path.
            for (auto* value : frames)
                if (virtualFrames.count(value))
                    VirtualFree(value, 0, MEM_RELEASE);
                else
                    delete[] value;
            for (auto* value : arrays)
                MtaGame::CAnimSequenceStorageSA::Free(value);
            for (auto* value : outers)
                delete value;
        }
        BYTE* Frames()
        {
            auto* value = new BYTE[32];
            frames.push_back(value);
            return value;
        }
        CAnimBlendSequenceSAInterface* Array(std::size_t count)
        {
            auto* value = MtaGame::CAnimSequenceStorageSA::Allocate(count);
            arrays.push_back(value);
            MtaGame::CustomAnimationSequenceSA::RegisterArray(*operations, value, count);
            return value;
        }
        std::shared_ptr<MtaGame::CustomAnimationSequenceSA::Frame> IssuedFrame(void* requested = nullptr)
        {
            auto frame = MtaGame::CustomAnimationSequenceSA::ReserveFrame(*operations);
            frame->entered = true;
            void* memory = VirtualAlloc(requested, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            Check(memory && (!requested || memory == requested));
            frames.push_back(static_cast<BYTE*>(memory));
            virtualFrames.insert(memory);
            frame->address = memory;
            frame->returned = true;
            return frame;
        }
        void Attach(CAnimBlendSequenceSAInterface* array, std::size_t index, const std::shared_ptr<MtaGame::CustomAnimationSequenceSA::Frame>& frame)
        {
            auto  issued = MtaGame::CustomAnimationSequenceSA::FindArray(*operations, array);
            auto* row = MtaGame::CAnimSequenceStorageSA::At(array, static_cast<std::uint32_t>(index));
            MtaGame::CustomAnimationSequenceSA::RegisterBorrowedRow(*operations, issued, row);
            auto ticket = MtaGame::CustomAnimationSequenceSA::Begin(operations, MtaGame::CustomAnimationSequenceSA::Kind::Attachment, row, nullptr, 1, false,
                                                                    true, frame->address);
            Check(ticket->Resume().status == ECustomAnimationSequenceStatus::Complete);
        }
        CAnimBlendHierarchySAInterface* Hierarchy()
        {
            auto* value = new CAnimBlendHierarchySAInterface;
            outers.push_back(value);
            Reserve(*owner, value);
            return value;
        }
        std::unique_ptr<CCustomAnimationCleanup> BeginCustomAnimationCleanup(const SCustomAnimationCleanupRequest& request) override
        {
            // Real CAnimManagerSA copies this tag from its actual SA facade
            // synchronously. This strict transport receiver starts at the
            // same helper boundary; it does not supply a fake whole game.
            return Begin(owner, request, beginHierarchy);
        }
        bool HasPendingCustomAnimationCleanup() const noexcept override { return owner->HasPending() || operations->HasPending(); }
        void RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface* value) override { After({Event::Cache, value}); }
        void FreeKeyFramesMemory(void* pKeyFrames) override;
        void NativeFreeEntry(void* value, DWORD entry)
        {
            Check(entry == FUNC_CAnimManager_FreeKeyFramesMemory);
            const auto found = std::find(frames.begin(), frames.end(), value);
            Check(found != frames.end());
            if (virtualFrames.erase(value))
                Check(VirtualFree(value, 0, MEM_RELEASE) != FALSE);
            else
                delete[] *found;
            frames.erase(found);
            After({Event::Frame, value});
        }
        void FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* value) override
        {
            const auto found = std::find(arrays.begin(), arrays.end(), value);
            Check(found != arrays.end());
            MtaGame::CAnimSequenceStorageSA::Free(value);
            if (const auto issued = MtaGame::CustomAnimationSequenceSA::FindArray(*operations, value))
                issued->released = true;
            arrays.erase(found);
            After({Event::Array, value});
        }
        void DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface* value) override
        {
            const auto found = std::find(outers.begin(), outers.end(), value);
            Check(found != outers.end());
            delete *found;
            outers.erase(found);
            After({Event::Outer, value});
        }
        void After(Event event)
        {
            events.push_back(event);
            if (reenter)
            {
                auto* value = reenter;
                reenter = nullptr;
                Check(value->Resume().status == ECustomAnimationCleanupStatus::Pending);
            }
            if (failAfter == events.size())
                throw std::runtime_error("Fixture operation returned no acknowledgement after its effect");
        }
        std::shared_ptr<Owner>                                      owner;
        std::shared_ptr<MtaGame::CustomAnimationSequenceSA::Owner>  operations;
        std::shared_ptr<MtaGame::CustomAnimationSequenceSA::Owner>& customSequenceOwner = operations;
        CAnimBlendHierarchySAInterface*                             beginHierarchy = nullptr;
        std::vector<Event>                                          events;
        std::size_t                                                 failAfter = 0;
        CCustomAnimationCleanup*                                    reenter = nullptr;

    private:
        std::vector<BYTE*>                           frames;
        std::set<void*>                              virtualFrames;
        std::vector<CAnimBlendSequenceSAInterface*>  arrays;
        std::vector<CAnimBlendHierarchySAInterface*> outers;
    };
#include "ClassicAnimationOperationBridge.generated.inl"
    struct Scenario
    {
        explicit Scenario(Manager& manager) : raw(manager.Hierarchy()), array(manager.Array(5))
        {
            shared = manager.Frames();
            individual = manager.Frames();
            array[0].sFlags = 8;
            array[0].pKeyFrames = shared;
            array[1].sFlags = 8;
            array[1].pKeyFrames = shared + 2;
            array[2].pKeyFrames = individual;
            array[3].pKeyFrames = individual;
            array[4].pKeyFrames = shared;
            manager.beginHierarchy = raw;
            request = {&manager, nullptr, array, 5, true};
        }
        CAnimBlendHierarchySAInterface* raw;
        CAnimBlendSequenceSAInterface*  array;
        BYTE*                           shared;
        BYTE*                           individual;
        SCustomAnimationCleanupRequest  request;
    };
}

int main()
{
    static_assert(sizeof(void*) == 4, "Classic runtime storage proof is x86 only");
    static_assert(std::has_virtual_destructor_v<CCustomAnimationCleanup>);
    {
        Manager  manager;
        Scenario scenario(manager);
        Check(manager.HasPendingCustomAnimationCleanup());
        auto ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        auto duplicate = manager.BeginCustomAnimationCleanup(scenario.request);
        scenario.request = {};
        manager.reenter = ticket.get();
        auto result = ticket->Resume();
        Check(result.status == ECustomAnimationCleanupStatus::Complete);
        Check(result.disposition == ECustomAnimationCleanupDisposition::ClassicExternalChildren);
        Check(manager.events.size() == 5);
        Check(manager.events[0].kind == Event::Cache);
        Check(manager.events[1].kind == Event::Frame && manager.events[1].pointer == scenario.individual);
        Check(manager.events[2].kind == Event::Array);
        Check(manager.events[3].kind == Event::Frame && manager.events[3].pointer == scenario.shared);
        Check(manager.events[4].kind == Event::Outer);
        Check(duplicate->Resume().status == ECustomAnimationCleanupStatus::Complete && manager.events.size() == 5);
        ticket.reset();
        Check(manager.HasPendingCustomAnimationCleanup());
        duplicate.reset();
        Check(!manager.HasPendingCustomAnimationCleanup());
    }
    for (std::size_t fail = 1; fail <= 5; ++fail)
    {
        Manager  manager;
        Scenario scenario(manager);
        manager.failAfter = fail;
        auto ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Refused);
        Check(manager.events.size() == fail);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Refused && manager.events.size() == fail);
        ticket.reset();
        Check(manager.HasPendingCustomAnimationCleanup());
        ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Refused && manager.events.size() == fail);
    }
    {
        Manager  manager;
        Scenario scenario(manager);
        scenario.request.cacheWasAttached = false;
        auto        ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        std::size_t wrong = 4;
        auto*       cookie = reinterpret_cast<unsigned char*>(scenario.array) - MtaGame::CAnimSequenceStorageSA::CookieBytes;
        std::memcpy(cookie, &wrong, sizeof(wrong));
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Refused && manager.events.empty());
        wrong = 5;
        std::memcpy(cookie, &wrong, sizeof(wrong));
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Complete);
        Check(manager.events.size() == 4 && manager.events.front().kind == Event::Frame);
    }
    {
        Manager  manager;
        Scenario scenario(manager);
        AllocationFault::ticket = true;
        bool threw = false;
        try
        {
            manager.BeginCustomAnimationCleanup(scenario.request);
        }
        catch (const std::bad_alloc&)
        {
            threw = true;
        }
        Check(threw && !AllocationFault::ticket && manager.owner->records.size() == 1);
        Check(manager.owner->records[0]->begun && manager.HasPendingCustomAnimationCleanup() && manager.events.empty());
        auto ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Complete);
    }
    {
        Manager  manager, other;
        Scenario scenario(manager);
        auto     foreign = scenario.request;
        foreign.origin = &other;
        bool refused = false;
        try
        {
            manager.BeginCustomAnimationCleanup(foreign);
        }
        catch (const std::invalid_argument&)
        {
            refused = true;
        }
        Check(refused && manager.events.empty());
        auto ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        manager.owner->creatorClosed = true;
        refused = false;
        try
        {
            manager.BeginCustomAnimationCleanup(scenario.request);
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused && manager.owner->activeBegins == 0);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Complete);
    }
    {
        Manager  manager;
        Scenario scenario(manager);
        auto     ticket = manager.BeginCustomAnimationCleanup(scenario.request);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Complete);
        ECustomAnimationCleanupStatus workerResume = ECustomAnimationCleanupStatus::Pending;
        std::thread                   worker(
            [&workerResume, value = std::move(ticket)]() mutable
            {
                // Native-bearing Resume has no wrong-thread admission, even for
                // a completed record. The CPU-only destructor still keeps code.
                workerResume = value->Resume().status;
                value.reset();
            });
        worker.join();
        Check(workerResume == ECustomAnimationCleanupStatus::Complete);
        Check(manager.owner->liveTickets == 0 && manager.owner->codeLifetimeUncertain);
        // Joining this fixture worker is not an owner-issued external code
        // quiescence ACK. There is intentionally no unpin/reset operation.
        Check(manager.HasPendingCustomAnimationCleanup());
        Check(manager.events.size() == 5);
    }
    {
        Manager manager;
        HANDLE  held = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE  release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(held && release);
        std::thread worker(
            [&]
            {
                AcquireSRWLockExclusive(&manager.owner->codeLifetimeLock);
                SetEvent(held);
                // Release only after the creator's actual query returns. A
                // blocking-query regression is caught by the owned run deadline.
                WaitForSingleObject(release, INFINITE);
                ReleaseSRWLockExclusive(&manager.owner->codeLifetimeLock);
            });
        Check(WaitForSingleObject(held, 5000) == WAIT_OBJECT_0);
        const bool retainedWhileBusy = manager.HasPendingCustomAnimationCleanup();
        Check(SetEvent(release) != FALSE);
        worker.join();
        Check(CloseHandle(held) != FALSE && CloseHandle(release) != FALSE);
        Check(retainedWhileBusy && manager.events.empty());
        Check(!manager.owner->codeLifetimeUncertain && manager.owner->liveTickets == 0);
        Check(!manager.HasPendingCustomAnimationCleanup());
    }
    {
        Manager manager;
        auto*   array = manager.Array(1);
        auto*   hierarchy = manager.Hierarchy();
        auto    oldFrame = manager.IssuedFrame();
        manager.Attach(array, 0, oldFrame);
        MtaGame::CustomAnimationSequenceSA::BeforeInitialize(*manager.operations, array);
        MtaGame::CAnimSequenceStorageSA::Initialize(*array);
        MtaGame::CustomAnimationSequenceSA::Initialized(*manager.operations, array);
        auto ticket = Begin(manager.owner, {&manager, nullptr, array, 1, false}, hierarchy);
        auto record = manager.owner->records.back();
        record->frameContext = manager.operations.get();
        record->identifyFrame = &MtaGame::CustomAnimationSequenceSA::IdentifyFrame;
        record->releaseFrame = &MtaGame::CustomAnimationSequenceSA::ReleaseFrame;
        record->extraFrames = MtaGame::CustomAnimationSequenceSA::SnapshotArrayFrames(*manager.operations, array);
        Check(record->extraFrames.size() == 1 && record->extraFrames[0].identity == oldFrame);
        Gather(*record);
        MtaGame::CustomAnimationSequenceSA::CloseArrayClaims(*manager.operations, array);
        void* base = oldFrame->address;
        manager.FreeKeyFramesMemory(base);
        Check(oldFrame->release == Progress::Returned);
        auto reused = manager.IssuedFrame(base);
        Check(reused->address == base && reused != oldFrame);
        const auto before = manager.events.size();
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Complete);
        Check(manager.events.size() == before + 2 && reused->release == Progress::Waiting);
        Check(manager.events[before].kind == Event::Array && manager.events[before + 1].kind == Event::Outer);
        ticket.reset();
        manager.FreeKeyFramesMemory(reused->address);
        Check(reused->release == Progress::Returned && !manager.HasPendingCustomAnimationCleanup());
    }
    {
        Manager manager;
        auto*   first = manager.Array(1);
        auto*   related = manager.Array(1);
        auto*   hierarchy = manager.Hierarchy();
        auto    frame = manager.IssuedFrame();
        manager.Attach(first, 0, frame);
        manager.Attach(related, 0, frame);
        bool roleRefused = false;
        try
        {
            MtaGame::CustomAnimationSequenceSA::Begin(manager.operations, MtaGame::CustomAnimationSequenceSA::Kind::Cleanup, first);
        }
        catch (const std::invalid_argument&)
        {
            roleRefused = true;
        }
        Check(roleRefused);  // Capacity one never changes an ArrayRow purpose.
        auto ticket = Begin(manager.owner, {&manager, nullptr, first, 1, false}, hierarchy);
        auto record = manager.owner->records.back();
        record->frameContext = manager.operations.get();
        record->identifyFrame = &MtaGame::CustomAnimationSequenceSA::IdentifyFrame;
        record->releaseFrame = &MtaGame::CustomAnimationSequenceSA::ReleaseFrame;
        record->extraFrames = MtaGame::CustomAnimationSequenceSA::SnapshotArrayFrames(*manager.operations, first);
        Gather(*record);
        MtaGame::CustomAnimationSequenceSA::CloseArrayClaims(*manager.operations, first);
        const auto before = manager.events.size();
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Pending);
        Check(manager.events.size() == before && frame->release == Progress::Waiting);
        MtaGame::CustomAnimationSequenceSA::CloseArrayClaims(*manager.operations, related);
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Complete);
        ticket.reset();
        manager.FreeCustomAnimSequences(related);
        Check(frame->release == Progress::Returned && !manager.HasPendingCustomAnimationCleanup());
    }
    {
        Manager manager;
        auto    retired = manager.IssuedFrame();
        void*   base = retired->address;
        manager.FreeKeyFramesMemory(base);
        auto  replacement = manager.IssuedFrame(base);
        auto* array = manager.Array(1);
        auto* hierarchy = manager.Hierarchy();
        array->pKeyFrames = static_cast<BYTE*>(replacement->address);
        auto ticket = Begin(manager.owner, {&manager, nullptr, array, 1, false}, hierarchy);
        auto record = manager.owner->records.back();
        record->frameContext = manager.operations.get();
        record->identifyFrame = &MtaGame::CustomAnimationSequenceSA::IdentifyFrame;
        record->releaseFrame = &MtaGame::CustomAnimationSequenceSA::ReleaseFrame;
        Release conflicting;
        conflicting.address = base;
        conflicting.identity = retired;
        record->extraFrames.push_back(conflicting);
        const auto before = manager.events.size();
        Check(ticket->Resume().status == ECustomAnimationCleanupStatus::Refused);
        Check(manager.events.size() == before && !record->gathered && replacement->release == Progress::Waiting);
        ticket.reset();
        // The real owner rejected this inconsistent identity set; fixture-only
        // CPU buffer destruction below is not a fabricated native ACK.
    }
    std::puts("Actual classic operation ownership fixture completed");
    std::printf("Passed %u actual classic cleanup journal checks (x86; native engine code not executed)\n", checks);
    return 0;
}
