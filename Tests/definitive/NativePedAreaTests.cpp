/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only area BYTE reads/CAS on owned generic/player native ped pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeAreaHistoricalTransformMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativePedAreaMemory.h"
#include "../../Client/game_de/NativePedAreaProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>
namespace AreaHost
{
    unsigned              exchanges = 0;
    bool                  failBefore = false, failAfter = false;
    std::function<void()> before, after;
    bool                  Exchange(std::uintptr_t at, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        CHAR e = 0, t = 0;
        std::memcpy(&e, &expected, 1);
        std::memcpy(&t, &target, 1);
        const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile CHAR*>(at), t, e);
        std::memcpy(&observed, &previous, 1);
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
        exchanges = 0;
        failBefore = failAfter = false;
        before = after = {};
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_AREA_EXCHANGE AreaHost::Exchange
#include "../../Client/game_de/NativePedAreaMemory.cpp"
#undef MTA_DE_PED_AREA_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace AreaP = NativePedAreaProfile;
    struct AreaFixtureBase
    {
        TransformFixture transform;
        explicit AreaFixtureBase(const std::filesystem::path& directory) : transform(directory)
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
    struct AreaFixture : AreaFixtureBase
    {
        explicit AreaFixture(const std::filesystem::path& path) : AreaFixtureBase(path)
        {
            for (const auto& pin : AreaP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& group : AreaP::Groups)
            {
                const auto entry = transform.image.image + group.entryRva;
                transform.image.Patch(group.slotRva, &entry, sizeof(entry), PAGE_READONLY);
                transform.image.Patch(group.tableRva + group.index * 4, &group.branchRva, sizeof(group.branchRva), PAGE_EXECUTE_READ);
            }
            Reset();
        }
        unsigned char* Address(std::size_t slot = 0) { return transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + AreaP::AreaOffset; }
        void           Reset()
        {
            AreaFixtureBase::Reset();
            AreaHost::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, AreaP::AreaOffset - 1, std::uint8_t{0xa5});
                Store(slot, AreaP::AreaOffset, std::uint8_t{17});
                Store(slot, AreaP::AreaOffset + 1, std::uint8_t{0x5a});
            }
        }
    };
    AreaFixture* changingArea = nullptr;
    void         ChangeArea()
    {
        *changingArea->Address() = 31;
    }
}
int main(int argc, char** argv)
{
    using AreaMemory = NativePedAreaMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "area evidence uses explicit workspace output");
    AreaMemory::Binding rejected{123};
    Check(!AreaMemory::Qualify(rejected) && rejected.image == 123, "wrong host cannot qualify native area binding");
    AreaFixture areaFixture(argv[1]);
    changingArea = &areaFixture;
    PoolTestHost::image = areaFixture.transform.image.image;
    const AreaMemory::Binding areaBinding{PoolTestHost::image};
    AreaMemory                area(areaBinding, &PoolTestHost::Owner), unownedArea(areaBinding, nullptr);
    Check(AreaMemory::Revalidate(areaBinding), "both exact named char area producer/consumer selections qualify the native BYTE field");
    auto areaPed = areaFixture.Ped();
    AreaHost::Reset();
    std::uint8_t        raw = 0xcc;
    AreaMemory::Receipt receipt;
    Check(unownedArea.ReadAreaCode(areaPed, raw) == ClockResult::NotInitialized && raw == 0xcc &&
              unownedArea.SetAreaCode(areaPed, 33, &receipt) == ClockResult::NotInitialized && !receipt.entered && !AreaHost::exchanges,
          "native field presence alone grants no callback authority");
    for (const auto handle : {std::uint32_t{1}, std::uint32_t{0x87f}})
        for (unsigned input = 0; input != 256; ++input)
        {
            areaFixture.Reset();
            areaPed = areaFixture.Ped(handle);
            AreaHost::Reset();
            const auto slot = areaPed.slot;
            const auto initial = static_cast<std::uint8_t>(input);
            areaFixture.Store(slot, AreaP::AreaOffset, initial);
            raw = 0xcc;
            Check(area.ReadAreaCode(areaPed, raw) == ClockResult::Ok && raw == initial,
                  "generic and player ped getter preserve all256 native area BYTE values");
            const auto target = static_cast<std::uint8_t>(input ^ 0xffu);
            receipt = {};
            Check(area.SetAreaCode(areaPed, target, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                      receipt.original == initial && receipt.target == target && receipt.observed == target &&
                      receipt.address == reinterpret_cast<std::uintptr_t>(areaFixture.Address(slot)) && AreaHost::exchanges == 1 &&
                      *areaFixture.Address(slot) == target && areaFixture.Address(slot)[-1] == 0xa5 && areaFixture.Address(slot)[1] == 0x5a,
                  "one exact BYTE CAS preserves every input and neighboring byte, without command interior/player effects");
        }
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    Check(area.SetAreaCode(areaPed, 17, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && AreaHost::exchanges == 1,
          "same-value BYTE store still passes current token/owner and the single conditional boundary");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    raw = 0xcc;
    Check(area.ReadAreaCode(areaPed, raw) == ClockResult::Ok, "measure actual area read boundaries");
    const auto areaReads = PoolTestHost::reads, areaOwners = PoolTestHost::ownerCalls;
    for (unsigned failedRead = 1; failedRead <= areaReads; ++failedRead)
    {
        areaFixture.Reset();
        areaPed = areaFixture.Ped();
        AreaHost::Reset();
        PoolTestHost::failAt = failedRead;
        raw = 0xcc;
        Check(area.ReadAreaCode(areaPed, raw) != ClockResult::Ok && raw == 0xcc && !AreaHost::exchanges,
              "every actual read failure preserves output without native write");
    }
    for (unsigned revoked = 1; revoked <= areaOwners; ++revoked)
    {
        areaFixture.Reset();
        areaPed = areaFixture.Ped();
        AreaHost::Reset();
        PoolTestHost::revokeAt = revoked;
        raw = 0xcc;
        Check(area.ReadAreaCode(areaPed, raw) == ClockResult::WrongThread && raw == 0xcc,
              "each owner boundary may refuse area observation without publishing fallback");
    }
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(areaFixture.Address());
    PoolTestHost::after = &ChangeArea;
    raw = 0xcc;
    Check(area.ReadAreaCode(areaPed, raw) == ClockResult::OwnerConflict && raw == 0xcc, "changed area between snapshots cannot publish mixed identity");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(areaFixture.Address());
    PoolTestHost::after = &ChangeArea;
    Check(area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::OwnerConflict && !receipt.entered && !AreaHost::exchanges && *areaFixture.Address() == 31,
          "pre-entry area change refuses without overwrite");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    AreaHost::before = [&] { *areaFixture.Address() = 39; };
    Check(area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && AreaHost::exchanges == 1 &&
              *areaFixture.Address() == 39,
          "CAS conflict preserves independently changed byte and never retries");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    AreaHost::before = [&]
    {
        areaFixture.Address()[-1] = 0x81;
        areaFixture.Address()[1] = 0x7f;
    };
    Check(area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::Ok && AreaHost::exchanges == 1 && *areaFixture.Address() == 55 &&
              areaFixture.Address()[-1] == 0x81 && areaFixture.Address()[1] == 0x7f,
          "neighbor changes survive exact-width CAS without wider rewriting or neighbor retry");
    for (const bool late : {false, true})
    {
        areaFixture.Reset();
        areaPed = areaFixture.Ped();
        AreaHost::Reset();
        AreaHost::failBefore = !late;
        AreaHost::failAfter = late;
        Check(area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted && !receipt.verified &&
                  AreaHost::exchanges == 1 && *areaFixture.Address() == (late ? 55 : 17),
              "unknown entered BYTE effect is retained; neither fault path replays or rolls back");
    }
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    AreaHost::after = [] { PoolTestHost::ownerResult = ClockResult::Expired; };
    Check(area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::Expired && receipt.entered && receipt.accepted && !receipt.verified &&
              AreaHost::exchanges == 1 && *areaFixture.Address() == 55,
          "accepted byte persists after actual callback loss, requiring owner Stop without replay");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    AreaHost::after = [&] { areaFixture.transform.pool.flags[0] = 2; };
    Check(area.SetAreaCode(areaPed, 55, &receipt) != ClockResult::Ok && receipt.accepted && !receipt.verified && AreaHost::exchanges == 1 &&
              *areaFixture.Address() == 55,
          "accepted byte persists after occupied pool identity loss; stale token never reacquires another ped");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    ClockResult         nested = ClockResult::Ok;
    AreaMemory::Receipt nestedReceipt;
    AreaHost::before = [&] { nested = area.SetAreaCode(areaPed, 77, &nestedReceipt); };
    Check(area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::Ok && nested == ClockResult::Busy && !nestedReceipt.entered && AreaHost::exchanges == 1,
          "recursive write cannot borrow the active mutation scope");
    for (const auto state : {std::uint32_t{6}, std::uint32_t{7}, std::uint32_t{8}})
    {
        areaFixture.Reset();
        areaPed = areaFixture.Ped();
        AreaHost::Reset();
        areaFixture.Global(NativeGameStateProfile::stateRva, state);
        raw = 0xcc;
        Check(area.ReadAreaCode(areaPed, raw) == ClockResult::NotInitialized && raw == 0xcc &&
                  area.SetAreaCode(areaPed, 55, &receipt) == ClockResult::NotInitialized && !receipt.entered && !AreaHost::exchanges,
              "frontend/loading state refuses area read/write before entry");
    }
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    areaFixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    Check(area.SetAreaCode(areaPed, 55, &receipt) != ClockResult::Ok && !receipt.entered && !AreaHost::exchanges, "native GC excludes area mutation");
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    ClockResult areaForeign = ClockResult::Ok;
    std::thread areaThread([&] { areaForeign = area.SetAreaCode(areaPed, 55, &receipt); });
    areaThread.join();
    Check(areaForeign == ClockResult::WrongThread && !receipt.entered && !AreaHost::exchanges, "callback alone cannot replace actual native thread identity");
    for (const auto& pin : AreaP::Code)
    {
        areaFixture.Reset();
        areaPed = areaFixture.Ped();
        AreaHost::Reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        areaFixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        raw = 0xcc;
        Check(area.ReadAreaCode(areaPed, raw) != ClockResult::Ok && raw == 0xcc, "each exact named producer/consumer dispatch pin is mandatory");
        areaFixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    for (const auto& group : AreaP::Groups)
    {
        areaFixture.Reset();
        areaPed = areaFixture.Ped();
        AreaHost::Reset();
        const auto changed = group.branchRva + 1;
        areaFixture.transform.image.Patch(group.tableRva + group.index * 4, &changed, sizeof(changed), PAGE_EXECUTE_READ);
        Check(area.SetAreaCode(areaPed, 55, &receipt) != ClockResult::Ok && !receipt.entered && !AreaHost::exchanges,
              "changed exact command selection refuses write authority");
        areaFixture.transform.image.Patch(group.tableRva + group.index * 4, &group.branchRva, sizeof(group.branchRva), PAGE_EXECUTE_READ);
    }
    areaFixture.Reset();
    areaPed = areaFixture.Ped();
    AreaHost::Reset();
    PoolTestHost::image = 0;
    raw = 0xcc;
    Check(area.ReadAreaCode(areaPed, raw) != ClockResult::Ok && raw == 0xcc && area.SetAreaCode(areaPed, 55, &receipt) != ClockResult::Ok && !receipt.entered,
          "image binding loss cannot reuse stale native property authority");
    std::puts("Production native ped area BYTE primitive fixture completed");
    std::printf("Passed %u native ped area checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
