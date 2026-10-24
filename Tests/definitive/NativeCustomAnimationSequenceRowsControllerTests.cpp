/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual controller row methods, shared metadata and frame view fences
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalCustomSequenceControllerRowsHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeAnimationSequenceRowCalls.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
namespace SequenceRowTestHost
{
    namespace Rows = NativeAnimationSequenceRowCalls;
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    unsigned writes = 0, mode = 0, controllerChecks = 0;
    void (*afterFrame)() = nullptr;
    NativeAnimationSequenceReferenceToken* caller = nullptr;
    bool                                   changeCaller = false;
    std::uintptr_t                         rowToChange = 0;
    ClockResult                            Write(std::uintptr_t, Rows::Operation, const Rows::Value&, std::uint32_t&) noexcept;
    void                                   CheckController(bool value, const char* why)
    {
        ++controllerChecks;
        Check(value, why);
    }
    ClockResult Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (changeCaller && caller)
        {
            changeCaller = false;
            caller->ownerGeneration = 0;
            caller->firstTag = 1;
            caller->index = (std::numeric_limits<std::size_t>::max)();
        }
        return result;
    }
    void ChangeRow()
    {
        const std::uint16_t count = 7;
        std::memcpy(reinterpret_cast<void*>(rowToChange + 0x44), &count, sizeof(count));
    }
    bool Same(const NativeAnimationSequenceValues& left, const NativeAnimationSequenceValues& right)
    {
        return left.interfaceAddress == right.interfaceAddress && left.keyFrames == right.keyFrames && left.frameCount == right.frameCount &&
               left.boneTag == right.boneTag && left.bigChunk == right.bigChunk;
    }
}
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE       ::SequenceRowTestHost::Write
#define CheckFramePointer                         SequenceRowsOriginalFrameQualifier
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef CheckFramePointer
#undef MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace mta::game_de::NativeAnimationSequenceRowCalls
{
    ClockResult CheckFramePointer(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::uintptr_t pointer, CheckOwner owner) noexcept
    {
        const auto result = SequenceRowsOriginalFrameQualifier(image, storage, generation, pointer, owner);
        if (result == ClockResult::Ok && SequenceRowTestHost::afterFrame)
        {
            const auto after = SequenceRowTestHost::afterFrame;
            SequenceRowTestHost::afterFrame = nullptr;
            after();
        }
        return result;
    }
}
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
namespace SequenceRowTestHost
{
    ClockResult Write(std::uintptr_t row, Rows::Operation operation, const Rows::Value& target, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteRow(row, operation, target, exception);
        if (result != ClockResult::Ok)
            return result;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace SequenceRowTestHost;
    using Controller = NativeCustomAnimSequenceController;
    constexpr std::uint64_t generation = 0xfedcba9876543247ull;
    Check(argc == 2, "row fixture owns its explicit output directory");
    SpecialFixture fixture(argv[1]);
    Fixture::State host;
    Fixture::active = &host;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        writes = mode = 0;
        afterFrame = nullptr;
        caller = nullptr;
        changeCaller = false;
    };
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frameBase = 0;
        Check(controller.Allocate(image, generation, 2, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 41, &Owner, frameBase) == ClockResult::Ok,
              "prepare mixed controller row fixture");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0}, second{generation, tag, 1};
        NativeAnimationSequenceValues         values;
        std::uint32_t                         hash = 0xdeadbeef;
        CheckController(controller.ReadSequence(image, token, &Owner, values) == ClockResult::Ok && values.boneTag == 0xffff &&
                            controller.ReadSequenceHash(image, token, &Owner, hash) == ClockResult::NotInitialized && hash == 0xdeadbeef,
                        "shared owner row starts low-defined and never fabricates a full hash from native name bytes");
        const NativeAnimationSequenceValues                        sentinel{1, 2, 3, 4, true};
        const std::array<NativeAnimationSequenceReferenceToken, 6> invalid{{{0, tag, 0},
                                                                            {generation + 1, tag, 0},
                                                                            {generation, 0, 0},
                                                                            {generation, tag + 1, 0},
                                                                            {generation, tag, 2},
                                                                            {generation, tag, (std::numeric_limits<std::size_t>::max)()}}};
        for (const auto& rejected : invalid)
        {
            values = sentinel;
            Controller::RowReceipt refused;
            const auto             before = writes;
            CheckController(controller.ReadSequence(image, rejected, &Owner, values) != ClockResult::Ok && Same(values, sentinel) &&
                                controller.InitializeSequence(image, rejected, &Owner, &refused) != ClockResult::Ok && !refused.entered && writes == before,
                            "full generation, exact firstTag and full size_t row bounds precede output publication or stores");
        }
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Controller::RowReceipt refused;
        values = sentinel;
        CheckController(controller.ReadSequence(image, token, &Owner, values) == ClockResult::WrongThread && Same(values, sentinel) &&
                            controller.ReadSequenceHash(image, token, &Owner, hash) == ClockResult::WrongThread && hash == 0xdeadbeef &&
                            controller.InitializeSequence(image, token, &Owner, &refused) == ClockResult::WrongThread && !refused.entered &&
                            controller.SetSequenceBoneTag(image, token, 4, &Owner, &refused) == ClockResult::WrongThread && !refused.entered && !writes,
                        "all four controller row methods refuse the foreign callback before state changes");
        PoolTestHost::ownerResult = ClockResult::Ok;
        CheckController(controller.ReadSequence(image, token, nullptr, values) == ClockResult::NotInitialized && Same(values, sentinel) &&
                            controller.ReadSequenceHash(image, token, nullptr, hash) == ClockResult::NotInitialized && hash == 0xdeadbeef &&
                            controller.InitializeSequence(image, token, nullptr, &refused) == ClockResult::NotInitialized && !refused.entered &&
                            controller.SetSequenceBoneTag(image, token, 4, nullptr, &refused) == ClockResult::NotInitialized && !refused.entered && !writes,
                        "missing owner is a refusal on every row operation, never an implied callback");
        CheckController(controller.SetSequenceBoneTag(image, token, 9, &Owner) == ClockResult::Ok &&
                            controller.ReadSequence(image, token, &Owner, values) == ClockResult::Ok && values.boneTag == 9 &&
                            controller.ReadSequence(image, second, &Owner, values) == ClockResult::Ok && values.boneTag == 0xffff,
                        "per-row logical state is shared independently of facades and isolated from the next row");
        CheckController(controller.SetSequenceBoneTag(image, second, 65535, &Owner) == ClockResult::Ok &&
                            controller.ReadSequence(image, second, &Owner, values) == ClockResult::Ok && values.boneTag == 0xffff &&
                            controller.ReadSequenceHash(image, second, &Owner, hash) == ClockResult::NotInitialized && hash == 0xdeadbeef,
                        "another current row preserves full-int65535 intent without defining the unknown high hash WORD");
        auto mutated = token;
        caller = &mutated;
        changeCaller = true;
        CheckController(controller.SetSequenceBoneTag(image, mutated, 7, &Owner) == ClockResult::Ok && !mutated.ownerGeneration && mutated.firstTag == 1 &&
                            controller.ReadSequence(image, token, &Owner, values) == ClockResult::Ok && values.boneTag == 7,
                        "controller copies generation, firstTag and full index before owner callbacks");
        const auto readPointer = [&](std::uintptr_t pointer)
        {
            std::memcpy(reinterpret_cast<void*>(tag + 0x48), &pointer, sizeof(pointer));
            return controller.ReadSequence(image, token, &Owner, values);
        };
        CheckController(readPointer(frameBase) == ClockResult::Ok && values.keyFrames == frameBase && readPointer(frameBase + 40) == ClockResult::Ok &&
                            values.keyFrames == frameBase + 40,
                        "controller publishes qualified base and interior using Frame ByteSize rather than zero Capacity");
        const auto             freeBeforeInitialize = host.freeCalls;
        Controller::RowReceipt initialized;
        CheckController(controller.InitializeSequence(image, token, &Owner, &initialized) == ClockResult::Ok && initialized.accepted &&
                            controller.ReadSequence(image, token, &Owner, values) == ClockResult::Ok && values.boneTag == 0xffff && !values.frameCount &&
                            !values.bigChunk && !values.keyFrames && host.Contains(frameBase) && host.freeCalls == freeBeforeInitialize &&
                            controller.IsRetained(),
                        "Initialize updates shared logical and physical state without freeing or dropping the independent frame journal");
        Controller     foreignOwner;
        std::uintptr_t foreignFrames = 0;
        Check(foreignOwner.AllocateFrames(image, generation, 41, &Owner, foreignFrames) == ClockResult::Ok,
              "prepare another controller's current frame allocation");
        values = sentinel;
        CheckController(readPointer(foreignFrames) != ClockResult::Ok && Same(values, sentinel),
                        "same-generation bytes issued by another controller cannot supply this owner's frame origin");
        Check(foreignOwner.FreeFrames(image, generation, foreignFrames, &Owner) == ClockResult::Ok, "retire the separate frame owner");
        values = sentinel;
        CheckController(readPointer(frameBase + 41) != ClockResult::Ok && Same(values, sentinel), "one-past frame view preserves the whole output");
        values = sentinel;
        CheckController(readPointer(tag) != ClockResult::Ok && Same(values, sentinel), "array pointer is not frame allocation authority");
        std::memcpy(reinterpret_cast<void*>(tag + 0x48), &frameBase, sizeof(frameBase));
        values = sentinel;
        rowToChange = tag;
        afterFrame = &ChangeRow;
        CheckController(controller.ReadSequence(image, token, &Owner, values) != ClockResult::Ok && Same(values, sentinel),
                        "row mutation during the actual frame qualifier is caught by final exact five-field bookends");
        CheckController(controller.FreeFrames(image, generation, frameBase, &Owner) == ClockResult::Ok &&
                            controller.ReadSequence(image, token, &Owner, values) != ClockResult::Ok,
                        "freed frame record cannot publish a dangling row view");
        Check(controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok, "retire actual array without whole hierarchy cleanup");
        CheckController(controller.ReadSequence(image, token, &Owner, values) != ClockResult::Ok,
                        "freed token refuses before same-address reissue, with no ABA claim");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Controller     controller;
        std::uintptr_t tag = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok, "prepare one current row for entered mutation failure");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        Controller::RowReceipt                receipt;
        mode = failure;
        CheckController(controller.SetSequenceBoneTag(image, token, 12, &Owner, &receipt) != ClockResult::Ok && receipt.entered && writes == 1 &&
                            receipt.accepted == (failure == 2) && controller.IsRetained() && controller.RecoveryPending(),
                        "actual completed writer distinguishes unknown returned failure from accepted physical stores with lost owner acknowledgement");
        PoolTestHost::ownerResult = ClockResult::Ok;
        mode = 0;
        CheckController(controller.SetSequenceBoneTag(image, token, 12, &Owner, &receipt) != ClockResult::Ok && writes == 1,
                        "faulted array cannot replay an entered row mutation");
        Controller::RetireProgress progress;
        CheckController(controller.Retire(image, generation, 2, &Owner, progress) == ClockResult::Ok && !progress.retained && host.freeCalls == 1,
                        "known unpublished row failure keeps ordinary storage-only retirement eligible without rollback");
    }
    Fixture::active = nullptr;
    std::printf("Passed %u custom animation sequence row controller checks (64-bit; no native engine code executed)\n", controllerChecks);
    std::puts("Production custom animation sequence row controller fixture completed");
    return 0;
}
