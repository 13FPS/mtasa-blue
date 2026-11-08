/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real IPL memory qualification with explicit controlled native calls
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldIplHistoricalLoadAllMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldIplFixture.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldIplMemory.cpp"
#include "../../Client/game_de/NativeWorldIplCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace WorldIplTest
{
    namespace W = NativeWorldIplFixture;
    namespace M = mta::game_de::NativeWorldIplMemory;
    namespace Calls = mta::game_de::NativeWorldIplCalls;
    namespace Plan = mta::game_de::NativeWorldIplPlan;
    W::State*                    state = nullptr;
    Calls::Controller*           controller = nullptr;
    unsigned                     removes = 0, adds = 0, clears = 0, mode = 0, filters = 0;
    ClockResult                  nested = ClockResult::NotInitialized;
    std::array<unsigned char, 8> copiedName{};
    unsigned                     afterCalls = 0;
    std::uintptr_t               lateWorldField = 0;
    ClockResult                  LateOwner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (result == ClockResult::Ok && ++afterCalls == 2)
            W::Put(lateWorldField, 0, std::uintptr_t{0});
        return result;
    }
    ClockResult Remove(Calls::RemoveFunction entry, std::int32_t index, std::uint32_t& exception) noexcept
    {
        ++removes;
        Check(reinterpret_cast<std::uintptr_t>(entry) == state->image + W::P::RemoveAndIgnoreRva && index > 0 && index < 256,
              "controlled remove receives only the pinned whole function and nongeneric native index");
        // This is explicitly an entry double: production memory qualification,
        // owner checks, snapshots, state0 admission and receipts stay genuine.
        W::Put(state->ipl.Row(index), W::P::DisabledOffset, std::uint16_t{1});
        if (mode == 1)
        {
            exception = 0xe0510001;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            state->Stream(2, 1);
        return ClockResult::Ok;
    }
    ClockResult Add(Calls::AddFunction entry, void* tree, void* row, const void* bounds, std::uint32_t& exception) noexcept
    {
        ++adds;
        Check(reinterpret_cast<std::uintptr_t>(entry) == state->image + W::P::QuadAddRva && reinterpret_cast<std::uintptr_t>(tree) == state->quad.Row(0) &&
                  row == bounds,
              "controlled leaf Add receives the genuine native tree and same native row/bounds address");
        std::size_t index = 0;
        while (index < 75000 && !(state->links.flags[index] & 0x80))
            ++index;
        Check(index < 75000, "exact production budget supplies one free native single-link slot");
        std::uintptr_t old = 0;
        std::memcpy(&old, reinterpret_cast<void*>(state->quad.Row(0) + 0x10), sizeof(old));
        W::Put(state->links.Row(index), 0, reinterpret_cast<std::uintptr_t>(row));
        W::Put(state->links.Row(index), 8, old);
        state->links.flags[index] = 1;
        W::Put(state->quad.Row(0), 0x10, state->links.Row(index));
        if (mode == 3)
        {
            exception = 0xe0510003;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
    ClockResult Clear(Calls::ClearFunction entry, void* head, std::uint32_t&) noexcept
    {
        ++clears;
        Check(reinterpret_cast<std::uintptr_t>(entry) == state->image + W::P::SingleClearRva &&
                  reinterpret_cast<std::uintptr_t>(head) == state->quad.Row(0) + 0x10,
              "controlled clear receives the genuine native leaf-head address");
        std::uintptr_t at = 0;
        std::memcpy(&at, head, sizeof(at));
        while (at)
        {
            const auto slot = (at - state->links.Row(0)) / 16;
            std::memcpy(&at, reinterpret_cast<void*>(at + 8), sizeof(at));
            state->links.flags[slot] |= 0x80;
        }
        W::Put(reinterpret_cast<std::uintptr_t>(head), 0, std::uintptr_t{0});
        return ClockResult::Ok;
    }
    bool Filter(const MtaGame::CGameWorldPolicy::IplName& name) noexcept
    {
        ++filters;
        if (name[0] == 't')
            copiedName = name;
        if (mode == 4)
            W::Put(state->ipl.Row(1), W::P::IplNameOffset, std::uint64_t{0});
        if (mode == 5)
            state->Stream(1, 3);
        if (mode == 6)
            nested = controller->Enable(state->image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &Filter);
        if (mode == 7)
            state->Global(W::P::IplPoolRva, std::uintptr_t{0});
        // Match CGameSA's existing barriers policy; the full eight bytes are
        // copied and may have no terminator. Generic index zero never arrives.
        return std::memcmp(name.data(), "barriers", 8) != 0;
    }
    constexpr Calls::Invocations invokes{&Remove, &Add, &Clear};
    void                         Plans()
    {
        Plan::Node       root{{-8, 8, 8, -8}, 1, {{-1, -1, -1, -1}}};
        Plan::Budget     budget{99, 88};
        const Plan::Rect centre{0, 0, 0, 0};
        Check(Plan::Require({root}, {centre}, 4, 4, budget) == ClockResult::Ok && budget.quadNodes == 4 && budget.links == 4,
              "inclusive midpoint overlap requires all four native quadrants");
        budget = {99, 88};
        Check(Plan::Require({root}, {centre}, 3, 4, budget) == ClockResult::NotInitialized && budget.quadNodes == 99 && budget.links == 88,
              "quad allocation exhaustion refuses before any output or engine allocation");
        Check(Plan::Require({root}, {centre}, 4, 3, budget) == ClockResult::NotInitialized && budget.quadNodes == 99,
              "single-link exhaustion refuses the whole operation before flag stores");
        Check(Plan::Require({root}, {centre, centre}, 4, 8, budget) == ClockResult::Ok && budget.quadNodes == 4 && budget.links == 8,
              "later selected rows share newly required nodes but consume independent leaf links");
        Plan::Node leaf{{-8, 8, 8, -8}, 0, {{-1, -1, -1, -1}}};
        Check(Plan::Require({leaf}, {{100, 101, 101, 100}}, 0, 1, budget) == ClockResult::Ok && !budget.quadNodes && budget.links == 1,
              "native level-zero adds unconditionally, even outside the root rectangle");
        auto invalid = root;
        invalid.child[0] = 1;
        Check(Plan::Require({invalid, leaf}, {centre}, 10, 10, budget) == ClockResult::BindingChanged,
              "a child with incorrect level/bounds is never normalized into a valid native graph");
        Check(Plan::Require({root, leaf}, {centre}, 10, 10, budget) == ClockResult::BindingChanged,
              "disconnected allocated nodes cannot enter the exact current-tree plan");
    }
}

int main(int argc, char** argv)
{
    using namespace WorldIplTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "IPL evidence uses one workspace-owned explicit directory");
    Plans();
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    W::State data(image, &LoadFixture::Write, &fixture);
    Check(data.Ready(), "own exact-capacity native IPL, quadtree and list allocations");
    state = &data;
    const auto reset = [&]
    {
        fixture.Reset();
        data.Reset();
        removes = adds = clears = filters = mode = 0;
        nested = ClockResult::NotInitialized;
        copiedName = {};
    };
    reset();
    Check(M::Profile(image), "whole code pins, real bank/heap/source profiles and readonly midpoint constant qualify");
    M::Snapshot snapshot;
    Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::Ok && snapshot.rows.size() == 3 &&
              snapshot.nodes.size() == 1 && snapshot.list.empty() && snapshot.ipl.free == 253 && snapshot.quad.free == 399 && snapshot.links.free == 75000,
          "actual memory capture validates exact pool layouts, allocation generations, typed world and current empty leaf");
    reset();
    lateWorldField = reinterpret_cast<std::uintptr_t>(fixture.application) + 0x20;
    afterCalls = 0;
    snapshot.tree = 0xdeadbeef;
    Check(M::Capture(image, &PoolTestHost::Owner, &LateOwner, snapshot, true) == ClockResult::BindingChanged && snapshot.tree == 0xdeadbeef,
          "last owner graph-field edit fails a fresh callback-free typed-world comparison, even with unchanged UObject membership");
    for (const auto& pin : W::P::Code)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1);
        fixture.transform.image.Patch(pin.rva + pin.size - 1, &bad, 1, PAGE_EXECUTE_READ);
        snapshot.tree = 0xdeadbeef;
        Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::UnsupportedBuild && snapshot.tree == 0xdeadbeef,
              "whole native IPL/tree code pin mutations refuse before output or effects, including the last logical fragment");
    }
    for (const auto value : {1, 2, 3, 4, 255})
    {
        reset();
        data.Stream(2, static_cast<unsigned char>(value));
        snapshot.tree = 0xdeadbeef;
        Calls::Controller c;
        Calls::Receipt    receipt;
        Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::UnsupportedBuild && snapshot.tree == 0xdeadbeef,
              "loaded/pending IPL refusal preserves the caller snapshot without admitting its entity destructor graph");
        Check(c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::UnsupportedBuild && !receipt.entered &&
                  !removes && !clears && !c.Retained() && data.ipl.objects[0x34 + 0x2f] == 0 && data.ipl.objects[0x34 + 0x30] == 0x5a &&
                  c.End(1) == ClockResult::Ok,
              "a later loaded row refuses the entire native Disable before the first earlier flag/tree effect");
    }
    for (unsigned scenario = 0; scenario != 6; ++scenario)
    {
        reset();
        if (scenario == 0)
            W::Put(data.ipl.Address(), 0x10, std::int32_t{255});
        if (scenario == 1)
            data.quad.flags[0] |= 0x80;
        if (scenario == 2)
            W::Put(data.quad.Address(), 8, reinterpret_cast<std::uintptr_t>(data.ipl.flags));
        if (scenario == 3)
            W::Put(data.quad.Row(0), 0x18, data.quad.Row(0));
        if (scenario == 4)
        {
            data.List(1);
            W::Put(data.links.Row(0), 8, data.links.Row(0));
        }
        if (scenario == 5)
        {
            data.List(1);
            data.ipl.flags[1] |= 0x80;
        }
        snapshot.tree = 0xdeadbeef;
        Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) != ClockResult::Ok && snapshot.tree == 0xdeadbeef,
              "mutated capacity, allocated flags, cross-pool alias, child/list cycles and free IPL list members refuse unchanged output");
    }
    reset();
    data.List(2);
    {
        Calls::Controller c;
        controller = &c;
        Calls::Receipt receipt;
        Check(c.End(0) == ClockResult::Ok && c.Ready(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner) == ClockResult::Ok,
              "unused End is idempotent and real bank readiness borrows the actual callback context");
        Check(c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::Ok && removes == 2 && clears == 1 &&
                  receipt.removeAcks == 2 && receipt.clearAcks == 1 && receipt.entered == 3 && receipt.returned == 3 && c.Retained() &&
                  data.ipl.objects[0x2f] == 0 && data.ipl.objects[0x30] == 0x5a && c.End(1) == ClockResult::Busy && c.End(2) == ClockResult::BindingChanged,
              "safe state0 whole Disable skips generic, acknowledges exact word stores/clear, and retains real disabled lifetime");
        data.Stream(1, 1);
        data.Stream(2, 1);
        mode = 6;
        Check(c.Enable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &Filter, &receipt, &invokes) == ClockResult::Ok && filters == 2 &&
                  copiedName == std::array<unsigned char, 8>{'t', 'e', 's', 't', 'a', 'b', 'c', 'd'} && nested == ClockResult::Busy && receipt.selected == 1 &&
                  receipt.flagStores == 1 && receipt.addAcks == 1 && receipt.entered == 2 && receipt.returned == 1 && data.ipl.objects[0x34 + 0x2f] == 0 &&
                  data.ipl.objects[2 * 0x34 + 0x2f] == 1 && !c.Retained() && c.End(1) == ClockResult::Ok && c.End(1) == ClockResult::Ok,
              "actual Enable admits loaded rows without removal, copies exact names, honors barriers filter, adds native membership and retires cleanly");
    }
    for (const auto selectedMode : {4, 5, 7})
    {
        reset();
        Calls::Controller c;
        controller = &c;
        Calls::Receipt receipt;
        Check(c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::Ok, "prepare an actual disabled bank");
        mode = selectedMode;
        const auto oldRemoves = removes;
        Check(c.Enable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &Filter, &receipt, &invokes) == ClockResult::BindingChanged && !receipt.entered &&
                  !adds && c.Retained() && removes == oldRemoves && data.ipl.objects[0x34 + 0x2f] == 1,
              "filter edits to raw name, stream row or root fail the callback-free seal before the first enabling store");
    }
    for (const auto selectedMode : {1, 2, 3})
    {
        reset();
        Calls::Controller c;
        controller = &c;
        Calls::Receipt receipt;
        if (selectedMode == 3)
            Check(c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::Ok, "prepare one uncertain Add prefix");
        mode = selectedMode;
        const auto result = selectedMode == 3 ? c.Enable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &Filter, &receipt, &invokes)
                                              : c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes);
        Check(result != ClockResult::Ok && receipt.entered && receipt.unknown && c.Retained() && c.End(1) == ClockResult::Busy,
              "exception or postentry state change retains the accepted unknown prefix with no invented rollback");
        const auto entries = removes + adds + clears;
        Check(c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::OwnerConflict &&
                  c.Ready(image, 2, &PoolTestHost::Owner, &PoolTestHost::Owner) == ClockResult::OwnerConflict && entries == removes + adds + clears,
              "unknown work cannot replay on the same generation or disappear into a replacement generation");
    }
    reset();
    {
        Calls::Controller c;
        controller = &c;
        Calls::Receipt receipt;
        Check(c.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::Ok, "prepare an allocation budget refusal");
        std::memset(data.links.flags, 1, 75000);
        Check(c.Enable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &Filter, &receipt, &invokes) == ClockResult::NotInitialized && !receipt.entered &&
                  !adds && data.ipl.objects[0x34 + 0x2f] == 1 && c.Retained(),
              "full native list pool refuses before the enabling byte, preserving known disabled state");
    }
    std::printf("Passed %u IPL checks (actual owned memory/profile/controller; explicit remove/add/clear entry doubles; no GTA native execution)\n", checks);
    return 0;
}
