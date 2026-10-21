/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine native ped vector reads and persistent partial CAS receipts
 ****************************************************************************/
#define main NativePedVelocityTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedVelocityMemory.h"
#include "../../Client/game_de/NativePedVelocityProfile.h"
#include <functional>
#include <intrin.h>

namespace VelocityTestHost
{
    using Memory = mta::game_de::NativePedVelocityMemory;
    unsigned              exchanges = 0, failBefore = 0, failAfter = 0, sourceReads = 0;
    bool                  sourceFailure = false, unstableSource = false;
    const CVector*        watchedSource = nullptr;
    std::function<void()> before, after;
    LONG                  Exchange(volatile LONG* address, LONG value, LONG expected)
    {
        ++exchanges;
        if (exchanges == failBefore)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        if (before)
            before();
        const auto observed = _InterlockedCompareExchange(address, value, expected);
        if (after)
            after();
        // The physical store may succeed even when its acknowledgement faults.
        // Production must retain honest attempted/confirmed masks, never undo it.
        if (exchanges == failAfter)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        return observed;
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (input == watchedSource && bytes == sizeof(CVector))
        {
            ++sourceReads;
            if (sourceFailure)
            {
                if (copied)
                    *copied = bytes / 2;
                return FALSE;
            }
        }
        const auto result = PoolTestHost::Read(process, input, output, bytes, copied);
        if (result && unstableSource && input == watchedSource && sourceReads == 2)
            static_cast<unsigned char*>(output)[0] ^= 1;
        return result;
    }
    void Reset()
    {
        exchanges = failBefore = failAfter = sourceReads = 0;
        sourceFailure = unstableSource = false;
        watchedSource = nullptr;
        before = after = {};
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = mta::game_de::ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
    }
}
// The existing OS image/pool/read guards remain production code. Only this
// prerequisite's scalar exchange is intercepted for controlled owned conflicts.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory VelocityTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#undef InterlockedCompareExchange
#define InterlockedCompareExchange VelocityTestHost::Exchange
#include "../../Client/game_de/NativePedVelocityMemory.cpp"
#undef InterlockedCompareExchange
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    using VelocityBits = std::array<std::uint32_t, 3>;
    CVector VelocityVector(const VelocityBits& bits)
    {
        CVector value;
        std::memcpy(&value, bits.data(), sizeof(value));
        return value;
    }
    VelocityBits VelocityRaw(const CVector& vector)
    {
        VelocityBits bits;
        std::memcpy(bits.data(), &vector, sizeof(vector));
        return bits;
    }
    struct VelocityFixture
    {
        TransformFixture transform;
        explicit VelocityFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundary{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto& section = sections[i];
                std::memcpy(section.Name, i == 0 ? ".text" : i == 1 ? ".rdata" : ".data", i == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundary[i]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundary[i + 1] - boundary[i]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + i * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (i == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                       : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (i == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            // Preserve actual section and OS page qualification while admitting
            // the shared native GT/GC protocol and genuine playing-state cell.
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedVelocityProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
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
            transform.Store(NativePedVelocityProfile::TypeOffset, static_cast<unsigned char>(3));
            transform.Store(NativePedVelocityProfile::LinearOffset, CVector(1, 2, 3));
            transform.Store(NativePedVelocityProfile::AngularOffset, CVector(4, 5, 6));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            VelocityTestHost::Reset();
        }
        VelocityBits Bits(NativePedVelocityKind kind) const
        {
            VelocityBits bits;
            std::memcpy(bits.data(),
                        transform.pool.objects +
                            (kind == NativePedVelocityKind::Linear ? NativePedVelocityProfile::LinearOffset : NativePedVelocityProfile::AngularOffset),
                        sizeof(bits));
            return bits;
        }
    };
}

int main(int argc, char** argv)
{
    using Memory = NativePedVelocityMemory;
    using Kind = NativePedVelocityKind;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native velocity evidence stays in its explicit workspace output directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123,
          "shipping qualification still refuses this actual non-GTA executable without publishing a binding");
    VelocityFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const CVector         target(10, 20, 30), sentinel(101, 102, 103);
    CVector               output = sentinel;
    Memory::Receipt       receipt;
    Check(unowned.Read(1, Kind::Linear, output) == ClockResult::NotInitialized && VelocityRaw(output) == VelocityRaw(sentinel) &&
              unowned.Set(1, Kind::Linear, target, &receipt) == ClockResult::NotInitialized && !receipt.entered && !VelocityTestHost::exchanges,
          "native pool/image identity alone cannot supply vector observation or mutation authority");
    Check(Memory::Revalidate(binding), "all actual native linear/angular producer and consumer pins qualify on real owned image pages");
    for (const auto kind : {Kind::Linear, Kind::Angular})
    {
        fixture.Reset();
        const auto original = fixture.Bits(kind);
        const auto otherKind = kind == Kind::Linear ? Kind::Angular : Kind::Linear;
        const auto other = fixture.Bits(otherKind);
        Check(memory.Read(1, kind, output) == ClockResult::Ok && VelocityRaw(output) == original,
              "actual native physical vectors copy raw units into caller storage without script scaling or a borrowed SDK alias");
        Check(memory.Set(1, kind, target, &receipt) == ClockResult::Ok && receipt.handle == 1 && receipt.kind == kind && receipt.entered && receipt.verified &&
                  receipt.nativePed == reinterpret_cast<std::uintptr_t>(fixture.transform.pool.objects) && receipt.original == original &&
                  receipt.target == VelocityRaw(target) && receipt.observed == VelocityRaw(target) && receipt.attemptedMask == 7 && receipt.appliedMask == 7 &&
                  receipt.observedMask == 7 && !receipt.foreignMask && !receipt.unchangedMask && VelocityTestHost::exchanges == 3 &&
                  fixture.Bits(kind) == VelocityRaw(target) && fixture.Bits(otherKind) == other,
              "three confirmed aligned scalar stores update only the selected actual vector and publish honest receipt masks");
        VelocityTestHost::Reset();
        Check(memory.Set(1, kind, target, &receipt) == ClockResult::Ok && receipt.verified && !receipt.entered && receipt.unchangedMask == 7 &&
                  !receipt.attemptedMask && !receipt.appliedMask && !VelocityTestHost::exchanges,
              "already-equal components are verified without manufacturing write entry or mutation debt");
    }
    fixture.Reset();
    const VelocityBits unusual{0x7fc12345, 0x80000000, 0x7f800000};
    const auto         raw = VelocityVector(unusual);
    Check(memory.Set(1, Kind::Angular, raw, &receipt) == ClockResult::Ok && fixture.Bits(Kind::Angular) == unusual &&
              memory.Read(1, Kind::Angular, output) == ClockResult::Ok && VelocityRaw(output) == unusual,
          "raw NaN payload, signed zero and infinity survive the genuine low-level contract without guessed clamps or fake SDK state repair");
    fixture.Reset();
    const auto* alias = reinterpret_cast<const CVector*>(fixture.transform.pool.objects + NativePedVelocityProfile::LinearOffset + sizeof(float));
    const auto  copied = VelocityRaw(*alias);
    Check(memory.Set(1, Kind::Linear, *alias, &receipt) == ClockResult::Ok && fixture.Bits(Kind::Linear) == copied,
          "a complete stable source copy precedes writes even when caller storage overlaps the destination vector");
    for (const auto invalid : {static_cast<Kind>(2), static_cast<Kind>(0xffffffff)})
    {
        fixture.Reset();
        output = sentinel;
        Check(memory.Read(1, invalid, output) == ClockResult::InvalidArgument && VelocityRaw(output) == VelocityRaw(sentinel) &&
                  memory.Set(1, invalid, target, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !VelocityTestHost::exchanges,
              "invalid typed selectors preserve output and refuse native mutation");
    }
    for (unsigned refusal = 0; refusal < 7; ++refusal)
    {
        fixture.Reset();
        output = sentinel;
        if (refusal == 0)
            fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
        if (refusal == 1)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (refusal == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (refusal == 3)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
        if (refusal == 4)
            fixture.transform.Store(NativePedVelocityProfile::TypeOffset, static_cast<unsigned char>(4));
        if (refusal == 5)
            fixture.transform.pool.flags[0] = 2;
        if (refusal == 6)
            PoolTestHost::ownerResult = ClockResult::Expired;
        Check(memory.Read(1, Kind::Linear, output) != ClockResult::Ok && VelocityRaw(output) == VelocityRaw(sentinel) &&
                  memory.Set(1, Kind::Linear, target, &receipt) != ClockResult::Ok && !receipt.entered && !VelocityTestHost::exchanges,
              "native GT/GC/playing scope, actual ped type/generation and borrowed owner remain mandatory for both vector paths");
    }
    fixture.Reset();
    Check(memory.Read(1, Kind::Linear, output) == ClockResult::Ok, "measure actual guarded read boundaries once for bounded refusal scheduling");
    const auto reads = PoolTestHost::reads;
    const auto owners = PoolTestHost::ownerCalls;
    for (const auto failure : {1u, reads / 2, reads})
    {
        fixture.Reset();
        PoolTestHost::failAt = failure;
        output = sentinel;
        Check(memory.Read(1, Kind::Linear, output) != ClockResult::Ok && VelocityRaw(output) == VelocityRaw(sentinel),
              "early, middle and final actual OS-read refusals preserve caller vector output");
    }
    for (const auto refusal : {1u, owners})
    {
        fixture.Reset();
        PoolTestHost::revokeAt = refusal;
        output = sentinel;
        Check(memory.Read(1, Kind::Linear, output) == ClockResult::WrongThread && VelocityRaw(output) == VelocityRaw(sentinel),
              "revoked first or final callback admission cannot publish a native vector copy");
    }
    for (const bool fail : {false, true})
    {
        fixture.Reset();
        VelocityTestHost::watchedSource = &target;
        VelocityTestHost::sourceFailure = fail;
        VelocityTestHost::unstableSource = !fail;
        Check(memory.Set(1, Kind::Linear, target, &receipt) == ClockResult::ReadFailure && !receipt.entered && !VelocityTestHost::exchanges,
              "unreadable or changing whole source snapshots refuse before any destination store");
    }
    for (unsigned failure = 1; failure <= 3; ++failure)
    {
        fixture.Reset();
        const auto original = fixture.Bits(Kind::Linear);
        VelocityTestHost::failBefore = failure;
        Check(memory.Set(1, Kind::Linear, target, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.verified &&
                  receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && receipt.attemptedMask == (1u << failure) - 1 &&
                  receipt.appliedMask == (1u << (failure - 1)) - 1 && VelocityTestHost::exchanges == failure,
              "failure at each scalar CAS reports attempted versus confirmed components without retry or deferred rollback");
        const auto actual = fixture.Bits(Kind::Linear);
        for (unsigned component = 0; component < 3; ++component)
            Check(actual[component] == (component < failure - 1 ? VelocityRaw(target)[component] : original[component]),
                  "a partial native vector keeps accepted components and leaves untouched components intact");
    }
    fixture.Reset();
    VelocityTestHost::failAfter = 1;
    Check(memory.Set(1, Kind::Linear, target, &receipt) == ClockResult::WriteFailure && receipt.entered && receipt.attemptedMask == 1 && !receipt.appliedMask &&
              VelocityTestHost::exchanges == 1 && fixture.Bits(Kind::Linear)[0] == VelocityRaw(target)[0],
          "an unacknowledged successful scalar write persists while the receipt honestly declines to claim confirmation");
    fixture.Reset();
    VelocityTestHost::before = [&]
    {
        if (VelocityTestHost::exchanges == 2)
        {
            const std::uint32_t foreign = 0x445e0000;
            std::memcpy(fixture.transform.pool.objects + NativePedVelocityProfile::LinearOffset + 4, &foreign, sizeof(foreign));
        }
    };
    Check(memory.Set(1, Kind::Linear, target, &receipt) == ClockResult::OwnerConflict && receipt.appliedMask == 1 && receipt.attemptedMask == 3 &&
              receipt.foreignMask == 2 && fixture.Bits(Kind::Linear)[0] == VelocityRaw(target)[0] && fixture.Bits(Kind::Linear)[1] == 0x445e0000 &&
              VelocityTestHost::exchanges == 2,
          "a foreign component arriving at CAS wins without being overwritten or repaired, while earlier accepted intent persists");
    fixture.Reset();
    VelocityTestHost::after = [&]
    {
        if (VelocityTestHost::exchanges == 1)
            PoolTestHost::ownerResult = ClockResult::Expired;
    };
    Check(memory.Set(1, Kind::Linear, target, &receipt) == ClockResult::Expired && receipt.entered && receipt.appliedMask == 1 && receipt.attemptedMask == 1 &&
              VelocityTestHost::exchanges == 1 && fixture.Bits(Kind::Linear)[0] == VelocityRaw(target)[0],
          "scope expiry after one accepted scalar forbids later stores and supplies no cross-callback repair lease");
    fixture.Reset();
    ClockResult     nestedResult = ClockResult::Ok;
    Memory::Receipt nested;
    VelocityTestHost::after = [&]
    {
        if (VelocityTestHost::exchanges == 1)
            nestedResult = memory.Set(1, Kind::Angular, target, &nested);
    };
    Check(memory.Set(1, Kind::Linear, target, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered &&
              VelocityTestHost::exchanges == 3,
          "a reentrant mutation cannot interleave a second native vector transaction with the current one");
    fixture.Reset();
    const auto foreign = static_cast<unsigned char>(NativePedVelocityProfile::Code[0].bytes[0] ^ 1);
    fixture.transform.image.Patch(NativePedVelocityProfile::Code[0].rva, &foreign, 1, PAGE_EXECUTE_READ);
    output = sentinel;
    Check(!Memory::Revalidate(binding) && memory.Read(1, Kind::Angular, output) == ClockResult::BindingChanged &&
              VelocityRaw(output) == VelocityRaw(sentinel) && memory.Set(1, Kind::Angular, target, &receipt) == ClockResult::BindingChanged &&
              !receipt.entered && !VelocityTestHost::exchanges,
          "foreign native producer/consumer instructions cannot reuse the vector layout or mutation path");
    PoolTestHost::image = 0;
    std::printf("Passed %u native ped-velocity checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
