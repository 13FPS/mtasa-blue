/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual read-only typed LOD descriptors plus pure hypothetical move plan
 ****************************************************************************/
#include "../../Client/game_de/NativeWorldLodRebasePlan.h"
#include "NativeWorldLodRebasePlanTests.inl"
namespace WorldLodRebaseOwnerFixture
{
    namespace LodTestW = NativeWorldIplFixture;
    namespace LodTestM = NativeWorldLodMemory;
    namespace LodTestPlan = NativeWorldLodRebasePlan;
    void Run(ClockImageFixture& fixture)
    {
        NativeWorldLodRebasePlanFixture::Run([](bool condition, const char* message) { Check(condition, message); });
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* value, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, value, bytes, protection); };
        const auto code = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        const auto typed = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        code(NativePedPoolProfile::Code);
        code(NativeModelBankProfile::Code);
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        LodTestW::State                      world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeWorldLodFixture::State         lods(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready() && lods.Ready(), "owned current typed Building/Dummy/Object LOD receivers are allocated");
        world.Reset();
        lods.Reset();
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "LOD controller preserves the shipping executable refusal before explicit owned-image test admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "LOD requests use the real current post-application Runtime");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "LOD work runs on the actual distinct admitted native engine thread");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one LOD action consumes one qualified engine callback");
        };

        LodTestM::Snapshot captured, refused;
        refused.identity = {0xf00d};
        Check(LodTestM::Capture(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, refused) != ClockResult::Ok &&
                  refused.identity == std::vector<std::uintptr_t>{0xf00d},
              "producer cannot borrow typed LOD admission and preserves prior output");
        pulse(
            [&](NativeGameTimingAccess&)
            {
                Check(LodTestM::Capture(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, captured) == ClockResult::Ok &&
                          captured.rows.size() == 5 && captured.representation == LodTestM::Representation::ResolvedPointers,
                      "actual current owner enumerates exactly occupied typed rows and qualified resolved64-bit targets");
                Check(!LodTestM::CompleteIplEntityArrayEnumeration && !NativeWorldLodProfile::FullIplArrayExtentsQualified,
                      "direct references do not manufacture per-IPL allocation lengths or complete resize readiness");
                const auto b0 = lods.entities.buildings.Row(0), b1 = lods.entities.buildings.Row(1), d0 = lods.entities.dummies.Row(0),
                           d1 = lods.entities.dummies.Row(1), object = lods.objects.Row(0);
                const auto current = [&](std::uintptr_t address) -> const LodTestM::Row&
                {
                    const auto row = std::find_if(captured.rows.begin(), captured.rows.end(), [&](const auto& item) { return item.address == address; });
                    Check(row != captured.rows.end(), "expected actual typed row is present");
                    return *row;
                };
                Check(current(b0).lod == b1 && current(b1).lod == 0 && current(d0).lod == d1 && current(d1).lod == b0 && current(object).lod == b1 &&
                          current(b1).lodChildren == 2,
                      "native QWORD LOD and independent BYTE child-count widths are observed separately");
                constexpr std::uintptr_t       destination = 0x20000000000ull;
                std::vector<LodTestPlan::Edit> edits;
                Check(LodTestPlan::Plan(captured, destination, 14000, edits) == ClockResult::Ok && edits.size() == 3,
                      "pure hypothetical move plans only pointers into the moved Building allocation");
                for (const auto& edit : edits)
                {
                    Check(edit.before == b0 || edit.before == b1, "cross-pool Dummy target is not shifted");
                    Check(edit.after == destination + static_cast<std::uintptr_t>(edit.targetIndex) * 0x70 && edit.after > 0xffffffffull,
                          "full-width index arithmetic preserves destinations beyond32bits without unsigned deltas");
                    const auto& owner = current(edit.sourceSlot - 0x60);
                    const auto  expectedCell = owner.pool == 0 ? destination + static_cast<std::uintptr_t>(owner.index) * 0x70 + 0x60 : owner.address + 0x60;
                    Check(edit.slot == expectedCell && edit.before == owner.lod,
                          "prospective Building cell rebases separately from its old source preimage while other owner cells stay put");
                }
                const auto saved = edits;
                const auto unchanged = [&]
                {
                    if (edits.size() != saved.size())
                        return false;
                    for (std::size_t i = 0; i < edits.size(); ++i)
                        if (edits[i].sourceSlot != saved[i].sourceSlot || edits[i].slot != saved[i].slot || edits[i].before != saved[i].before ||
                            edits[i].after != saved[i].after)
                            return false;
                    return true;
                };
                Check(LodTestPlan::Plan(captured, destination, 1, edits) != ClockResult::Ok && unchanged(),
                      "truncating a referenced target refuses the whole plan and preserves prior output");
                Check(LodTestPlan::Plan(captured, (std::numeric_limits<std::uintptr_t>::max)() - 7, 14000, edits) != ClockResult::Ok && unchanged(),
                      "destination extent overflow refuses without wraparound or partial edits");
                auto placement = captured;
                placement.representation = LodTestM::Representation::PlacementIndices;
                Check(LodTestPlan::Plan(placement, destination, 14000, edits) == ClockResult::UnsupportedBuild && unchanged(),
                      "raw placement-index phase cannot enter the resolved-pointer planner");
                Check(LodTestM::Recheck(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, captured) == ClockResult::Ok,
                      "planning leaves every actual native header, generation, row, LOD/count and current scene identity unchanged");
                LodTestW::Put(d0, 0x60, std::uintptr_t{7});
                Check(LodTestM::Capture(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, refused) != ClockResult::Ok &&
                          refused.identity == std::vector<std::uintptr_t>{0xf00d},
                      "small placement-like index cannot masquerade as a current resolved target");
                LodTestW::Put(d0, 0x60, d1);
                lods.entities.buildings.flags[1] |= 0x80;
                Check(LodTestM::Capture(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, refused) != ClockResult::Ok,
                      "reference to an unoccupied former target refuses before producing a plan");
                lods.entities.buildings.flags[1] = 2;
                LodTestW::Put(b1, 0x68, static_cast<unsigned char>(3));
                Check(LodTestM::Recheck(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, captured) != ClockResult::Ok,
                      "child-count observation is byte-exact and cannot be silently refreshed into a prior plan");
                LodTestW::Put(b1, 0x68, static_cast<unsigned char>(2));
                Check(LodTestM::Recheck(fixture.image, &CheckTimingOwner, &CheckApplicationTimingOwner, captured) == ClockResult::Ok,
                      "restored current observation agrees after the bounded negative cases");
            });
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation), "read-only LOD enumeration and CPU planning leave no native retention debt");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production typed LOD snapshot and planner completed");
    }
}
