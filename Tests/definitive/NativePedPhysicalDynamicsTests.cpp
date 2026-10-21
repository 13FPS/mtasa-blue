/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified ped dynamics with exact pool ownership and honest partial vector intent
 ****************************************************************************/
#define main NativePedDynamicsTransformFixtureMain
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

namespace
{
    using DynamicsBits = std::array<std::uint32_t, 3>;
    CVector DynamicsVector(const DynamicsBits& bits)
    {
        CVector vector;
        std::memcpy(&vector, bits.data(), sizeof(vector));
        return vector;
    }
    DynamicsBits DynamicsRaw(const CVector& vector)
    {
        DynamicsBits bits;
        std::memcpy(bits.data(), &vector, sizeof(vector));
        return bits;
    }
    DynamicsBits DynamicsNative(const PhysicalScalarsFixture& fixture, std::size_t slot = 0)
    {
        return {fixture.Raw(0xdc, slot), fixture.Raw(0xe0, slot), fixture.Raw(0xe4, slot)};
    }
    void DynamicsOnlyChanged(const PhysicalScalarsFixture& fixture, const PhysicalScalarsFixture::Bytes& before, std::size_t offset, std::size_t bytes,
                             std::size_t slot = 0)
    {
        const auto after = fixture.Snapshot(slot);
        Check(!std::memcmp(after.data(), before.data(), offset) &&
                  !std::memcmp(after.data() + offset + bytes, before.data() + offset + bytes, after.size() - offset - bytes),
              "dynamics writes preserve all neighbouring mass, buoyancy, velocity, vitals and identity bytes");
    }
}

int main(int argc, char** argv)
{
    using Memory = NativePedPhysicalScalarsMemory;
    using Field = NativePedPhysicalScalarField;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native dynamics evidence uses its explicit workspace output directory");
    PhysicalScalarsFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding  binding{fixture.transform.image.image};
    Memory                 memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    constexpr DynamicsBits original{0x3f800000, 0xc0000000, 0x40400000};
    constexpr DynamicsBits target{0x7fc01234, 0x80000000, 0x7f800000};
    const auto             reset = [&]()
    {
        fixture.Reset();
        fixture.transform.Store(0xd0, std::uint32_t{0x3f7d70a4});
        fixture.transform.Store(0xd4, std::uint32_t{0x3fa00000});
        fixture.transform.Store(0xd8, std::uint32_t{0x1234abcd});
        fixture.transform.Store(0xdc, original);
        fixture.transform.Store(0xe8, std::uint32_t{0x89abcdef});
        return fixture.Ped();
    };
    auto ped = reset();
    Check(Memory::Revalidate(binding), "dynamics use exact qualified constructors and independent force/collision consumers");
    Memory::Receipt                        receipt;
    Memory::VectorReceipt                  vectorReceipt;
    constexpr std::array<std::uint32_t, 9> patterns{0, 0x80000000, 0x3f800001, 0xbf800000, 0x7f7fffff, 0x7f800000, 0xff800000, 0x7fc12345, 0x7fa54321};
    for (const auto field : {Field::AirResistance, Field::Elasticity})
        for (const auto bits : patterns)
        {
            ped = reset();
            const auto offset = field == Field::AirResistance ? std::size_t{0xd0} : std::size_t{0xd4};
            const auto before = fixture.Snapshot();
            const auto was = fixture.Raw(offset);
            Check(memory.Set(ped, field, ScalarFloat(bits), &receipt) == ClockResult::Ok && receipt.field == field &&
                      receipt.address == ped.nativePed + offset && receipt.original == was && receipt.target == bits && receipt.entered && receipt.attempted &&
                      receipt.applied && receipt.verified && PhysicalScalarsTestHost::exchanges == 1,
                  "each qualified dynamics scalar preserves exact raw float intent through one real float32 CAS");
            float output = ScalarFloat(0x4a123456);
            Check(memory.Read(ped, field, output) == ClockResult::Ok && ScalarBits(output) == bits,
                  "qualified dynamics getters preserve signed zero, infinities and NaN payload bits without invented filtering");
            DynamicsOnlyChanged(fixture, before, offset, sizeof(float));
        }
    ped = reset();
    auto output = DynamicsVector(target);
    Check(memory.ReadCenterOfMass(ped, output) == ClockResult::Ok && DynamicsRaw(output) == original,
          "centre of mass copies all three qualified native components into caller storage");
    const auto before = fixture.Snapshot();
    Check(memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) == ClockResult::Ok && vectorReceipt.handle == ped.handle &&
              vectorReceipt.nativePed == ped.nativePed && vectorReceipt.address == ped.nativePed + 0xdc && vectorReceipt.original == original &&
              vectorReceipt.target == target && vectorReceipt.entered && vectorReceipt.attemptedMask == 7 && vectorReceipt.appliedMask == 7 &&
              vectorReceipt.observedMask == 7 && vectorReceipt.observed == target && vectorReceipt.verified && PhysicalScalarsTestHost::exchanges == 3,
          "centre-of-mass intent uses three exact native scalar exchanges with an honest final whole-vector receipt");
    Check(DynamicsNative(fixture) == target, "successful raw centre-of-mass intent persists without restoring its old vector");
    DynamicsOnlyChanged(fixture, before, 0xdc, sizeof(CVector));

    ped = reset();
    Check(memory.SetCenterOfMass(ped, DynamicsVector(original), &vectorReceipt) == ClockResult::Ok && !vectorReceipt.entered &&
              vectorReceipt.unchangedMask == 7 && !vectorReceipt.attemptedMask && !vectorReceipt.appliedMask && vectorReceipt.verified &&
              !PhysicalScalarsTestHost::exchanges,
          "unchanged raw center-of-mass intent verifies all components without inventing a native store");
    ped = reset();
    PhysicalScalarsTestHost::afterRead = [&](std::uintptr_t address, std::size_t bytes)
    {
        if (address == ped.nativePed + 0xdc && bytes == sizeof(CVector))
            fixture.transform.pool.flags[0] = 2;
    };
    output = DynamicsVector(target);
    Check(memory.ReadCenterOfMass(ped, output) != ClockResult::Ok && DynamicsRaw(output) == target && !PhysicalScalarsTestHost::exchanges,
          "post-copy native generation loss preserves caller output instead of publishing a stale vector");
    // The existing independently occupied player-class row is a separate ped,
    // with no current-player root. Its vector cannot alias the generic row.
    ped = reset();
    auto       player = fixture.Ped((8u << 8) | 127u);
    const auto playerBefore = fixture.Snapshot(8);
    Check(memory.SetCenterOfMass(player, DynamicsVector(target), &vectorReceipt) == ClockResult::Ok && DynamicsNative(fixture, 8) == target &&
              DynamicsNative(fixture) == original && vectorReceipt.nativePed == player.nativePed,
          "the exact player-class pool row supports the same dynamics without substituting current-player context");
    DynamicsOnlyChanged(fixture, playerBefore, 0xdc, sizeof(CVector), 8);
    Check(memory.Set(player, Field::Elasticity, ScalarFloat(0x7fc54321), &receipt) == ClockResult::Ok && fixture.Raw(0xd4, 8) == 0x7fc54321 &&
              fixture.Raw(0xd4) == 0x3fa00000,
          "player-class dynamics scalar identity stays bound to the requested occupied row");

    ped = reset();
    output = DynamicsVector(target);
    Check(unowned.ReadCenterOfMass(ped, output) == ClockResult::NotInitialized && DynamicsRaw(output) == target &&
              unowned.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) == ClockResult::NotInitialized && !vectorReceipt.entered &&
              !PhysicalScalarsTestHost::exchanges,
          "raw centre-of-mass identity cannot replace the required callback owner");
    struct DefaultAccess final : NativePedPhysicalScalarsAccess
    {
    } defaultAccess;
    NativePedReferenceToken token;
    Check(defaultAccess.ReadPedCenterOfMass(token, output) == ClockResult::NotInitialized && DynamicsRaw(output) == target &&
              defaultAccess.SetPedCenterOfMass(token, DynamicsVector(target)) == ClockResult::NotInitialized,
          "optional centre-of-mass owner methods default to explicit refusal with preserved caller output");
    for (unsigned guard = 0; guard < 5; ++guard)
    {
        ped = reset();
        if (guard == 0)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (guard == 1)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (guard == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (guard == 3)
            fixture.transform.pool.flags[0] = 2;
        if (guard == 4)
            fixture.transform.Store(0x6a, static_cast<unsigned char>(4));
        output = DynamicsVector(target);
        Check(memory.ReadCenterOfMass(ped, output) != ClockResult::Ok && DynamicsRaw(output) == target &&
                  memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) != ClockResult::Ok && !vectorReceipt.entered &&
                  !PhysicalScalarsTestHost::exchanges,
              "vector access rechecks playing state, no-GC scope, callback, type and exact pool generation before entry");
    }

    // Partial native intent is persistent. The enclosing Runtime can stop on
    // this receipt; a seven-bit pool generation is no future repair lease.
    ped = reset();
    PhysicalScalarsTestHost::after = []()
    {
        if (PhysicalScalarsTestHost::exchanges == 1)
            PhysicalScalarsTestHost::failBefore = true;
    };
    Check(memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) != ClockResult::Ok && vectorReceipt.entered && vectorReceipt.attemptedMask == 3 &&
              vectorReceipt.appliedMask == 1 && !vectorReceipt.verified && PhysicalScalarsTestHost::exchanges == 2 &&
              DynamicsNative(fixture) == DynamicsBits{target[0], original[1], original[2]},
          "a second-component fault preserves accepted X and original Y/Z with no rollback or extra CAS");
    ped = reset();
    PhysicalScalarsTestHost::after = []()
    {
        if (PhysicalScalarsTestHost::exchanges == 1)
            PoolTestHost::ownerResult = ClockResult::Expired;
    };
    Check(memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) == ClockResult::Expired && vectorReceipt.entered &&
              vectorReceipt.appliedMask == 1 && !vectorReceipt.verified && PhysicalScalarsTestHost::exchanges == 1 &&
              DynamicsNative(fixture) == DynamicsBits{target[0], original[1], original[2]},
          "revoked post-store authority preserves accepted X while forbidding the next component and any automatic repair");
    ped = reset();
    PhysicalScalarsTestHost::before = [&]()
    {
        if (PhysicalScalarsTestHost::exchanges == 2)
            fixture.transform.Store(0xe0, std::uint32_t{0x41100000});
    };
    Check(memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) != ClockResult::Ok && vectorReceipt.entered && vectorReceipt.appliedMask == 1 &&
              (vectorReceipt.foreignMask & 2) && !vectorReceipt.verified && PhysicalScalarsTestHost::exchanges == 2 &&
              DynamicsNative(fixture) == DynamicsBits{target[0], 0x41100000, original[2]},
          "an intervening foreign Y component wins its compare-exchange conflict without erasing accepted X or retrying foreign state");
    ped = reset();
    PhysicalScalarsTestHost::failAfter = true;
    Check(memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) != ClockResult::Ok && vectorReceipt.entered && vectorReceipt.attemptedMask == 1 &&
              vectorReceipt.appliedMask == 0 && !vectorReceipt.verified && DynamicsNative(fixture) == DynamicsBits{target[0], original[1], original[2]},
          "lost acknowledgement after a physical CAS leaves the component persistent but honestly unconfirmed");

    ped = reset();
    const auto& pin = NativePedPhysicalScalarsProfile::Code.back();
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    output = DynamicsVector(target);
    float scalar = ScalarFloat(0x4a123456);
    Check(memory.Read(ped, Field::AirResistance, scalar) != ClockResult::Ok && ScalarBits(scalar) == 0x4a123456 &&
              memory.ReadCenterOfMass(ped, output) != ClockResult::Ok && DynamicsRaw(output) == target &&
              memory.SetCenterOfMass(ped, DynamicsVector(target), &vectorReceipt) != ClockResult::Ok && !vectorReceipt.entered &&
              !PhysicalScalarsTestHost::exchanges,
          "foreign qualified dynamics consumer bytes refuse without any field write or output publication");
    fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    PoolTestHost::image = 0;
    output = DynamicsVector(target);
    Check(memory.ReadCenterOfMass(ped, output) != ClockResult::Ok && DynamicsRaw(output) == target,
          "a lost main-image binding cannot reuse a prior dynamics qualification");
    std::puts("Production native ped physical dynamics primitive fixture completed");
    std::puts("Production native ped centre-of-mass partial intent fixture completed");
    std::printf("Passed %u native ped-physical-dynamics checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
