/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine loaded-IPL memory/controller; final original entry doubles
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldIplLoadedHistoricalState0Main
#include "NativeWorldIplTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldIplLoadedFixture.h"
namespace LoadedIplTest
{
    namespace L = NativeWorldIplLoadedFixture;
    namespace LoadedProfile = mta::game_de::NativeWorldIplLoadedProfile;
    namespace M = mta::game_de::NativeWorldIplMemory;
    namespace Calls = mta::game_de::NativeWorldIplCalls;
    namespace C = mta::game_de::NativeStreamingCompletionContext;
    namespace Loaded = mta::game_de::NativeWorldIplLoadedMemory;
    namespace W = NativeWorldIplFixture;
    L::State*   active = nullptr;
    unsigned    entries = 0, mode = 0;
    ClockResult Remove(Calls::RemoveFunction entry, std::int32_t index, std::uint32_t& exception) noexcept
    {
        ++entries;
        Check(reinterpret_cast<std::uintptr_t>(entry) == active->image + W::P::RemoveAndIgnoreRva && index > 0 && index < 256,
              "loaded original double receives exact whole native RemoveAndIgnore and allocated nongeneric index");
        // This double acknowledges only the effects the real controller must
        // observe. It never executes UE mobility, renderer or destructor code.
        if (mode == 3)
        {
            exception = 0xe0530003;
            return ClockResult::OriginalException;
        }
        active->Acknowledge(static_cast<std::uint32_t>(index), mode != 1);
        if (mode == 2)
            active->ipl.Stream(static_cast<std::uint32_t>(index), 1);
        if (mode == 4)
            W::Put(active->Items(), 12 * 24 + 0x10, std::uint32_t{999});
        if (mode == 5)
            active->Global(LoadedProfile::ActorClass, active->Class(1));
        return ClockResult::Ok;
    }
    constexpr Calls::Invocations invokes{&Remove, &WorldIplTest::Add, &WorldIplTest::Clear};
    unsigned                     afterCalls = 0;
    ClockResult                  LateOwner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (result == ClockResult::Ok && ++afterCalls == 2)
            W::Put(active->Actor(), 0x130, std::uintptr_t{0});
        return result;
    }
}
int main(int argc, char** argv)
{
    using namespace LoadedIplTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "loaded IPL uses explicit workspace-owned evidence directory");
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    W::State data(image, &LoadFixture::Write, &fixture);
    L::State loaded(image, reinterpret_cast<std::uintptr_t>(fixture.application), reinterpret_cast<std::uintptr_t>(fixture.provider), &LoadFixture::Write,
                    &fixture, data);
    Check(data.Ready() && loaded.Ready(), "own real-capacity native building/dummy/object and IPL/tree allocations");
    active = &loaded;
    WorldIplTest::state = &data;
    const auto reset = [&](bool dynamic = false)
    {
        fixture.Reset();
        data.Reset();
        loaded.Reset(fixture.graph, dynamic);
        entries = mode = afterCalls = 0;
        WorldIplTest::mode = WorldIplTest::removes = WorldIplTest::adds = WorldIplTest::clears = WorldIplTest::filters = 0;
    };
    reset();
    M::Snapshot snapshot;
    Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::Ok && snapshot.loaded.pools.size() == 3 &&
              snapshot.loaded.deleted.size() == 1 && !snapshot.loaded.objects.empty(),
          "actual Memory admits the current typed building/StaticMeshActor root graph and real sparse model map before any original call");
    Check(entries == 0 && M::Seal(image, snapshot), "loaded admission and raw bookend execute no native original");
    Check(snapshot.loaded.tokens.size() < 200, "one selected row captures shared flags and small current graph, not every pool row per IPL");
    reset(true);
    loaded.WithChild();
    Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::Ok,
          "genuine DynamicIPLMapActor and constructor-owned component child chain qualify under ordinary UE API invariants");
    reset();
    loaded.WithRelatedDummy();
    Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::Ok && snapshot.loaded.deleted.size() == 2,
          "plain Object related+200 is an allocated typed Dummy with same model/IPL and genuine reciprocal CEntity reference slot");
    for (unsigned scenario = 0; scenario != 21; ++scenario)
    {
        reset();
        if (scenario == 0)
            W::Put(loaded.Actor(), 0x130, std::uintptr_t{0});
        if (scenario == 1)
            W::Put(loaded.Actor(), 0x298, loaded.Child());
        if (scenario == 2)
            W::Put(loaded.Component(), 0x10, loaded.Class(0));
        if (scenario == 3)
            W::Put(loaded.Component(), 0x20, fixture.graph.Game());
        if (scenario == 4)
            W::Put(loaded.Items(), 16 * 24 + 8, std::uint32_t{0x10000000});
        if (scenario == 5)
            W::Put(loaded.buildings.Address(), 0x10, std::int32_t{LoadedProfile::BuildingCapacity - 1});
        if (scenario == 6)
            W::Put(data.ipl.Row(1), 0x24, std::int16_t{LoadedProfile::BuildingCapacity});
        if (scenario == 7)
        {
            loaded.WithChild();
            W::Put(loaded.Child(), 0xc0, loaded.Child());
        }
        if (scenario == 8)
        {
            loaded.WithChild();
            loaded.children[0] = loaded.Component();
        }
        if (scenario == 9)
            loaded.mapRows[0].count = 0;
        if (scenario == 10)
            W::Put(loaded.buildings.Row(0), 0x60, loaded.buildings.Row(0));
        if (scenario == 11)
            W::Put(loaded.buildings.Row(0), 0, std::uintptr_t{image + LoadedProfile::ObjectVtable});
        if (scenario == 12)
        {
            loaded.WithRelatedDummy();
            W::Put(loaded.objects.Row(0), 0x1f0, loaded.Actor());
        }
        if (scenario == 13)
        {
            loaded.WithRelatedDummy();
            loaded.reference[1] = loaded.objects.Row(0) + 0x208;
        }
        if (scenario == 14)
        {
            loaded.WithRelatedDummy();
            loaded.dummies.flags[0] |= 0x80;
        }
        if (scenario == 15)
            W::Put(loaded.Actor(), 0x5c, static_cast<unsigned char>(8));
        if (scenario == 16)
            W::Put(loaded.Actor(), 8, std::uint32_t{0x10});
        if (scenario == 17)
            W::Put(loaded.buildings.Row(0), 0x34, static_cast<unsigned char>(0x20));
        if (scenario == 18)
            W::Put(loaded.Component(), 0x14f, static_cast<unsigned char>(3));
        if (scenario == 19)
        {
            loaded.WithRelatedDummy();
            W::Put(loaded.objects.Row(0), 0x1b0, loaded.objects.Row(1));
        }
        if (scenario == 20)
            loaded.chains[1][0] = loaded.Class(2) + 0x30;
        snapshot.tree = 0xdeadbeef;
        Calls::Controller controller;
        Calls::Receipt    receipt;
        Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) != ClockResult::Ok && snapshot.tree == 0xdeadbeef,
              "invalid typed actor/component/registry/pool/range/map/LOD/optionalFX/related graph refuses unchanged snapshot");
        Check(controller.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) != ClockResult::Ok && !receipt.entered && !entries &&
                  !controller.Retained() && data.ipl.objects[0x34 + W::P::DisabledOffset] == 0 && controller.End(1) == ClockResult::Ok,
              "all selected loaded graphs preflight before the first earlier IPL flag or native removal entry");
    }
    for (const auto& pin : LoadedProfile::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1);
        fixture.transform.image.Patch(pin.rva + pin.size - 1, &bad, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        snapshot.tree = 0xdeadbeef;
        Check(M::Capture(image, &PoolTestHost::Owner, &PoolTestHost::Owner, snapshot, true) == ClockResult::UnsupportedBuild && snapshot.tree == 0xdeadbeef &&
                  !entries,
              "every exact loaded producer/whole destructor/mobility/UE API final pin byte is required before mutation");
    }
    reset();
    snapshot.tree = 0xdeadbeef;
    Check(M::Capture(image, &PoolTestHost::Owner, &LateOwner, snapshot, true) == ClockResult::BindingChanged && snapshot.tree == 0xdeadbeef && !entries,
          "last owner actor-root edit fails the callback-free loaded graph seal");
    for (unsigned graph = 0; graph != 3; ++graph)
    {
        reset(graph == 1);
        if (graph == 1)
            loaded.WithChild();
        if (graph == 2)
            loaded.WithRelatedDummy();
        Calls::Controller controller;
        Calls::Receipt    receipt;
        Check(controller.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::Ok && receipt.removeAcks == 2 &&
                  receipt.returned == 2 && entries == 2 && controller.Retained() && controller.End(1) == ClockResult::Busy,
              "actual loaded Disable requires stream/IPL flag and generation-specific native slot release acknowledgements, then retains known disabled work");
        Check(controller.Enable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &WorldIplTest::Filter, &receipt, &invokes) == ClockResult::Ok &&
                  !controller.Retained() && controller.End(1) == ClockResult::Ok,
              "existing genuine copied-name filter/CAS/tree enable retires explicit loaded removal without automatic rollback");
    }
    for (unsigned failure = 1; failure != 6; ++failure)
    {
        reset();
        mode = failure;
        Calls::Controller controller;
        Calls::Receipt    receipt;
        Check(controller.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) != ClockResult::Ok && receipt.entered == 1 &&
                  receipt.unknown && controller.Retained() && controller.End(1) == ClockResult::Busy,
              "entered missing slot/stream acknowledgement, surviving class identity loss or original exception retains unknown without inventing successful "
              "destruction");
        const auto prior = entries;
        Check(controller.Disable(image, 1, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &invokes) == ClockResult::OwnerConflict &&
                  controller.Ready(image, 2, &PoolTestHost::Owner, &PoolTestHost::Owner) == ClockResult::OwnerConflict && entries == prior,
              "entered loaded prefix neither replays nor disappears into a replacement owner generation");
    }
    std::printf("Passed %u loaded IPL checks (actual memory/controller; final original entries explicit doubles; no GTA/native UE execution)\n", checks);
    return 0;
}
