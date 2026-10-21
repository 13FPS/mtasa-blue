/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current-player-only raw ped vitals over real owned image/pool pages
 ****************************************************************************/
#define main NativePedVitalsTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedVitalsMemory.h"
#include "../../Client/game_de/NativePedVitalsProfile.h"
#include "../../Client/game_de/NativePlayerInfoProfile.h"
#include "../../Client/game_de/NativePlayerInfoExtraProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>

namespace VitalsTestHost
{
    unsigned                                         exchanges = 0;
    bool                                             buildAllowed = true, failBefore = false, failAfter = false;
    std::function<void()>                            before, after;
    std::function<void(std::uintptr_t, std::size_t)> afterRead;
    BOOL WINAPI                                      Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = PoolTestHost::Read(process, input, output, bytes, copied);
        if (result && afterRead)
            afterRead(reinterpret_cast<std::uintptr_t>(input), bytes);
        return result;
    }
    bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        observed = static_cast<std::uint32_t>(
            _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(target), static_cast<LONG>(expected)));
        if (after)
            after();
        // A scalar store can succeed before its acknowledgement is lost. The
        // receipt must describe uncertainty without undoing native intent.
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
    void Reset()
    {
        exchanges = 0;
        buildAllowed = true;
        failBefore = failAfter = false;
        before = after = {};
        afterRead = {};
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = mta::game_de::ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
    }
}
namespace mta::game_de::NativeImageAccess
{
    bool VitalsFixtureExecutable() noexcept
    {
        return VitalsTestHost::buildAllowed && (PoolTestHost::image || CheckExecutableBuild());
    }
}
// Reuse the actual player primitive once. These private helper-name aliases
// only avoid anonymous-namespace collisions with the included pool utility.
// Bind's build seam admits owned bytes; the shipping qualifier still hashes
// and refuses this executable before the controlled image is selected.
#define CheckExecutableBuild VitalsFixtureExecutable
#define GetModuleHandleW     PoolTestHost::Module
#define ReadProcessMemory    VitalsTestHost::Read
#define P                    VitalsPlayerInfoProfile
#define Binding              VitalsPlayerInfoBinding
#define Context              VitalsPlayerInfoContext
#define Object               VitalsPlayerInfoObject
#define ObjectPages          VitalsPlayerInfoObjectPages
#define Global               VitalsPlayerInfoGlobal
#define Owner                VitalsPlayerInfoOwner
#define Exchange             VitalsPlayerInfoExchange
#define PlayerIdentity       VitalsPlayerInfoIdentity
#include "../../Client/game_de/NativePlayerInfoMemory.cpp"
#undef PlayerIdentity
#undef Exchange
#undef Owner
#undef Global
#undef ObjectPages
#undef Object
#undef Context
#undef Binding
#undef P
#undef CheckExecutableBuild
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#define MTA_DE_PED_VITALS_EXCHANGE VitalsTestHost::Exchange
#include "../../Client/game_de/NativePedVitalsMemory.cpp"
#undef MTA_DE_PED_VITALS_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    float VitalsFloat(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t VitalsBits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    struct VitalsFixture
    {
        TransformFixture       transform;
        NativePlayerInfoMemory player;
        explicit VitalsFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundaries{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto& section = sections[i];
                std::memcpy(section.Name, i == 0 ? ".text" : i == 1 ? ".rdata" : ".data", i == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundaries[i]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundaries[i + 1] - boundaries[i]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + i * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (i == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                       : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (i == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            // Keep real PE section and OS page checks while adding the already
            // proved player, GameThread/GC and playing-state prerequisites.
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePlayerInfoProfile::Code)
                image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativePlayerInfoExtraProfile::Code)
                image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativePedVitalsProfile::Code)
                image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            const auto control = image.image + NativePlayerInfoProfile::PlayerControlRva;
            image.Patch(NativePlayerInfoProfile::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        void Reset()
        {
            transform.Reset(false);
            VitalsTestHost::Reset();
            player.Reset();
            transform.Store(0x6a, static_cast<unsigned char>(3));
            transform.Store(0x5f0, transform.image.image + NativePlayerInfoProfile::BankRva + 8);
            transform.Store(0x7e0, std::int32_t{0});
            transform.Store(0x76c, std::uint32_t{0x42c80000});
            transform.Store(0x770, std::uint32_t{0x432acdef});
            transform.Store(0x774, std::uint32_t{0x42480000});
            Global(NativePlayerInfoProfile::BankRva, reinterpret_cast<std::uintptr_t>(transform.pool.objects));
            Global(NativePlayerInfoProfile::PlayerIndexRva, static_cast<unsigned char>(0));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        }
        void Bind() { Check(player.Bind() == ClockResult::Ok, "the actual player primitive binds the owned exact image and producer thread"); }
        NativePedPoolReader::Value Ped(std::uint32_t handle = 1)
        {
            NativePedPoolReader        reader({transform.image.image}, &PoolTestHost::Owner);
            NativePedPoolReader::Value value;
            Check(reader.Resolve(handle, value) == ClockResult::Ok, "resolve a genuine occupied pool slot before borrowing ped vitals");
            return value;
        }
        std::uint32_t Raw(std::size_t offset, std::size_t slot = 0) const
        {
            std::uint32_t value;
            std::memcpy(&value, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, sizeof(value));
            return value;
        }
        void Neighbours(std::uint32_t health, std::uint32_t armor) const
        {
            Check(Raw(0x76c) == health && Raw(0x770) == 0x432acdef && Raw(0x774) == armor,
                  "exact scalar access leaves the neighbouring native max-health and other vital untouched");
        }
    };
}

int main(int argc, char** argv)
{
    using Memory = NativePedVitalsMemory;
    using Field = NativePedVitalField;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native vitals evidence uses its explicit workspace output directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping native vitals qualification refuses the actual non-GTA test executable");
    VitalsFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner, fixture.player);
    Memory                unowned(binding, nullptr, fixture.player);
    const auto            reset = [&]()
    {
        fixture.Reset();
        fixture.Bind();
        return fixture.Ped();
    };
    auto ped = reset();
    Check(Memory::Revalidate(binding), "native vitals reuse the exact qualified player producers and real pool image pages");
    Memory::Receipt receipt;
    float           output = VitalsFloat(0x4a123456);
    Check(memory.Read(ped, Field::Health, output) == ClockResult::Ok && VitalsBits(output) == 0x42c80000,
          "a genuine current-player pool observation reads native health without substituting a wrapper or ambient handle");
    fixture.Neighbours(0x42c80000, 0x42480000);
    struct DefaultAccess final : NativePedVitalsAccess
    {
    } defaultAccess;
    NativePedReferenceToken token{};
    output = VitalsFloat(0x4a123456);
    Check(defaultAccess.ReadPedVital(token, Field::Health, output) == ClockResult::NotInitialized && VitalsBits(output) == 0x4a123456 &&
              defaultAccess.SetPedVital(token, Field::Armor, 1.0f) == ClockResult::NotInitialized,
          "the optional internal token seam refuses until the actual Runtime owner supplies this service");

    constexpr std::array<std::uint32_t, 11> patterns{0,          0x80000000, 0x3f800000, 0xbf800000, 0x447a0000, 0x7f7fffff,
                                                     0xff7fffff, 0x7f800000, 0xff800000, 0x7fc12345, 0x7fa54321};
    for (const auto field : {Field::Health, Field::Armor})
        for (const auto bits : patterns)
        {
            ped = reset();
            const auto offset = field == Field::Health ? std::size_t{0x76c} : std::size_t{0x774};
            const auto original = fixture.Raw(offset);
            Check(memory.Set(ped, field, VitalsFloat(bits), &receipt) == ClockResult::Ok && receipt.field == field && receipt.handle == 1 &&
                      receipt.nativePed == ped.nativePed && receipt.address == ped.nativePed + offset && receipt.original == original &&
                      receipt.target == bits && receipt.entered && receipt.attempted && receipt.applied && receipt.observedValid && receipt.observed == bits &&
                      receipt.verified && VitalsTestHost::exchanges == 1,
                  "raw health and armour accept each exact float32 intent without clamping, filtering or widening its scalar CAS");
            output = VitalsFloat(0x4a123456);
            Check(memory.Read(ped, field, output) == ClockResult::Ok && VitalsBits(output) == bits,
                  "raw current-player vitals preserve signed zero, out-of-range values, infinities and NaN payload bits");
            fixture.Neighbours(field == Field::Health ? bits : 0x42c80000, field == Field::Armor ? bits : 0x42480000);
        }

    ped = reset();
    output = VitalsFloat(0x4a123456);
    Check(unowned.Read(ped, Field::Health, output) == ClockResult::NotInitialized && VitalsBits(output) == 0x4a123456 &&
              unowned.Set(ped, Field::Health, 7.0f, &receipt) == ClockResult::NotInitialized && !receipt.entered && !VitalsTestHost::exchanges,
          "native pool and player identity do not replace callback authority or consume caller output");
    Check(memory.Read(ped, static_cast<Field>(99), output) == ClockResult::InvalidArgument && VitalsBits(output) == 0x4a123456 &&
              memory.Set(ped, static_cast<Field>(99), 7.0f, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !VitalsTestHost::exchanges,
          "invalid typed fields cannot alias neighbouring max-health or another native member");

    // Each independent immediate prerequisite is changed in owned memory.
    // A non-current occupied slot remains a real ped observation; it must not
    // inherit the current-player-only support by sharing this image or pool.
    for (unsigned guard = 0; guard < 12; ++guard)
    {
        ped = reset();
        if (guard == 0)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (guard == 1)
            fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
        if (guard == 2)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (guard == 3)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{3});
        if (guard == 4)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (guard == 5)
            fixture.Global(NativePlayerInfoProfile::PlayerIndexRva, static_cast<unsigned char>(1));
        if (guard == 6)
            fixture.Global(NativePlayerInfoProfile::BankRva, std::uintptr_t{0});
        if (guard == 7)
            fixture.transform.Store(0x5f0, std::uintptr_t{0});
        if (guard == 8)
            fixture.transform.Store(0x7e0, std::int32_t{1});
        if (guard == 9)
            fixture.transform.pool.flags[0] = 2;
        if (guard == 10)
            fixture.transform.Store(0x6a, static_cast<unsigned char>(4));
        if (guard == 11)
            fixture.player.Reset();
        output = VitalsFloat(0x4a123456);
        Check(memory.Read(ped, Field::Armor, output) != ClockResult::Ok && VitalsBits(output) == 0x4a123456 &&
                  memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !VitalsTestHost::exchanges,
              "native GT/GC/playing state, fresh player identity, exact current slot and callback remain mandatory before scalar entry");
        fixture.Neighbours(0x42c80000, 0x42480000);
    }
    ped = reset();
    auto other = fixture.Ped((8u << 8) | 127u);
    output = VitalsFloat(0x4a123456);
    Check(memory.Read(other, Field::Health, output) != ClockResult::Ok && VitalsBits(output) == 0x4a123456 &&
              memory.Set(other, Field::Armor, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !VitalsTestHost::exchanges &&
              fixture.Raw(0x774, 8) == 0,
          "a different genuine occupied ped cannot borrow the current local player's vitals contract");
    fixture.Neighbours(0x42c80000, 0x42480000);
    ClockResult foreignThread = ClockResult::Ok;
    std::thread foreign(
        [&]()
        {
            float untouched = VitalsFloat(0x4a123456);
            foreignThread = memory.Read(ped, Field::Health, untouched);
            if (VitalsBits(untouched) != 0x4a123456)
                foreignThread = ClockResult::Ok;
        });
    foreign.join();
    Check(foreignThread != ClockResult::Ok && !VitalsTestHost::exchanges,
          "a real foreign host thread cannot reuse the native GameThread/player producer binding");

    ped = reset();
    VitalsTestHost::failBefore = true;
    Check(memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.attempted && !receipt.applied &&
              receipt.observedValid && receipt.observed == 0x42c80000 && receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && VitalsTestHost::exchanges == 1,
          "an entered scalar exchange fault reports uncertainty without claiming a confirmed write or silently retrying");
    fixture.Neighbours(0x42c80000, 0x42480000);
    ped = reset();
    VitalsTestHost::failAfter = true;
    Check(memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.attempted && !receipt.applied &&
              receipt.observedValid && receipt.observed == 0x42c80000 && receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && VitalsTestHost::exchanges == 1,
          "a physically accepted store with failed acknowledgement stays honestly unconfirmed and is never rewound or retried");
    fixture.Neighbours(0x40e00000, 0x42480000);

    ped = reset();
    VitalsTestHost::after = []() { PoolTestHost::ownerResult = ClockResult::Expired; };
    Check(memory.Set(ped, Field::Armor, 7.0f, &receipt) == ClockResult::Expired && receipt.entered && receipt.applied && receipt.observedValid &&
              !receipt.verified && VitalsTestHost::exchanges == 1,
          "revoked acknowledgement authority reports an accepted persistent scalar effect for the caller's stop-without-replay decision");
    fixture.Neighbours(0x42c80000, 0x40e00000);
    ped = reset();
    VitalsTestHost::after = []() { PoolTestHost::failAt = PoolTestHost::reads + 1; };
    Check(memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.applied && !receipt.verified &&
              VitalsTestHost::exchanges == 1,
          "post-store read failure preserves accepted effect and exact receipt instead of repairing with a stale old scalar");
    fixture.Neighbours(0x40e00000, 0x42480000);

    ped = reset();
    VitalsTestHost::before = [&]() { fixture.transform.Store(0x76c, std::uint32_t{0x41d00000}); };
    Check(memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.attempted && !receipt.applied &&
              receipt.observedValid && receipt.observed == 0x41d00000 && VitalsTestHost::exchanges == 1,
          "an intervening foreign scalar wins its compare-exchange conflict without any retry or overwrite");
    fixture.Neighbours(0x41d00000, 0x42480000);

    ped = reset();
    DWORD previous = 0, ignored = 0;
    Check(VirtualProtect(fixture.transform.pool.objects, 0x1000, PAGE_READONLY, &previous) != FALSE,
          "make the actual owned ped page read-only before testing write admission");
    const auto protectedResult = memory.Set(ped, Field::Health, 7.0f, &receipt);
    Check(VirtualProtect(fixture.transform.pool.objects, 0x1000, previous, &ignored) != FALSE, "restore exact owned ped page protection after refusal");
    Check(protectedResult != ClockResult::Ok && !receipt.entered && !VitalsTestHost::exchanges,
          "actual OS page protection must admit a writable scalar before any native intent is attempted");
    fixture.Neighbours(0x42c80000, 0x42480000);

    ped = reset();
    const auto& pin = NativePedVitalsProfile::Code[0];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = VitalsFloat(0x4a123456);
    Check(memory.Read(ped, Field::Health, output) != ClockResult::Ok && VitalsBits(output) == 0x4a123456 &&
              memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !VitalsTestHost::exchanges,
          "foreign native producer bytes cannot inherit a previously qualified layout or consume caller output");
    fixture.transform.image.Patch(pin.rva, pin.bytes.data(), 1, PAGE_EXECUTE_READ);
    PoolTestHost::image = 0;
    output = VitalsFloat(0x4a123456);
    Check(memory.Read(ped, Field::Armor, output) != ClockResult::Ok && VitalsBits(output) == 0x4a123456 &&
              memory.Set(ped, Field::Health, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !VitalsTestHost::exchanges,
          "changed main-image identity invalidates the current-player and native ped observation together");
    std::puts("Production native ped vitals primitive fixture completed");
    std::printf("Passed %u native ped-vitals checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
