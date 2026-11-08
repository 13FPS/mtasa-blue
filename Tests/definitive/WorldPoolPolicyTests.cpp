/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual shared pool itinerary and live CDynamicPool part lifetimes
 ****************************************************************************/
#include <game/CWorldPoolBackupPolicy.h>
#include <game/CDynamicPoolCapacityPolicy.h>
#include <game/CGameWorldPolicy.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>
#include "../../Client/game_sa/CDynamicPool.h"

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    struct PartValue
    {
        static unsigned constructed, destroyed, throwAt;
        unsigned        value = 0;
        PartValue()
        {
            if (throwAt && constructed + 1 == throwAt)
                throw std::bad_alloc();
            ++constructed;
        }
        ~PartValue() { ++destroyed; }
    };
    unsigned PartValue::constructed = 0, PartValue::destroyed = 0, PartValue::throwAt = 0;
    void     Itinerary()
    {
        using P = MtaGame::CWorldPoolBackupPolicy;
        std::vector<std::string> trace;
        const auto               mark = [&](const char* name, std::size_t index)
        {
            trace.emplace_back(std::string(name) + std::to_string(index));
            return true;
        };
        Check(P::BackupSlots(
                  3, [](std::size_t i) { return i != 1; }, [&](std::size_t i) { return mark("remove", i); }, [&](std::size_t i) { return mark("matrix", i); },
                  [&](std::size_t i) { return mark("release", i); }, [&](std::size_t i) { return mark("capture", i); },
                  [&](std::size_t i) { return mark("absent", i); }),
              "complete actual slot itinerary succeeds");
        Check(trace == std::vector<std::string>{"remove0", "matrix0", "release0", "capture0", "absent1", "remove2", "matrix2", "release2", "capture2"},
              "backup preserves cleanup/matrix/release/capture order and exact absent slots");
        trace.clear();
        Check(!P::BackupSlots(
                  2, [](std::size_t) { return true; }, [&](std::size_t i) { return mark("remove", i); },
                  [&](std::size_t i)
                  {
                      mark("matrix", i);
                      return i == 0;
                  },
                  [&](std::size_t i) { return mark("release", i); }, [&](std::size_t i) { return mark("capture", i); },
                  [&](std::size_t i) { return mark("absent", i); }),
              "known native callback refusal retains its accepted prefix");
        Check(trace == std::vector<std::string>{"remove0", "matrix0", "release0", "capture0", "remove1", "matrix1"},
              "refusal never invents later release/capture or automatic rollback");
        trace.clear();
        Check(P::RestoreSlots(
                  4, 2, [](std::size_t) { return true; }, [&](std::size_t i) { return mark("copy", i); }, [&](std::size_t i)
                  { return mark("staticMatrix", i); }, [&](std::size_t i) { return mark("add", i); }, [&](std::size_t i) { return mark("reapply", i); }),
              "bounded restore succeeds");
        Check(trace == std::vector<std::string>{"copy0", "staticMatrix0", "add0", "reapply0", "copy1", "staticMatrix1", "add1", "reapply1"},
              "restore clips to current capacity and preserves static-list/world/removal callback order");
        Check(P::InMovedRange(100, 100, 200) && P::InMovedRange(199, 100, 200) && !P::InMovedRange(200, 100, 200) && !P::InMovedRange(99, 100, 200),
              "rebases include only the old allocation's half-open range");
        std::string order;
        P::RebaseMovedBuildings(true, [&] { order += 'I'; }, [&] { order += 'O'; }, [&] { order += 'B'; }, [&] { order += 'D'; });
        P::RebaseMovedBuildings(false, [&] { order += 'X'; }, [&] { order += 'X'; }, [&] { order += 'X'; }, [&] { order += 'X'; });
        P::SweepLinks(false, [&] { order += 'V'; }, [&] { order += 'P'; }, [&] { order += 'E'; });
        P::SweepLinks(true, [&] { order += 'X'; }, [&] { order += 'X'; }, [&] { order += 'X'; });
        Check(order == "IOBDVPE", "existing LOD rebase/link sweep ordering and once-per-cycle skip remain exact");
        std::string world;
        const auto  resized = MtaGame::CGameWorldPolicy::SetBuildingPoolSize(
            8, false, [] { return 4; }, [&] { world += 'R'; }, [&] { world += 'D'; },
            [&](int count)
            {
                Check(count == 8, "requested size reaches real resize callback");
                world += 'S';
                return false;
            },
            [&] { world += 'B'; });
        Check(!resized && world == "RSB", "failed allocation/resize still restores the already-removed world through existing MTA policy");
    }
    void DynamicParts()
    {
        PartValue::constructed = PartValue::destroyed = PartValue::throwAt = 0;
        {
            CDynamicPool<PartValue, PoolGrowAddStrategy<2, 2>> pool;
            auto*                                              first = pool.AllocateItem();
            first->value = 77;
            Check(pool.GetCapacity() == 2 && pool.GetUsedSize() == 1, "actual dynamic part starts with the original capacity");
            Check(!pool.SetCapacity(0) && !pool.SetCapacity(2), "zero and same-size requests preserve actual false status");
            Check(pool.SetCapacity(5) && pool.GetCapacity() == 6 && first->value == 77,
                  "growth overshoots by whole parts without moving an existing live value");
            Check(!pool.SetCapacity(3) && pool.GetCapacity() == 4 && first->value == 77,
                  "nonrepresentable shrink preserves an already-removed empty part prefix");
            Check(pool.SetCapacity(2) && pool.GetCapacity() == 2 && !pool.SetCapacity(1) && pool.GetCapacity() == 2,
                  "used tail parts cannot be discarded and exact empty-part shrink still succeeds");
            pool.RemoveItem(first);
            Check(pool.GetUsedSize() == 0 && !pool.SetCapacity(0) && pool.GetCapacity() == 2,
                  "returning an item does not manufacture zero-capacity shrink semantics");
        }
        Check(PartValue::constructed == PartValue::destroyed, "real removed parts and final destruction retire all actual element lifetimes");
        PartValue::constructed = PartValue::destroyed = PartValue::throwAt = 0;
        {
            CDynamicPool<PartValue, PoolGrowAddStrategy<2, 2>> pool;
            PartValue::throwAt = 5;
            Check(!pool.SetCapacity(6) && pool.GetCapacity() == 4, "allocation failure keeps the successfully allocated actual part prefix");
            PartValue::throwAt = 0;
            Check(pool.SetCapacity(6) && pool.GetCapacity() == 6, "later explicit growth resumes from the current real capacity");
        }
        Check(PartValue::constructed == PartValue::destroyed, "failed part construction and later cleanup retain no live element leaks");
    }
}
int main()
{
    try
    {
        Itinerary();
        DynamicParts();
        std::puts("Actual world pool policies and dynamic part lifetimes completed");
        std::printf("Passed %u actual world pool policy checks (%u-bit; no native engine code)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "World pool policy fixture failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
