/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual grant admission and readers with only the final native entry replaced
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeGrantHistoricalLoadMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativePedWeaponGrantFixture.h"
#include "../../Client/game_de/NativePedWeaponGrantCalls.h"
namespace mta::game_de::NativeImageAccess
{
    // This seam qualifies only our already-created owned SEC_IMAGE, never an
    // external executable or an invented reader. Bind/Read remain real code.
    bool GrantFixtureExecutable() noexcept
    {
        return PoolTestHost::image != 0;
    }
}
namespace
{
    namespace WG = mta::game_de::NativePedWeaponGrantCalls;
    ClockResult GrantEntry(WG::NativeFunction, std::uintptr_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t&, std::uint32_t&) noexcept;
}
#define GetModuleHandleW     PoolTestHost::Module
#define ReadProcessMemory    PoolTestHost::Read
#define CheckExecutableBuild GrantFixtureExecutable
#include "../../Client/game_de/NativeWeaponInfoReader.cpp"
#undef CheckExecutableBuild
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#include "../../Client/game_de/NativePedWeaponContext.cpp"
#define MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE ::GrantEntry
#include "../../Client/game_de/NativePedWeaponGrantCalls.cpp"
#undef MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace GP = NativePedWeaponGrantProfile;
    namespace WI = NativeWeaponInfoProfile;
    struct GrantFixture : LoadFixture
    {
        NativeWeaponInfoReader reader;
        explicit GrantFixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        template <class T>
        void PedAt(std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + offset, &value, sizeof(value));
        }
        void Reset()
        {
            PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
            PoolTestHost::ownerResult = ClockResult::Ok;
            PoolTestHost::watched = 0;
            PoolTestHost::after = nullptr;
            std::memset(transform.pool.objects, 0, NativePedPoolProfile::ObjectStride);
            LoadFixture::Reset();
            NativePedWeaponGrantFixture::Install(transform.image.image, &LoadFixture::Write, this);
            PedAt(0, transform.image.image + NativePedMetadataProfile::PedVtableRva);
            PedAt(NativePedMetadataProfile::ModelOffset, std::uint16_t{17});
            PedAt(NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(3));
            PedAt(GP::CurrentSlotOffset, static_cast<signed char>(2));
            PedAt(0x7f0 + 2 * 0x28, std::uint32_t{23});
            PoolTestHost::image = transform.image.image;
            reader.Reset();
            Check(reader.Bind() == Status::Ok, "actual weapon-info reader binds the owned image and calling thread");
        }
        void Info(std::uint32_t index, std::int32_t slot, std::int32_t model = -1, std::int32_t model2 = -1)
        {
            NativeWeaponInfoValues value;
            value.model = model;
            value.model2 = model2;
            value.slot = slot;
            value.clipAmmo = 17;
            NativePedWeaponGrantFixture::Info(&LoadFixture::Write, this, index, value);
        }
        void Model(std::uint32_t index, std::uintptr_t nativeModel, bool loaded)
        {
            NativePedWeaponGrantFixture::Model(&LoadFixture::Write, this, index, nativeModel, loaded);
        }
        WG::Ped Ped()
        {
            WG::Ped    ped;
            const auto status = NativePedPoolReader({transform.image.image}, &PoolTestHost::Owner).Resolve(1, ped);
            if (status != ClockResult::Ok)
            {
                MEMORY_BASIC_INFORMATION page{};
                VirtualQuery(reinterpret_cast<void*>(transform.image.image + 0x10d86da), &page, sizeof(page));
                std::fprintf(stderr, "grant fixture Resolve status=%u code10d86da.protection=%08lx flag0=%02x\n", static_cast<unsigned>(status), page.Protect,
                             static_cast<unsigned>(transform.pool.flags[0]));
            }
            Check(status == ClockResult::Ok, "grant borrows current occupied receiver");
            return ped;
        }
    };
    GrantFixture* activeGrant = nullptr;
    WG::Ped*      mutableGrantCaller = nullptr;
    unsigned      grantCalls = 0, grantEffects = 0, grantScenario = 0;
    std::uint32_t expectedType = 22, expectedAmmo = 0, nativeSlot = 2;
    ClockResult   nestedGrant = ClockResult::Ok, nestedLoad = ClockResult::Ok;
    bool          nestedMutation = true;
    ClockResult   MutatingGrantOwner() noexcept
    {
        if (mutableGrantCaller)
            *mutableGrantCaller = {};
        return PoolTestHost::Owner();
    }
    ClockResult GrantEntry(WG::NativeFunction function, std::uintptr_t ped, std::uint32_t type, std::uint32_t ammo, std::int32_t unused, std::uint32_t& slot,
                           std::uint32_t& exception) noexcept
    {
        auto& f = *activeGrant;
        Check(reinterpret_cast<std::uintptr_t>(function) == f.transform.image.image + GP::EntryRva &&
                  ped == reinterpret_cast<std::uintptr_t>(f.transform.pool.objects) && type == expectedType && ammo == expectedAmmo && unused == 1,
              "one exact four-argument entry receives copied receiver/full type/raw ammo and unused SDK one");
        ++grantCalls;
        ++grantEffects;
        f.application[0x79] = static_cast<unsigned char>(grantEffects);
        slot = nativeSlot;
        if (grantScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (grantScenario == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (grantScenario == 3)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (grantScenario == 4)
            f.Put(f.graph.items.data(), 0x10, std::uint32_t{999});
        if (grantScenario == 5)
            f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (grantScenario == 6)
            f.reader.Reset();
        if (grantScenario == 7)
            f.transform.pool.flags[0] = 0x80;
        if (grantScenario == 8)
        {
            const auto& pin = GP::Pins[0];
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (grantScenario == 9)
        {
            // Genuine effects may replace row/current selection, retire the
            // old model, change refcounts/actors and enter a different world
            // mode. None is stable authority that postflight may freeze.
            f.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{22});
            f.PedAt(GP::CurrentSlotOffset, static_cast<signed char>(4));
            f.PedAt(0x700, std::uintptr_t{0x12345678});
            f.PedAt(0x720, std::uintptr_t{0});
            f.Model(100, 0, false);
            f.Model(101, 0x9871, true);
            f.graph.world[LPF::WorldModeOffset] = 1;
        }
        if (grantScenario == 10)
        {
            auto          receiver = f.Ped();
            std::uint32_t ignored = 0;
            WG::Receipt   nested;
            nestedGrant = WG::Grant(f.transform.image.image, receiver, f.reader, 22, 0, ignored, &PoolTestHost::Owner, &nested);
            Check(!nested.entered, "recursive grant cannot borrow outer native admission");
            LC::Receipt load;
            nestedLoad = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &load);
            NativePedWeaponContext::Mutation nestedGrantMutation;
            nestedMutation = static_cast<bool>(nestedGrantMutation);
        }
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "grant evidence stays in explicit workspace path");
    GrantFixture fixture(argv[1]);
    activeGrant = &fixture;
    const auto    image = fixture.transform.image.image;
    WG::Receipt   receipt;
    std::uint32_t output = 0xdeadbeef;
    const auto    reset = [&]
    {
        fixture.Reset();
        grantCalls = grantEffects = grantScenario = 0;
        expectedType = 22;
        expectedAmmo = 0;
        nativeSlot = 2;
        mutableGrantCaller = nullptr;
        nestedGrant = nestedLoad = ClockResult::Ok;
        nestedMutation = true;
        output = 0xdeadbeef;
    };
    reset();
    auto ped = fixture.Ped();
    Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && output == 0xdeadbeef,
          "raw receiver and bound bank do not provide owner authority");
    for (const auto ammo :
         {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{99999}, std::uint32_t{100000}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        reset();
        ped = fixture.Ped();
        expectedAmmo = ammo;
        nativeSlot = 0xf1234567;
        Check(WG::Grant(image, ped, fixture.reader, 22, ammo, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && grantCalls == 1 &&
                  receipt.entered && receipt.returned && output == nativeSlot && receipt.rawSlot == nativeSlot && receipt.ammo == ammo &&
                  receipt.world == fixture.graph.World(),
              "raw ammo and raw native slot survive one call without adapter saturation or equip");
    }
    reset();
    ped = fixture.Ped();
    fixture.Info(22, -1, 20001, 20002);
    fixture.PedAt(GP::CurrentSlotOffset, static_cast<signed char>(-1));
    nativeSlot = 0;
    Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == 0 && grantCalls == 1 &&
              receipt.returned,
          "STD slot -1 preserves genuine early0 without reading unused inline rows or requiring unused models");
    for (unsigned failure = 0; failure < 12; ++failure)
    {
        reset();
        ped = fixture.Ped();
        switch (failure)
        {
            case 0:
                fixture.reader.Reset();
                break;
            case 1:
                fixture.Info(22, 13);
                break;
            case 2:
                fixture.PedAt(0x7f0 + 2 * 0x28, std::uint32_t{0xffff});
                break;
            case 3:
                fixture.PedAt(GP::CurrentSlotOffset, static_cast<signed char>(-1));
                break;
            case 4:
                fixture.Info(22, 2, 100);
                fixture.Model(100, 0, true);
                break;
            case 5:
                fixture.Info(22, 2, 100);
                fixture.Model(100, 0x1231, false);
                break;
            case 6:
                fixture.Info(22 + 47, 2, 101);
                fixture.Model(101, 0x1231, false);
                break;
            case 7:
                fixture.Info(47, 2, 102);
                fixture.Model(102, 0x1231, false);
                break;
            case 8:
                fixture.Info(23 + 36, 2, 103);
                fixture.Model(103, 0x1231, false);
                break;
            case 9:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 10:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 11:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
        }
        Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !grantCalls &&
                  output == 0xdeadbeef,
              "unbound reader, unsafe native indices and missing current world/preloaded old/new skill models refuse before entry");
    }
    for (const auto type : {std::uint32_t{47}, std::uint32_t{0x10016}, std::uint32_t{0xffff}, std::uint32_t{0xffffffff}})
    {
        reset();
        ped = fixture.Ped();
        Check(WG::Grant(image, ped, fixture.reader, type, 0, output, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered &&
                  !grantCalls,
              "unsupported full type cannot alias a low WORD bank row");
    }
    for (unsigned scenario = 1; scenario <= 8; ++scenario)
    {
        reset();
        ped = fixture.Ped();
        grantScenario = scenario;
        Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && grantCalls == 1 &&
                  grantEffects == 1 && fixture.application[0x79] == 1 && output == 0xdeadbeef && receipt.returned == (scenario != 1),
              "entered fault or lost ped/world/bank/owner/provenance keeps effects and output isolation without replay or rollback");
        if (scenario == 1)
            Check(receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION, "exact native fault identity survives");
    }
    reset();
    ped = fixture.Ped();
    fixture.Info(23, 2, 100);
    fixture.Info(22, 2, 101);
    fixture.Model(100, 0x1231, true);
    fixture.Model(101, 0x9871, true);
    grantScenario = 9;
    Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == 2 && grantCalls == 1 &&
              fixture.Stream(100)[1] == 0 && fixture.graph.world[LPF::WorldModeOffset] == 1,
          "legitimate old release/new attachment/row/current-slot transitions acknowledge stable authority and preserve native effects");
    reset();
    ped = fixture.Ped();
    grantScenario = 10;
    Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && nestedGrant == ClockResult::Busy &&
              nestedLoad == ClockResult::Busy && !nestedMutation && grantCalls == 1,
          "shared native scope and weapon mutation scope exclude synchronous reentry");
    grantScenario = 0;
    Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && grantCalls == 2,
          "both scopes release after normal completion");
    reset();
    ped = fixture.Ped();
    const auto original = ped;
    mutableGrantCaller = &ped;
    Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &MutatingGrantOwner, &receipt) == ClockResult::Ok && !ped.nativePed &&
              receipt.nativePed == original.nativePed,
          "resolved receiver is copied before callbacks mutate the source value");
    reset();
    ped = fixture.Ped();
    std::thread foreign(
        [&]
        {
            Check(WG::Grant(image, ped, fixture.reader, 22, 0, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !grantCalls,
                  "foreign thread cannot borrow bound weapon bank or native GT authority");
        });
    foreign.join();
    std::printf("Passed %u native ped-weapon-grant checks (64-bit; no native engine code executed)\n", checks);
    std::puts("Production native ped weapon grant operation fixture completed");
    return 0;
}
