/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Both qualified native ped classes, raw physical scalars and exact pool ownership
 ****************************************************************************/
#define main NativePedPhysicalScalarsTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedPhysicalScalarsMemory.h"
#include "../../Client/game_de/NativePedPhysicalScalarsProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>

namespace PhysicalScalarsTestHost
{
    unsigned                                         exchanges = 0;
    bool                                             failBefore = false, failAfter = false;
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
        failBefore = failAfter = false;
        before = after = {};
        afterRead = {};
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = mta::game_de::ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
    }
}
// Only current-image/read selection and the final scalar CAS are controlled.
// Generic vitals have no PlayerInfo bind or current-player substitution seam.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PhysicalScalarsTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#define MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE PhysicalScalarsTestHost::Exchange
#include "../../Client/game_de/NativePedPhysicalScalarsMemory.cpp"
#undef MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    float ScalarFloat(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t ScalarBits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    struct PhysicalScalarsFixture
    {
        TransformFixture transform;
        explicit PhysicalScalarsFixture(const std::filesystem::path& directory) : transform(directory)
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
            // proved native layout, GameThread/GC and playing-state prerequisites.
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedPhysicalScalarsProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            image.Patch(NativePedPhysicalScalarsProfile::InverseMassOneRva, &NativePedPhysicalScalarsProfile::InverseMassOneBits, sizeof(std::uint32_t),
                        PAGE_READONLY);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
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
            PhysicalScalarsTestHost::Reset();
            transform.Store(0x6a, static_cast<unsigned char>(3));
            transform.Store(0, transform.image.image + std::uintptr_t{0x4318270});
            transform.Store(0x5f0, std::uintptr_t{0});
            transform.Store(0x7e0, std::int32_t{-1});
            transform.Store(0xc4, std::uint32_t{0x42c80000});
            transform.Store(0xcc, std::uint32_t{0x432acdef});
            transform.Store(0xc8, std::uint32_t{0x42480000});
            transform.Store(0xc0, std::uint32_t{0xaabbccdd});
            transform.Store(0xd0, std::uint32_t{0x1234abcd});
            transform.Store(0x76c, std::uint32_t{0x42c80000});
            transform.Store(0x770, std::uint32_t{0x432a1234});
            transform.Store(0x774, std::uint32_t{0x42480000});
            transform.Store(0x7c, std::uint32_t{0x7fc01234});
            transform.Store(0x80, std::uint32_t{0x80000000});
            transform.Store(0x84, std::uint32_t{0x7f800000});
            transform.Store(0x88, std::uint32_t{0xc0200000});
            transform.Store(0x8c, std::uint32_t{0x3f800001});
            transform.Store(0x90, std::uint32_t{0xffc05678});
            transform.Store(0x3a, std::uint16_t{0xaffe});
            // Keep the old current-player prerequisites deliberately absent.
            // Neither actual ped class may inherit authority from that row.
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
            const auto other = transform.pool.objects + 8 * NativePedPoolProfile::ObjectStride;
            const auto playerVtable = transform.image.image + std::uintptr_t{0x4319730};
            std::memcpy(other, &playerVtable, sizeof(playerVtable));
            const unsigned char type = 3;
            std::memcpy(other + 0x6a, &type, sizeof(type));
            const std::array<std::uint32_t, 3> otherScalars{0x41480000, 0x41c80000, 0x432acdef};
            std::memcpy(other + 0xc4, otherScalars.data(), sizeof(otherScalars));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        }
        NativePedPoolReader::Value Ped(std::uint32_t handle = 1)
        {
            NativePedPoolReader        reader({transform.image.image}, &PoolTestHost::Owner);
            NativePedPoolReader::Value value;
            Check(reader.Resolve(handle, value) == ClockResult::Ok, "resolve a genuine occupied pool slot before borrowing ped mass scalars");
            return value;
        }
        std::uint32_t Raw(std::size_t offset, std::size_t slot = 0) const
        {
            std::uint32_t value;
            std::memcpy(&value, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, sizeof(value));
            return value;
        }
        using Bytes = std::array<unsigned char, NativePedPoolProfile::ObjectStride>;
        Bytes Snapshot(std::size_t slot = 0) const
        {
            Bytes result{};
            std::memcpy(result.data(), transform.pool.objects + slot * NativePedPoolProfile::ObjectStride, result.size());
            return result;
        }
        void OnlyChanged(const Bytes& before, std::size_t offset, std::size_t slot = 0) const
        {
            const auto after = Snapshot(slot);
            Check(!std::memcmp(after.data(), before.data(), offset) &&
                      !std::memcmp(after.data() + offset + sizeof(float), before.data() + offset + sizeof(float), after.size() - offset - sizeof(float)),
                  "exact float32 CAS cannot widen into neighbouring physical, velocity, vital, model or identity bytes");
        }
        void Neighbours(std::uint32_t health, std::uint32_t armor) const
        {
            Check(Raw(0xc4) == health && Raw(0xcc) == 0x432acdef && Raw(0xc8) == armor,
                  "exact scalar access leaves the neighbouring native scalar neighbour and other physical scalar untouched");
            Check(Raw(0xc0) == 0xaabbccdd && Raw(0xd0) == 0x1234abcd && Raw(0x76c) == 0x42c80000 && Raw(0x770) == 0x432a1234 && Raw(0x774) == 0x42480000 &&
                      Raw(0x7c) == 0x7fc01234 && Raw(0x80) == 0x80000000 && Raw(0x84) == 0x7f800000 && Raw(0x88) == 0xc0200000 && Raw(0x8c) == 0x3f800001 &&
                      Raw(0x90) == 0xffc05678,
                  "mass intent changes none of the qualified vitals, maximum health or raw linear/angular velocity controls");
        }
    };
}

int main(int argc, char** argv)
{
    using Memory = NativePedPhysicalScalarsMemory;
    using Field = NativePedPhysicalScalarField;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native physical scalars evidence uses its explicit workspace output directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping native physical scalars qualification refuses the actual non-GTA test executable");
    PhysicalScalarsFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner);
    Memory                unowned(binding, nullptr);
    const auto            reset = [&]()
    {
        fixture.Reset();
        return fixture.Ped();
    };
    auto ped = reset();
    Check(Memory::Revalidate(binding), "native physical scalars qualify exact generic constructors/consumers and real pool image pages");
    Memory::Receipt receipt;
    float           output = ScalarFloat(0x4a123456);
    Check(memory.Read(ped, Field::Mass, output) == ClockResult::Ok && ScalarBits(output) == 0x42c80000,
          "a genuine generic ped pool observation reads native mass with no current-player root, selector or wrapper");
    fixture.Neighbours(0x42c80000, 0x42480000);
    struct DefaultAccess final : NativePedPhysicalScalarsAccess
    {
    } defaultAccess;
    NativePedReferenceToken token{};
    output = ScalarFloat(0x4a123456);
    Check(defaultAccess.ReadPedPhysicalScalar(token, Field::Mass, output) == ClockResult::NotInitialized && ScalarBits(output) == 0x4a123456 &&
              defaultAccess.SetPedPhysicalScalar(token, Field::TurnMass, 1.0f) == ClockResult::NotInitialized,
          "the optional internal token seam refuses until the actual Runtime owner supplies this service");

    constexpr std::array<std::uint32_t, 11> patterns{0,          0x80000000, 0x3f800000, 0xbf800000, 0x447a0000, 0x7f7fffff,
                                                     0xff7fffff, 0x7f800000, 0xff800000, 0x7fc12345, 0x7fa54321};
    for (const auto field : {Field::Mass, Field::TurnMass})
        for (const auto bits : patterns)
        {
            ped = reset();
            const auto offset = field == Field::Mass ? std::size_t{0xc4} : std::size_t{0xc8};
            const auto original = fixture.Raw(offset);
            const auto allBytes = fixture.Snapshot();
            Check(memory.Set(ped, field, ScalarFloat(bits), &receipt) == ClockResult::Ok && receipt.field == field && receipt.handle == 1 &&
                      receipt.nativePed == ped.nativePed && receipt.address == ped.nativePed + offset && receipt.original == original &&
                      receipt.target == bits && receipt.entered && receipt.attempted && receipt.applied && receipt.observedValid && receipt.observed == bits &&
                      receipt.verified && PhysicalScalarsTestHost::exchanges == 1,
                  "raw mass and turn mass accept each exact float32 intent without clamping, filtering or widening its scalar CAS");
            output = ScalarFloat(0x4a123456);
            Check(memory.Read(ped, field, output) == ClockResult::Ok && ScalarBits(output) == bits,
                  "raw generic-ped physical scalars preserve signed zero, out-of-range values, infinities and NaN payload bits");
            fixture.Neighbours(field == Field::Mass ? bits : 0x42c80000, field == Field::TurnMass ? bits : 0x42480000);
            fixture.OnlyChanged(allBytes, offset);
        }

    ped = reset();
    output = ScalarFloat(0x4a123456);
    Check(unowned.Read(ped, Field::Mass, output) == ClockResult::NotInitialized && ScalarBits(output) == 0x4a123456 &&
              unowned.Set(ped, Field::Mass, 7.0f, &receipt) == ClockResult::NotInitialized && !receipt.entered && !PhysicalScalarsTestHost::exchanges,
          "native pool identity does not replace callback authority or consume caller output");
    Check(memory.Read(ped, static_cast<Field>(99), output) == ClockResult::InvalidArgument && ScalarBits(output) == 0x4a123456 &&
              memory.Set(ped, static_cast<Field>(99), 7.0f, &receipt) == ClockResult::InvalidArgument && !receipt.entered &&
              !PhysicalScalarsTestHost::exchanges,
          "invalid typed fields cannot alias neighbouring scalar neighbour or another native member");

    // The common constructor and independent generic script consumer qualify
    // both exact classes. Pool lifetime and normal GT/GC authority remain
    // necessary even when there is no loaded current-player row or selector.
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
            fixture.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
        if (guard == 6)
            fixture.transform.pool.Put(0, std::uintptr_t{0});
        if (guard == 7)
            fixture.transform.pool.flags[0] = 0x81;
        if (guard == 8)
            fixture.transform.pool.Put(16, std::int32_t{999});
        if (guard == 9)
            fixture.transform.pool.flags[0] = 2;
        if (guard == 10)
            fixture.transform.Store(0x6a, static_cast<unsigned char>(4));
        if (guard == 11)
        {
            fixture.transform.Store(0, fixture.transform.image.image + std::uintptr_t{0x1300010});
            ped = fixture.Ped();
        }
        output = ScalarFloat(0x4a123456);
        Check(memory.Read(ped, Field::TurnMass, output) != ClockResult::Ok && ScalarBits(output) == 0x4a123456 &&
                  memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !PhysicalScalarsTestHost::exchanges,
              "native GT/GC/playing state, exact occupied slot, known class and callback remain mandatory before scalar entry");
        fixture.Neighbours(0x42c80000, 0x42480000);
    }
    ped = reset();
    auto       other = fixture.Ped((8u << 8) | 127u);
    const auto playerBytes = fixture.Snapshot(8);
    output = ScalarFloat(0x4a123456);
    Check(other.vtable == fixture.transform.image.image + 0x4319730 && memory.Read(other, Field::Mass, output) == ClockResult::Ok &&
              ScalarBits(output) == 0x41480000 && memory.Set(other, Field::TurnMass, ScalarFloat(0x7fc23456), &receipt) == ClockResult::Ok &&
              receipt.handle == ((8u << 8) | 127u) && receipt.nativePed == other.nativePed && receipt.verified && fixture.Raw(0xc4, 8) == 0x41480000 &&
              fixture.Raw(0xcc, 8) == 0x432acdef && fixture.Raw(0xc8, 8) == 0x7fc23456,
          "the independently occupied exact player-class ped accepts raw physical scalars without any current-player root or local selector");
    fixture.OnlyChanged(playerBytes, 0xc8, 8);
    fixture.Neighbours(0x42c80000, 0x42480000);
    ped = reset();
    ClockResult foreignThread = ClockResult::Ok;
    std::thread foreign(
        [&]()
        {
            float untouched = ScalarFloat(0x4a123456);
            foreignThread = memory.Read(ped, Field::Mass, untouched);
            if (ScalarBits(untouched) != 0x4a123456)
                foreignThread = ClockResult::Ok;
        });
    foreign.join();
    Check(foreignThread != ClockResult::Ok && !PhysicalScalarsTestHost::exchanges, "a real foreign host thread cannot reuse the native GameThread admission");

    ped = reset();
    PhysicalScalarsTestHost::failBefore = true;
    Check(memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.attempted && !receipt.applied &&
              receipt.observedValid && receipt.observed == 0x42c80000 && receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION &&
              PhysicalScalarsTestHost::exchanges == 1,
          "an entered scalar exchange fault reports uncertainty without claiming a confirmed write or silently retrying");
    fixture.Neighbours(0x42c80000, 0x42480000);
    ped = reset();
    PhysicalScalarsTestHost::failAfter = true;
    Check(memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.attempted && !receipt.applied &&
              receipt.observedValid && receipt.observed == 0x42c80000 && receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION &&
              PhysicalScalarsTestHost::exchanges == 1,
          "a physically accepted store with failed acknowledgement stays honestly unconfirmed and is never rewound or retried");
    fixture.Neighbours(0x40e00000, 0x42480000);

    ped = reset();
    PhysicalScalarsTestHost::after = []() { PoolTestHost::ownerResult = ClockResult::Expired; };
    Check(memory.Set(ped, Field::TurnMass, 7.0f, &receipt) == ClockResult::Expired && receipt.entered && receipt.applied && receipt.observedValid &&
              !receipt.verified && PhysicalScalarsTestHost::exchanges == 1,
          "revoked acknowledgement authority reports an accepted persistent scalar effect for the caller's stop-without-replay decision");
    fixture.Neighbours(0x42c80000, 0x40e00000);
    ped = reset();
    PhysicalScalarsTestHost::after = []() { PoolTestHost::failAt = PoolTestHost::reads + 1; };
    Check(memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.applied && !receipt.verified &&
              PhysicalScalarsTestHost::exchanges == 1,
          "post-store read failure preserves accepted effect and exact receipt instead of repairing with a stale old scalar");
    fixture.Neighbours(0x40e00000, 0x42480000);

    ped = reset();
    PhysicalScalarsTestHost::before = [&]() { fixture.transform.Store(0xc4, std::uint32_t{0x41d00000}); };
    Check(memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && receipt.entered && receipt.attempted && !receipt.applied &&
              receipt.observedValid && receipt.observed == 0x41d00000 && PhysicalScalarsTestHost::exchanges == 1,
          "an intervening foreign scalar wins its compare-exchange conflict without any retry or overwrite");
    fixture.Neighbours(0x41d00000, 0x42480000);

    ped = reset();
    DWORD previous = 0, ignored = 0;
    Check(VirtualProtect(fixture.transform.pool.objects, 0x1000, PAGE_READONLY, &previous) != FALSE,
          "make the actual owned ped page read-only before testing write admission");
    const auto protectedResult = memory.Set(ped, Field::Mass, 7.0f, &receipt);
    Check(VirtualProtect(fixture.transform.pool.objects, 0x1000, previous, &ignored) != FALSE, "restore exact owned ped page protection after refusal");
    Check(protectedResult != ClockResult::Ok && !receipt.entered && !PhysicalScalarsTestHost::exchanges,
          "actual OS page protection must admit a writable scalar before any native intent is attempted");
    fixture.Neighbours(0x42c80000, 0x42480000);

    ped = reset();
    const auto& pin = NativePedPhysicalScalarsProfile::Code[0];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = ScalarFloat(0x4a123456);
    Check(memory.Read(ped, Field::Mass, output) != ClockResult::Ok && ScalarBits(output) == 0x4a123456 &&
              memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !PhysicalScalarsTestHost::exchanges,
          "foreign native producer bytes cannot inherit a previously qualified layout or consume caller output");
    fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    const auto foreignOne = NativePedPhysicalScalarsProfile::InverseMassOneBits ^ 1;
    fixture.transform.image.Patch(NativePedPhysicalScalarsProfile::InverseMassOneRva, &foreignOne, sizeof(foreignOne), PAGE_READONLY);
    output = ScalarFloat(0x4a123456);
    Check(memory.Read(ped, Field::Mass, output) != ClockResult::Ok && ScalarBits(output) == 0x4a123456 &&
              memory.Set(ped, Field::TurnMass, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !PhysicalScalarsTestHost::exchanges,
          "changed inverse-mass consumer constant cannot inherit the qualified physics scalar contract");
    fixture.transform.image.Patch(NativePedPhysicalScalarsProfile::InverseMassOneRva, &NativePedPhysicalScalarsProfile::InverseMassOneBits,
                                  sizeof(std::uint32_t), PAGE_READONLY);
    PoolTestHost::image = 0;
    output = ScalarFloat(0x4a123456);
    Check(memory.Read(ped, Field::TurnMass, output) != ClockResult::Ok && ScalarBits(output) == 0x4a123456 &&
              memory.Set(ped, Field::Mass, 7.0f, &receipt) != ClockResult::Ok && !receipt.entered && !PhysicalScalarsTestHost::exchanges,
          "changed main-image identity invalidates the generic native ped observation");
    std::puts("Production native ped physical scalars primitive fixture completed");
    std::puts("Production generic native ped physical scalars primitive fixture completed");
    std::printf("Passed %u native ped-physical-scalars checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
