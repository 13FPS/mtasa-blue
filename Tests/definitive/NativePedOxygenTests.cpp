/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw oxygen reads/CAS require the registered player and its own published inline data
 ****************************************************************************/
#define main NativeOxygenHistoricalTransformMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedOxygenMemory.h"
#include "../../Client/game_de/NativePedOxygenProfile.h"
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>
namespace OxygenHost
{
    std::function<void()> watchedAction;
    void                  Watched()
    {
        if (watchedAction)
            watchedAction();
    }
    unsigned                                         exchanges = 0, exchangeRead = 0;
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
    bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        exchangeRead = PoolTestHost::reads;
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        LONG e = 0, t = 0;
        std::memcpy(&e, &expected, 4);
        std::memcpy(&t, &target, 4);
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), t, e);
        std::memcpy(&observed, &previous, 4);
        if (after)
            after();
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
    void Reset()
    {
        exchanges = exchangeRead = 0;
        failBefore = failAfter = false;
        before = after = {};
        afterRead = {};
        watchedAction = {};
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory OxygenHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_OXYGEN_EXCHANGE OxygenHost::Exchange
#include "../../Client/game_de/NativePedOxygenMemory.cpp"
#undef MTA_DE_PED_OXYGEN_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    float OxygenFloat(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    std::uint32_t OxygenBits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        return bits;
    }
    struct OxygenFixture
    {
        TransformFixture transform;
        explicit OxygenFixture(const std::filesystem::path& directory) : transform(directory)
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
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedMetadataProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativePedOxygenProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto control = image.image + NativePedOxygenProfile::PlayerControlRva;
            image.Patch(NativePedOxygenProfile::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        template <class T>
        void Store(std::size_t slot, std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, &value, sizeof(value));
        }
        void Reset()
        {
            transform.Reset(false);
            OxygenHost::Reset();
            Store(0, 0, transform.image.image + std::uintptr_t{0x4318270});
            Store(8, 0, transform.image.image + std::uintptr_t{0x4319730});
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, 0x3a, std::uint16_t{0x1234});
                Store(slot, 0x6a, static_cast<unsigned char>(0xfb));
                Store(slot, 0x5f0, std::uintptr_t{0});
                Store(slot, 0x7e0, std::int32_t{-1});
            }
            // These observations come from genuine occupied pool slots. No
            // current-player pointer, selector or model lookup grants access.
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
            // This exact occupied player owns the constructor-linked inline
            // data. The unrelated current-player index deliberately stays FF.
            const auto player = reinterpret_cast<std::uintptr_t>(transform.pool.objects + 8 * NativePedPoolProfile::ObjectStride);
            std::array<unsigned char, NativePedOxygenProfile::RowBytes> row;
            row.fill(static_cast<unsigned char>(0xa5));
            transform.image.Patch(NativePedOxygenProfile::BankRva, row.data(), row.size(), PAGE_READWRITE);
            Global(NativePedOxygenProfile::BankRva, player);
            Store(8, NativePedOxygenProfile::PlayerDataOffset,
                  transform.image.image + NativePedOxygenProfile::BankRva + NativePedOxygenProfile::InlineDataOffset);
            Store(8, NativePedOxygenProfile::PlayerTagOffset, std::int32_t{0});
            Global(NativePedOxygenProfile::BankRva + NativePedOxygenProfile::InlineDataOffset + NativePedOxygenProfile::BreathOffset,
                   std::uint32_t{0x3f800000});
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        }
        NativePedPoolReader::Value Ped(std::uint32_t handle = 1)
        {
            NativePedPoolReader        reader({transform.image.image}, &PoolTestHost::Owner);
            NativePedPoolReader::Value value;
            Check(reader.Resolve(handle, value) == ClockResult::Ok, "metadata requires a real occupied native slot and issued handle observation");
            return value;
        }
    };
    using OxygenBytes = std::array<unsigned char, NativePedOxygenProfile::RowBytes>;
    OxygenBytes OxygenSnapshot(const OxygenFixture& f)
    {
        OxygenBytes bytes;
        std::memcpy(bytes.data(), reinterpret_cast<void*>(f.transform.image.image + NativePedOxygenProfile::BankRva), bytes.size());
        return bytes;
    }
    std::uint32_t OxygenRaw(const OxygenFixture& f, std::size_t offset = NativePedOxygenProfile::BreathOffset)
    {
        std::uint32_t value;
        std::memcpy(&value,
                    reinterpret_cast<void*>(f.transform.image.image + NativePedOxygenProfile::BankRva + NativePedOxygenProfile::InlineDataOffset + offset), 4);
        return value;
    }
    void OxygenOnlyChanged(const OxygenFixture& f, OxygenBytes before, std::uint32_t bits)
    {
        std::memcpy(before.data() + NativePedOxygenProfile::InlineDataOffset + NativePedOxygenProfile::BreathOffset, &bits, 4);
        Check(OxygenSnapshot(f) == before, "raw oxygen changes precisely its DWORD, preserving stamina, capacity, player-info and every adjacent byte");
    }
}
int main(int argc, char** argv)
{
    using Memory = NativePedOxygenMemory;
    namespace P = NativePedOxygenProfile;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "oxygen evidence stays in its explicit workspace directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping oxygen binding refuses the actual non-GTA test host");
    OxygenFixture f(argv[1]);
    PoolTestHost::image = f.transform.image.image;
    const auto            image = f.transform.image.image;
    const Memory::Binding binding{image};
    const auto            at = image + P::BankRva + P::InlineDataOffset + P::BreathOffset;
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto            reset = [&]()
    {
        f.Reset();
        const auto result = f.Ped((8u << 8) | 127u);
        // Resolve is setup; each measured operation and fault starts at zero.
        OxygenHost::Reset();
        return result;
    };
    auto            ped = reset();
    Memory::Receipt receipt;
    Check(Memory::Revalidate(binding), "oxygen provenance includes independent depletion/replenishment and exact player-data publication");
    struct DefaultOxygen final : NativePedOxygenAccess
    {
    } defaults;
    float output = OxygenFloat(0x1234abcd);
    Check(defaults.ReadPedOxygenLevel({}, output) == ClockResult::NotInitialized && OxygenBits(output) == 0x1234abcd &&
              defaults.SetPedOxygenLevel({}, 1.0f) == ClockResult::NotInitialized,
          "optional oxygen token access refuses by default without real callback ownership");
    constexpr std::array<std::uint32_t, 12> patterns{0,          0x80000000, 0x3f800000, 0xbf800000, 0x447a0000, 0x7f7fffff,
                                                     0xff7fffff, 0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7f800001};
    for (const auto bits : patterns)
    {
        ped = reset();
        f.Global(P::BankRva + P::InlineDataOffset + P::BreathOffset, bits);
        output = OxygenFloat(0x1234abcd);
        Check(memory.ReadOxygen(ped, output) == ClockResult::Ok && OxygenBits(output) == bits,
              "own published player data exposes all raw breath bits without stamina/stat math, percent scaling or clamping");
        const auto before = OxygenSnapshot(f);
        const auto target = bits ^ 0x80000000u;
        Check(memory.SetOxygen(ped, OxygenFloat(target), &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                  receipt.original == bits && receipt.target == target && receipt.observed == target && receipt.observedValid &&
                  receipt.nativePed == ped.nativePed && receipt.playerData == image + P::BankRva + P::InlineDataOffset && receipt.address == at &&
                  OxygenHost::exchanges == 1,
              "raw oxygen setter performs exactly one conditional DWORD exchange on this ped's constructor-owned inline data");
        OxygenOnlyChanged(f, before, target);
    }
    for (const auto index : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(0xff)})
    {
        ped = reset();
        f.Global(0x522bcf6, index);
        Check(memory.ReadOxygen(ped, output) == ClockResult::Ok && memory.SetOxygen(ped, 2.0f, &receipt) == ClockResult::Ok,
              "the unrelated current-player selector never substitutes a different registered receiver or data pointer");
    }
    for (unsigned mode = 0; mode != 13; ++mode)
    {
        ped = reset();
        if (mode == 0)
            f.Store(8, P::PlayerDataOffset, std::uintptr_t{0});
        if (mode == 1)
            f.Store(8, P::PlayerDataOffset, std::uintptr_t{1});
        if (mode == 2)
            f.Store(8, P::PlayerDataOffset, image + P::BankRva + P::RowBytes + P::InlineDataOffset);
        if (mode == 3)
            f.Store(8, P::PlayerDataOffset, ped.nativePed + 0x900);
        if (mode == 4)
            f.Store(8, P::PlayerTagOffset, std::int32_t{-1});
        if (mode == 5)
            f.Global(P::BankRva, std::uintptr_t{0});
        if (mode == 6)
            f.Global(P::BankRva, reinterpret_cast<std::uintptr_t>(f.transform.pool.objects));
        if (mode == 7)
        {
            ped = f.Ped();
            OxygenHost::Reset();
        }
        if (mode == 8)
        {
            f.Store(0, P::PlayerDataOffset, image + P::BankRva + P::InlineDataOffset);
            f.Store(0, P::PlayerTagOffset, std::int32_t{0});
            ped = f.Ped();
            OxygenHost::Reset();
        }
        if (mode == 9)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (mode == 10)
            f.transform.pool.flags[8] = 126;
        if (mode == 11)
            f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 12)
            f.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
        const auto before = OxygenSnapshot(f);
        output = OxygenFloat(0x1234abcd);
        Check(memory.ReadOxygen(ped, output) != ClockResult::Ok && OxygenBits(output) == 0x1234abcd &&
                  memory.SetOxygen(ped, 2.0f, &receipt) != ClockResult::Ok && !receipt.entered && !OxygenHost::exchanges && OxygenSnapshot(f) == before,
              "null/foreign/unpublished player data, generic borrowing, stale pool, owner, GC and state loss refuse without touching output or data");
    }
    ped = reset();
    auto before = OxygenSnapshot(f);
    output = OxygenFloat(0x1234abcd);
    Check(unowned.ReadOxygen(ped, output) == ClockResult::NotInitialized && OxygenBits(output) == 0x1234abcd &&
              unowned.SetOxygen(ped, 2.0f, &receipt) == ClockResult::NotInitialized && !receipt.entered && OxygenSnapshot(f) == before,
          "native player and breath bytes cannot replace registered callback authority");
    OxygenHost::before = [&] { f.Global(P::BankRva + P::InlineDataOffset + P::BreathOffset, std::uint32_t{0x42f60000}); };
    Check(memory.SetOxygen(ped, 2.0f, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && OxygenHost::exchanges == 1,
          "competing native breath intent refuses without replay or overwrite");
    OxygenOnlyChanged(f, before, 0x42f60000);
    ped = reset();
    before = OxygenSnapshot(f);
    PoolTestHost::watched = at;
    OxygenHost::watchedAction = [&] { f.Global(P::BankRva + P::InlineDataOffset + P::BreathOffset, std::uint32_t{0x41200000}); };
    PoolTestHost::after = &OxygenHost::Watched;
    output = OxygenFloat(0x1234abcd);
    Check(memory.ReadOxygen(ped, output) == ClockResult::OwnerConflict && OxygenBits(output) == 0x1234abcd && !OxygenHost::exchanges,
          "unstable raw breath bookends refuse without consuming caller output");
    OxygenOnlyChanged(f, before, 0x41200000);
    for (unsigned mode = 0; mode != 7; ++mode)
    {
        ped = reset();
        OxygenHost::after = [&]
        {
            if (mode == 0)
                f.Global(P::BankRva + P::InlineDataOffset + P::BreathOffset, std::uint32_t{0x42f60000});
            if (mode == 1)
                PoolTestHost::ownerResult = ClockResult::Expired;
            if (mode == 2)
                f.transform.pool.flags[8] = 126;
            if (mode == 3)
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (mode == 4)
                f.Global(P::BankRva, std::uintptr_t{0});
            if (mode == 5)
                f.Store(8, P::PlayerDataOffset, std::uintptr_t{0});
            if (mode == 6)
                f.Global(P::BankRva + P::InlineDataOffset + 0x24, std::uint32_t{0x43210000});
        };
        const auto result = memory.SetOxygen(ped, 2.0f, &receipt);
        Check(receipt.entered && receipt.accepted && OxygenHost::exchanges == 1 && (mode == 6 ? result == ClockResult::Ok : result != ClockResult::Ok),
              "post-CAS breath/owner/slot/GC/publisher/link loss preserves accepted effects without replay or resurrection");
        Check(OxygenRaw(f) == (mode == 0 ? 0x42f60000u : 0x40000000u), "accepted and later foreign oxygen values are never rolled back");
        if (mode == 6)
            Check(OxygenRaw(f, 0x24) == 0x43210000, "independently changed stamina remains owned by its native writer");
    }
    for (const auto after : {false, true})
    {
        ped = reset();
        before = OxygenSnapshot(f);
        OxygenHost::failBefore = !after;
        OxygenHost::failAfter = after;
        Check(memory.SetOxygen(ped, 2.0f, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted &&
                  receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && OxygenHost::exchanges == 1,
              "failed exchange acknowledgement preserves uncertain effects for owning-runtime retirement");
        OxygenOnlyChanged(f, before, after ? 0x40000000u : 0x3f800000u);
    }
    ped = reset();
    ClockResult nested = ClockResult::Ok;
    OxygenHost::before = [&] { nested = memory.SetOxygen(ped, 4.0f); };
    Check(memory.SetOxygen(ped, 2.0f, &receipt) == ClockResult::Ok && nested == ClockResult::Busy && OxygenHost::exchanges == 1,
          "reentrant raw oxygen writes cannot replay the outer transaction");
    ped = reset();
    Check(memory.SetOxygen(ped, 2.0f, &receipt) == ClockResult::Ok, "measure real oxygen owner and guarded-copy path after Resolve setup");
    const auto owners = PoolTestHost::ownerCalls, reads = PoolTestHost::reads, exchangeRead = OxygenHost::exchangeRead;
    unsigned   acceptedExpiries = 0;
    for (unsigned step = 1; step <= owners; ++step)
    {
        ped = reset();
        PoolTestHost::revokeAt = step;
        Check(memory.SetOxygen(ped, 2.0f, &receipt) != ClockResult::Ok && OxygenHost::exchanges <= 1,
              "every actual callback owner boundary refuses revocation without replay");
        Check(OxygenRaw(f) == (receipt.accepted ? 0x40000000u : 0x3f800000u), "owner revocation preserves precisely accepted oxygen effects");
        acceptedExpiries += receipt.accepted ? 1u : 0u;
    }
    Check(acceptedExpiries != 0, "oxygen owner expiry includes persistent accepted effects");
    for (const auto step : {1u, exchangeRead, exchangeRead + 1, reads})
    {
        ped = reset();
        PoolTestHost::failAt = step;
        Check(memory.SetOxygen(ped, 2.0f, &receipt) != ClockResult::Ok && OxygenHost::exchanges <= 1,
              "early, pre-CAS and post-CAS guarded-copy faults refuse honestly");
        Check(OxygenRaw(f) == (receipt.accepted ? 0x40000000u : 0x3f800000u), "guarded-copy faults never resurrect old breath");
    }
    for (const auto& pin : P::Code)
        for (const auto index : {std::size_t{0}, pin.size - 1})
        {
            ped = reset();
            const auto rva = pin.rva + index;
            const auto original = *reinterpret_cast<const unsigned char*>(image + rva);
            const auto changed = static_cast<unsigned char>(original ^ 1);
            f.transform.image.Patch(rva, &changed, 1, PAGE_EXECUTE_READ);
            output = OxygenFloat(0x1234abcd);
            Check(!Memory::Revalidate(binding) && memory.ReadOxygen(ped, output) != ClockResult::Ok && OxygenBits(output) == 0x1234abcd &&
                      memory.SetOxygen(ped, 2.0f, &receipt) != ClockResult::Ok && !receipt.entered && !OxygenHost::exchanges,
                  "every exact breath/data-owner producer or consumer pin endpoint is required before observation or mutation");
            f.transform.image.Patch(rva, &original, 1, PAGE_EXECUTE_READ);
        }
    ped = reset();
    DWORD prior = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(image + P::BankRva), P::RowBytes, PAGE_READONLY, &prior) != 0,
          "owned inline player-data row can simulate runtime write revocation");
    Check(memory.SetOxygen(ped, 2.0f, &receipt) != ClockResult::Ok && !receipt.entered && !OxygenHost::exchanges,
          "read-only inline data refuses before attempting a native write");
    DWORD ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(image + P::BankRva), P::RowBytes, prior, &ignored) != 0, "restore own row protection");
    ped = reset();
    const auto control = image + P::PlayerControlRva;
    const auto wrongControl = control + 1;
    f.transform.image.Patch(P::PlayerVtableRva + 0x50, &wrongControl, sizeof(wrongControl), PAGE_READONLY);
    Check(memory.SetOxygen(ped, 2.0f, &receipt) != ClockResult::Ok && !receipt.entered && !OxygenHost::exchanges,
          "exact player-data owner requires the genuine player-control virtual relation");
    f.transform.image.Patch(P::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
    ped = reset();
    before = OxygenSnapshot(f);
    ClockResult foreignResult = ClockResult::Ok;
    std::thread foreign([&] { foreignResult = memory.SetOxygen(ped, 2.0f, &receipt); });
    foreign.join();
    Check(foreignResult == ClockResult::WrongThread && !receipt.entered && !OxygenHost::exchanges && OxygenSnapshot(f) == before,
          "raw breath retains actual native game-thread admission");
    std::printf("Production native ped oxygen fixture completed\n");
    std::printf("Passed %u native ped-oxygen checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
