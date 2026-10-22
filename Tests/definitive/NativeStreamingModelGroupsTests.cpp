/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual native group observation and MTA selection over owned OS pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelGroupsHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingModelGroupsReader.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeStreamingModelGroupsReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace GP = NativeStreamingModelGroupsProfile;
    using GroupReader = NativeStreamingModelGroupsReader;
    struct GroupFixture : BankFixture
    {
        GroupReader::Value current{};
        explicit GroupFixture(const std::filesystem::path& path) : BankFixture(path)
        {
            for (const auto& pin : GP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Publish()
        {
            Global(GP::PedCountRva, current.pedCount);
            Global(GP::PedSlotsRva, current.peds);
            Global(GP::VehicleSlotsRva, current.vehicles);
        }
        void Reset()
        {
            BankFixture::Reset();
            current = {};
            current.pedCount = 3;
            current.peds.fill(-1);
            current.peds[0] = 17;
            current.peds.back() = 19999;
            for (std::size_t i = 0; i < current.vehicles.size(); ++i)
                current.vehicles[i] = static_cast<std::int16_t>(400 + i);
            Publish();
        }
    };
    GroupFixture* activeGroups = nullptr;
    void          ChangeGroupCount()
    {
        activeGroups->Global(GP::PedCountRva, std::uint32_t{77});
    }
    bool SameGroup(const GroupReader::Value& a, const GroupReader::Value& b)
    {
        return a.pedCount == b.pedCount && a.peds == b.peds && a.vehicles == b.vehicles;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2, "durable group evidence directory supplied");
    GroupFixture fixture{std::filesystem::path(argv[1])};
    activeGroups = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    GroupReader        reader({image}, &PoolTestHost::Owner);
    GroupReader::Value value{};
    const auto         reset = [&] { fixture.Reset(); };
    reset();
    Check(reader.Read(value) == ClockResult::Ok && SameGroup(value, fixture.current),
          "read current native24DWORD/50WORD tracker arrays and raw DWORDped counter");
    Check(value.pedCount == 3 && GroupReader::IsPedTracked(value, 17) && GroupReader::IsPedTracked(value, 19999),
          "ped count stays the native counter instead of manufactured occupied-slot count");
    Check(GroupReader::VehicleCount(value) == 50 && GroupReader::IsVehicleTracked(value, 449),
          "DE aggregate vehicle tracker includes every proved slot beyond classic23");
    Check(reader.Recheck(value) == ClockResult::Ok, "exact immediate group snapshot rechecks on the same current owner");
    for (const auto count : {std::uint32_t{0}, std::uint32_t{24}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        reset();
        fixture.current.pedCount = count;
        fixture.Publish();
        Check(reader.Read(value) == ClockResult::Ok && value.pedCount == count,
              "native scalar counter remains a raw DWORD observation without clamping/recounting");
    }
    reset();
    fixture.current.peds.fill(-1);
    fixture.current.peds[1] = -2;
    fixture.current.peds[2] = 0xffff;
    fixture.current.peds[3] = 0x10000;
    fixture.current.peds[4] = 0x10011;
    fixture.current.peds[5] = 0x7fffffff;
    fixture.current.vehicles.fill(-1);
    fixture.current.vehicles[0] = -2;
    fixture.current.vehicles[1] = 400;
    fixture.current.vehicles[2] = -1;
    fixture.current.vehicles[3] = 401;
    fixture.Publish();
    Check(reader.Read(value) == ClockResult::Ok && !GroupReader::IsPedTracked(value, 0) && !GroupReader::IsPedTracked(value, 17) &&
              GroupReader::IsPedTracked(value, 0xffff),
          "MTA ped membership skips negative/out-of-WORD slots without narrowing high-DWORD aliases");
    Check(GroupReader::VehicleCount(value) == 2 && GroupReader::IsVehicleTracked(value, 400) && !GroupReader::IsVehicleTracked(value, 401) &&
              !GroupReader::IsVehicleTracked(value, 0xfffe) && !GroupReader::IsVehicleTracked(value, 0xffff),
          "MTA vehicle count/membership preserve distinct signed-negative and ffff terminator rules");
    reset();
    fixture.current.vehicles.fill(-1);
    fixture.current.vehicles.back() = 500;
    fixture.Publish();
    Check(reader.Read(value) == ClockResult::Ok && GroupReader::VehicleCount(value) == 0 && !GroupReader::IsVehicleTracked(value, 500),
          "entries after first native unused marker are not visible as loaded-group membership");
    const auto original = value;
    fixture.current.vehicles[0] = 600;
    fixture.Publish();
    Check(reader.Recheck(original) == ClockResult::BindingChanged, "changed aggregate slot invalidates a previous group observation");
    const auto sentinel = []
    {
        GroupReader::Value v;
        v.pedCount = 0xa5a5a5a5;
        v.peds.fill(-777);
        v.vehicles.fill(-123);
        return v;
    }();
    for (const auto state : {0, 5, 6, 7, 8})
    {
        reset();
        fixture.Global(NativeGameStateProfile::stateRva, static_cast<std::uint32_t>(state));
        value = sentinel;
        Check(reader.Read(value) != ClockResult::Ok && SameGroup(value, sentinel), "unqualified native lifecycle cannot publish group values");
    }
    reset();
    fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    value = sentinel;
    Check(reader.Read(value) == ClockResult::Busy && SameGroup(value, sentinel), "GC does not grant a borrowed scalar snapshot owner");
    reset();
    GroupReader noOwner({image}, nullptr);
    value = sentinel;
    Check(noOwner.Read(value) == ClockResult::NotInitialized && SameGroup(value, sentinel),
          "default/missing callback owner cannot publish empty-looking groups");
    reset();
    PoolTestHost::watched = image + GP::PedCountRva;
    PoolTestHost::after = &ChangeGroupCount;
    value = sentinel;
    Check(reader.Read(value) == ClockResult::BindingChanged && SameGroup(value, sentinel),
          "counter/array tear is refused rather than exposed as one coherent tracker state");
    reset();
    DWORD      previous = 0;
    const auto page = image + (GP::PedCountRva & ~std::uintptr_t{0xfff});
    Check(VirtualProtect(reinterpret_cast<void*>(page), 0x1000, PAGE_READONLY, &previous) != 0, "own tracker page made read-only");
    value = sentinel;
    Check(reader.Read(value) != ClockResult::Ok && SameGroup(value, sentinel),
          "native writable data page contract is enforced even for read-only observations");
    DWORD ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(page), 0x1000, previous, &ignored) != 0, "own tracker page restored");
    for (const auto& pin : GP::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        value = sentinel;
        Check(reader.Read(value) != ClockResult::Ok && SameGroup(value, sentinel),
              "each independent native tracker producer/consumer instruction signature is required");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    Check(reader.Read(value) == ClockResult::Ok, "successful group read establishes concrete current copy/owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (const auto fail : {1u, reads / 2, reads})
    {
        reset();
        PoolTestHost::failAt = fail;
        value = sentinel;
        Check(reader.Read(value) != ClockResult::Ok && SameGroup(value, sentinel), "failed guarded copy preserves all prior group outputs");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        value = sentinel;
        Check(reader.Read(value) != ClockResult::Ok && SameGroup(value, sentinel), "each callback owner boundary can revoke a scalar group observation");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    value = sentinel;
    std::thread worker([&] { foreign = reader.Read(value); });
    worker.join();
    Check(foreign == ClockResult::WrongThread && SameGroup(value, sentinel), "real foreign thread cannot borrow native GameThread group admission");
    GroupReader::Binding wrong{image + 0x1000};
    GroupReader          wrongReader(wrong, &PoolTestHost::Owner);
    value = sentinel;
    Check(wrongReader.Read(value) != ClockResult::Ok && SameGroup(value, sentinel), "wrong image does not inherit supported native group globals");
    std::puts("Production native streaming model-groups reader fixture completed");
    std::printf("Passed %u native model-groups checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
