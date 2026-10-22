/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw bike-knockoff bitfield reads/masked CAS over both actual native ped classes
 ****************************************************************************/
#define main NativeBikeFlagsHistoricalTransformMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedBikeFlagsMemory.h"
#include "../../Client/game_de/NativePedBikeFlagsProfile.h"
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>
namespace BikeFlagsHost
{
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
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory BikeFlagsHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_BIKE_FLAGS_EXCHANGE BikeFlagsHost::Exchange
#include "../../Client/game_de/NativePedBikeFlagsMemory.cpp"
#undef MTA_DE_PED_BIKE_FLAGS_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    struct BikeFlagsFixture
    {
        TransformFixture transform;
        explicit BikeFlagsFixture(const std::filesystem::path& directory) : transform(directory)
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
            for (const auto& pin : NativePedBikeFlagsProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
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
            BikeFlagsHost::Reset();
            Store(0, 0, transform.image.image + std::uintptr_t{0x4318270});
            Store(8, 0, transform.image.image + std::uintptr_t{0x4319730});
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, 0x3a, std::uint16_t{0xffff});
                Store(slot, 0x638, std::uint32_t{0xdeadbeef});
                Store(slot, 0x63c, std::uint32_t{0xa5a5a5a5});
                Store(slot, 0x640, std::uint32_t{0x10203040});
                Store(slot, 0x6a, static_cast<unsigned char>(0xfb));
                Store(slot, 0x5f0, std::uintptr_t{0});
                Store(slot, 0x7e0, std::int32_t{-1});
            }
            // These observations come from genuine occupied pool slots. No
            // current-player pointer, selector or model lookup grants access.
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
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
    using BikeFlagsBytes = std::array<unsigned char, NativePedPoolProfile::ObjectStride>;
    BikeFlagsBytes BikeFlagsSnapshot(const BikeFlagsFixture& f, std::size_t slot = 0)
    {
        BikeFlagsBytes bytes;
        std::memcpy(bytes.data(), f.transform.pool.objects + slot * NativePedPoolProfile::ObjectStride, bytes.size());
        return bytes;
    }
    std::uint32_t BikeFlagsRaw(const BikeFlagsFixture& f, std::size_t slot = 0)
    {
        std::uint32_t value;
        std::memcpy(&value, f.transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + NativePedBikeFlagsProfile::FlagsOffset, 4);
        return value;
    }
    void BikeFlagsOnlyChanged(const BikeFlagsFixture& f, BikeFlagsBytes before, std::uint32_t bits, std::size_t slot = 0)
    {
        std::memcpy(before.data() + NativePedBikeFlagsProfile::FlagsOffset, &bits, 4);
        Check(BikeFlagsSnapshot(f, slot) == before, "two-bit bike storage preserves every other ped word, flag, player-data link and transform byte");
    }
}
int main(int argc, char** argv)
{
    using Memory = NativePedBikeFlagsMemory;
    namespace P = NativePedBikeFlagsProfile;
    constexpr auto mask = P::KnockoffMask;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "bike flags evidence stays in its explicit workspace directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping bike flags binding refuses actual non-GTA test host");
    BikeFlagsFixture f(argv[1]);
    PoolTestHost::image = f.transform.image.image;
    const auto            image = f.transform.image.image;
    const Memory::Binding binding{image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto            reset = [&]()
    {
        f.Reset();
        const auto result = f.Ped();
        // Resolve is setup, so measured operations and fault injections agree.
        BikeFlagsHost::Reset();
        return result;
    };
    auto            ped = reset();
    Memory::Receipt receipt;
    Check(Memory::Revalidate(binding), "bike flags qualify exact named DWORD producer and common two-bit initialization");
    struct Defaults final : NativePedBikeFlagsAccess
    {
    } defaults;
    std::uint32_t output = 0x1234abcd;
    Check(defaults.ReadPedCantBeKnockedOffBike({}, output) == ClockResult::NotInitialized && output == 0x1234abcd &&
              defaults.SetPedCantBeKnockedOffBike({}, 0xffffffff) == ClockResult::NotInitialized,
          "default token bike flags access refuses without real callback ownership");
    constexpr std::array<std::uint32_t, 12> inputs{0, 1, 2, 3, 4, 5, 6, 7, 0x80000000, 0x80000001, 0xdeadbeef, 0xffffffff};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (std::uint32_t original = 0; original != 4; ++original)
            for (const auto input : inputs)
            {
                f.Reset();
                ped = f.Ped(slot == 0 ? 1u : (8u << 8) | 127u);
                BikeFlagsHost::Reset();
                const auto initial = (0xa5a5a5a5u & ~mask) | (original << P::Shift);
                f.Store(slot, P::FlagsOffset, initial);
                output = 0x1234abcd;
                Check(memory.ReadCantBeKnockedOffBike(ped, output) == ClockResult::Ok && output == original,
                      "both exact native ped classes expose the raw two-bit value including3 with no player or gameplay substitution");
                const auto before = BikeFlagsSnapshot(f, slot);
                const auto target = (initial & ~mask) | ((input & P::StoredValueMask) << P::Shift);
                Check(memory.SetCantBeKnockedOffBike(ped, input, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.requested == input && receipt.original == initial && receipt.target == target && receipt.observed == target &&
                          receipt.observedValid && receipt.attempts == 1 && BikeFlagsHost::exchanges == 1 && BikeFlagsRaw(f, slot) == target,
                      "full DWORD SDK intent truncates only through the genuine two-bit mask without clamping values0..2");
                BikeFlagsOnlyChanged(f, before, target, slot);
            }
    ped = reset();
    auto before = BikeFlagsSnapshot(f);
    output = 0x1234abcd;
    Check(unowned.ReadCantBeKnockedOffBike(ped, output) == ClockResult::NotInitialized && output == 0x1234abcd &&
              unowned.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::NotInitialized && !receipt.entered && BikeFlagsSnapshot(f) == before,
          "native two-bit bytes cannot replace the registered-ped callback owner");
    for (unsigned bit = 0; bit != 32; ++bit)
    {
        const auto neighbor = std::uint32_t{1} << bit;
        if (neighbor & mask)
            continue;
        ped = reset();
        before = BikeFlagsSnapshot(f);
        const auto original = BikeFlagsRaw(f);
        BikeFlagsHost::before = [&]
        {
            if (BikeFlagsHost::exchanges == 1)
                f.Store(0, P::FlagsOffset, BikeFlagsRaw(f) ^ neighbor);
        };
        const auto target = ((original ^ neighbor) & ~mask) | (2u << P::Shift);
        Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 2 &&
                  BikeFlagsHost::exchanges == 2 && BikeFlagsRaw(f) == target,
              "each unowned neighbor bit can be reacquired after one failed CAS without losing its writer's change");
        BikeFlagsOnlyChanged(f, before, target);
    }
    ped = reset();
    before = BikeFlagsSnapshot(f);
    const auto original = BikeFlagsRaw(f);
    BikeFlagsHost::before = [&] { f.Store(0, P::FlagsOffset, BikeFlagsRaw(f) ^ 1u); };
    Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              receipt.attempts == P::MaximumAttempts && BikeFlagsHost::exchanges == P::MaximumAttempts,
          "continuously changing neighboring flags exhaust the bounded unaccepted CAS budget without a write");
    BikeFlagsOnlyChanged(f, before, original ^ (P::MaximumAttempts % 2 ? 1u : 0u));
    for (const auto owned : {1u << P::Shift, 2u << P::Shift})
    {
        ped = reset();
        before = BikeFlagsSnapshot(f);
        const auto initial = BikeFlagsRaw(f);
        BikeFlagsHost::before = [&] { f.Store(0, P::FlagsOffset, BikeFlagsRaw(f) ^ owned); };
        Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
                  BikeFlagsHost::exchanges == 1,
              "a concurrent change to either owned bit is refused rather than replayed as a neighboring update");
        BikeFlagsOnlyChanged(f, before, initial ^ owned);
    }
    for (const auto owned : {false, true})
    {
        ped = reset();
        const auto initial = BikeFlagsRaw(f);
        unsigned   fieldReads = 0;
        BikeFlagsHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + P::FlagsOffset && bytes == 4 && ++fieldReads == 1)
                f.Store(0, P::FlagsOffset, initial ^ (owned ? 1u << P::Shift : 1u));
        };
        output = 0x1234abcd;
        const auto result = memory.ReadCantBeKnockedOffBike(ped, output);
        Check(owned ? result == ClockResult::OwnerConflict && output == 0x1234abcd : result == ClockResult::Ok && output == ((initial & mask) >> P::Shift),
              "getter coherence belongs only to the two owned bits; changing unowned flags do not become false conflicts");
    }
    for (const auto owned : {false, true})
    {
        ped = reset();
        const auto initial = BikeFlagsRaw(f);
        unsigned   fieldReads = 0;
        BikeFlagsHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + P::FlagsOffset && bytes == 4 && ++fieldReads == 1)
                f.Store(0, P::FlagsOffset, initial ^ (owned ? 1u << P::Shift : 1u));
        };
        const auto result = memory.SetCantBeKnockedOffBike(ped, 2, &receipt);
        Check(owned
                  ? result == ClockResult::OwnerConflict && !receipt.entered && !BikeFlagsHost::exchanges
                  : result == ClockResult::Ok && receipt.accepted && receipt.attempts == 1 && BikeFlagsRaw(f) == (((initial ^ 1u) & ~mask) | (2u << P::Shift)),
              "pre-entry bookends refuse stale owned pair intent while preserving independent neighbor changes in the first accepted write");
    }
    for (unsigned mode = 0; mode != 5; ++mode)
    {
        ped = reset();
        const auto initial = BikeFlagsRaw(f);
        BikeFlagsHost::after = [&]
        {
            if (mode == 0)
                f.Store(0, P::FlagsOffset, BikeFlagsRaw(f) ^ (1u << P::Shift));
            if (mode == 1)
                PoolTestHost::ownerResult = ClockResult::Expired;
            if (mode == 2)
                f.transform.pool.flags[0] = 2;
            if (mode == 3)
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (mode == 4)
                f.Store(0, P::FlagsOffset, BikeFlagsRaw(f) ^ 1u);
        };
        const auto result = memory.SetCantBeKnockedOffBike(ped, 2, &receipt);
        const auto accepted = (initial & ~mask) | (2u << P::Shift);
        Check(receipt.entered && receipt.accepted && BikeFlagsHost::exchanges == 1 && (mode == 4 ? result == ClockResult::Ok : result != ClockResult::Ok),
              "accepted pair changes are never replayed after owner/pool/GC/value loss; later neighboring writes remain independent");
        Check(BikeFlagsRaw(f) == (mode == 0   ? accepted ^ (1u << P::Shift)
                                  : mode == 4 ? accepted ^ 1u
                                              : accepted),
              "accepted and foreign flag effects persist without rollback");
    }
    for (const auto after : {false, true})
    {
        ped = reset();
        before = BikeFlagsSnapshot(f);
        const auto initial = BikeFlagsRaw(f);
        BikeFlagsHost::failBefore = !after;
        BikeFlagsHost::failAfter = after;
        Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted &&
                  receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && BikeFlagsHost::exchanges == 1,
              "failed exchange acknowledgements retain unknown partial effects and require owner retirement");
        BikeFlagsOnlyChanged(f, before, after ? (initial & ~mask) | (2u << P::Shift) : initial);
    }
    ped = reset();
    ClockResult nested = ClockResult::Ok;
    BikeFlagsHost::before = [&] { nested = memory.SetCantBeKnockedOffBike(ped, 3); };
    Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::Ok && nested == ClockResult::Busy && BikeFlagsHost::exchanges == 1,
          "reentrant pair intent cannot replay or replace the outer native write");
    ped = reset();
    Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) == ClockResult::Ok, "measure owner and guarded-copy boundaries after Resolve setup");
    const auto owners = PoolTestHost::ownerCalls, reads = PoolTestHost::reads, exchangeRead = BikeFlagsHost::exchangeRead;
    unsigned   acceptedExpiries = 0;
    for (unsigned step = 1; step <= owners; ++step)
    {
        ped = reset();
        const auto initial = BikeFlagsRaw(f);
        PoolTestHost::revokeAt = step;
        Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) != ClockResult::Ok && BikeFlagsHost::exchanges <= 1,
              "every actual callback owner boundary refuses revocation without replay");
        Check(BikeFlagsRaw(f) == (receipt.accepted ? (initial & ~mask) | (2u << P::Shift) : initial),
              "owner revocation retains precisely accepted bitfield effects");
        acceptedExpiries += receipt.accepted ? 1u : 0u;
    }
    Check(acceptedExpiries != 0, "owner expiry includes persistent accepted pair effects");
    for (const auto step : {1u, exchangeRead, exchangeRead + 1, reads})
    {
        ped = reset();
        const auto initial = BikeFlagsRaw(f);
        PoolTestHost::failAt = step;
        Check(memory.SetCantBeKnockedOffBike(ped, 2, &receipt) != ClockResult::Ok && BikeFlagsHost::exchanges <= 1,
              "early, pre-CAS and post-CAS guarded-copy faults refuse honestly");
        Check(BikeFlagsRaw(f) == (receipt.accepted ? (initial & ~mask) | (2u << P::Shift) : initial), "read faults never undo an accepted pair write");
    }
    for (const auto& pin : P::Code)
        for (const auto index : {std::size_t{0}, pin.size - 1})
        {
            ped = reset();
            const auto rva = pin.rva + index;
            const auto originalByte = *reinterpret_cast<const unsigned char*>(image + rva);
            const auto changed = static_cast<unsigned char>(originalByte ^ 1);
            f.transform.image.Patch(rva, &changed, 1, PAGE_EXECUTE_READ);
            output = 0x1234abcd;
            Check(!Memory::Revalidate(binding) && memory.ReadCantBeKnockedOffBike(ped, output) != ClockResult::Ok && output == 0x1234abcd &&
                      memory.SetCantBeKnockedOffBike(ped, 2, &receipt) != ClockResult::Ok && !receipt.entered && !BikeFlagsHost::exchanges,
                  "every exact named storage/common initialization pin endpoint is required before observation or mutation");
            f.transform.image.Patch(rva, &originalByte, 1, PAGE_EXECUTE_READ);
        }
    ped = reset();
    before = BikeFlagsSnapshot(f);
    ClockResult foreignResult = ClockResult::Ok;
    std::thread foreign([&] { foreignResult = memory.SetCantBeKnockedOffBike(ped, 2, &receipt); });
    foreign.join();
    Check(foreignResult == ClockResult::WrongThread && !receipt.entered && !BikeFlagsHost::exchanges && BikeFlagsSnapshot(f) == before,
          "raw two-bit storage retains actual native game-thread admission");
    std::printf("Production native ped bike flags fixture completed\n");
    std::printf("Passed %u native ped-bike-flags checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
