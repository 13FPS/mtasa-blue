/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine LOS-clear flags and entry admission over owned world roots
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldLineHistoricalGroundMain
#include "NativeWorldGroundCallsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldLineFixture.h"
#include "../../Client/game_de/NativeWorldLineCalls.h"
namespace
{
    mta::game_de::ClockResult InvokeLine(mta::game_de::NativeWorldLineCalls::NativeFunction, const CVector&, const CVector&,
                                         const mta::game_de::NativeWorldLineCalls::Flags&, bool&, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                PoolTestHost::Module
#define ReadProcessMemory               PoolTestHost::Read
#define MTA_DE_WORLD_LINE_NATIVE_INVOKE ::InvokeLine
#include "../../Client/game_de/NativeWorldLineCalls.cpp"
#undef MTA_DE_WORLD_LINE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace NL = NativeWorldLineCalls;
    namespace NLP = NativeWorldLineProfile;
    struct LineFixture : GroundFixture
    {
        CVector           end{-89.25f, 76.5f, -54.75f};
        SLineOfSightFlags flags;
        explicit LineFixture(const std::filesystem::path& directory) : GroundFixture(directory) { Reset(); }
        void Reset()
        {
            GroundFixture::Reset();
            NativeWorldLineFixture::Install(&LoadFixture::Write, this);
            end = CVector(-89.25f, 76.5f, -54.75f);
            flags = SLineOfSightFlags();
        }
        NL::Flags Consumed() const
        {
            return {flags.bCheckBuildings,
                    flags.bCheckVehicles,
                    flags.bCheckPeds,
                    flags.bCheckObjects,
                    flags.bCheckDummies,
                    flags.bSeeThroughStuff,
                    flags.bIgnoreSomeObjectsForCamera};
        }
        void FlagsPattern(unsigned pattern)
        {
            const std::array<bool*, 7> fields{&flags.bCheckBuildings,
                                              &flags.bCheckVehicles,
                                              &flags.bCheckPeds,
                                              &flags.bCheckObjects,
                                              &flags.bCheckDummies,
                                              &flags.bSeeThroughStuff,
                                              &flags.bIgnoreSomeObjectsForCamera};
            for (std::size_t i = 0; i != fields.size(); ++i)
                *fields[i] = (pattern & (1u << i)) != 0;
            flags.bShootThroughStuff = (pattern & 128u) != 0;
            flags.bCheckCarTires = (pattern & 256u) != 0;
        }
    };
    LineFixture*       activeLine = nullptr;
    unsigned           lineCalls = 0, lineEffects = 0, lineScenario = 0;
    bool               nativeLineResult = false;
    NL::NativeFunction passedLine = nullptr;
    CVector            passedStart, passedEnd;
    NL::Flags          linePassedFlags{};
    bool               passedOwnedInputs = false;
    ClockResult        nestedLineResult = ClockResult::Ok, nestedGroundResult = ClockResult::Ok, nestedAllResult = ClockResult::Ok;
    void               ChangeLineIntent()
    {
        auto& f = *activeLine;
        f.position = CVector(900, 901, 902);
        f.end = CVector(903, 904, 905);
        const auto consumed = f.Consumed();
        unsigned   opposite = 0;
        for (std::size_t i = 0; i != consumed.size(); ++i)
            opposite |= !consumed[i] ? 1u << i : 0;
        f.FlagsPattern(opposite);
    }
    ClockResult InvokeLine(NL::NativeFunction function, const CVector& start, const CVector& end, const NL::Flags& flags, bool& clear,
                           std::uint32_t& exception) noexcept
    {
        ++lineCalls;
        ++lineEffects;
        auto& f = *activeLine;
        passedLine = function;
        passedStart = start;
        passedEnd = end;
        linePassedFlags = flags;
        passedOwnedInputs = &start != &f.position && &end != &f.end;
        const auto scan = *reinterpret_cast<const std::uint16_t*>(f.transform.image.image + WP::ScanRva);
        f.Global(WP::ScanRva, scan == 0xffff ? std::uint16_t{1} : static_cast<std::uint16_t>(scan + 1));
        // Model an irreversible native scan effect. The query does not own a
        // rollback even when later callback/profile/world acknowledgement fails.
        f.Global(WP::HitScratchRva, std::uint32_t{57});
        clear = nativeLineResult;
        switch (lineScenario)
        {
            case 1:
                exception = EXCEPTION_ACCESS_VIOLATION;
                return ClockResult::OriginalException;
            case 2:
                f.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 3:
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 5:
                f.Put(f.graph.items.data(), 0x10, std::uint32_t{999});
                break;
            case 6:
                ChangeLineIntent();
                break;
            case 7:
                ActivateFreeMatrix();
                break;
            case 8:
            {
                NL::Receipt lineReceipt;
                W::Receipt  groundReceipt;
                LC::Receipt allReceipt;
                bool        boolean = true;
                float       height = 91;
                nestedLineResult = NL::Clear(f.transform.image.image, f.position, f.end, f.flags, boolean, &PoolTestHost::Owner, &lineReceipt);
                nestedGroundResult = W::Find(f.transform.image.image, f.position, height, &PoolTestHost::Owner, &groundReceipt);
                nestedAllResult = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &allReceipt);
                if (lineReceipt.entered || groundReceipt.entered || allReceipt.entered || !boolean || height != 91)
                    std::abort();
                break;
            }
            case 9:
            {
                const auto bad = static_cast<unsigned char>(NLP::Pins[0].bytes[0] ^ 1);
                f.transform.image.Patch(NLP::Pins[0].rva, &bad, 1, PAGE_EXECUTE_READ);
                break;
            }
        }
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "LOS evidence uses workspace-owned explicit directory");
    LineFixture fixture(argv[1]);
    activeLine = &fixture;
    activeGround = &fixture;
    activeLoad = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    NL::Receipt receipt;
    bool        output = true;
    const auto  reset = [&]
    {
        fixture.Reset();
        lineCalls = lineEffects = lineScenario = groundCalls = groundEffects = groundScenario = loadCalls = 0;
        nativeLineResult = false;
        output = true;
        passedLine = nullptr;
        passedOwnedInputs = false;
        linePassedFlags = {};
        nestedLineResult = nestedGroundResult = nestedAllResult = ClockResult::Ok;
    };
    reset();
    Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, nullptr, &receipt) == ClockResult::NotInitialized && output &&
              !receipt.entered && !lineCalls,
          "a loaded world alone grants no native LOS query authority");
    for (unsigned pattern = 0; pattern != 512; ++pattern)
    {
        reset();
        fixture.FlagsPattern(pattern);
        const auto wanted = fixture.Consumed();
        const auto start = fixture.position, end = fixture.end;
        nativeLineResult = (pattern & 16u) != 0;
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
                  output == nativeLineResult && receipt.nativeClear == nativeLineResult && receipt.entered && receipt.returned && lineCalls == 1 &&
                  lineEffects == 1 && linePassedFlags == wanted && receipt.flags == wanted && passedOwnedInputs &&
                  !std::memcmp(&start, &passedStart, sizeof(start)) && !std::memcmp(&end, &passedEnd, sizeof(end)) &&
                  reinterpret_cast<std::uintptr_t>(passedLine) == image + NLP::EntryRva && receipt.scanBefore == 19 && receipt.scanAfter == 20,
              "all seven consumed flags retain classic order, both unused fields stay unused, and native clear/blocked bool publishes unchanged");
    }
    const auto successfulOwnerCalls = PoolTestHost::ownerCalls;
    for (const auto scenario : {1u, 2u, 3u, 4u, 5u, 9u})
    {
        reset();
        lineScenario = scenario;
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output &&
                  receipt.entered && receipt.returned == (scenario != 1) && lineCalls == 1 && lineEffects == 1 &&
                  *reinterpret_cast<const std::uint32_t*>(image + WP::HitScratchRva) == 57 &&
                  (scenario != 1 || receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION),
              "entered exception or lost owner/world/GC/member/profile preserves effects and old caller bool without retry");
        reset();
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output &&
                  lineCalls == 1,
              "shared completion gate releases after failed entered LOS acknowledgement");
    }
    reset();
    lineScenario = 6;
    const auto initialFlags = fixture.Consumed();
    Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
              passedStart.fX == 123.25f && passedEnd.fX == -89.25f && linePassedFlags == initialFlags && fixture.position.fX == 900,
          "entered callbacks cannot replace copied endpoint or flag intent");
    reset();
    lineScenario = 7;
    fixture.Global(WP::ScanRva, std::uint16_t{0xffff});
    Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output &&
              receipt.scanBefore == 0xffff && receipt.scanAfter == 1 && lineCalls == 1 &&
              *reinterpret_cast<const std::uintptr_t*>(image + WP::ActiveStartRva + WP::NextOffset) == fixture.ground.Address(fixture.ground.free[0].data()),
          "same world admits coherent native free-to-active matrix movement and scan wrap rather than requiring old heads or scan values");
    reset();
    lineScenario = 8;
    Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
              nestedLineResult == ClockResult::Busy && nestedGroundResult == ClockResult::Busy && nestedAllResult == ClockResult::Busy && lineCalls == 1 &&
              !groundCalls && !loadCalls,
          "one shared completion gate rejects recursive LOS, ground and real LoadAll entries");
    reset();
    Check(DC::TryEnter(), "LOS uses existing native completion serialization");
    {
        DC::Scope held;
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Busy && output &&
                  !receipt.entered && !lineCalls,
              "an existing completion refuses new LOS before entry");
    }
    for (const auto& pin : NLP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output &&
                  !receipt.entered && !lineCalls,
              "each exact LOS/sector/lazy-matrix/named-branch/constant profile is mandatory");
    }
    for (const auto value : {-0.0f, (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::infinity)(), -(std::numeric_limits<float>::infinity)(),
                             (std::numeric_limits<float>::quiet_NaN)()})
    {
        reset();
        fixture.position.fY = value;
        fixture.end.fZ = value;
        const auto start = fixture.position, end = fixture.end;
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
                  !std::memcmp(&start, &passedStart, sizeof(start)) && !std::memcmp(&end, &passedEnd, sizeof(end)),
              "native endpoints retain raw bits without scene scaling, guessed clamps, endpoint sorting or fabricated clear result");
    }
    reset();
    PoolTestHost::watched = image + NativeStartGameProfile::ThreadInitializedRva;
    PoolTestHost::after = &ChangeLineIntent;
    const auto originalFlags = fixture.Consumed();
    Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
              passedStart.fX == 123.25f && passedEnd.fX == -89.25f && linePassedFlags == originalFlags,
          "both endpoints and consumed flags are captured before supplied owner/read callbacks");
    for (unsigned revoke = 1; revoke <= successfulOwnerCalls; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output &&
                  lineCalls <= 1,
              "all live owner checkpoints refuse stale LOS publication with at most one entry");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = NL::Clear(image, fixture.position, fixture.end, fixture.flags, output, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && output && !receipt.entered && !lineCalls, "foreign thread cannot borrow loaded native LOS authority");
    reset();
    Check(
        NL::Clear(image, fixture.position, fixture.position, fixture.flags, fixture.flags.bCheckBuildings, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
            !fixture.flags.bCheckBuildings && linePassedFlags[0] && !std::memcmp(&passedStart, &passedEnd, sizeof(passedStart)),
        "equal/aliased endpoint and output flag storage do not rewrite already captured native intent");
    std::puts("Production native world LOS-clear primitive fixture completed");
    std::printf("Passed %u native world LOS-clear checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
