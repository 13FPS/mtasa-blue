/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual World Add pool budgets and raw current-owner observations
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldAddHistoricalMatrixMain
#include "NativeWorldPoolMatrixTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeWorldAddBudgetMemory.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldAddBudgetMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace WorldAddTest
{
    namespace AddP = mta::game_de::NativeWorldAddProfile;
    namespace AddPlan = mta::game_de::NativeWorldAddPlan;
    namespace AddMem = mta::game_de::NativeWorldAddBudgetMemory;
    namespace AddW = NativeWorldIplFixture;
    namespace AddF = NativeWorldPoolMatrixFixture;
}
int main(int argc, char** argv)
{
    using namespace WorldAddTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "World Add evidence has an explicit workspace-owned output directory");
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    AddW::State world(image, &LoadFixture::Write, &fixture);
    AddF::State entities(image, &LoadFixture::Write, &fixture);
    AddW::Pool  doubles(static_cast<std::int32_t>(AddP::DoubleCapacity), AddP::DoubleBytes);
    Check(world.Ready() && entities.Ready() && doubles.Ready(), "actual typed native entity pools and fixed native list allocations are owned");
    const auto reset = [&]
    {
        fixture.Reset();
        world.Reset();
        entities.Reset();
        doubles.Reset();
        for (const auto& pin : AddP::Code)
            LoadFixture::Write(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& scalar : AddP::Scalars)
            LoadFixture::Write(&fixture, scalar.rva, &scalar.bits, sizeof(scalar.bits), PAGE_READONLY);
        for (const auto& slot : AddP::Slots)
        {
            const auto target = image + slot.target;
            LoadFixture::Write(&fixture, slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        world.Global(AddP::DoubleRootRva, doubles.Address());
        std::vector<unsigned char> sectors(AddP::SectorSide * AddP::SectorSide * 16), lod(AddP::LodSide * AddP::LodSide * 8);
        LoadFixture::Write(&fixture, AddP::SectorsRva, sectors.data(), sectors.size(), PAGE_READWRITE);
        LoadFixture::Write(&fixture, AddP::LodRva, lod.data(), lod.size(), PAGE_READWRITE);
    };
    const AddPlan::Rect        line{0, 0, 100, 0};
    std::vector<unsigned char> flags(AddP::SingleCapacity, 0);
    flags[74999] = 0x85;
    flags[0] = 0xff;
    flags[7] = 0x80;
    AddPlan::Value plan;
    Check(AddPlan::Require(AddPlan::Kind::Building, false, line, 74998, flags, plan) == ClockResult::Ok && plan.steps.size() == 3 &&
              plan.steps[0].slot == 74999 && plan.steps[1].slot == 0 && plan.steps[2].slot == 7 && plan.cursorAfter == 7 && plan.steps[0].after == 6 &&
              plan.steps[1].after == 0 && plan.steps[2].after == 1 && flags[0] == 0xff,
          "exact increment-before-search allocator follows holes/wrap and recycles generation127 without changing caller input");
    Check(plan.ranges == std::array<unsigned char, 4>{60, 62, 60, 60} && plan.steps[0].headRva == AddP::SectorsRva + (60 * 120 + 60) * 16 &&
              plan.steps[2].headRva == plan.steps[0].headRva + 32,
          "normal native cells advance x inside y using the genuine120x120 Building list heads");
    flags[7] = 0;
    const auto previous = plan.steps[0].slot;
    Check(AddPlan::Require(AddPlan::Kind::Building, false, line, 74998, flags, plan) == ClockResult::NotInitialized && plan.steps[0].slot == previous &&
              plan.steps.size() == 3,
          "one missing fixed native slot refuses the complete requested prefix before output mutation or engine entry");
    std::vector<unsigned char> doubleFlags(AddP::DoubleCapacity, 0x80);
    Check(AddPlan::Require(AddPlan::Kind::Dummy, false, line, -1, doubleFlags, plan) == ClockResult::Ok && plan.pool == AddPlan::PoolKind::Double &&
              plan.steps[0].headRva == AddP::SectorsRva + (60 * 120 + 60) * 16 + 8,
          "normal Dummy uses genuine DoubleLink slots and the second pointer in each native sector");
    flags.assign(AddP::SingleCapacity, 0x80);
    const AddPlan::Rect all{-100000, 100000, 100000, -100000};
    Check(AddPlan::Require(AddPlan::Kind::Dummy, true, all, -1, flags, plan) == ClockResult::Ok && plan.pool == AddPlan::PoolKind::Single &&
              plan.steps.size() == 900 && plan.ranges == std::array<unsigned char, 4>{0, 29, 0, 29} && plan.steps.front().headRva == AddP::LodRva &&
              plan.steps.back().headRva == AddP::LodRva + 899 * 8,
          "LOD flag selects the real30x30 SingleLink path even for typed Dummy and clamps out-of-world bounds");
    Check(AddPlan::Require(AddPlan::Kind::Building, false, all, -1, flags, plan) == ClockResult::Ok && plan.steps.size() == 14400 &&
              plan.ranges == std::array<unsigned char, 4>{0, 119, 0, 119},
          "normal maximum native rectangle is bounded to14400 selected cells");
    for (unsigned scenario = 0; scenario != 5; ++scenario)
    {
        auto bounds = line;
        auto cursor = -1;
        auto kind = AddPlan::Kind::Building;
        if (scenario == 0)
            bounds[0] = (std::numeric_limits<float>::quiet_NaN)();
        if (scenario == 1)
            bounds[1] = -1;
        if (scenario == 2)
            cursor = -2;
        if (scenario == 3)
            cursor = static_cast<std::int32_t>(flags.size());
        if (scenario == 4)
            kind = static_cast<AddPlan::Kind>(255);
        const auto count = plan.steps.size();
        Check(AddPlan::Require(kind, false, bounds, cursor, flags, plan) != ClockResult::Ok && plan.steps.size() == count,
              "nonfinite/inverted bounds, invalid owner kind and bad native cursor refuse unchanged output");
    }
    reset();
    AddMem::Value value;
    Check(AddMem::CaptureForRect(image, AddPlan::Kind::Building, 0, line, value) == ClockResult::Ok && value.plan.steps.size() == 3 &&
              value.pool.capacity == 75000 && value.plan.pool == AddPlan::PoolKind::Single && AddMem::Seal(image, value),
          "actual current source/world/heap/typed Building and fixed SingleLink pool seal the supplied-rectangle budget without native entry");
    Check(AddMem::CaptureForRect(image, AddPlan::Kind::Dummy, 0, line, value) == ClockResult::Ok && value.pool.capacity == 7500 &&
              value.plan.pool == AddPlan::PoolKind::Double,
          "actual typed Dummy separately selects the already qualified Cover DoubleLink schema");
    AddW::Put(doubles.Address(), 0x1f, static_cast<unsigned char>(0xa5));
    Check(AddMem::CaptureForRect(image, AddPlan::Kind::Dummy, 0, line, value) == ClockResult::Ok && value.pool.header[0x1f] == 0xa5,
          "unwritten pool-header padding is opaque and preserved rather than required zero");
    const auto head = image + value.plan.steps[0].headRva;
    doubles.flags[30] = 5;
    AddW::Put(doubles.Row(30), 0, entities.buildings.Row(0));
    AddW::Put(head, 0, doubles.Row(30));
    Check(AddMem::CaptureForRect(image, AddPlan::Kind::Dummy, 0, line, value) == ClockResult::Ok && value.heads[0].value == doubles.Row(30),
          "a current occupied native old-head node with previous0 is separately admitted before its future neighbour write");
    for (unsigned scenario = 0; scenario != 11; ++scenario)
    {
        reset();
        if (scenario == 0)
            world.Global(AddP::DoubleRootRva, std::uintptr_t{0});
        if (scenario == 1)
            AddW::Put(doubles.Address(), 0x10, std::int32_t{7499});
        if (scenario == 2)
            AddW::Put(doubles.Address(), 0x14, std::int32_t{7500});
        if (scenario == 3)
            AddW::Put(doubles.Address(), 8, doubles.Row(0));
        if (scenario == 4)
            AddW::Put(entities.dummies.Row(0), 0x20, entities.buildings.Row(0));
        if (scenario == 5)
            entities.dummies.flags[0] |= 0x80;
        if (scenario == 6)
            std::memset(doubles.flags, 0, AddP::DoubleCapacity);
        if (scenario == 7)
            AddW::Put(image + AddP::SectorsRva + (60 * 120 + 60) * 16 + 8, 0, doubles.Row(0));
        if (scenario == 8)
        {
            doubles.flags[30] = 3;
            AddW::Put(doubles.Row(30), 0x10, doubles.Row(31));
            AddW::Put(image + AddP::SectorsRva + (60 * 120 + 60) * 16 + 8, 0, doubles.Row(30));
        }
        if (scenario == 9)
            AddW::Put(entities.dummies.Row(0), 0, image + AddF::EntityP::BuildingVtable);
        if (scenario == 10)
            AddW::Put(doubles.Address(), 0x18, static_cast<unsigned char>(0));
        value.plan.cursorAfter = 0x1234;
        Check(AddMem::CaptureForRect(image, AddPlan::Kind::Dummy, 0, line, value) != ClockResult::Ok && value.plan.cursorAfter == 0x1234,
              "bad root/geometry/cursor/alias/actor/generation/exhaustion/old-head/type/owner refuses before output or any native call");
    }
    for (unsigned scenario = 0; scenario != 5; ++scenario)
    {
        reset();
        Check(AddMem::CaptureForRect(image, AddPlan::Kind::Building, 0, line, value) == ClockResult::Ok, "capture genuine budget before final raw mutation");
        if (scenario == 0)
            world.links.flags[500] ^= 1;
        if (scenario == 1)
            AddW::Put(world.links.Address(), 0x1f, static_cast<unsigned char>(1));
        if (scenario == 2)
            AddW::Put(entities.buildings.Row(0), 0x56, static_cast<unsigned char>(3));
        if (scenario == 3)
            AddW::Put(value.allocatedPrefix[0].address, 0, std::uintptr_t{0x1234});
        if (scenario == 4)
            AddW::Put(image + value.heads[0].rva, 0, world.links.Row(10));
        Check(!AddMem::Seal(image, value), "all flags/header opaque bytes/typed row/free prefix/sector head participate in callback-free raw bookends");
    }
    for (const auto& pin : AddP::Code)
    {
        reset();
        const auto byte = static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1);
        fixture.transform.image.Patch(pin.rva + pin.size - 1, &byte, 1, PAGE_EXECUTE_READ);
        value.plan.cursorAfter = 0x1234;
        Check(AddMem::CaptureForRect(image, AddPlan::Kind::Building, 0, line, value) == ClockResult::UnsupportedBuild && value.plan.cursorAfter == 0x1234,
              "each genuine forward body/allocator/publisher last-byte mutation refuses without output or engine entry");
    }
    std::printf("Passed %u world add budget checks (actual source/world/heap/typed pool memory; supplied rect only; no WorldAdd/native execution)\n", checks);
    return 0;
}
