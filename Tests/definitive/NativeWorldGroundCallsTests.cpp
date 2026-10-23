/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real ground-query admission on owned typed world and matrix roots
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldGroundHistoricalLoadAllMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldGroundFixture.h"
#include "../../Client/game_de/NativeWorldGroundCalls.h"
#include <cmath>
namespace
{
    mta::game_de::ClockResult InvokeGround(mta::game_de::NativeWorldGroundCalls::NativeFunction, const CVector&, bool*, std::uintptr_t*, CVector*, float&,
                                           std::uint32_t&) noexcept;
}
// Keep actual OS image/private-page checks, typed world membership and the
// shared completion gate. Only the final native entry and read faults change.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldGroundContext.cpp"
#define MTA_DE_WORLD_GROUND_NATIVE_INVOKE ::InvokeGround
#include "../../Client/game_de/NativeWorldGroundCalls.cpp"
#undef MTA_DE_WORLD_GROUND_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace W = NativeWorldGroundCalls;
    namespace WP = NativeWorldGroundProfile;
    namespace DC = NativeStreamingCompletionContext;
    struct GroundFixture : LoadFixture
    {
        NativeWorldGroundFixture::Roots ground;
        // The entered native seam can lazily allocate one real owned matrix
        // from the free list to a second owned inline entity, just like the
        // genuine allocator. Root readiness after entry must accept this change.
        alignas(std::uintptr_t) std::array<unsigned char, 0x20> allocatedOwner{};
        CVector position{123.25f, -456.5f, 78.75f};
        explicit GroundFixture(const std::filesystem::path& directory) : LoadFixture(directory), ground(transform.image.image, &LoadFixture::Write, this)
        {
            Reset();
        }
        void Reset(bool emptyFree = false)
        {
            LoadFixture::Reset();
            ground.InitializePins();
            ground.Reset(emptyFree);
            allocatedOwner.fill(0);
            position = CVector(123.25f, -456.5f, 78.75f);
        }
    };
    GroundFixture*    activeGround = nullptr;
    unsigned          groundCalls = 0, groundEffects = 0, groundScenario = 0;
    W::NativeFunction passedFunction = nullptr;
    CVector           passedPosition;
    bool              optionalOutputsCorrect = false;
    ClockResult       nestedGround = ClockResult::Ok, nestedLoadAll = ClockResult::Ok;
    void              ActivateFreeMatrix()
    {
        auto&      f = *activeGround;
        const auto freeStart = f.transform.image.image + WP::FreeStartRva;
        const auto activeStart = f.transform.image.image + WP::ActiveStartRva;
        const auto a = f.ground.Address(f.ground.free[0].data()), b = f.ground.Address(f.ground.free[1].data());
        const auto oldActive = f.ground.Address(f.ground.active.data());
        // Unlink the first free node, then publish it as the new active first.
        // The old active tail survives; neither head nor scan remains equal.
        f.ground.Global(WP::FreeStartRva + WP::NextOffset, b);
        f.ground.Put(f.ground.free[1].data(), WP::PrevOffset, freeStart);
        f.ground.Global(WP::ActiveStartRva + WP::NextOffset, a);
        f.ground.Put(f.ground.free[0].data(), WP::PrevOffset, activeStart);
        f.ground.Put(f.ground.free[0].data(), WP::NextOffset, oldActive);
        f.ground.Put(f.ground.active.data(), WP::PrevOffset, a);
        f.ground.Put(f.ground.free[0].data(), WP::OwnerOffset, f.ground.Address(f.allocatedOwner.data()));
        f.ground.Put(f.allocatedOwner.data(), 0x18, a);
    }
    ClockResult InvokeGround(W::NativeFunction function, const CVector& position, bool* found, std::uintptr_t* entity, CVector* normal, float& output,
                             std::uint32_t& exception) noexcept
    {
        ++groundCalls;
        ++groundEffects;
        passedFunction = function;
        passedPosition = position;
        optionalOutputsCorrect = found && !entity && !normal;
        if (!optionalOutputsCorrect)
            return ClockResult::InvalidArgument;
        auto&      f = *activeGround;
        const auto before = *reinterpret_cast<const std::uint16_t*>(f.transform.image.image + WP::ScanRva);
        f.Global(WP::ScanRva, before == 0xffff ? std::uint16_t{1} : static_cast<std::uint16_t>(before + 1));
        f.Global(WP::HitScratchRva, std::uint32_t{42});
        *found = groundScenario != 11;
        output = *found ? 37.5f : 0.0f;
        switch (groundScenario)
        {
            case 1:
                exception = EXCEPTION_ACCESS_VIOLATION;
                return ClockResult::OriginalException;
            case 2:
                f.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 3:
                f.Put(f.graph.level.data(), 0xd0, std::uintptr_t{0});
                break;
            case 4:
                f.Put(f.graph.items.data(), 0x10, std::uint32_t{999});
                break;
            case 5:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 6:
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 7:
                f.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 8:
                f.ground.Global(WP::FreeStartRva + WP::NextOffset, std::uintptr_t{0});
                break;
            case 9:
            {
                const auto bad = static_cast<unsigned char>(WP::Pins[0].bytes[0] ^ 1);
                f.transform.image.Patch(WP::Pins[0].rva, &bad, 1, PAGE_EXECUTE_READ);
                break;
            }
            case 10:
            {
                W::Receipt  query;
                LC::Receipt load;
                float       sentinel = 91;
                nestedGround = W::Find(f.transform.image.image, f.position, sentinel, &PoolTestHost::Owner, &query);
                nestedLoadAll = LC::Load(f.transform.image.image, false, &PoolTestHost::Owner, &load);
                if (query.entered || load.entered || sentinel != 91)
                    std::abort();
                break;
            }
            case 12:
                output = -0.0f;
                break;
            case 13:
                output = (std::numeric_limits<float>::quiet_NaN)();
                break;
            case 14:
                f.position.fX = 999;
                break;
            case 15:
                ActivateFreeMatrix();
                break;
        }
        return ClockResult::Ok;
    }
    void BreakFreeRoot()
    {
        activeGround->ground.Global(WP::FreeStartRva + WP::NextOffset, std::uintptr_t{0});
    }
    void MutatePosition()
    {
        activeGround->position.fX = 999;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "ground evidence uses explicit workspace-owned path");
    GroundFixture fixture(argv[1]);
    activeGround = &fixture;
    activeLoad = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    W::Receipt receipt;
    float      output = 91;
    const auto reset = [&](bool emptyFree = false)
    {
        fixture.Reset(emptyFree);
        groundCalls = groundEffects = groundScenario = loadCalls = 0;
        passedFunction = nullptr;
        passedPosition = CVector();
        optionalOutputsCorrect = false;
        nestedGround = nestedLoadAll = ClockResult::Ok;
        output = 91;
    };
    reset();
    Check(W::Find(image, fixture.position, output, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered && !groundCalls && output == 91,
          "loaded world never substitutes for current callback authority");
    for (unsigned char mode = 0; mode != 3; ++mode)
    {
        reset();
        fixture.graph.world[LPF::WorldModeOffset] = mode;
        const auto original = fixture.position;
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == 37.5f && receipt.entered &&
                  receipt.returned && receipt.found && receipt.nativeZ == output && groundCalls == 1 && groundEffects == 1 && optionalOutputsCorrect &&
                  reinterpret_cast<std::uintptr_t>(passedFunction) == image + WP::EntryRva && !std::memcmp(&passedPosition, &original, sizeof(original)) &&
                  receipt.scanBefore == 19 && receipt.scanAfter == 20 && receipt.root == fixture.Address(fixture.application) &&
                  receipt.gameMode == fixture.graph.Game() && receipt.level == fixture.graph.Level() && receipt.world == fixture.graph.World(),
              "exact float XYZ entry receives copied values/localfound/nullentity/nullnormal once under current typed world");
    }
    const auto successfulOwnerCalls = PoolTestHost::ownerCalls;
    for (unsigned scenario = 1; scenario <= 9; ++scenario)
    {
        reset();
        groundScenario = scenario;
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == 91 && receipt.entered &&
                  receipt.returned == (scenario != 1) && groundCalls == 1 && groundEffects == 1 &&
                  *reinterpret_cast<const std::uint32_t*>(image + WP::HitScratchRva) == 42 &&
                  (scenario != 1 || receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION),
              "entered fault or lost owner/world/member/root/profile leaves effects intact and caller output untouched, without retry");
        reset();
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && groundCalls == 1,
              "completion gate releases after every entered failure");
    }
    reset();
    groundScenario = 10;
    Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && nestedGround == ClockResult::Busy &&
              nestedLoadAll == ClockResult::Busy && groundCalls == 1 && !loadCalls,
          "one shared gate rejects recursive query and genuine LoadAll, not native internal engine work");
    reset();
    Check(DC::TryEnter(), "use existing native completion gate");
    {
        DC::Scope held;
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Busy && !receipt.entered && !groundCalls && output == 91,
              "held streaming completion refuses ground query before entry");
    }
    for (unsigned failure = 0; failure != 18; ++failure)
    {
        reset(failure >= 14 && failure <= 16);
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 2:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
                break;
            case 3:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
                fixture.graph.world[LPF::WorldModeOffset] = 3;
                break;
            case 5:
                fixture.ground.Global(WP::FreeStartRva + WP::NextOffset, std::uintptr_t{0});
                break;
            case 6:
                fixture.ground.Put(fixture.ground.free[0].data(), WP::PrevOffset, std::uintptr_t{0});
                break;
            case 7:
                fixture.ground.Put(fixture.ground.free[0].data(), WP::NextOffset, std::uintptr_t{1});
                break;
            case 8:
                fixture.ground.Global(WP::FreeEndRva + WP::PrevOffset, std::uintptr_t{0});
                break;
            case 9:
                fixture.ground.Put(fixture.ground.free[1].data(), WP::PrevOffset, std::uintptr_t{0});
                break;
            case 10:
                fixture.ground.Global(WP::ActiveStartRva + WP::NextOffset, std::uintptr_t{0});
                break;
            case 11:
                fixture.ground.Put(fixture.ground.active.data(), WP::PrevOffset, std::uintptr_t{0});
                break;
            case 12:
                fixture.ground.Put(fixture.ground.active.data(), WP::NextOffset, std::uintptr_t{0});
                break;
            case 13:
                fixture.ground.Global(WP::ActiveEndRva + WP::PrevOffset, std::uintptr_t{0});
                break;
            case 14:
                fixture.ground.Put(fixture.ground.active.data(), WP::OwnerOffset, std::uintptr_t{0});
                break;
            case 15:
                fixture.ground.Put(fixture.ground.owner.data(), 0x18, std::uintptr_t{0});
                break;
            case 16:
                fixture.ground.Global(WP::ActiveStartRva + WP::NextOffset, image + WP::ActiveEndRva);
                fixture.ground.Global(WP::ActiveEndRva + WP::PrevOffset, image + WP::ActiveStartRva);
                break;
            case 17:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
        }
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == 91 && !receipt.entered && !groundCalls,
              "uninitialized or incoherent world/thread/GC/native free-active roots refuse before effects");
    }
    for (const auto rva : {WP::SectorsRva, WP::SectorsEndRva - 1, WP::HitScratchRva, WP::ScanRva, WP::ActiveStartRva, WP::FreeStartRva})
    {
        reset();
        const auto value = *reinterpret_cast<const unsigned char*>(image + rva);
        fixture.transform.image.Patch(rva, &value, 1, PAGE_READONLY);
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == 91 && !receipt.entered && !groundCalls,
              "each genuine static native writable sector/scan/scratch/matrix storage gate is required");
    }
    reset(true);
    Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && groundCalls == 1,
          "empty free list admits genuine native tail-owner retirement fallback without synthetic allocation");
    reset();
    fixture.ground.Global(WP::ActiveStartRva + WP::NextOffset, image + WP::ActiveEndRva);
    fixture.ground.Global(WP::ActiveEndRva + WP::PrevOffset, image + WP::ActiveStartRva);
    Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "initialized empty active list is valid while native free links remain");
    for (const auto scan : {std::uint16_t{0}, std::uint16_t{0xfffe}, std::uint16_t{0xffff}})
    {
        reset();
        fixture.Global(WP::ScanRva, scan);
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.scanBefore == scan &&
                  receipt.scanAfter == (scan == 0xffff ? 1 : scan + 1) && groundCalls == 1,
              "native scan zero/increment/wrap remain admitted genuine effects instead of a stuck ffff refusal");
    }
    for (unsigned scenario = 11; scenario <= 15; ++scenario)
    {
        reset();
        groundScenario = scenario;
        if (scenario == 15)
            fixture.Global(WP::ScanRva, std::uint16_t{0xffff});
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && groundCalls == 1,
              "native miss, signed-zero/nonfinite scalar, caller mutation and genuine free-to-active matrix transition retain exact scalar semantics");
        if (scenario == 11)
            Check(!receipt.found && output == 0 && !std::signbit(output), "actual no-hit positive-zero is successful native output, not adapter refusal");
        if (scenario == 12)
            Check(std::signbit(output), "native returned negative zero is not rewritten");
        if (scenario == 13)
            Check(std::isnan(output), "query returns actual native scalar without invented fallback policy");
        if (scenario == 14)
            Check(passedPosition.fX == 123.25f && fixture.position.fX == 999, "native receives owned XYZ rather than caller buffer");
        if (scenario == 15)
            Check(receipt.scanBefore == 0xffff && receipt.scanAfter == 1 && output == 37.5f &&
                      *reinterpret_cast<const std::uintptr_t*>(image + WP::FreeStartRva + WP::NextOffset) ==
                          fixture.ground.Address(fixture.ground.free[1].data()) &&
                      *reinterpret_cast<const std::uintptr_t*>(image + WP::ActiveStartRva + WP::NextOffset) ==
                          fixture.ground.Address(fixture.ground.free[0].data()) &&
                      *reinterpret_cast<const std::uintptr_t*>(image + WP::ActiveEndRva + WP::PrevOffset) ==
                          fixture.ground.Address(fixture.ground.active.data()) &&
                      *reinterpret_cast<const std::uintptr_t*>(fixture.allocatedOwner.data() + 0x18) == fixture.ground.Address(fixture.ground.free[0].data()),
                  "entered scan wrap plus coherent real free-to-active node/owner publication is acknowledged without old-topology equality or rollback");
    }
    for (const float value : {-0.0f, (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::infinity)(), -(std::numeric_limits<float>::infinity)(),
                              (std::numeric_limits<float>::quiet_NaN)()})
    {
        reset();
        fixture.position.fX = value;
        const auto original = fixture.position;
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok &&
                  !std::memcmp(&passedPosition, &original, sizeof(original)),
              "actual MTA ground XYZ is passed bit-for-bit without importing scene finite-scale or map-clamp policy");
    }
    for (const auto& pin : WP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == 91 && !receipt.entered && !groundCalls,
              "every exact native entry/scanwrap/sector/allocator/command/constant pin remains mandatory");
    }
    reset();
    PoolTestHost::watched = image + WP::FreeStartRva + WP::NextOffset;
    PoolTestHost::after = &BreakFreeRoot;
    Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == 91 && !receipt.entered && !groundCalls,
          "matrix root moving between actual preflight observations refuses without entry");
    reset();
    PoolTestHost::watched = image + NativeStartGameProfile::ThreadInitializedRva;
    PoolTestHost::after = &MutatePosition;
    Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && passedPosition.fX == 123.25f &&
              fixture.position.fX == 999,
          "input snapshot predates supplied owner/read callbacks");
    for (unsigned revoke = 1; revoke <= successfulOwnerCalls; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output == 91 && groundCalls <= 1,
              "every live owner checkpoint revocation prevents scalar publication with at most one entry");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = W::Find(image, fixture.position, output, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && output == 91 && !receipt.entered && !groundCalls, "foreign thread cannot borrow loaded-world query authority");
    reset();
    Check(W::Find(image, fixture.position, fixture.position.fX, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && fixture.position.fX == 37.5f &&
              passedPosition.fX == 123.25f,
          "caller output alias cannot change the already copied input XYZ");
    std::puts("Production native world ground primitive fixture completed");
    std::printf("Passed %u native world-ground checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
