/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Prove the existing SDK manager borrow never adopts opaque tasks
 ****************************************************************************/
#include "../../Client/game_de/CPedIntelligenceDE.h"
#include <windows.h>
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CTaskManagerDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "PedIntelligenceFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Ped intelligence check %u failed: %s\n", checks, reason);
            throw std::runtime_error(reason);
        }
    }
    bool Same(const NativePedReferenceToken& a, const NativePedReferenceToken& b) noexcept
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    enum class Drift
    {
        None,
        NativeIntelligence,
        EmbeddedManager,
        Ped,
        OpaqueTask
    };
    class Owner final : public NativeGameTimingAccess
    {
    public:
        NativePedReferenceToken            expected{19, 7, 2};
        NativePedIntelligenceReader::Value value;
        ClockResult                        failure = ClockResult::NotInitialized, removalResult = ClockResult::Ok;
        unsigned                           reads = 0, failAt = 0, removals = 0;
        int                                removedPriority = 0;
        NativePedReferenceToken            removedToken;
        Drift                              drift = Drift::None;
        Owner()
        {
            // Portable test facts are opaque. Deliberately invalid task words
            // make an accidental task dereference or virtual lookup fail.
            auto& p = value.ped;
            p.handle = 0x101;
            p.slot = 1;
            p.pool = 0x20000000;
            p.objects = 0x21000000;
            p.flags = 0x22000000;
            p.nativePed = 0x23000000;
            p.vtable = 0x24000000;
            p.firstMethod = 0x25000000;
            p.capacity = 140;
            p.cursor = 1;
            p.generation = 1;
            p.ownsAllocations = 1;
            value.pool = 0x31000000;
            value.objects = 0x32000000;
            value.flags = 0x33000000;
            value.nativeIntelligence = 0x34000000;
            value.taskManager = 0x35000000;
            value.slot = 2;
            value.capacity = 140;
            value.cursor = 2;
            value.generation = 2;
            value.ownsAllocations = 1;
            value.rawType = 3;
            value.primaryTasks.fill((std::numeric_limits<std::uintptr_t>::max)());
            value.secondaryTasks.fill((std::numeric_limits<std::uintptr_t>::max)() - 1);
        }
        ClockResult SetTimeScale(float) noexcept override { std::abort(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { std::abort(); }
        ClockResult Pause(bool) noexcept override { std::abort(); }
        ClockResult ReadReferencedPedIntelligence(const NativePedReferenceToken& token, NativePedIntelligenceReader::Value& output) noexcept override
        {
            ++reads;
            if (!Same(token, expected))
                return ClockResult::OwnerConflict;
            if (reads == failAt)
                return failure;
            auto next = value;
            if (reads == 2)
            {
                switch (drift)
                {
                    case Drift::NativeIntelligence:
                        ++next.nativeIntelligence;
                        break;
                    case Drift::EmbeddedManager:
                        ++next.taskManager;
                        break;
                    case Drift::Ped:
                        ++next.ped.handle;
                        break;
                    case Drift::OpaqueTask:
                        ++next.primaryTasks[0];
                        break;
                    case Drift::None:
                        break;
                }
            }
            output = next;
            return ClockResult::Ok;
        }
        ClockResult RemoveReferencedPedSecondaryTask(const NativePedReferenceToken& token, int priority) noexcept override
        {
            ++removals;
            removedToken = token;
            removedPriority = priority;
            return removalResult;
        }
    };
    class Manager final : public ped_intelligence_fixture::ManagerBase
    {
        unsigned& destructions;

    public:
        Manager(NativeTaskRemovalAccess& owner, const NativePedReferenceToken& token, unsigned& count) noexcept : ManagerBase(owner, token), destructions(count)
        {
        }
        ~Manager() { ++destructions; }
    };
    template <class Call>
    bool Refuses(Call&& call, ClockResult result, const char* operation = "GetTaskManager")
    {
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            const std::string expected = std::string("DE ped intelligence owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result));
            return error.what() == expected;
        }
        return false;
    }
    static_assert(std::is_abstract_v<CPedIntelligenceDE> && std::is_abstract_v<CTaskManagerDE> && !std::has_virtual_destructor_v<CPedIntelligenceDE> &&
                  !std::is_copy_constructible_v<CPedIntelligenceDE> && !std::is_move_constructible_v<CPedIntelligenceDE>);
    void Borrowing()
    {
        Owner    owner;
        unsigned deaths = 0;
        {
            Manager manager(owner, owner.expected, deaths);
            {
                ped_intelligence_fixture::Intelligence intelligence(owner, owner.expected, manager);
                Check(!owner.reads && !owner.removals, "construction checks CPU provenance without native effects or task lookup");
                CPedIntelligence& sdk = intelligence;
                Check(sdk.GetTaskManager() == static_cast<CTaskManager*>(&manager) && owner.reads == 2 && !owner.removals,
                      "two current observations publish only the caller's existing complete typed SDK manager");
                owner.reads = 0;
                ++owner.value.nativeIntelligence;
                ++owner.value.taskManager;
                Check(sdk.GetTaskManager() == static_cast<CTaskManager*>(&manager) && owner.reads == 2,
                      "later publication freshly observes the current same-ped identity rather than caching a native pointer lease");
                auto* selected = sdk.GetTaskManager();
                selected->RemoveTaskSecondary(-7);
                Check(owner.removals == 1 && owner.removedPriority == -7 && Same(owner.removedToken, owner.expected),
                      "the existing actual secondary-removal method forwards its exact old owner/token/priority without new policy");
            }
            Check(!deaths && owner.removals == 1, "typed intelligence destruction neither deletes the borrowed manager nor mutates native tasks");
        }
        Check(deaths == 1 && owner.removals == 1, "the enclosing caller explicitly owns manager retirement after intelligence and users quiesce");
        std::puts("Ped intelligence actual SDK manager borrow fixture completed");
    }
    void Provenance()
    {
        Owner    first, second;
        unsigned deaths = 0;
        Manager  manager(first, first.expected, deaths);
        Check(Refuses(
                  [&]
                  {
                      ped_intelligence_fixture::Intelligence wrong(second, first.expected, manager);
                      (void)wrong;
                  },
                  ClockResult::OwnerConflict, "constructor") &&
                  !first.reads && !second.reads,
              "different typed task-removal owner refuses before a native read");
        for (unsigned field = 0; field != 3; ++field)
        {
            auto token = first.expected;
            if (field == 0)
                ++token.version;
            else if (field == 1)
                ++token.ownerGeneration;
            else
                ++token.slot;
            Check(Refuses(
                      [&]
                      {
                          ped_intelligence_fixture::Intelligence wrong(first, token, manager);
                          (void)wrong;
                      },
                      ClockResult::OwnerConflict, "constructor") &&
                      !first.reads,
                  "every immutable CPU token component must name the exact supplied manager's ped");
        }
        auto invalid = first.expected;
        invalid.version = 0;
        Check(Refuses(
                  [&]
                  {
                      ped_intelligence_fixture::Intelligence wrong(first, invalid, manager);
                      (void)wrong;
                  },
                  ClockResult::InvalidArgument, "constructor") &&
                  !first.reads,
              "an invalid token cannot turn a generic manager address into provenance");
        ped_intelligence_fixture::Intelligence intelligence(first, first.expected, manager);
        ++first.expected.version;
        Check(Refuses([&] { intelligence.GetTaskManager(); }, ClockResult::OwnerConflict) && first.reads == 1,
              "the existing current ped resolver still rejects a withdrawn or stale native reference");
        std::puts("Ped intelligence exact owner and token provenance fixture completed");
    }
    void RefusalsAndBookends()
    {
        Owner                                  owner;
        unsigned                               deaths = 0;
        Manager                                manager(owner, owner.expected, deaths);
        ped_intelligence_fixture::Intelligence intelligence(owner, owner.expected, manager);
        for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::Stopped})
        {
            owner.reads = 0;
            owner.failAt = 1;
            owner.failure = status;
            CTaskManager* output = nullptr;
            Check(Refuses([&] { output = intelligence.GetTaskManager(); }, status) && !output && owner.reads == 1,
                  "first native owner refusal remains explicit and publishes no SDK manager");
        }
        owner.reads = 0;
        owner.failAt = 2;
        owner.failure = ClockResult::BindingChanged;
        CTaskManager* output = nullptr;
        Check(Refuses([&] { output = intelligence.GetTaskManager(); }, ClockResult::BindingChanged) && !output && owner.reads == 2,
              "a final current-owner refusal preserves the uncommitted output");
        owner.reads = 0;
        owner.failAt = 0;
        const auto nativeIntelligence = owner.value.nativeIntelligence;
        owner.value.nativeIntelligence = 0;
        Check(Refuses([&] { intelligence.GetTaskManager(); }, ClockResult::NotInitialized) && owner.reads == 1,
              "a missing qualified intelligence does not silently return the supplied CPU manager");
        owner.value.nativeIntelligence = nativeIntelligence;
        for (const auto drift : {Drift::NativeIntelligence, Drift::EmbeddedManager, Drift::Ped, Drift::OpaqueTask})
        {
            owner.reads = 0;
            owner.drift = drift;
            output = nullptr;
            Check(Refuses([&] { output = intelligence.GetTaskManager(); }, ClockResult::BindingChanged) && !output && owner.reads == 2,
                  "changed ped/intelligence/manager identity or opaque task words refuse before SDK publication");
        }
        owner.reads = 0;
        owner.drift = Drift::None;
        Check(intelligence.GetTaskManager() == static_cast<CTaskManager*>(&manager) && owner.reads == 2 && !owner.removals,
              "a later fresh coherent observation neither adopts tasks nor replays a mutation");
        std::puts("Ped intelligence current identity bookends and refusals fixture completed");
        std::puts("Four intelligence methods and task-manager factory obligations remain abstract");
    }
}
int main()
{
    try
    {
        Borrowing();
        Provenance();
        RefusalsAndBookends();
        std::printf("Passed %u ped intelligence SDK checks (%u-bit; opaque provider doubles only)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped intelligence SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
