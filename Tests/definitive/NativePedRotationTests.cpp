/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw current-facing reads/CAS over both actual occupied ped classes
 ****************************************************************************/
#define main NativeRotationHistoricalTransformMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedRotationMemory.h"
#include "../../Client/game_de/NativePedRotationProfile.h"
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>
namespace RotationHost
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
#define ReadProcessMemory RotationHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_ROTATION_EXCHANGE RotationHost::Exchange
#include "../../Client/game_de/NativePedRotationMemory.cpp"
#undef MTA_DE_PED_ROTATION_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    float RotationFloat(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    std::uint32_t RotationBits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        return bits;
    }
    struct RotationFixture
    {
        TransformFixture transform;
        explicit RotationFixture(const std::filesystem::path& directory) : transform(directory)
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
            for (const auto& pin : NativePedRotationProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            image.Patch(NativePedRotationProfile::ScriptRadiansScaleRva, &NativePedRotationProfile::ScriptRadiansScaleBits, sizeof(std::uint32_t),
                        PAGE_READONLY);
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
            RotationHost::Reset();
            Store(0, 0, transform.image.image + std::uintptr_t{0x4318270});
            Store(8, 0, transform.image.image + std::uintptr_t{0x4319730});
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, 0x3a, std::uint16_t{0xffff});
                Store(slot, 0x780, std::uint32_t{0xaabbccdd});
                Store(slot, 0x784, std::uint32_t{0x3f800000});
                Store(slot, 0x788, std::uint32_t{0xffc01234});
                Store(slot, 0x78c, std::uint32_t{0x41700000});
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
    using RotationBytes = std::array<unsigned char, NativePedPoolProfile::ObjectStride>;
    RotationBytes RotationSnapshot(const RotationFixture& f, std::size_t slot = 0)
    {
        RotationBytes bytes;
        std::memcpy(bytes.data(), f.transform.pool.objects + slot * NativePedPoolProfile::ObjectStride, bytes.size());
        return bytes;
    }
    std::uint32_t RotationRaw(const RotationFixture& f, std::size_t offset = 0x784, std::size_t slot = 0)
    {
        std::uint32_t value;
        std::memcpy(&value, f.transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, 4);
        return value;
    }
    void RotationOnlyChanged(const RotationFixture& f, RotationBytes before, std::uint32_t bits, std::size_t slot = 0)
    {
        std::memcpy(before.data() + 0x784, &bits, 4);
        Check(RotationSnapshot(f, slot) == before,
              "current-facing intent changes precisely one DWORD and leaves the adjacent angle, speed and placement untouched");
    }
}
int main(int argc, char** argv)
{
    using Memory = NativePedRotationMemory;
    namespace P = NativePedRotationProfile;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "current-angle evidence stays in its explicit workspace directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping current-angle binding refuses the actual non-GTA test host");
    RotationFixture f(argv[1]);
    PoolTestHost::image = f.transform.image.image;
    const auto            image = f.transform.image.image;
    const Memory::Binding binding{image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto            reset = [&]()
    {
        f.Reset();
        return f.Ped();
    };
    auto            ped = reset();
    Memory::Receipt receipt;
    Check(Memory::Revalidate(binding), "current-angle provenance includes exact common producer and independent player-facing consumer");
    struct DefaultRotation final : NativePedRotationAccess
    {
    } defaults;
    float output = RotationFloat(0x1234abcd);
    Check(defaults.ReadPedCurrentRotation({}, output) == ClockResult::NotInitialized && RotationBits(output) == 0x1234abcd &&
              defaults.SetPedCurrentRotation({}, 1.0f) == ClockResult::NotInitialized,
          "optional token access refuses by default without a retained native owner");
    constexpr std::array<std::uint32_t, 12> patterns{0,          0x80000000, 0x3f800000, 0xbf800000, 0x447a0000, 0x7f7fffff,
                                                     0xff7fffff, 0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7f800001};
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto bits : patterns)
        {
            f.Reset();
            const auto handle = slot == 0 ? std::uint32_t{1} : (8u << 8) | 127u;
            ped = f.Ped(handle);
            f.Store(slot, 0x784, bits);
            output = RotationFloat(0x1234abcd);
            Check(memory.ReadCurrent(ped, output) == ClockResult::Ok && RotationBits(output) == bits,
                  "both actual ped classes expose all raw angle bits without arithmetic, clamping, degrees or normalization");
            const auto before = RotationSnapshot(f, slot);
            const auto target = bits ^ 0x80000000u;
            Check(memory.SetCurrent(ped, RotationFloat(target), &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                      receipt.original == bits && receipt.target == target && receipt.observed == target && receipt.observedValid &&
                      RotationHost::exchanges == 1,
                  "actual raw scalar setter performs one acknowledged width-checked current-facing exchange");
            RotationOnlyChanged(f, before, target, slot);
        }
    ped = reset();
    output = RotationFloat(0x1234abcd);
    auto before = RotationSnapshot(f);
    Check(unowned.ReadCurrent(ped, output) == ClockResult::NotInitialized && RotationBits(output) == 0x1234abcd &&
              unowned.SetCurrent(ped, 2.0f, &receipt) == ClockResult::NotInitialized && !receipt.entered && RotationSnapshot(f) == before,
          "native bytes alone do not replace real registered-ped callback authority");
    RotationHost::before = [&] { f.Store(0, 0x784, std::uint32_t{0x42f60000}); };
    Check(memory.SetCurrent(ped, 2.0f, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && RotationHost::exchanges == 1,
          "a competing final angle store is refused without replay or overwrite");
    RotationOnlyChanged(f, before, 0x42f60000);
    ped = reset();
    before = RotationSnapshot(f);
    unsigned fieldReads = 0;
    RotationHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
    {
        if (at == ped.nativePed + P::CurrentOffset && bytes == 4 && ++fieldReads == 1)
            f.Store(0, 0x784, std::uint32_t{0x41200000});
    };
    output = RotationFloat(0x1234abcd);
    Check(memory.ReadCurrent(ped, output) == ClockResult::OwnerConflict && RotationBits(output) == 0x1234abcd && !RotationHost::exchanges,
          "unstable raw angle snapshots refuse without replacing caller output");
    RotationOnlyChanged(f, before, 0x41200000);
    for (unsigned mode = 0; mode != 5; ++mode)
    {
        ped = reset();
        before = RotationSnapshot(f);
        RotationHost::after = [&]
        {
            if (mode == 0)
                f.Store(0, 0x784, std::uint32_t{0x42f60000});
            if (mode == 1)
                PoolTestHost::ownerResult = ClockResult::Expired;
            if (mode == 2)
                f.transform.pool.flags[0] = 2;
            if (mode == 3)
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (mode == 4)
                f.Store(0, 0x788, std::uint32_t{0x43210000});
        };
        const auto result = memory.SetCurrent(ped, 2.0f, &receipt);
        Check(receipt.entered && receipt.accepted && RotationHost::exchanges == 1 && (mode == 4 ? result == ClockResult::Ok : result != ClockResult::Ok),
              "post-exchange angle/owner/slot/GC failure retains accepted effects; independently owned neighbor changes do not authorize repair");
        Check(RotationRaw(f) == (mode == 0 ? 0x42f60000u : 0x40000000u), "accepted and foreign current-facing values are never rolled back");
        if (mode != 4)
            RotationOnlyChanged(f, before, mode == 0 ? 0x42f60000u : 0x40000000u);
        else
            Check(RotationRaw(f, 0x788) == 0x43210000, "accepted current-angle intent preserves a later foreign adjacent angle");
    }
    for (const auto after : {false, true})
    {
        ped = reset();
        before = RotationSnapshot(f);
        RotationHost::failBefore = !after;
        RotationHost::failAfter = after;
        Check(memory.SetCurrent(ped, 2.0f, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted &&
                  receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && RotationHost::exchanges == 1,
              "failed exchange acknowledgements preserve uncertainty and require owning-runtime retirement");
        RotationOnlyChanged(f, before, after ? 0x40000000u : 0x3f800000u);
    }
    ped = reset();
    ClockResult nested = ClockResult::Ok;
    RotationHost::before = [&] { nested = memory.SetCurrent(ped, 4.0f); };
    Check(memory.SetCurrent(ped, 2.0f, &receipt) == ClockResult::Ok && nested == ClockResult::Busy && RotationHost::exchanges == 1,
          "reentrant current-facing writes cannot replay the outer operation");
    ped = reset();
    // Resolve is fixture setup, not part of the operation's fault boundaries.
    // Start both the measured path and each injection at the same counter zero.
    RotationHost::Reset();
    Check(memory.SetCurrent(ped, 2.0f, &receipt) == ClockResult::Ok, "measure real current-angle owner and guarded-copy path");
    const auto owners = PoolTestHost::ownerCalls, reads = PoolTestHost::reads, exchangeRead = RotationHost::exchangeRead;
    unsigned   acceptedExpiries = 0;
    for (unsigned step = 1; step <= owners; ++step)
    {
        ped = reset();
        RotationHost::Reset();
        PoolTestHost::revokeAt = step;
        Check(memory.SetCurrent(ped, 2.0f, &receipt) != ClockResult::Ok && RotationHost::exchanges <= 1,
              "every actual owner boundary refuses revocation without a replay");
        Check(RotationRaw(f) == (receipt.accepted ? 0x40000000u : 0x3f800000u), "owner revocation preserves precisely the accepted raw scalar effect");
        acceptedExpiries += receipt.accepted ? 1u : 0u;
    }
    Check(acceptedExpiries != 0, "current-facing expiry coverage includes persistent accepted effects");
    for (const auto step : {1u, exchangeRead, exchangeRead + 1, reads})
    {
        ped = reset();
        RotationHost::Reset();
        PoolTestHost::failAt = step;
        Check(memory.SetCurrent(ped, 2.0f, &receipt) != ClockResult::Ok && RotationHost::exchanges <= 1,
              "early, immediate pre-CAS and post-CAS guarded-copy faults refuse honestly");
        Check(RotationRaw(f) == (receipt.accepted ? 0x40000000u : 0x3f800000u), "guarded-copy faults cannot resurrect a prior angle");
    }
    for (const auto& pin : P::Code)
        for (const auto index : {std::size_t{0}, pin.size - 1})
        {
            ped = reset();
            const auto rva = pin.rva + index;
            const auto original = *reinterpret_cast<const unsigned char*>(image + rva);
            const auto changed = static_cast<unsigned char>(original ^ 1);
            f.transform.image.Patch(rva, &changed, 1, PAGE_EXECUTE_READ);
            output = RotationFloat(0x1234abcd);
            Check(!Memory::Revalidate(binding) && memory.ReadCurrent(ped, output) != ClockResult::Ok && RotationBits(output) == 0x1234abcd &&
                      memory.SetCurrent(ped, 2.0f, &receipt) != ClockResult::Ok && !receipt.entered && !RotationHost::exchanges,
                  "every exact current-angle producer/consumer pin endpoint is necessary before native observation or mutation");
            f.transform.image.Patch(rva, &original, 1, PAGE_EXECUTE_READ);
        }
    ped = reset();
    const auto wrongScale = P::ScriptRadiansScaleBits ^ 1;
    f.transform.image.Patch(P::ScriptRadiansScaleRva, &wrongScale, 4, PAGE_READONLY);
    Check(!Memory::Revalidate(binding) && memory.SetCurrent(ped, 2.0f, &receipt) != ClockResult::Ok && !receipt.entered,
          "native radian-producer provenance is independently pinned, never applied as SDK conversion");
    f.transform.image.Patch(P::ScriptRadiansScaleRva, &P::ScriptRadiansScaleBits, 4, PAGE_READONLY);
    ped = reset();
    before = RotationSnapshot(f);
    ClockResult foreignResult = ClockResult::Ok;
    std::thread foreign([&] { foreignResult = memory.SetCurrent(ped, 2.0f, &receipt); });
    foreign.join();
    Check(foreignResult == ClockResult::WrongThread && !receipt.entered && !RotationHost::exchanges && RotationSnapshot(f) == before,
          "current-facing raw storage retains actual native game-thread admission");
    std::printf("Production native ped current rotation fixture completed\n");
    std::printf("Passed %u native ped-current-rotation checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
