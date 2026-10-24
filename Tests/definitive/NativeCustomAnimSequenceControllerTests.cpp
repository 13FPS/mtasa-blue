/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual controller journaling, issued lookup and bounded retirement
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalCustomSequenceControllerHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#include <memory>
#include <type_traits>
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

namespace CustomSequenceControllerTests
{
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeCustomAnimSequenceStorageCalls;
    using Controller = NativeCustomAnimSequenceController;
    constexpr std::uint64_t  Generation = 29;
    constexpr std::uintptr_t Sentinel = 0x12345678;
    unsigned                 controllerChecks = 0;
    void                     Verify(bool condition, const char* message)
    {
        ++controllerChecks;
        Check(condition, message);
    }
    struct Context
    {
        Controller*    controller = nullptr;
        std::uint64_t* callerGeneration = nullptr;
        std::size_t*   callerCapacity = nullptr;
        bool           inspectEntry = false, revokeOwner = false, retainedAtEntry = false;
        ClockResult    nested = ClockResult::NotInitialized;
        std::uintptr_t nestedOutput = Sentinel;
    };
    void AfterMalloc(Fixture::State& host) noexcept
    {
        auto& context = *static_cast<Context*>(host.context);
        if (context.inspectEntry)
        {
            context.retainedAtEntry = context.controller->IsRetained();
            context.nested = context.controller->Allocate(host.image, Generation, 1, &PoolTestHost::Owner, context.nestedOutput);
        }
        if (context.callerGeneration)
            *context.callerGeneration = Generation + 1;
        if (context.callerCapacity)
            *context.callerCapacity = 65536;
        if (context.revokeOwner)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
    void AfterFree(Fixture::State&) noexcept
    {
        PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
}

int main(int argc, char** argv)
{
    using namespace CustomSequenceControllerTests;
    if (argc != 2)
        return 2;
    static_assert(!std::is_copy_constructible_v<Controller> && !std::is_move_constructible_v<Controller>);
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    const auto image = fixture.transform.image.image;
    Context    context;
    const auto reset = [&]
    {
        // These buffers belong solely to the host. Releasing test backing
        // never purports to discharge retained production allocation debt.
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        context = {};
        host.context = &context;
    };
    Calls::Receipt             receipt;
    Controller::RetireProgress progress;
    reset();
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, output, &receipt) == ClockResult::WrongThread && output == Sentinel &&
                   !receipt.entered && !host.mallocCalls && !controller.IsRetained() && !controller.RecoveryPending(),
               "controller refuses the wrong callback before journaling or changing its output");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.Allocate(image, Generation, 0, &PoolTestHost::Owner, output, &receipt) == ClockResult::Ok && !output && !host.mallocCalls &&
                   !controller.IsRetained(),
               "zero-capacity result admits the controller owner but creates no native journal resource");
        output = Sentinel;
        std::uint64_t generation = Generation;
        std::size_t   capacity = 2;
        context = {&controller, &generation, &capacity, true};
        host.afterMalloc = &AfterMalloc;
        Verify(controller.Allocate(image, generation, capacity, &PoolTestHost::Owner, output, &receipt) == ClockResult::Ok && output == receipt.readyTag &&
                   output == host.base + 8 && receipt.generation == Generation && receipt.capacity == 2 && generation == Generation + 1 && capacity == 65536 &&
                   context.retainedAtEntry && context.nested == ClockResult::Busy && context.nestedOutput == Sentinel && host.mallocCalls == 1 &&
                   controller.IsRetained() && !controller.RecoveryPending(),
               "stable CPU journal precedes malloc; copied generation/capacity and one committed output survive caller mutation and reentry");
        host.afterMalloc = nullptr;
        Verify(controller.FreeStorage(image, Generation + 1, output, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && !host.freeCalls &&
                   controller.IsRetained(),
               "possession of an issued tag does not grant a different generation cleanup authority");
        Verify(controller.FreeStorage(image, Generation, output + 1, &PoolTestHost::Owner, &receipt) == ClockResult::BindingChanged && !host.freeCalls &&
                   controller.IsRetained(),
               "controller accepts only its current exact issued first tag, never an interior or adopted pointer");
        Verify(controller.FreeStorage(image, Generation, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.released &&
                   host.freeCalls == 1 && !controller.IsRetained() && !controller.RecoveryPending(),
               "actual native storage retirement removes a consumed journal entry and clears conservative retention");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        host.nullMalloc = true;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, output, &receipt) == ClockResult::WriteFailure && receipt.entered &&
                   receipt.returned && !receipt.allocated && !receipt.debt && output == Sentinel && !controller.IsRetained() && !controller.RecoveryPending(),
               "normal returned malloc-null leaves output unchanged and discharges its empty journal entry");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        context.controller = &controller;
        context.revokeOwner = true;
        host.afterMalloc = &AfterMalloc;
        Verify(controller.Allocate(image, Generation, 2, &PoolTestHost::Owner, output, &receipt) == ClockResult::OwnerConflict && receipt.entered &&
                   receipt.returned && receipt.allocated && receipt.debt && !receipt.readyTag && output == Sentinel && controller.IsRetained() &&
                   controller.RecoveryPending(),
               "known returned allocation with failed acknowledgement remains private journal debt without publishing its tag");
        PoolTestHost::ownerResult = ClockResult::Ok;
        host.afterMalloc = nullptr;
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && progress.attempted == 1 &&
                   progress.released == 1 && !progress.retained && host.freeCalls == 1 && !controller.IsRetained() && !controller.RecoveryPending(),
               "recovered cleanup owner drains known unpublished allocation through the genuine storage-only free");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        host.mallocResult = ClockResult::OriginalException;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, output, &receipt) == ClockResult::OriginalException && receipt.entered &&
                   !receipt.returned && receipt.debt && output == Sentinel && controller.IsRetained() && controller.RecoveryPending() && host.mallocCalls == 1,
               "unreturned malloc output is never trusted as an issued tag and unknown allocation debt remains retained");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && !progress.attempted &&
                   !progress.released && progress.retained == 1 && !host.freeCalls && controller.IsRetained() && controller.RecoveryPending(),
               "unknown allocation effects are conservatively retained rather than guessed into retry or free");
        output = Sentinel;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, output, &receipt) == ClockResult::OwnerConflict && output == Sentinel &&
                   host.mallocCalls == 1,
               "retirement closes producer admission while native debt remains outstanding");
    }
    reset();
    for (const bool nativeReturned : {true, false})
    {
        Controller     controller;
        std::uintptr_t output = Sentinel;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, output, &receipt) == ClockResult::Ok,
               "prepare one current issued tag for controller free acknowledgement semantics");
        if (nativeReturned)
            host.afterFree = &AfterFree;
        else
        {
            host.freeResult = ClockResult::OriginalException;
            host.consumeFailedFree = true;
        }
        const auto result = controller.FreeStorage(image, Generation, output, &PoolTestHost::Owner, &receipt);
        Verify(result != ClockResult::Ok && receipt.entered && host.freeCalls == 1 &&
                   (nativeReturned ? receipt.returned && receipt.released && !controller.IsRetained()
                                   : !receipt.returned && receipt.debt && controller.IsRetained() && controller.RecoveryPending()),
               "known returned free is consumed before failed acknowledgement; unreturned free keeps unknown debt");
        PoolTestHost::ownerResult = ClockResult::Ok;
        host.afterFree = nullptr;
        Verify(controller.FreeStorage(image, Generation, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && host.freeCalls == 1,
               "neither known returned free nor unknown entered free can replay through the issued-tag controller");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && !progress.attempted && host.freeCalls == 1 &&
                   progress.retained == (nativeReturned ? 0u : 1u),
               "retirement preserves consumed-free no-replay and retains only the unknown-effect journal");
        reset();
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t pinned = Sentinel, unpublished = Sentinel;
        Verify(controller.Allocate(image, Generation, 1, &PoolTestHost::Owner, pinned, &receipt) == ClockResult::Ok &&
                   controller.PinPublication(Generation, pinned, image + 0x1000) == ClockResult::Ok,
               "prepare a conservatively pinned first record without inventing a native hierarchy attachment");
        context.controller = &controller;
        context.revokeOwner = true;
        host.afterMalloc = &AfterMalloc;
        Verify(controller.Allocate(image, Generation, 2, &PoolTestHost::Owner, unpublished, &receipt) == ClockResult::OwnerConflict && receipt.returned &&
                   receipt.allocated && unpublished == Sentinel,
               "prepare a later known unpublished record whose SDK acknowledgement failed");
        PoolTestHost::ownerResult = ClockResult::Ok;
        host.afterMalloc = nullptr;
        Verify(controller.Retire(image, Generation, 0, &PoolTestHost::Owner, progress) == ClockResult::Ok && !progress.scanned && !progress.attempted &&
                   progress.retained == 2 && controller.IsRetained() && controller.RecoveryPending(),
               "zero retirement budget bounds native work while publishing outstanding cleanup retention");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && !progress.attempted &&
                   progress.retained == 2 && !host.freeCalls,
               "one bounded pass preserves and rotates the pinned front instead of freeing published storage");
        Verify(controller.Retire(image, Generation, 1, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && progress.attempted == 1 &&
                   progress.released == 1 && progress.retained == 1 && host.freeCalls == 1,
               "next bounded pass fairly reaches and drains later known unpublished storage behind the pinned record");
        Verify(controller.Retire(image, Generation, 3, &PoolTestHost::Owner, progress) == ClockResult::Ok && progress.scanned == 1 && !progress.attempted &&
                   progress.retained == 1 && host.freeCalls == 1 && controller.IsRetained() && controller.RecoveryPending(),
               "remaining publication affinity retains module-owner debt with no generic detach, full End or slot readiness claim");
    }
    reset();
    {
        auto           controller = std::make_unique<Controller>();
        std::uintptr_t output = Sentinel;
        Verify(controller->Allocate(image, Generation, 1, &PoolTestHost::Owner, output, &receipt) == ClockResult::Ok,
               "prepare a live controller record for the CPU-only destruction boundary");
        const auto base = host.base;
        controller.reset();
        Verify(host.Contains(base) && !host.freeCalls,
               "controller destruction never silently frees native storage; actual runtime must retain its callback and module owner");
    }
    Fixture::active = nullptr;
    std::printf("Passed %u native custom sequence controller checks (64-bit; no native engine code executed)\n", controllerChecks);
    std::puts("Production custom sequence controller fixture completed");
}
