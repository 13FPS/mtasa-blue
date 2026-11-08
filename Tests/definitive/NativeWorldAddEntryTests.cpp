/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual WorldAdd whole-entry admission/ACK with final-body doubles
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldAddEntryHistoricalBoundMain
#include "NativeWorldBoundRectTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeWorldAddEntryCalls.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldAddBudgetMemory.cpp"
#include "../../Client/game_de/NativeWorldAddEntryMemory.cpp"
#include "../../Client/game_de/NativeWorldAddEntryCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace AddEntryTest
{
    namespace AddEntryP = mta::game_de::NativeWorldAddProfile;
    namespace AddEntryC = mta::game_de::NativeWorldAddEntryCalls;
    namespace AddEntryM = mta::game_de::NativeWorldAddEntryMemory;
    namespace AddEntryW = NativeWorldIplFixture;
    NativeWorldPoolMatrixFixture::State* entities = nullptr;
    AddEntryW::Pool*                     singles = nullptr;
    AddEntryW::Pool*                     doubles = nullptr;
    unsigned                             entries = 0, mode = 0;
    bool                                 invalid = false;
    template <class T>
    T Read(std::uintptr_t address, std::size_t at = 0) noexcept
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void*>(address + at), sizeof(value));
        return value;
    }
    ClockResult OriginalDouble(AddEntryC::Entry original, std::uintptr_t entity, std::uint32_t& exception) noexcept
    {
        ++entries;
        if (!entities || reinterpret_cast<std::uintptr_t>(original) != entities->image + AddEntryP::WorldAddRva || Read<std::uintptr_t>(entity, 0x20))
        {
            invalid = true;
            return ClockResult::InvalidArgument;
        }
        const auto type = static_cast<unsigned char>(Read<unsigned char>(entity, 0x6a) & 7);
        if (type != 1 && type != 5)
        {
            invalid = true;
            return ClockResult::UnsupportedBuild;
        }
        const bool lod = (Read<std::uint32_t>(entity, 0x30) & AddEntryP::LodFlag) != 0;
        auto*      pool = type == 5 && !lod ? doubles : singles;
        const auto image = entities->image;
        // Reuse the existing labelled genuine four-corner original double.
        // The real whole WorldAdd body calls this producer a second time.
        BoundTest::BC::Rect  rect{};
        BoundTest::BC::Rect* returned = nullptr;
        const auto result = BoundTest::OriginalDouble(reinterpret_cast<BoundTest::BC::Entry>(image + mta::game_de::NativeWorldBoundRectProfile::EntryRva),
                                                      entity, rect, returned, exception);
        if (result != ClockResult::Ok || returned != &rect)
            return ClockResult::ReadFailure;
        // This is the final native AddRect-body double only. It deliberately
        // does not call production Budget/Plan/Expected: source branch order,
        // fixed allocator and neighbour writes form an independent oracle.
        const auto cell = [&](float coordinate)
        {
            coordinate = (std::max)(-3000.0f, (std::min)(2999.0f, coordinate));
            const float scaled = coordinate * (lod ? 0.005f : 0.02f);
            const float shifted = scaled + (lod ? 15.0f : 60.0f);
            return lod ? static_cast<std::int32_t>(shifted) : static_cast<std::int32_t>(std::floor(shifted));
        };
        const auto                         left = cell(rect[0]), right = cell(rect[2]), bottom = cell(rect[3]), top = cell(rect[1]);
        const std::array<unsigned char, 4> cache{static_cast<unsigned char>(left), static_cast<unsigned char>(right), static_cast<unsigned char>(bottom),
                                                 static_cast<unsigned char>(top)};
        for (std::size_t at = 0; at < cache.size(); ++at)
            AddEntryW::Put(entity, 0x3c + at, cache[at]);
        auto       cursor = Read<std::int32_t>(pool->Address(), 0x14);
        const auto capacity = Read<std::int32_t>(pool->Address(), 0x10);
        for (auto y = bottom; y <= top; ++y)
            for (auto x = left; x <= right; ++x)
            {
                std::int32_t searched = 0;
                do
                {
                    if (++cursor == capacity)
                        cursor = 0;
                    if (++searched > capacity)
                    {
                        invalid = true;
                        return ClockResult::NotInitialized;
                    }
                } while (!(pool->flags[cursor] & 0x80));
                AddEntryW::Put(pool->Address(), 0x14, cursor);
                pool->flags[cursor] = static_cast<unsigned char>(((pool->flags[cursor] & 0x7f) + 1) & 0x7f);
                const auto node = pool->Row(static_cast<std::size_t>(cursor));
                const auto offset = static_cast<std::size_t>(y) * (lod ? AddEntryP::LodSide : AddEntryP::SectorSide) + static_cast<std::size_t>(x);
                const auto head = image + (lod ? AddEntryP::LodRva + offset * 8 : AddEntryP::SectorsRva + offset * 16 + (pool == doubles ? 8 : 0));
                const auto old = Read<std::uintptr_t>(head);
                AddEntryW::Put(node, 0, entity);
                AddEntryW::Put(node, 8, old);
                if (pool == doubles)
                {
                    AddEntryW::Put(node, 0x10, std::uintptr_t{0});
                    if (old)
                        AddEntryW::Put(old, 0x10, node);
                }
                AddEntryW::Put(head, 0, node);
            }
        if (mode == 1)
        {
            exception = 0xe05c0001;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            AddEntryW::Put(pool->Row(500), 0, std::uintptr_t{0x1234});
        if (mode == 3)
            AddEntryW::Put(pool->Address(), 0x14, std::int32_t{333});
        if (mode == 4)
            AddEntryW::Put(entity, 0x56, static_cast<unsigned char>(9));
        if (mode == 5)
            AddEntryW::Put(image + AddEntryP::LodRva + 899 * 8, 0, pool->Row(40));
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace AddEntryTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "WorldAdd whole-entry evidence stays in an explicit workspace folder");
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    AddEntryW::State                    world(image, &LoadFixture::Write, &fixture);
    NativeWorldPoolMatrixFixture::State owned(image, &LoadFixture::Write, &fixture);
    AddEntryW::Pool                     doublePool(static_cast<std::int32_t>(AddEntryP::DoubleCapacity), AddEntryP::DoubleBytes);
    Check(world.Ready() && owned.Ready() && doublePool.Ready(), "actual typed entity and native fixed Single/Double allocator fixtures are owned");
    entities = BoundTest::selected = &owned;
    singles = &world.links;
    doubles = &doublePool;
    const auto reset = [&](bool dummy, bool lod, bool matrix)
    {
        fixture.Reset();
        world.Reset();
        owned.Reset();
        doublePool.Reset();
        NativeWorldBoundRectFixture::Install(image, &LoadFixture::Write, &fixture);
        NativeWorldBoundRectFixture::PublishProvider(fixture.graph.root, fixture.graph.Game(), reinterpret_cast<std::uintptr_t>(fixture.rows), 20000);
        for (const auto& pin : AddEntryP::Code)
            LoadFixture::Write(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& scalar : AddEntryP::Scalars)
            LoadFixture::Write(&fixture, scalar.rva, &scalar.bits, sizeof(scalar.bits), PAGE_READONLY);
        for (const auto& slot : AddEntryP::Slots)
        {
            const auto target = image + slot.target;
            LoadFixture::Write(&fixture, slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        world.Global(AddEntryP::DoubleRootRva, doublePool.Address());
        std::vector<unsigned char> sectors(AddEntryP::SectorSide * AddEntryP::SectorSide * 16), lodHeads(AddEntryP::LodSide * AddEntryP::LodSide * 8);
        LoadFixture::Write(&fixture, AddEntryP::SectorsRva, sectors.data(), sectors.size(), PAGE_READWRITE);
        LoadFixture::Write(&fixture, AddEntryP::LodRva, lodHeads.data(), lodHeads.size(), PAGE_READWRITE);
        auto*      pool = dummy && !lod ? doubles : singles;
        const auto capacity = Read<std::int32_t>(pool->Address(), 0x10);
        std::memset(pool->flags, 0, static_cast<std::size_t>(capacity));
        pool->flags[capacity - 1] = 0x85;
        pool->flags[0] = 0xff;
        pool->flags[7] = 0x80;
        pool->flags[9] = 0x81;
        AddEntryW::Put(pool->Address(), 0x14, capacity - 2);
        AddEntryW::Put(pool->Address(), 0x1f, static_cast<unsigned char>(0xa5));
        const auto entity = dummy ? owned.dummies.Row(0) : owned.buildings.Row(0);
        AddEntryW::Put(entity, 0x30, lod ? AddEntryP::LodFlag : std::uint32_t{0});
        if (matrix)
            owned.Move(0, mta::game_de::NativeWorldPoolMatrixMemory::List::Static, entity);
        unsigned at = 0;
        for (unsigned y = lod ? 14u : 59u; y <= (lod ? 15u : 60u); ++y)
            for (unsigned x = lod ? 14u : 59u; x <= (lod ? 15u : 60u); ++x)
            {
                const auto old = pool->Row(30 + at);
                AddEntryW::Put(old, 0, owned.buildings.Row(0));
                AddEntryW::Put(old, 8, std::uintptr_t{0});
                if (pool == doubles)
                    AddEntryW::Put(old, 0x10, std::uintptr_t{0});
                const auto offset = y * (lod ? AddEntryP::LodSide : AddEntryP::SectorSide) + x;
                const auto head = image + (lod ? AddEntryP::LodRva + offset * 8 : AddEntryP::SectorsRva + offset * 16 + (pool == doubles ? 8 : 0));
                AddEntryW::Put(head, 0, old);
                ++at;
            }
        entries = mode = BoundTest::entries = BoundTest::mode = 0;
        invalid = false;
        return entity;
    };
    AddEntryC::Receipt receipt;
    for (unsigned scenario = 0; scenario != 3; ++scenario)
    {
        const bool dummy = scenario != 0, lod = scenario == 2, matrix = scenario == 2;
        const auto entity = reset(dummy, lod, matrix);
        const auto kind = dummy ? AddEntryC::Kind::Dummy : AddEntryC::Kind::Building;
        const auto result =
            AddEntryC::Add(image, 1, kind, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble, &BoundTest::OriginalDouble);
        Check(result == ClockResult::Ok && receipt.entered && receipt.returned && receipt.acknowledged && !receipt.uncertain && receipt.entity == entity &&
                  receipt.nodes == 4 && entries == 1 && BoundTest::entries == 2 && !invalid,
              "whole native WorldAdd itinerary has genuine preflight bounds plus nested original, exact fixed allocation and complete resource ACK");
        auto*      pool = dummy && !lod ? doubles : singles;
        const auto capacity = Read<std::int32_t>(pool->Address(), 0x10);
        Check(pool->flags[capacity - 1] == 6 && pool->flags[0] == 0 && pool->flags[7] == 1 && pool->flags[9] == 2 &&
                  Read<std::int32_t>(pool->Address(), 0x14) == 9 && Read<unsigned char>(pool->Address(), 0x1f) == 0xa5,
              "increment-before-search wrap and low7 generation transitions preserve opaque pool padding");
        Check(Read<std::uintptr_t>(pool->Row(9)) == entity && Read<std::uintptr_t>(pool->Row(9), 8) == pool->Row(33) &&
                  Read<unsigned char>(entity, 0x3c) == (lod ? 14 : 59),
              "only genuine cached sector-byte ranges and allocated intrusive list membership change");
        Check(pool != doubles || Read<std::uintptr_t>(pool->Row(33), 0x10) == pool->Row(9), "DoubleLink old head previous is updated to the new owned node");
    }
    reset(false, false, false);
    AddEntryW::Put(owned.buildings.Row(0), 0x20, owned.dummies.Row(0));
    Check(AddEntryC::Add(image, 1, AddEntryC::Kind::Building, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble,
                         &BoundTest::OriginalDouble) != ClockResult::Ok &&
              !receipt.entered && !entries && !BoundTest::entries,
          "actor-null subset refuses before either original instead of entering UE updates");
    reset(false, false, false);
    std::memset(singles->flags, 0, AddEntryP::SingleCapacity);
    Check(AddEntryC::Add(image, 1, AddEntryC::Kind::Building, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble,
                         &BoundTest::OriginalDouble) == ClockResult::NotInitialized &&
              !receipt.entered && !entries && BoundTest::entries == 1 && receipt.rectangle.entered && receipt.rectangle.returned,
          "genuine bounds can be acknowledged while exhausted fixed node budget refuses before mutating WorldAdd");
    for (unsigned failure = 1; failure != 6; ++failure)
    {
        reset(true, false, false);
        mode = failure;
        Check(AddEntryC::Add(image, 1, AddEntryC::Kind::Dummy, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble,
                             &BoundTest::OriginalDouble) != ClockResult::Ok &&
                  receipt.entered && !receipt.acknowledged && receipt.uncertain && entries == 1,
              "entered exception or unrelated node/cursor/entity/grid drift preserves unknown whole-entry receipt without replay");
        Check(receipt.returned == (failure != 1), "returned original and exact resource acknowledgement are distinct receipt edges");
    }
    entities = BoundTest::selected = nullptr;
    singles = doubles = nullptr;
    std::printf("Passed %u native world add entry checks (actual input/budget/whole-entry ACK; final original doubles; no GTA execution)\n", checks);
    return 0;
}
