/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Honest prepared dependency HALF over actual issued records
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalPreparedDependencyHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "NativeAnimationBlockFactoryFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#include "../../Client/game_de/NativePrivateAnimationHierarchyController.h"
#include <cstring>
#include <iostream>
#include <string>

#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeAnimationBlockMemory.cpp"
#include "../../Client/game_de/NativeAnimationManagerMemory.cpp"
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#include "../../Client/game_de/NativePrivateAnimationHierarchyStorage.cpp"
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
#include "../../Client/game_de/NativePrivateAnimationHierarchyController.cpp"

namespace PreparedDependencyHost
{
    using Controller = NativeCustomAnimSequenceController;
    using Key = Controller::PrivateDependencyKey;
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    constexpr std::uint64_t Generation = 0x123456789abcull;
    unsigned                checks = 0;
    Key*                    callerKey = nullptr;
    void                    Verify(bool condition, const char* why)
    {
        ++checks;
        Check(condition, why);
    }
    ClockResult Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (callerKey)
        {
            *callerKey = {};
            callerKey = nullptr;
        }
        return result;
    }
    struct ActionContext
    {
        Controller*    controller = nullptr;
        std::uintptr_t image = 0, array = 0;
        bool           revoke = false;
        unsigned       calls = 0;
        ClockResult    nested = ClockResult::NotInitialized;
        bool           epochMatches = false, gateHeld = false;
    };
    ClockResult Borrow(NativeCustomAnimSequenceStorage& array, const NativeAnimationSequenceDependencyCalls::Epoch& epoch, void* opaque) noexcept
    {
        auto& context = *static_cast<ActionContext*>(opaque);
        ++context.calls;
        context.epochMatches = epoch.Image() == context.image && epoch.Generation() == Generation && array.IssuedTag() == context.array;
        context.gateHeld = !NativeStreamingCompletionContext::TryEnter();
        if (!context.gateHeld)
            NativeStreamingCompletionContext::Leave();
        context.nested = context.controller->FreeStorage(context.image, Generation, context.array, &Owner);
        if (context.revoke)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        return ClockResult::Ok;
    }
    void Install(SpecialFixture& fixture, std::uintptr_t image)
    {
        namespace Block = NativeAnimationBlockProfile;
        for (const auto& pin : Block::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto group = image + Block::GroupRva;
        SpecialFixture::Patch(&fixture, Block::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
        const auto selector = static_cast<std::uint8_t>(Block::LoadedSelector);
        SpecialFixture::Patch(&fixture, Block::RemapRva + Block::LoadedOpcodeIndex, &selector, 1, PAGE_EXECUTE_READ);
        const auto branch = static_cast<std::uint32_t>(Block::LoadedBranchRva);
        SpecialFixture::Patch(&fixture, Block::DispatchRva + Block::LoadedSelector * 4, &branch, 4, PAGE_EXECUTE_READ);
        std::array<unsigned char, Block::ObservationLimit * Block::RowBytes> bank{};
        SpecialFixture::Patch(&fixture, Block::BankRva, bank.data(), bank.size(), PAGE_READWRITE);
        const std::int32_t zero = 0;
        SpecialFixture::Patch(&fixture, Block::CountRva, &zero, sizeof(zero), PAGE_READWRITE);
        SpecialFixture::Patch(&fixture, NativeAnimationManagerProfile::AnimationCountRva, &zero, sizeof(zero), PAGE_READWRITE);
        std::array<unsigned char, 40> hierarchy{};
        SpecialFixture::Patch(&fixture, NativeAnimationBlockHierarchyProfile::BankRva, hierarchy.data(), hierarchy.size(), PAGE_READWRITE);
        NativeAnimationBlockFactoryFixture::Install(image, &SpecialFixture::Patch, &fixture);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        SpecialFixture::Patch(&fixture, 0xfec67a, PrivateAnimationHierarchyDetail::CacheProof, sizeof(PrivateAnimationHierarchyDetail::CacheProof),
                              PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    void Summary(const std::string& scenario)
    {
        std::cout << "Passed " << checks << " prepared dependency HALF checks (" << scenario << "; no parent attach/detach ACK)\n";
        std::cout << "Production prepared dependency HALF fixture completed\n" << std::flush;
    }
}
int main(int argc, char** argv)
{
    using namespace PreparedDependencyHost;
    Check(argc == 3, "prepared dependency child owns its output and exact scenario");
    const std::string scenario = argv[2];
    Check(scenario == "baseline" || scenario == "held" || scenario == "foreign" || scenario == "owner" || scenario == "span" || scenario == "stride",
          "only finite prepared dependency cases run");
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    fixture.Reset();
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    host.Reset(image, &SpecialFixture::Patch, &fixture);
    Install(fixture, image);
    PoolTestHost::ownerResult = ClockResult::Ok;
    PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
    PoolTestHost::watched = 0;
    PoolTestHost::after = nullptr;
    Controller                                controller;
    NativePrivateAnimationHierarchyController parent;
    NativeAnimationHierarchyReferenceToken    parentToken;
    Verify(parent.Create(image, Generation, &Owner, parentToken) == ClockResult::Ok && parent.Owns(parentToken),
           "command affinity comes from an actual issued CPU parent owner");
    std::uintptr_t array = 0, frames = 0;
    Verify(controller.Allocate(image, Generation, 3, &Owner, array) == ClockResult::Ok &&
               controller.AllocateFrames(image, Generation, 20, &Owner, frames) == ClockResult::Ok,
           "prepare actual issued array/frame journals");
    NativeAnimationSequenceReferenceToken row0{Generation, array, 0}, row1{Generation, array, 1}, row2{Generation, array, 2};
    Verify(controller.PrepareFreshCompressedSequence(image, row0, 2, false, frames, &Owner) == ClockResult::Ok &&
               controller.PrepareFreshCompressedSequence(image, row1, 2, false, frames, &Owner) == ClockResult::Ok,
           "prepare duplicate frame aliases before reservation");
    // This owner keeps its planned command for the whole dedicated process.
    // No physical parent attachment or acknowledged detach is fabricated.
    Key                                  planned{Generation, 1, parentToken.locator, array};
    Controller::PrivateDependencyReceipt receipt;
    if (scenario == "baseline")
    {
        auto invalid = planned;
        invalid.command = 0;
        Verify(controller.ReservePrivatePreparedArray(image, invalid, 3, &Owner, &receipt) == ClockResult::OwnerConflict && !receipt.reserved,
               "zero/non-owner command is not a private dependency");
        invalid = planned;
        invalid.ownerGeneration++;
        Verify(controller.ReservePrivatePreparedArray(image, invalid, 3, &Owner, &receipt) == ClockResult::OwnerConflict && !receipt.reserved,
               "foreign generation cannot reserve current records");
        Verify(controller.ReservePrivatePreparedArray(image, planned, 2, &Owner, &receipt) == ClockResult::InvalidArgument && !receipt.reserved,
               "full actual capacity must match the caller-owned array");
        ActionContext action{&controller, image, array};
        Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::BindingChanged && !action.calls,
               "a raw key cannot manufacture a reservation or epoch");
        Verify(controller.FreeFrames(image, Generation, frames, &Owner) == ClockResult::Ok &&
                   controller.FreeStorage(image, Generation, array, &Owner) == ClockResult::Ok && !controller.IsRetained(),
               "unreserved native records retain their actual storage-only frees");
        Verify(parent.Destroy(image, parentToken, &Owner) == ClockResult::Ok && !parent.IsRetained(), "empty CPU owner deletes plainly");
        Summary(scenario);
        return 0;
    }
    if (scenario == "span" || scenario == "stride")
    {
        // Corrupt only one source-owned row field, leaving the original frame
        // allocation genuine. Parent entry must still require a full span.
        if (scenario == "span")
        {
            const std::uint16_t count = 3;
            std::memcpy(reinterpret_cast<void*>(array + 0x44), &count, sizeof(count));
        }
        else
        {
            const std::uint16_t flags = 0x03;  // uncompressed root:32 bytes/frame
            std::memcpy(reinterpret_cast<void*>(array + 0x42), &flags, sizeof(flags));
        }
        ActionContext action{&controller, image, array};
        Verify(controller.ReservePrivatePreparedArray(image, planned, 3, &Owner, &receipt) == ClockResult::InvalidArgument && receipt.reserved &&
                   !receipt.verified,
               "genuine original allocation must cover count times actual flagged stride");
        Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) != ClockResult::Ok && !action.calls,
               "underallocated prepared rows cannot reach a prospective parent action");
        Verify(controller.FreeFrames(image, Generation, frames, &Owner) == ClockResult::OwnerConflict &&
                   controller.FreeStorage(image, Generation, array, &Owner) == ClockResult::OwnerConflict && !host.freeCalls,
               "failed preparation retains its planned-command journal until actual cancellation proof");
        Summary(scenario);
        ExitProcess(0);
    }
    if (scenario == "owner")
    {
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Verify(controller.ReservePrivatePreparedArray(image, planned, 3, &Owner, &receipt) == ClockResult::WrongThread && receipt.reserved,
               "CPU reservation survives a callback failure before a parent write");
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
    auto caller = planned;
    callerKey = &caller;
    Verify(controller.ReservePrivatePreparedArray(image, caller, 3, &Owner, &receipt) == ClockResult::Ok && !caller.command && receipt.reserved &&
               receipt.verified && receipt.capacity == 3 && receipt.frameRecords == 1,
           "copied intent reserves exact current records and deduplicates frame aliases");
    ActionContext action{&controller, image, array};
    Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::Ok && action.epochMatches &&
               action.gateHeld && action.nested == ClockResult::Busy && !receipt.released,
           "one borrowed gate cannot reenter/free or imply a parent ACK");
    Verify(controller.SetSequenceBoneTag(image, row0, 9, &Owner) == ClockResult::Ok, "safe bone selector changes do not change private frame dependencies");
    NativeAnimationSequenceName name{};
    name[0] = 'x';
    Verify(controller.SetSequenceName(image, row0, name, &Owner) == ClockResult::Ok, "safe name metadata remains writable while frame edges freeze");
    Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::Ok && !receipt.released,
           "frame-affecting validation permits name and bone-only updates");
    Controller::RowReceipt       rowReceipt;
    Controller::CopyReceipt      copyReceipt;
    Controller::FrameBindReceipt bindReceipt;
    Verify(controller.InitializeSequence(image, row2, &Owner, &rowReceipt) == ClockResult::OwnerConflict && !rowReceipt.entered,
           "held target cannot reset its frame pointer");
    Verify(controller.CopySequenceProperties(image, row2, array, &Owner, &copyReceipt) == ClockResult::OwnerConflict && !copyReceipt.entered,
           "held target cannot shallow-copy a new frame edge");
    Verify(controller.PrepareFreshCompressedSequence(image, row2, 2, false, frames, &Owner, &bindReceipt) == ClockResult::OwnerConflict && !bindReceipt.entered,
           "held fresh row cannot introduce another frame edge");
    if (scenario == "foreign")
    {
        const auto foreign = frames + 1;
        std::memcpy(reinterpret_cast<void*>(array + 0x48), &foreign, sizeof(foreign));
        Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::BindingChanged && receipt.uncertain,
               "an actually observed foreign edge retains lifetime uncertainty");
        std::memcpy(reinterpret_cast<void*>(array + 0x48), &frames, sizeof(frames));
        Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::OwnerConflict,
               "restoring raw fields cannot unpin observed uncertainty");
    }
    if (scenario == "owner")
    {
        action.revoke = true;
        Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::OwnerConflict && !receipt.released,
               "lost owner acknowledgement retains the command and resources");
        action.revoke = false;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.WithPrivatePreparedArray(image, planned, &Owner, &Borrow, &action, &receipt) == ClockResult::Ok && !receipt.released,
               "same-command qualification may resume without a raw rewrite or fake detach ACK");
    }
    Verify(controller.FreeFrames(image, Generation, frames, &Owner) == ClockResult::OwnerConflict &&
               controller.FreeStorage(image, Generation, array, &Owner) == ClockResult::OwnerConflict && !host.freeCalls,
           "both array and deduplicated frame dependencies block native free");
    Controller::RetireProgress progress;
    Verify(controller.Retire(image, Generation, 20, &Owner, progress) == ClockResult::Ok && !progress.attempted && !progress.released &&
               progress.retained == 2 && !host.freeCalls && controller.IsRetained() && controller.RecoveryPending(),
           "fair retirement preserves held resources without native cleanup");
    Summary(scenario);
    // The HALF cannot manufacture the missing parent detach acknowledgement.
    // This dedicated child exits with held journals; fixture teardown is not
    // claimed as production End/module/native-resource recovery.
    ExitProcess(0);
}
