/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual raw world area copy over owned image and current world pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldAreaHistoricalLoadAllMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldAreaFixture.h"
#include "../../Client/game_de/NativeWorldAreaMemory.h"
#include "../../Client/game_de/NativeWorldGroundProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldAreaMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace AR = NativeWorldAreaMemory;
    namespace AP = NativeWorldAreaProfile;
    namespace AC = NativeStreamingCompletionContext;
    struct AreaFixture : LoadFixture
    {
        explicit AreaFixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        void Reset()
        {
            LoadFixture::Reset();
            NativeWorldAreaFixture::Install(transform.image.image, &LoadFixture::Write, this);
            Global(AP::AreaRva, std::uint32_t{17});
        }
    };
    AreaFixture* activeArea = nullptr;
    unsigned     firstDataRead = 0, secondDataRead = 0;
    void         RecordSecondAreaRead()
    {
        secondDataRead = PoolTestHost::reads;
    }
    void RecordFirstAreaRead()
    {
        firstDataRead = PoolTestHost::reads;
        PoolTestHost::after = &RecordSecondAreaRead;
    }
    void ChangeArea()
    {
        activeArea->Global(AP::AreaRva, std::uint32_t{0xfedcba98});
    }
    void RevokeAreaOwner()
    {
        PoolTestHost::ownerResult = ClockResult::Expired;
    }
    void ChangeAreaWorld()
    {
        activeArea->Global(LPF::RootRva, std::uintptr_t{0});
    }
    void ChangeAreaMember()
    {
        activeArea->Put(activeArea->graph.items.data(), 0x10, std::uint32_t{999});
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "area evidence uses explicit workspace-owned path");
    AreaFixture fixture(argv[1]);
    activeArea = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    AR::Receipt             receipt;
    constexpr std::uint32_t sentinel = 0xaabbccdd;
    std::uint32_t           output = sentinel;
    const auto              reset = [&]
    {
        fixture.Reset();
        output = sentinel;
    };
    reset();
    Check(AR::Read(image, output, nullptr, &receipt) == ClockResult::NotInitialized && output == sentinel && !receipt.read,
          "read requires actual current native callback authority, not a plausible area DWORD");
    for (const std::uint32_t value : {0u, 1u, 255u, 256u, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0x12345678u})
    {
        reset();
        fixture.Global(AP::AreaRva, value);
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == value && receipt.value == value && receipt.read &&
                  receipt.areaRva == AP::AreaRva && receipt.root == fixture.Address(fixture.application) && receipt.gameMode == fixture.graph.Game() &&
                  receipt.level == fixture.graph.Level() && receipt.world == fixture.graph.World() &&
                  *reinterpret_cast<const std::uint32_t*>(image + AP::AreaRva) == value,
              "actual world area returns every raw DWORD bit, including valid zero and high bits, without native call or write");
    }
    const auto successfulOwnerCalls = PoolTestHost::ownerCalls;
    reset();
    PoolTestHost::watched = image + AP::AreaRva;
    PoolTestHost::after = &RecordFirstAreaRead;
    Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && firstDataRead && secondDataRead > firstDataRead,
          "observe actual two sample read sites without replacing the production image copy");
    for (const auto read : {firstDataRead, secondDataRead})
    {
        reset();
        PoolTestHost::failAt = read;
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::ReadFailure && output == sentinel && !receipt.read,
              "either actual DWORD-copy failure preserves caller output and never manufactures area zero");
    }
    for (const auto mutate : {&ChangeArea, &RevokeAreaOwner, &ChangeAreaWorld, &ChangeAreaMember})
    {
        reset();
        PoolTestHost::watched = image + AP::AreaRva;
        PoolTestHost::after = mutate;
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
              "moving DWORD, lost current world/member or revoked owner refuses incoherent snapshot without write or fallback");
        if (mutate == &ChangeArea)
            Check(*reinterpret_cast<const std::uint32_t*>(image + AP::AreaRva) == 0xfedcba98, "reader never rolls back externally changed native current area");
    }
    for (const auto& pin : AP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
              "each exact getter/writer/dispatch/independent streaming provenance pin remains required");
    }
    for (const auto& group : AP::Groups)
    {
        reset();
        const std::uintptr_t missing = 0;
        fixture.transform.image.Patch(group.slotRva, &missing, sizeof(missing), PAGE_READONLY);
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
              "changed native named-command group identity refuses full-width field interpretation");
        reset();
        const auto wrongBranch = static_cast<std::uint32_t>(group.branchRva + 1);
        fixture.transform.image.Patch(group.tableSlotRva, &wrongBranch, sizeof(wrongBranch), PAGE_READONLY);
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
              "wrong exact command selector target refuses before observing current area");
        if (group.remapRva)
        {
            reset();
            const auto selector = static_cast<unsigned char>(group.selector + 1);
            fixture.transform.image.Patch(group.remapRva, &selector, sizeof(selector), PAGE_READONLY);
            Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
                  "changed native compressed setter selector is not accepted as the same width evidence");
        }
    }
    for (unsigned failure = 0; failure != 7; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 2:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 3:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
                break;
            case 4:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 5:
                fixture.Put(fixture.graph.items.data(), 8, std::uint32_t{0x10000000});
                break;
            case 6:
            {
                fixture.Global(AP::AreaRva, std::uint32_t{17});
                const auto value = std::uint32_t{17};
                fixture.transform.image.Patch(AP::AreaRva, &value, sizeof(value), PAGE_READONLY);
                break;
            }
        }
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
              "current native state/thread/GC/world/image-data admission is independent of a plausible DWORD value");
    }
    for (unsigned revoke = 1; revoke <= successfulOwnerCalls; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == sentinel && !receipt.read,
              "every current owner checkpoint revocation prevents area snapshot publication");
    }
    reset();
    fixture.Global(NativeWorldGroundProfile::FreeStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::ActiveStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::ScanRva, std::uint16_t{0xffff});
    Check(AC::TryEnter(), "retain an unrelated genuine native completion scope");
    {
        AC::Scope held;
        Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == 17 && receipt.read &&
                  *reinterpret_cast<const std::uint16_t*>(image + NativeWorldGroundProfile::ScanRva) == 0xffff,
              "readonly area needs neither ground scan/matrix roots nor engine-entry gate and never advances native scan");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = AR::Read(image, output, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && output == sentinel && !receipt.read, "foreign thread cannot borrow the current loaded world area reader");
    reset();
    Check(AR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == 17 && receipt.read,
          "valid readonly area snapshot remains usable after previous refusals");
    std::puts("Production native world area reader fixture completed");
    std::printf("Passed %u native world-area checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
