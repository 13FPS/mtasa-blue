/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual ignored-pointer admission/CAS/recovery on owned OS pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeIgnoredHistoricalLoadMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldIgnoredEntityFixture.h"
#include "../../Client/game_de/NativeWorldIgnoredEntityMemory.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#include "../../Client/game_de/NativeWorldIgnoredEntityMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace IG = NativeWorldIgnoredEntityMemory;
    namespace IP = NativeWorldIgnoredEntityProfile;
    struct IgnoreFixture : LoadFixture
    {
        explicit IgnoreFixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        template <class T>
        void PedAt(std::size_t index, std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + index * NativePedPoolProfile::ObjectStride + offset, &value, sizeof(value));
        }
        void Reset()
        {
            LoadFixture::Reset();
            NativeWorldIgnoredEntityFixture::Install(transform.image.image, &LoadFixture::Write, this);
            for (const auto index : {std::size_t{0}, std::size_t{8}})
            {
                PedAt(index, 0, transform.image.image + NativePedMetadataProfile::PedVtableRva);
                PedAt(index, NativePedMetadataProfile::ModelOffset, std::uint16_t{17});
                PedAt(index, NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(3));
            }
            PoolTestHost::image = transform.image.image;
        }
        IG::Ped Ped(std::uint32_t handle = 1)
        {
            IG::Ped ped;
            Check(NativePedPoolReader({transform.image.image}, &PoolTestHost::Owner).Resolve(handle, ped) == ClockResult::Ok,
                  "ignore fixture borrows real occupied native ped");
            return ped;
        }
        std::uintptr_t Current() const
        {
            std::uintptr_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(transform.image.image + IP::GlobalRva), sizeof(value));
            return value;
        }
        void Value(std::uintptr_t value) { Global(IP::GlobalRva, value); }
    };
    IgnoreFixture* activeIgnore = nullptr;
    IG::Ped*       callerIgnore = nullptr;
    unsigned       casCalls = 0, casMode = 0;
    ClockResult    nestedIgnore = ClockResult::Ok, nestedLoadIgnore = ClockResult::Ok;
    std::uintptr_t foreignIgnore = 0x123456780;
    ClockResult    ChangingCallerIgnoreOwner() noexcept
    {
        if (callerIgnore)
            *callerIgnore = {};
        return PoolTestHost::Owner();
    }
    ClockResult RecordedCompare(std::uintptr_t address, std::uintptr_t expected, std::uintptr_t desired, std::uintptr_t& observed,
                                std::uint32_t& exception) noexcept
    {
        ++casCalls;
        auto& f = *activeIgnore;
        Check(address == f.transform.image.image + IP::GlobalRva, "CAS receives only exact aligned native ignored global");
        if (casMode == 8)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (casMode == 10)
            f.Value(foreignIgnore);
        observed = reinterpret_cast<std::uintptr_t>(
            InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(address), reinterpret_cast<void*>(desired), reinterpret_cast<void*>(expected)));
        if (casMode == 1)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (casMode == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (casMode == 3)
            f.transform.pool.flags[0] = 0x80;
        if (casMode == 4)
            f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (casMode == 5)
        {
            const auto& pin = IP::Pins[0];
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (casMode == 6)
            f.Value(foreignIgnore);
        if (casMode == 7)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (casMode == 9)
        {
            IG::Receipt nested;
            nestedIgnore = IG::ClearOwned(f.transform.image.image, desired, 0, &PoolTestHost::Owner, &nested);
            LC::Receipt load;
            nestedLoadIgnore = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &load);
        }
        return ClockResult::Ok;
    }
    void MoveIgnoredValue()
    {
        activeIgnore->Value(foreignIgnore + 8);
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "ignored-global evidence stays in explicit workspace directory");
    IgnoreFixture fixture(argv[1]);
    activeIgnore = &fixture;
    const auto  image = fixture.transform.image.image;
    IG::Receipt receipt;
    const auto  reset = [&]
    {
        fixture.Reset();
        casCalls = casMode = 0;
        callerIgnore = nullptr;
        nestedIgnore = nestedLoadIgnore = ClockResult::Ok;
    };
    reset();
    auto a = fixture.Ped();
    auto b = fixture.Ped(0x87f);
    Check(IG::Exchange(image, 0, &a, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.attempted && !fixture.Current(),
          "resolved target is not owner authority");
    Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.attempted && receipt.applied && receipt.acknowledged &&
              fixture.Current() == a.nativePed,
          "actual production CAS installs the copied occupied native ped pointer, not handle/cell/wrapper");
    Check(IG::Exchange(image, a.nativePed, &b, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.applied && receipt.before == a.nativePed &&
              fixture.Current() == b.nativePed,
          "owned replacement uses one expected prior value and current target bookends");
    Check(IG::Exchange(image, b.nativePed, nullptr, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.applied && !fixture.Current(),
          "normal null clears only admitted own baseline");
    for (unsigned failure = 0; failure < 8; ++failure)
    {
        reset();
        a = fixture.Ped();
        if (failure == 0)
            fixture.Value(foreignIgnore);
        if (failure == 1)
            fixture.Global(LPF::RootRva, std::uintptr_t{0});
        if (failure == 2)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (failure == 3)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (failure == 4)
            fixture.transform.pool.flags[0] = 0x80;
        if (failure == 5)
            fixture.PedAt(0, NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(2));
        if (failure == 6)
        {
            const auto& pin = IP::Pins[0];
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (failure == 7)
        {
            DWORD old = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(image + IP::GlobalRva), 8, PAGE_READONLY, &old) != FALSE,
                  "owned ignored field can lose writable pages");
        }
        Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt, &RecordedCompare) != ClockResult::Ok && !receipt.attempted && !casCalls,
              "foreign baseline/missing world/state/GC/retired or wrong-type ped/profile/read-only storage refuse before CAS");
    }
    for (unsigned mode = 1; mode <= 8; ++mode)
    {
        reset();
        a = fixture.Ped();
        casMode = mode;
        Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt, &RecordedCompare) != ClockResult::Ok && receipt.attempted && !receipt.acknowledged &&
                  casCalls == 1,
              "uncertain CAS or lost postflight authority never acknowledges, retries or rolls back accepted effects");
        Check(fixture.Current() == (mode == 8   ? 0
                                    : mode == 6 ? foreignIgnore
                                                : a.nativePed),
              "accepted effects or foreign replacement remain untouched on failure");
        if (mode == 7 || mode == 8)
            Check(receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION, "uncertain CAS preserves exact failure identity");
    }
    for (unsigned mode : {7u, 8u})
    {
        reset();
        a = fixture.Ped();
        b = fixture.Ped(0x87f);
        fixture.Value(a.nativePed);
        casMode = mode;
        Check(IG::Exchange(image, a.nativePed, &b, &PoolTestHost::Owner, &receipt, &RecordedCompare) != ClockResult::Ok && receipt.attempted &&
                  !receipt.acknowledged,
              "uncertain replace can leave either committed or pending candidate");
        casMode = 0;
        casCalls = 0;
        Check(IG::ClearOwned(image, a.nativePed, b.nativePed, &PoolTestHost::Owner, &receipt, &RecordedCompare) == ClockResult::Ok && receipt.acknowledged &&
                  !fixture.Current() && casCalls == 1 && receipt.expected == (mode == 7 ? b.nativePed : a.nativePed),
              "recovery chooses actual owned candidate within one gate and one nulling CAS");
    }
    reset();
    a = fixture.Ped();
    fixture.Value(a.nativePed);
    fixture.Global(LPF::RootRva, std::uintptr_t{0});
    fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
    fixture.transform.pool.flags[0] = 0x80;
    Check(IG::ClearOwned(image, a.nativePed, 0, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.acknowledged && !fixture.Current(),
          "narrow nulling recovery survives missing loaded world/state9 and retired target without dereferencing it");
    reset();
    a = fixture.Ped();
    b = fixture.Ped(0x87f);
    Check(IG::ClearOwned(image, a.nativePed, b.nativePed, &PoolTestHost::Owner, &receipt, &RecordedCompare) == ClockResult::Ok && receipt.acknowledged &&
              !receipt.attempted && !receipt.foreignReplacement && !casCalls,
          "already-zero recovery acknowledges absence without CAS");
    fixture.Value(foreignIgnore);
    Check(IG::ClearOwned(image, a.nativePed, b.nativePed, &PoolTestHost::Owner, &receipt, &RecordedCompare) == ClockResult::Ok && receipt.acknowledged &&
              receipt.foreignReplacement && !receipt.attempted && !casCalls && fixture.Current() == foreignIgnore,
          "stable foreign replacement proves both candidates absent and retires debt without overwriting foreign state");
    PoolTestHost::watched = image + IP::GlobalRva;
    PoolTestHost::after = &MoveIgnoredValue;
    Check(IG::ClearOwned(image, a.nativePed, b.nativePed, &PoolTestHost::Owner, &receipt, &RecordedCompare) != ClockResult::Ok && !receipt.acknowledged &&
              !casCalls,
          "moving foreign observations are uncertain and cannot retire debt");
    for (unsigned failure = 0; failure < 3; ++failure)
    {
        reset();
        a = fixture.Ped();
        fixture.Value(a.nativePed);
        if (failure == 0)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (failure == 1)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (failure == 2)
        {
            PoolTestHost::watched = 0;
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        }
        Check(IG::ClearOwned(image, a.nativePed, 0, &PoolTestHost::Owner, &receipt, &RecordedCompare) != ClockResult::Ok && !receipt.acknowledged &&
                  !casCalls && fixture.Current() == a.nativePed,
              "wrong GC/callback or unreadable recovery retains debt and native value");
    }
    reset();
    a = fixture.Ped();
    casMode = 10;
    Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt, &RecordedCompare) == ClockResult::OwnerConflict && receipt.attempted && !receipt.applied &&
              !receipt.acknowledged && fixture.Current() == foreignIgnore && casCalls == 1,
          "CAS conflict never overwrites competing producer or retries");
    reset();
    a = fixture.Ped();
    casMode = 9;
    Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt, &RecordedCompare) == ClockResult::Ok && nestedIgnore == ClockResult::Busy &&
              nestedLoadIgnore == ClockResult::Busy && casCalls == 1,
          "tiny write scope serializes other native operations without an outer held FireBullet gate");
    Check(IG::ClearOwned(image, a.nativePed, 0, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "shared gate releases for later clear");
    reset();
    a = fixture.Ped();
    const auto original = a;
    callerIgnore = &a;
    Check(IG::Exchange(image, 0, &a, &ChangingCallerIgnoreOwner, &receipt) == ClockResult::Ok && !a.nativePed && fixture.Current() == original.nativePed,
          "target observation is copied before callbacks can mutate source value");
    reset();
    a = fixture.Ped();
    fixture.Global(NativeWorldGroundProfile::FreeStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::SectorsRva, std::uintptr_t{1});
    Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "ignore storage does not borrow sector/matrix allocator prerequisites");
    reset();
    a = fixture.Ped();
    std::thread foreign(
        [&]
        {
            Check(IG::Exchange(image, 0, &a, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.attempted,
                  "foreign thread cannot borrow native GT authority");
        });
    foreign.join();
    std::printf("Passed %u native world-ignored-entity checks (64-bit; no native engine code executed)\n", checks);
    std::puts("Production native world ignored entity memory fixture completed");
    return 0;
}
