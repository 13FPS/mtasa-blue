/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA task factory choices and deletion callback order
 *****************************************************************************/
#include <game/CTaskWrapperPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <list>
#include <stdexcept>
#include <vector>

namespace
{
    using Policy = MtaGame::CTaskWrapperPolicy;
    using Factory = Policy::Factory;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
            std::exit(EXIT_FAILURE);
        }
    }

    void FactoryCases()
    {
        // This is the typed public SDK contract of the old real factory. The
        // test executes no native GetTaskType or wrapper constructor.
        constexpr std::array<Policy::Member, 25> expected = {{
            {TASK_SIMPLE_GANG_DRIVEBY, Factory::GangDriveBy},
            {TASK_SIMPLE_USE_GUN, Factory::UseGun},
            {TASK_SIMPLE_FIGHT, Factory::Fight},
            {TASK_COMPLEX_USE_MOBILE_PHONE, Factory::UseMobilePhone},
            {TASK_SIMPLE_ANIM, Factory::RunAnim},
            {TASK_SIMPLE_NAMED_ANIM, Factory::RunNamedAnim},
            {TASK_COMPLEX_DIE, Factory::Die},
            {TASK_SIMPLE_STEALTH_KILL, Factory::StealthKill},
            {TASK_COMPLEX_SUNBATHE, Factory::Sunbathe},
            {TASK_SIMPLE_SWIM, Factory::Swim},
            {TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER, Factory::CarSetPassenger},
            {TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER, Factory::CarSetDriver},
            {TASK_SIMPLE_CAR_SET_PED_OUT, Factory::CarSetOut},
            {TASK_COMPLEX_ENTER_CAR_AS_DRIVER, Factory::EnterCarDriver},
            {TASK_COMPLEX_ENTER_CAR_AS_PASSENGER, Factory::EnterCarPassenger},
            {TASK_COMPLEX_ENTER_BOAT_AS_DRIVER, Factory::EnterBoatDriver},
            {TASK_COMPLEX_LEAVE_CAR, Factory::LeaveCar},
            {TASK_COMPLEX_WANDER, Factory::WanderStandard},
            {TASK_SIMPLE_IK_CHAIN, Factory::IkChain},
            {TASK_SIMPLE_IK_LOOK_AT, Factory::IkLookAt},
            {TASK_SIMPLE_IK_MANAGER, Factory::IkManager},
            {TASK_SIMPLE_CLIMB, Factory::Climb},
            {TASK_SIMPLE_JETPACK, Factory::JetPack},
            {TASK_SIMPLE_CHOKING, Factory::Choking},
            {TASK_SIMPLE_DUCK, Factory::Duck},
        }};
        static_assert(Policy::SelectFactory(TASK_SIMPLE_DUCK) == Factory::Duck);
        static_assert(Policy::SelectFactory(9999) == Factory::Base);
        Check(Policy::Members.size() == expected.size(), "specialized factory set remains exactly 25 logical types");
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            Check(Policy::Members[i].type == expected[i].type && Policy::Members[i].factory == expected[i].factory,
                  "factory row order and typed SDK association remain unchanged");
            Check(Policy::SelectFactory(expected[i].type) == expected[i].factory, "every actual specialization is selected");
            Check(expected[i].factory != Factory::Base, "no specialized row silently becomes a base wrapper");
            for (std::size_t j = 0; j < i; ++j)
                Check(expected[j].type != expected[i].type && expected[j].factory != expected[i].factory,
                      "specializations remain unambiguous logical types and wrapper kinds");
        }
        // Test the gaps as well as the advertised rows. In particular many
        // genuine SDK task types were always handled by the real base class.
        for (int type = -3; type != 10241; ++type)
        {
            Factory wanted = Factory::Base;
            for (const auto& member : expected)
                if (member.type == type)
                    wanted = member.factory;
            Check(Policy::SelectFactory(type) == wanted, "all specialized types and intervening SDK/unknown values retain their established choice");
        }
        for (int type : {(std::numeric_limits<int>::min)(), -1, 9999, (std::numeric_limits<int>::max)()})
            Check(Policy::SelectFactory(type) == Factory::Base, "pure-call sentinel and malformed integer IDs retain base fallback");
    }

    // Ordinary CPU identities below are deliberately not CTask, native task
    // interfaces or SDK wrappers. The real classic caller owns validation and
    // invokes its real DestroyJustThis after the shared ordered removal.
    struct NativeTag
    {
        unsigned id;
    };
    struct WrapperTag
    {
        unsigned id;
    };
    struct Entry
    {
        NativeTag*  native;
        WrapperTag* wrapper;
        unsigned    id;
    };
    using Entries = std::list<Entry*>;
    auto        NativeOf = [](const Entry* entry) { return entry->native; };
    auto        WrapperOf = [](const Entry* entry) { return entry->wrapper; };
    WrapperTag* Find(const Entries& entries, NativeTag* native)
    {
        return Policy::FindCached(entries, native, NativeOf, WrapperOf);
    }
    void ClearEntries(Entries& entries)
    {
        for (auto* entry : entries)
            delete entry;
        entries.clear();
    }
    bool Contains(const Entries& entries, const Entry* wanted)
    {
        for (const auto* entry : entries)
            if (entry == wanted)
                return true;
        return false;
    }

    void RegistryCases()
    {
        NativeTag  a{1}, b{2}, absent{3};
        WrapperTag first{1}, second{2}, independent{3};
        Entries    entries, other;
        unsigned   deleted = 0, retired = 0;
        auto       destroy = [&](Entry* entry)
        {
            ++deleted;
            delete entry;
        };
        auto retire = [&](WrapperTag*) { ++retired; };
        Check(!Find(entries, &a), "empty cached lookup does not synthesize a wrapper");
        Check(!Policy::RetireFirst(entries, &a, NativeOf, WrapperOf, destroy, retire) && !deleted && !retired, "empty removal invokes no ownership callbacks");
        auto* empty = new Entry{&a, nullptr, 1};
        auto* firstEntry = new Entry{&a, &first, 2};
        auto* otherEntry = new Entry{&b, &independent, 3};
        auto* duplicate = new Entry{&a, &second, 4};
        entries = {empty, firstEntry, otherEntry, duplicate};
        other.push_back(new Entry{&a, &independent, 5});
        Check(Find(entries, &a) == &first, "cached lookup skips a matching null wrapper and returns first nonnull duplicate");
        Check(Find(entries, &b) == &independent && !Find(entries, &absent), "native identity comparison does not use list position or wrapper type");
        Check(Find(other, &a) == &independent, "separate registries do not share cached CPU state");
        Check(!Policy::RetireFirst(entries, &absent, NativeOf, WrapperOf, destroy, retire) && !deleted && !retired && entries.size() == 4,
              "missing native removal preserves all entries and callback counts");
        unsigned comparisons = 0;
        Check(Policy::RetireFirst(
                  entries, &a,
                  [&](const Entry* entry)
                  {
                      ++comparisons;
                      return entry->native;
                  },
                  WrapperOf, destroy, retire),
              "first matching entry is retired even when its wrapper is null");
        Check(comparisons == 1 && deleted == 1 && retired == 0 && entries.size() == 3 && entries.front() == firstEntry,
              "removal does not skip null wrappers, consume later duplicates or retire a null wrapper");
        Check(Find(entries, &a) == &first && Find(other, &a) == &independent, "removing a null duplicate changes neither lookup nor another registry");
        bool deleteBeforeErase = false, erasedBeforeRetire = false;
        Check(Policy::RetireFirst(
                  entries, &a, NativeOf, WrapperOf,
                  [&](Entry* entry)
                  {
                      deleteBeforeErase = entry == firstEntry && Contains(entries, entry);
                      ++deleted;
                      delete entry;
                  },
                  [&](WrapperTag* wrapper)
                  {
                      ++retired;
                      erasedBeforeRetire = wrapper == &first && !Contains(entries, firstEntry) && Find(entries, &a) == &second;
                  }),
              "nonnull first matching wrapper is retired once");
        Check(deleteBeforeErase && erasedBeforeRetire && deleted == 2 && retired == 1 && entries.size() == 2,
              "entry is deleted, list node erased, then wrapper callback runs with only later duplicates visible");
        Check(Find(entries, &b) == &independent && Find(entries, &a) == &second, "removal preserves unrelated insertion order and later duplicate");
        // Reentrant native deletion is safe because the outer registry entry is
        // already gone. Retire the next duplicate in the same real algorithm.
        entries.push_back(new Entry{&a, &first, 6});
        unsigned reentered = 0;
        Check(Policy::RetireFirst(entries, &a, NativeOf, WrapperOf, destroy,
                                  [&](WrapperTag* wrapper)
                                  {
                                      Check(wrapper == &second && Find(entries, &a) == &first, "reentrant callback sees the next remaining duplicate");
                                      ++retired;
                                      if (Policy::RetireFirst(entries, &a, NativeOf, WrapperOf, destroy, retire))
                                          ++reentered;
                                  }),
              "outer retirement completes despite a reentrant second removal");
        Check(reentered == 1 && deleted == 4 && retired == 3 && entries.size() == 1 && entries.front() == otherEntry && !Find(entries, &a),
              "reentrant retirement consumes each intended entry once and preserves unrelated native identity");
        Check(Policy::RetireFirst(entries, &b, NativeOf, WrapperOf, destroy, retire) && entries.empty() && deleted == 5 && retired == 4,
              "last valid entry retires without deleting caller-owned wrapper identities");
        Check(first.id == 1 && second.id == 2 && independent.id == 3, "policy owns no wrapper/native destruction beyond supplied callbacks");
        ClearEntries(other);
    }

    void InterruptedRetirement()
    {
        NativeTag     native{1};
        WrapperTag    first{1}, second{2};
        Entries       entries{new Entry{&native, &first, 1}, new Entry{&native, &second, 2}};
        volatile bool refuse = true;
        unsigned      deletes = 0, retires = 0;
        try
        {
            Policy::RetireFirst(
                entries, &native, NativeOf, WrapperOf,
                [&](Entry* entry)
                {
                    ++deletes;
                    delete entry;
                },
                [&](WrapperTag*)
                {
                    ++retires;
                    if (refuse)
                        throw std::runtime_error("caller retirement failed");
                });
            Check(false, "caller exception should propagate");
        }
        catch (const std::runtime_error&)
        {
            Check(deletes == 1 && retires == 1 && entries.size() == 1 && Find(entries, &native) == &second,
                  "failed wrapper callback cannot revive the already removed first entry or hide later duplicates");
        }
        ClearEntries(entries);
    }
}

int main()
{
    FactoryCases();
    RegistryCases();
    InterruptedRetirement();
    std::puts("Production MTA task wrapper policy fixture completed");
    std::printf("Passed %u actual MTA task wrapper policy checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
    return EXIT_SUCCESS;
}
