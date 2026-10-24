/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only actual controller SetName, shared hash and private row effects
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalSequenceNameControllerHostMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#include "../../Client/game_de/NativeAnimationSequenceNameCalls.h"
namespace SequenceNameControllerHost
{
    namespace Fixture = NativeCustomAnimSequenceStorageFixture;
    namespace NameCalls = NativeAnimationSequenceNameCalls;
    unsigned                               nameChecks = 0, writes = 0, mode = 0;
    NativeAnimationSequenceReferenceToken* callerToken = nullptr;
    NativeAnimationSequenceName*           callerName = nullptr;
    void                                   Verify(bool value, const char* why)
    {
        ++nameChecks;
        Check(value, why);
    }
    ClockResult WriteName(std::uintptr_t, const NameCalls::Name&, std::size_t, bool, std::uint16_t, std::uint32_t&) noexcept;
    ClockResult Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (callerToken)
        {
            *callerToken = {};
            callerToken = nullptr;
        }
        if (callerName)
        {
            callerName->fill('!');
            (*callerName)[1] = 0;
            callerName = nullptr;
        }
        return result;
    }
}
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE   ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_NAME_WRITE      ::SequenceNameControllerHost::WriteName
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_ANIMATION_SEQUENCE_NAME_WRITE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
namespace SequenceNameControllerHost
{
    ClockResult WriteName(std::uintptr_t row, const NameCalls::Name& name, std::size_t bytes, bool project, std::uint16_t bone,
                          std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteName(row, name, bytes, project, bone, exception);
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
    using namespace SequenceNameControllerHost;
    using Controller = NativeCustomAnimSequenceController;
    constexpr std::uint64_t generation = 0xfedcba9876543259ull;
    Check(argc == 2, "name controller fixture owns its output directory");
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
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        writes = mode = 0;
        callerToken = nullptr;
        callerName = nullptr;
    };
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 2, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 41, &Owner, frames) == ClockResult::Ok,
              "prepare current private names and independent frames");
        NativeAnimationSequenceReferenceToken selected{generation, tag, 0}, unselected{generation, tag, 1};
        Check(controller.SetSequenceBoneTag(image, selected, 123, &Owner) == ClockResult::Ok, "prepare active source selector");
        for (const auto row : {tag, tag + 80u})
        {
            const std::uint16_t count = 0x1234;
            std::memcpy(reinterpret_cast<void*>(row + 0x44), &count, sizeof(count));
            std::memcpy(reinterpret_cast<void*>(row + 0x48), &frames, sizeof(frames));
        }
        const char                  text[] = {'m', 'i', 'X', static_cast<char>(0xe9), 0};
        NativeAnimationSequenceName name;
        Check(NativeAnimationSequenceNamePolicy::Copy(text, name), "prepare raw mixed/high-byte name value");
        constexpr std::uint32_t expectedHash = 0x633a9bccu;
        for (const auto& token : {selected, unselected})
        {
            const auto                    row = tag + 80u * token.index;
            std::array<unsigned char, 80> expected{};
            std::memcpy(expected.data(), reinterpret_cast<void*>(row), expected.size());
            std::memcpy(expected.data(), text, sizeof(text));
            const std::uint16_t projected = static_cast<std::uint16_t>(expectedHash);
            if (!token.index)
                std::memcpy(expected.data() + 0x40, &projected, sizeof(projected));
            Controller::NameReceipt receipt;
            Verify(controller.SetSequenceName(image, token, name, &Owner, &receipt) == ClockResult::Ok && receipt.accepted && receipt.rawWritten &&
                       receipt.verified && !std::memcmp(expected.data(), reinterpret_cast<void*>(row), expected.size()) && !host.freeCalls,
                   "actual owner writes raw prefix+NUL only and conditionally projects selected bone while preserving tail/flags/count/frames");
            std::uint32_t                 hash = 0;
            NativeAnimationSequenceValues value;
            Verify(controller.ReadSequenceHash(image, token, &Owner, hash) == ClockResult::Ok && hash == expectedHash &&
                       controller.ReadSequence(image, token, &Owner, value) == ClockResult::Ok && value.boneTag == projected && value.keyFrames == frames,
                   "shared owner logical union becomes fully defined independently of inactive physical bone40");
        }
        NativeAnimationSequenceName empty{};
        Controller::NameReceipt     receipt;
        Verify(controller.SetSequenceName(image, selected, empty, &Owner, &receipt) == ClockResult::Ok && receipt.accepted &&
                   *reinterpret_cast<const std::uint16_t*>(tag + 0x40) == 0xffff && (*reinterpret_cast<const std::uint16_t*>(tag + 0x42) & 0x10) != 0 &&
                   !host.freeCalls,
               "empty-name CRC projects ffff with active selector rather than invoking full-int -1 SetBone no-op");
        NativeAnimationSequenceName invalid;
        invalid.fill('x');
        const auto beforeInvalid = writes;
        Verify(controller.SetSequenceName(image, selected, invalid, &Owner, &receipt) != ClockResult::Ok && !receipt.entered && writes == beforeInvalid,
               "nonterminated typed input cannot enter a physical name store");
        auto changed = unselected;
        auto changedName = name;
        callerToken = &changed;
        callerName = &changedName;
        Verify(controller.SetSequenceName(image, changed, changedName, &Owner, &receipt) == ClockResult::Ok && !changed.ownerGeneration &&
                   changedName[0] == '!' && !std::memcmp(reinterpret_cast<void*>(tag + 80u), text, sizeof(text)),
               "controller copies token and full raw name before owner callbacks mutate caller storage");
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        const auto beforeRefusal = writes;
        Verify(controller.SetSequenceName(image, selected, name, &Owner, &receipt) == ClockResult::WrongThread && !receipt.entered && writes == beforeRefusal,
               "wrong callback refuses before any source-name store");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok &&
                  controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok,
              "retire original frame and array journals once");
        Verify(!controller.IsRetained() && !controller.RecoveryPending() && host.freeCalls == 2,
               "SetName neither adopts nor frees the independent allocation journals");
    }
    for (const unsigned failure : {1u, 2u})
    {
        reset();
        Controller     controller;
        std::uintptr_t tag = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok, "prepare entered-name failure record");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        NativeAnimationSequenceName           name;
        Check(NativeAnimationSequenceNamePolicy::Copy("name", name), "prepare captured failure name");
        Controller::NameReceipt receipt;
        mode = failure;
        Verify(controller.SetSequenceName(image, token, name, &Owner, &receipt) != ClockResult::Ok && receipt.entered && writes == 1 &&
                   receipt.accepted == (failure == 2) && receipt.rawWritten == (failure == 2) && controller.IsRetained() && controller.RecoveryPending() &&
                   !host.freeCalls,
               "unknown write failure and completed-store lost acknowledgement preserve their distinct receipt and journals");
        mode = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.SetSequenceName(image, token, name, &Owner, &receipt) != ClockResult::Ok && writes == 1,
               "faulted name store cannot replay or roll back");
        Controller::RetireProgress progress;
        Verify(controller.Retire(image, generation, 2, &Owner, progress) == ClockResult::Ok && !progress.retained && host.freeCalls == 1,
               "known unpublished failed name storage still retires once without matcher/cache/child cleanup");
    }
    Fixture::active = nullptr;
    std::puts("Production sequence SetName controller fixture completed");
    std::printf("Passed %u sequence name controller checks (64-bit; no native engine code executed)\n", nameChecks);
    return 0;
}
