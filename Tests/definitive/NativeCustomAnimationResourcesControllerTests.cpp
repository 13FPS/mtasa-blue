/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One owner journal dispatches raw frames and opaque sequence arrays
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalMixedAnimationResourcesHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"

namespace MixedAnimationResourcesControllerTests
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    using Controller = NativeCustomAnimSequenceController;
    using Receipt = Controller::Receipt;
    constexpr std::uint64_t  Generation = 37;
    constexpr std::uintptr_t Sentinel = 0x12345678;
    unsigned                 mixedChecks = 0;
    void                     Verify(bool condition, const char* message)
    {
        ++mixedChecks;
        Check(condition, message);
    }
    void LoseOwner(Host::State&) noexcept
    {
        PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
}

int main(int argc, char** argv)
{
    using namespace MixedAnimationResourcesControllerTests;
    if (argc != 2)
        return 2;
    SpecialFixture fixture(argv[1]);
    Host::State    host;
    Host::active = &host;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
    };
    Receipt                    receipt;
    Controller::RetireProgress progress;
    reset();
    {
        Controller     controller;
        std::uintptr_t array = Sentinel, frames = Sentinel;
        Verify(controller.Allocate(image, Generation, 2, &PoolTestHost::Owner, array, &receipt) == ClockResult::Ok && array == host.base + 8,
               "mixed owner first journals a genuine cookie-backed sequence array");
        Verify(controller.AllocateFrames(image, Generation, 41, &PoolTestHost::Owner, frames, &receipt) == ClockResult::Ok && frames == host.base &&
                   receipt.bytes == 41 && !receipt.capacity && !receipt.initialized && controller.IsRetained(),
               "same journal issues raw frame base with exact requested bytes and no sequence cookie or row initialization");
        auto* payload = reinterpret_cast<unsigned char*>(frames);
        bool  preserved = true;
        for (std::size_t index = 0; index != 41; ++index)
            preserved = preserved && payload[index] == 0xa5;
        std::memset(payload, 0x6d, 41);
        payload[0] = 0x11;
        payload[40] = 0x7e;
        Verify(preserved && payload[0] == 0x11 && payload[40] == 0x7e,
               "all frame payload bytes remain caller-owned, including its first bytes rather than a fabricated array cookie");
        Verify(controller.FreeStorage(image, Generation, frames, &PoolTestHost::Owner, &receipt) == ClockResult::BindingChanged && !receipt.entered &&
                   !host.freeCalls && controller.IsRetained(),
               "array free cannot adopt a frame record with the same generation");
        Verify(controller.FreeFrames(image, Generation, array, &PoolTestHost::Owner, &receipt) == ClockResult::BindingChanged && !receipt.entered &&
                   !host.freeCalls && controller.IsRetained(),
               "frame free cannot consume a sequence-array record or skip its typed lookup");
        Verify(controller.FreeFrames(image, Generation + 1, frames, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !host.freeCalls,
               "frame record lookup keeps the original owner generation");
        Verify(controller.FreeFrames(image, Generation, frames, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.released && host.freeCalls == 1 &&
                   controller.IsRetained(),
               "correct frame free accepts changed first payload bytes without a cookie read and leaves the array journal intact");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.attempted == 1 &&
                   progress.released == 1 && !progress.retained && host.freeCalls == 2 && !controller.IsRetained(),
               "same bounded retirement dispatch releases the remaining array kind once");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Verify(controller.AllocateFrames(image, Generation, 41, &PoolTestHost::Owner, output, &receipt) == ClockResult::WrongThread && output == Sentinel &&
                   !host.mallocCalls && !controller.IsRetained(),
               "frame producer refusal precedes native allocation and preserves its output");
        PoolTestHost::ownerResult = ClockResult::Ok;
        host.nullMalloc = true;
        Verify(controller.AllocateFrames(image, Generation, 41, &PoolTestHost::Owner, output, &receipt) == ClockResult::Ok && !output && receipt.returned &&
                   receipt.verified && !receipt.allocated && !controller.IsRetained(),
               "qualified returned malloc-null preserves MTA's nullable frame allocation contract with no owned resource");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t pinned = Sentinel, frames = Sentinel;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, pinned, &receipt) == ClockResult::Ok &&
                   controller.PinPublication(Generation, pinned, image + 0x1000) == ClockResult::Ok,
               "prepare a conservatively pinned array at the front of the mixed journal");
        host.afterMalloc = &LoseOwner;
        Verify(controller.AllocateFrames(image, Generation, 41, &PoolTestHost::Owner, frames, &receipt) == ClockResult::OwnerConflict && frames == Sentinel &&
                   receipt.returned && receipt.allocated && controller.RecoveryPending(),
               "known returned frame allocation with lost acknowledgement remains private cleanup debt in the shared journal");
        host.afterMalloc = nullptr;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && !progress.attempted &&
                   progress.retained == 2 && !host.freeCalls,
               "bounded cleanup preserves and rotates the pinned array without mistaking it for raw frames");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && progress.attempted == 1 &&
                   progress.released == 1 && progress.retained == 1 && host.freeCalls == 1,
               "next bounded pass fairly reaches the later known frame record and dispatches its storage-only free");
        Verify(controller.IsRetained() && controller.RecoveryPending(), "remaining pinned array retains the same owner journal after frame cleanup");
    }
    for (const bool nativeReturned : {true, false})
    {
        reset();
        Controller     controller;
        std::uintptr_t frames = Sentinel;
        Verify(controller.AllocateFrames(image, Generation, 41, &PoolTestHost::Owner, frames, &receipt) == ClockResult::Ok,
               "prepare raw frame record for returned versus unknown free acknowledgement");
        if (nativeReturned)
            host.afterFree = &LoseOwner;
        else
        {
            host.freeResult = ClockResult::OriginalException;
            host.consumeFailedFree = true;
        }
        Verify(controller.FreeFrames(image, Generation, frames, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && host.freeCalls == 1 &&
                   (nativeReturned ? receipt.released && !controller.IsRetained() : receipt.debt && controller.IsRetained()),
               "frame free records known consumption before failed acknowledgement while preserving unknown entered effects");
        host.afterFree = nullptr;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.FreeFrames(image, Generation, frames, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.freeCalls == 1,
               "typed controller never repeats either a known consumed or unknown entered frame free");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        host.mallocResult = ClockResult::OriginalException;
        Verify(controller.AllocateFrames(image, Generation, 41, &PoolTestHost::Owner, output, &receipt) == ClockResult::OriginalException &&
                   output == Sentinel && receipt.debt && controller.IsRetained() && controller.RecoveryPending(),
               "unknown entered frame allocation never publishes its unreturned raw base");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && !progress.attempted &&
                   progress.retained == 1 && !host.freeCalls,
               "mixed retirement retains unknown frame effects without native allocation/free replay");
    }
    Host::active = nullptr;
    std::printf("Passed %u mixed custom animation resource controller checks (64-bit; no native engine code executed)\n", mixedChecks);
    std::puts("Production mixed custom animation resource controller fixture completed");
}
