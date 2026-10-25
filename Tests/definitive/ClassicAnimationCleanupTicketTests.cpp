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
#include "ClassicCleanupFixture.generated.h"
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
    class Manager final : public ClassicCleanupRejectingManager
    {
    public:
        Manager() : owner(std::make_shared<Owner>(*this)) { events.reserve(32); }
        ~Manager()
        {
            owner->liveOrigin = nullptr;
            // These are only this fixture's unconsumed CPU buffers. Production
            // entered debt is never retried or discharged through this path.
            for (auto* value : frames)
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
            return value;
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
        bool HasPendingCustomAnimationCleanup() const noexcept override { return owner->HasPending(); }
        void RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface* value) override { After({Event::Cache, value}); }
        void FreeKeyFramesMemory(void* value) override
        {
            const auto found = std::find(frames.begin(), frames.end(), value);
            Check(found != frames.end());
            delete[] *found;
            frames.erase(found);
            After({Event::Frame, value});
        }
        void FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* value) override
        {
            const auto found = std::find(arrays.begin(), arrays.end(), value);
            Check(found != arrays.end());
            MtaGame::CAnimSequenceStorageSA::Free(value);
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
        std::shared_ptr<Owner>          owner;
        CAnimBlendHierarchySAInterface* beginHierarchy = nullptr;
        std::vector<Event>              events;
        std::size_t                     failAfter = 0;
        CCustomAnimationCleanup*        reenter = nullptr;

    private:
        std::vector<BYTE*>                           frames;
        std::vector<CAnimBlendSequenceSAInterface*>  arrays;
        std::vector<CAnimBlendHierarchySAInterface*> outers;
    };
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
    std::printf("Passed %u actual classic cleanup journal checks (x86; native engine code not executed)\n", checks);
}
