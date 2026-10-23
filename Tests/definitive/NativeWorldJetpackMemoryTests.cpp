/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual raw jetpack float copies over owned image and current world pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldJetpackHistoricalLoadAllMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldJetpackFixture.h"
#include "../../Client/game_de/NativeWorldJetpackMemory.h"
#include "../../Client/game_de/NativeWorldGroundProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldJetpackMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace JR = NativeWorldJetpackMemory;
    namespace JP = NativeWorldJetpackProfile;
    namespace JC = NativeStreamingCompletionContext;
    std::uint32_t Bits(const float& value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    struct JetpackFixture : LoadFixture
    {
        explicit JetpackFixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        void Limit(std::uint32_t bits)
        {
            // A fixture may arrange arbitrary native float bits; production
            // only reads these real image pages and never opens their rights.
            transform.image.Patch(JP::LimitRva, &bits, sizeof(bits), PAGE_READONLY);
        }
        void Reset()
        {
            LoadFixture::Reset();
            NativeWorldJetpackFixture::Install(transform.image.image, &LoadFixture::Write, this);
            Limit(0x42c80000);
        }
    };
    JetpackFixture* activeJetpack = nullptr;
    unsigned        firstDataRead = 0, secondDataRead = 0;
    void            RecordSecondJetpackRead()
    {
        secondDataRead = PoolTestHost::reads;
    }
    void RecordFirstJetpackRead()
    {
        firstDataRead = PoolTestHost::reads;
        PoolTestHost::after = &RecordSecondJetpackRead;
    }
    void ChangeJetpack()
    {
        activeJetpack->Limit(0x3f400000);
    }
    void ChangeZeroSign()
    {
        activeJetpack->Limit(0x80000000);
    }
    void RevokeJetpackOwner()
    {
        PoolTestHost::ownerResult = ClockResult::Expired;
    }
    void ChangeJetpackWorld()
    {
        activeJetpack->Global(LPF::RootRva, std::uintptr_t{0});
    }
    void ChangeJetpackMember()
    {
        activeJetpack->Put(activeJetpack->graph.items.data(), 0x10, std::uint32_t{999});
    }
    void ChangeJetpackProfile()
    {
        const auto& pin = JP::Pins.front();
        const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        activeJetpack->transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "jetpack evidence uses explicit workspace-owned path");
    JetpackFixture fixture(argv[1]);
    activeJetpack = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    JR::Receipt             receipt;
    constexpr std::uint32_t sentinel = 0x7fc12345;
    float                   output = 0;
    const auto              reset = [&]
    {
        fixture.Reset();
        std::memcpy(&output, &sentinel, sizeof(output));
    };
    reset();
    Check(JR::Read(image, output, nullptr, &receipt) == ClockResult::NotInitialized && Bits(output) == sentinel && !receipt.read,
          "read requires actual current native callback authority, not the disk value100");
    for (const std::uint32_t bits : {0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x42c80000u, 0x7f800000u, 0xff800000u, 0x7fcabcdeu, 0xffc12345u,
                                     0x7f812345u, 0x00000001u, 0x7f7fffffu})
    {
        reset();
        fixture.Limit(bits);
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && Bits(output) == bits && Bits(receipt.value) == bits &&
                  receipt.read && receipt.limitRva == JP::LimitRva && receipt.root == fixture.Address(fixture.application) &&
                  receipt.gameMode == fixture.graph.Game() && receipt.level == fixture.graph.Level() && receipt.world == fixture.graph.World() &&
                  *reinterpret_cast<const std::uint32_t*>(image + JP::LimitRva) == bits,
              "actual read-only native float returns every raw bit, including NaNs and signedzero, without clamp, cached100 or store");
        MEMORY_BASIC_INFORMATION pages{};
        Check(VirtualQuery(reinterpret_cast<const void*>(image + JP::LimitRva), &pages, sizeof(pages)) == sizeof(pages) && pages.Type == MEM_IMAGE &&
                  (pages.Protect & 0xff) == PAGE_READONLY,
              "successful read retains actual read-only image protection");
    }
    const auto successfulOwnerCalls = PoolTestHost::ownerCalls;
    reset();
    PoolTestHost::watched = image + JP::LimitRva;
    PoolTestHost::after = &RecordFirstJetpackRead;
    Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && firstDataRead && secondDataRead > firstDataRead,
          "observe both actual copied-read sites without replacing the production image copy");
    for (const auto read : {firstDataRead, secondDataRead})
    {
        reset();
        PoolTestHost::failAt = read;
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::ReadFailure && Bits(output) == sentinel && !receipt.read,
              "either actual float-copy failure preserves caller output and never substitutes native default100");
    }
    for (const auto mutate : {&ChangeJetpack, &RevokeJetpackOwner, &ChangeJetpackWorld, &ChangeJetpackMember, &ChangeJetpackProfile})
    {
        reset();
        PoolTestHost::watched = image + JP::LimitRva;
        PoolTestHost::after = mutate;
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "moving float, lost world/member, changed consumer or revoked owner refuses an incoherent snapshot");
        if (mutate == &ChangeJetpack)
            Check(*reinterpret_cast<const std::uint32_t*>(image + JP::LimitRva) == 0x3f400000, "reader never rolls back an externally changed native float");
    }
    reset();
    fixture.Limit(0);
    PoolTestHost::watched = image + JP::LimitRva;
    PoolTestHost::after = &ChangeZeroSign;
    Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::BindingChanged && Bits(output) == sentinel && !receipt.read,
          "positivezero to negativezero mutation is detected by bit comparison despite float equality");
    for (const auto& pin : JP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "each exact named-task/usage/type/ProcessPed/thrust/ground-reference provenance pin remains required");
    }
    for (const auto& group : JP::Groups)
    {
        reset();
        const std::uintptr_t missing = 0;
        fixture.transform.image.Patch(group.slotRva, &missing, sizeof(missing), PAGE_READONLY);
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "changed supported named-command group identity refuses consumer interpretation");
        reset();
        const auto wrongBranch = static_cast<std::uint32_t>(group.branchRva + 1);
        fixture.transform.image.Patch(group.tableSlotRva, &wrongBranch, sizeof(wrongBranch), PAGE_READONLY);
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "wrong exact named task/query selector target refuses before observing the height float");
    }
    for (const auto& slot : JP::Slots)
    {
        reset();
        const auto wrongTarget = image + slot.targetRva + 1;
        fixture.transform.image.Patch(JP::VtableRva + slot.offset, &wrongTarget, sizeof(wrongTarget), PAGE_READONLY);
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "native task vtable must still connect named construction/type to the consumed thrust path");
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
                DWORD previous = 0;
                Check(VirtualProtect(reinterpret_cast<void*>(image + JP::LimitRva), sizeof(float), PAGE_NOACCESS, &previous) != FALSE,
                      "deny the actual native float fixture page");
                break;
            }
        }
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "native state/thread/GC/world/image-page admission is independent of a plausible float value");
    }
    for (unsigned revoke = 1; revoke <= successfulOwnerCalls; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && Bits(output) == sentinel && !receipt.read,
              "every current owner checkpoint revocation prevents float snapshot publication");
    }
    reset();
    fixture.Global(NativeWorldGroundProfile::FreeStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::ActiveStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::ScanRva, std::uint16_t{0xffff});
    Check(JC::TryEnter(), "retain an unrelated genuine native completion scope");
    {
        JC::Scope held;
        Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && Bits(output) == 0x42c80000 && receipt.read &&
                  *reinterpret_cast<const std::uint16_t*>(image + NativeWorldGroundProfile::ScanRva) == 0xffff,
              "readonly jetpack snapshot needs neither Ground roots nor engine-entry gate and never advances native scan");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = JR::Read(image, output, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && Bits(output) == sentinel && !receipt.read, "foreign thread cannot borrow the current loaded-world jetpack reader");
    reset();
    fixture.Limit(0x43fa0000);
    Check(JR::Read(image, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && Bits(output) == 0x43fa0000 && receipt.read,
          "fresh native value remains readable after refusals with no stale cached100");
    std::puts("Production native world jetpack reader fixture completed");
    std::printf("Passed %u native world-jetpack checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
