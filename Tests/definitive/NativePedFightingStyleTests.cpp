/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual MTA style/extra policy over qualified native storage
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedFightingStyleHistoricalShotMain
#include "NativePedShotFlagsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedFightingStyleMemory.h"
#include "../../Client/game_de/NativePedFightingStyleProfile.h"
#include <game/CPedFightingStylePolicy.h>

// Historical owned-image/pool machinery and its final CAS seam compile once.
// Only the final exchange/read boundary is injected; the new property uses its
// production metadata, image, thread, GC and pool identity checks throughout.
#define GetModuleHandleW                   PoolTestHost::Module
#define ReadProcessMemory                  ShotHost::Read
#define MTA_DE_PED_FIGHTING_STYLE_EXCHANGE ShotHost::Exchange
#include "../../Client/game_de/NativePedFightingStyleMemory.cpp"
#undef MTA_DE_PED_FIGHTING_STYLE_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace FP = NativePedFightingStyleProfile;
    struct FightingStyleFixture : ShotFixture
    {
        explicit FightingStyleFixture(const std::filesystem::path& directory) : ShotFixture(directory)
        {
            // The native move table shares executable .text pages with code;
            // admitting it read-only would invalidate neighboring code pins.
            for (const auto& pin : FP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Reset()
        {
            ShotFixture::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
                Store(slot, FP::WordOffset, std::uint32_t{0xa5030411});
        }
        std::uint32_t Word(std::size_t slot = 0) const
        {
            std::uint32_t value = 0;
            std::memcpy(&value, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + FP::WordOffset, sizeof(value));
            return value;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = NativePedFightingStyleMemory;
    using Policy = MtaGame::CPedFightingStylePolicy;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "style proof writes only to its explicit workspace-owned evidence directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "the current unsupported executable cannot publish a style binding");
    FightingStyleFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Memory::Receipt       receipt;
    const auto            reset = [&]
    {
        fixture.Reset();
        return fixture.Ped();
    };
    Check(Memory::Revalidate(binding), "whole named style/move branch and exact constructor stores qualify inherited ped storage");
    struct DefaultAccess final : NativePedFightingStyleAccess
    {
    } defaultAccess;
    std::uint8_t actual = 0xda;
    Check(defaultAccess.ReadPedFightingStyle(NativePedReferenceToken{}, actual) == ClockResult::NotInitialized && actual == 0xda &&
              defaultAccess.SetPedFightingStyle(NativePedReferenceToken{}, 0xff, 0xff) == ClockResult::NotInitialized,
          "default access preserves raw output and refuses setter authority");

    // These independent literal masks also test the shared header used by the
    // actual classic caller; the fixture does not use that header as its oracle.
    const std::array<std::uint8_t, 6> extraMasks{1, 2, 4, 8, 16, 32};
    for (unsigned extra = 0; extra != 256; ++extra)
    {
        const auto mask = extra >= 1 && extra <= 6 ? extraMasks[extra - 1] : std::uint8_t{0};
        Check(Policy::ExtraMask(static_cast<std::uint8_t>(extra)) == mask, "shared classic policy preserves the full raw extra byte and exact six bit choices");
    }
    for (unsigned current = 0; current != 256; ++current)
    {
        const auto value = static_cast<std::uint8_t>(current);
        Check(!Policy::ChangesStyle(value, value) && Policy::ChangesStyle(value, static_cast<std::uint8_t>(value ^ 0xff)),
              "shared early return compares every raw style byte without restricting enum values");
    }

    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto style : {std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{4}, std::uint8_t{16}, std::uint8_t{254}, std::uint8_t{255}})
        {
            fixture.Reset();
            const auto word = std::uint32_t{0xa5030011} | (static_cast<std::uint32_t>(style) << FP::StyleShift);
            fixture.Store(slot, FP::WordOffset, word);
            const auto ped = fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
            actual = static_cast<std::uint8_t>(style ^ 0xff);
            Check(memory.ReadFightingStyle(ped, actual) == ClockResult::Ok && actual == style && fixture.Word(slot) == word && !ShotHost::exchanges,
                  "both registered native classes return raw style bytes beyond enum names with no extra-field interpretation or write");
        }

    auto ped = reset();
    Check(memory.SetFightingStyle(ped, 4, 6, &receipt) == ClockResult::Ok && receipt.verified && !receipt.changed && !receipt.entered && !receipt.accepted &&
              !receipt.attempts && fixture.Word() == 0xa5030411 && !ShotHost::exchanges,
          "same-style setter returns before changing extra exactly as the actual MTA caller does");
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto extra : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}, std::uint8_t{4}, std::uint8_t{5}, std::uint8_t{6},
                                 std::uint8_t{7}, std::uint8_t{255}})
        {
            fixture.Reset();
            const auto selected = fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
            const auto mask = extra >= 1 && extra <= 6 ? extraMasks[extra - 1] : std::uint8_t{0};
            const auto target = std::uint32_t{0xa5030811} | (static_cast<std::uint32_t>(mask) << FP::ExtraShift);
            Check(memory.SetFightingStyle(selected, 8, static_cast<std::uint8_t>(extra), &receipt) == ClockResult::Ok && receipt.changed && receipt.entered &&
                      receipt.accepted && receipt.verified && receipt.attempts == 1 && receipt.handle == selected.handle &&
                      receipt.nativePed == selected.nativePed && receipt.address == selected.nativePed + FP::WordOffset && receipt.requestedStyle == 8 &&
                      receipt.requestedExtra == extra && receipt.original == 0xa5030411 && receipt.target == target && receipt.observedValid &&
                      receipt.observed == target && fixture.Word(slot) == target && ShotHost::exchanges == 1 && ShotHost::acceptedWrites == 1,
                  "changed style applies full MTA OR policy in one CAS, preserving existing extra and both adjacent bytes");
        }
    for (const auto initial :
         {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{0x20}, std::uint8_t{0x80}, std::uint8_t{0xdf}, std::uint8_t{0xff}})
    {
        ped = reset();
        fixture.Store(0, FP::WordOffset, std::uint32_t{0xa5000411} | (static_cast<std::uint32_t>(initial) << FP::ExtraShift));
        const auto target = std::uint32_t{0xa5200811} | (static_cast<std::uint32_t>(initial) << FP::ExtraShift);
        Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::Ok && receipt.verified && fixture.Word() == target && ShotHost::acceptedWrites == 1,
              "old low/high extra bits are retained by OR; native07FE reset and move-six policy are never substituted");
    }
    ped = reset();
    actual = 0xda;
    Check(unowned.ReadFightingStyle(ped, actual) == ClockResult::NotInitialized && actual == 0xda &&
              unowned.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::NotInitialized && !receipt.entered && !ShotHost::exchanges,
          "readable native storage cannot replace current callback authority");

    ped = reset();
    ShotHost::before = [&]
    {
        if (ShotHost::exchanges == 1)
            fixture.Store(0, FP::WordOffset, fixture.Word() ^ std::uint32_t{0x00800001});
    };
    Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 2 &&
              ShotHost::exchanges == 2 && ShotHost::acceptedWrites == 1 && fixture.Word() == 0xa5a30810,
          "unaccepted changes to neighbors and an unselected extra bit are reacquired and merged without accepted replay");
    ped = reset();
    ShotHost::after = [&] { fixture.Store(0, FP::WordOffset, fixture.Word() ^ std::uint32_t{0x00800001}); };
    Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified && receipt.attempts == 1 &&
              ShotHost::acceptedWrites == 1 && fixture.Word() == 0xa5a30810 && fixture.Word() != receipt.target,
          "post-acceptance independent extra and neighbor changes are retained without replay");
    for (const auto owned : {std::uint32_t{0x00000100}, std::uint32_t{0x00200000}})
    {
        ped = reset();
        ShotHost::before = [&] { fixture.Store(0, FP::WordOffset, fixture.Word() ^ owned); };
        Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
                  ShotHost::exchanges == 1 && !ShotHost::acceptedWrites && fixture.Word() == (0xa5030411 ^ owned),
              "competing style or selected extra bit effects are never overwritten or retried");
    }
    ped = reset();
    ShotHost::before = [&] { fixture.Store(0, FP::WordOffset, fixture.Word() ^ std::uint32_t{1}); };
    Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted &&
              receipt.attempts == FP::MaximumAttempts && ShotHost::exchanges == FP::MaximumAttempts && !ShotHost::acceptedWrites,
          "continually changing unowned neighbors cannot cause an endless setter loop");
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        ped = reset();
        if (failure == 0)
            ShotHost::failBefore = true;
        if (failure == 1)
            ShotHost::failAfter = true;
        if (failure == 2)
            ShotHost::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (failure == 3)
            ShotHost::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (failure == 4)
            ShotHost::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (failure == 5)
            ShotHost::after = [&] { fixture.Store(0, FP::WordOffset, (fixture.Word() & ~FP::StyleMask) | std::uint32_t{0x900}); };
        Check(memory.SetFightingStyle(ped, 8, 6, &receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.attempts == 1 &&
                  ShotHost::exchanges == 1 && ShotHost::acceptedWrites == (failure == 0 ? 0u : 1u) &&
                  (failure >= 1 && failure <= 4 ? fixture.Word() == 0xa5230811 : true),
              "entered faults and lost acknowledgements retain possible effects without rollback or replay");
    }
    ped = reset();
    Memory::Receipt nested;
    ClockResult     nestedResult = ClockResult::Ok;
    ShotHost::before = [&] { nestedResult = memory.SetFightingStyle(ped, 9, 5, &nested); };
    Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::Ok && nestedResult == ClockResult::Busy && !nested.entered && ShotHost::exchanges == 1 &&
              ShotHost::acceptedWrites == 1,
          "nested style writes cannot borrow a live mutation admission");
    for (bool neighborOnly : {false, true})
    {
        ped = reset();
        unsigned seen = 0;
        ShotHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
        {
            if (at == ped.nativePed + FP::WordOffset && bytes == 4 && ++seen == 1)
                fixture.Store(0, FP::WordOffset, fixture.Word() ^ (neighborOnly ? std::uint32_t{0x00800001} : std::uint32_t{0x100}));
        };
        actual = 0xda;
        const auto result = memory.ReadFightingStyle(ped, actual);
        Check(neighborOnly ? result == ClockResult::Ok && actual == 4 : result == ClockResult::OwnerConflict && actual == 0xda,
              "getter coherence concerns only the raw style byte while independent extra and neighbor changes remain unowned");
    }
    for (const auto& pin : FP::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        actual = 0xda;
        Check(memory.ReadFightingStyle(ped, actual) != ClockResult::Ok && actual == 0xda && memory.SetFightingStyle(ped, 8, 6, &receipt) != ClockResult::Ok &&
                  !receipt.entered && !ShotHost::exchanges,
              "every exact native storage-role and initialization pin remains required before admission");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.SetFightingStyle(ped, 8, 6, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !ShotHost::exchanges, "a foreign thread cannot mutate qualified style storage");
    ped = reset();
    Check(memory.SetFightingStyle(ped, 8, 6, &receipt) == ClockResult::Ok && receipt.verified, "write admission releases after every bounded failure path");
    std::puts("Production native ped fighting-style primitive fixture completed");
    std::printf("Passed %u native ped-fighting-style checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
