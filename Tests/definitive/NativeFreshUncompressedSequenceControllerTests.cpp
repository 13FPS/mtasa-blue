/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real accepted row-format authority on current issued allocations
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalUncompressedControllerFixtureMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativeCustomAnimSequenceController.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameBindProfile.h"
#include "../../Client/game_de/NativeAnimationSequenceNameProfile.h"
namespace UncompressedControllerHost
{
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    namespace Bind = NativeAnimationSequenceFrameBindCalls;
    using Controller = NativeCustomAnimSequenceController;
    using Format = Controller::FrameFormat;
    using Accepted = Controller::AcceptedFrameFormat;
    unsigned       localChecks = 0, writes = 0, failureMode = 0;
    std::uintptr_t reuseAddress = 0;
    void           Verify(bool value, const char* why)
    {
        ++localChecks;
        Check(value, why);
    }
    ClockResult Owner() noexcept
    {
        return PoolTestHost::Owner();
    }
    ClockResult Write(std::uintptr_t, const Bind::Value&, std::uint32_t&) noexcept;
    bool        Same(const Accepted& a, const Accepted& b) noexcept
    {
        return a.format == b.format && a.frameBase == b.frameBase && a.count == b.count && a.root == b.root;
    }
    Accepted Sentinel() noexcept
    {
        return {Format::Compressed, 0x1234u, 0xdeadbeefu, true};
    }
    void Fill(std::uintptr_t base, std::size_t count, bool root)
    {
        // Explicit already-native float records: quaternion(0,0,0,1),
        // absolute time1 and optional native translation. Binding converts none.
        const std::array<std::uint32_t, 8> bits{{0, 0, 0, 0x3f800000u, 0x3f800000u, 0x40000000u, 0x40400000u, 0x40800000u}};
        const std::size_t                  stride = root ? 32u : 20u;
        for (std::size_t index = 0; index != count; ++index)
            std::memcpy(reinterpret_cast<void*>(base + index * stride), bits.data(), stride);
    }
    ClockResult ReusingMalloc(NativeCustomAnimSequenceStorageCalls::MallocFunction function, std::uintptr_t heap, std::size_t bytes, std::uint32_t alignment,
                              std::uintptr_t& output, std::uint32_t& exception) noexcept
    {
        const auto result = Heap::Malloc(function, heap, bytes, alignment, output, exception);
        if (result != ClockResult::Ok || !output || !reuseAddress || output == reuseAddress)
            return result;
        auto& state = *Heap::active;
        if (!state.Release(output) || state.allocationCount == state.allocations.size())
            return ClockResult::WriteFailure;
        // Force actual owned virtual memory reuse; format authority must not
        // resurrect merely because the allocator returns the old raw address.
        const auto allocated =
            reinterpret_cast<std::uintptr_t>(VirtualAlloc(reinterpret_cast<void*>(reuseAddress), bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (allocated != reuseAddress)
            return ClockResult::WriteFailure;
        state.allocations[state.allocationCount++] = allocated;
        state.base = output = allocated;
        std::memset(reinterpret_cast<void*>(allocated), 0xa5, bytes);
        return ClockResult::Ok;
    }
}
#define GetModuleHandleW                           PoolTestHost::Module
#define ReadProcessMemory                          PoolTestHost::Read
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE  ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE    ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE ::UncompressedControllerHost::Write
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
namespace UncompressedControllerHost
{
    ClockResult Write(std::uintptr_t row, const Bind::Value& value, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteCompressedFrames(row, value, exception);
        if (result != ClockResult::Ok)
            return result;
        if (failureMode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (failureMode == 2)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace UncompressedControllerHost;
    constexpr std::uint64_t generation = 0xfedcba98765432a1ull;
    Check(argc == 2, "uncompressed controller owns its evidence directory");
    SpecialFixture fixture(argv[1]);
    Heap::State    host;
    Heap::active = &host;
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
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        writes = failureMode = 0;
        reuseAddress = 0;
    };
    NativeAnimationSequenceAccess missing;
    Verify(missing.PrepareFreshUncompressedAnimationSequence({}, (std::numeric_limits<std::size_t>::max)(), true, 0) == ClockResult::NotInitialized,
           "optional uncompressed builder is not a default general setter");
    for (bool root : {false, true})
    {
        reset();
        Controller        controller;
        std::uintptr_t    tag = 0, frames = 0;
        const std::size_t count = root ? 3u : 65536u, stride = root ? 32u : 20u;
        const auto        bytes = static_cast<std::uint32_t>(count * stride);
        Check(controller.Allocate(image, generation, 2, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, bytes, &Owner, frames) == ClockResult::Ok,
              "issue real array/frame records");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        Accepted                              output = Sentinel();
        const auto                            original = output;
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::NotInitialized && Same(output, original),
               "fresh row has no invented format");
        Fill(frames, count, root);
        const std::vector<unsigned char> payload(reinterpret_cast<unsigned char*>(frames), reinterpret_cast<unsigned char*>(frames) + bytes);
        Controller::FrameBindReceipt     receipt;
        Verify(controller.PrepareFreshUncompressedSequence(image, token, count, root, frames, &Owner, &receipt) == ClockResult::Ok && receipt.accepted &&
                   receipt.verified && receipt.count == count && receipt.bytes == bytes && writes == 1,
               "real fresh uncompressed20/32 transaction validates full count before WORD");
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::Ok && output.format == Format::Uncompressed &&
                   output.frameBase == frames && output.count == count && output.root == root,
               "current accepted descriptor lost full count/root/original Frame affinity");
        Verify(*reinterpret_cast<std::uint16_t*>(tag + 0x42) == (root ? 3u : 1u) &&
                   *reinterpret_cast<std::uint16_t*>(tag + 0x44) == static_cast<std::uint16_t>(count) &&
                   !std::memcmp(payload.data(), reinterpret_cast<void*>(frames), bytes) && !host.freeCalls,
               "binding inferred compressed/shared bits, changed payload or freed an owner");
        Verify(controller.PrepareFreshUncompressedSequence(image, token, count, root, frames, &Owner, &receipt) != ClockResult::Ok && !receipt.entered &&
                   writes == 1,
               "bound row cannot replay a fresh transaction, even raw WORD0");
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok, "retire original Frame without following raw row aliases");
        output = original;
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::NotInitialized && Same(output, original),
               "known Frame retirement did not invalidate accepted authority");
        Check(controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok && !controller.IsRetained(), "retire original array");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0, oldFrames = 0;
        Check(controller.Allocate(image, generation, 3, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 96, &Owner, frames) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 30, &Owner, oldFrames) == ClockResult::Ok,
              "issue descriptor copy/overwrite records");
        NativeAnimationSequenceReferenceToken source{generation, tag, 0}, target{generation, tag, 1}, unknown{generation, tag, 2};
        Check(controller.PrepareFreshUncompressedSequence(image, source, 3, true, frames, &Owner) == ClockResult::Ok &&
                  controller.PrepareFreshCompressedSequence(image, target, 3, false, oldFrames, &Owner) == ClockResult::Ok,
              "prepare both actual shared binder branches");
        Accepted output;
        Verify(controller.ReadSequenceFrameFormat(image, target, &Owner, output) == ClockResult::Ok && output.format == Format::Compressed && !output.root &&
                   output.count == 3,
               "backward compressed10 branch lost its accepted descriptor");
        Controller::CopyReceipt copied;
        Verify(controller.CopySequenceProperties(image, target, tag, &Owner, &copied) == ClockResult::Ok && copied.rawCopied &&
                   controller.ReadSequenceFrameFormat(image, target, &Owner, output) == ClockResult::Ok && output.format == Format::Uncompressed &&
                   output.root && output.frameBase == frames,
               "actual80-byte Copy did not transfer matching accepted encoding authority");
        Verify(host.Contains(oldFrames) && host.Contains(frames) && !host.freeCalls, "copy erased the overwritten or aliased Frame journals");
        Verify(controller.CopySequenceProperties(image, source, tag, &Owner, &copied) == ClockResult::Ok && copied.selfCopy && !copied.entered &&
                   !copied.rawCopied,
               "selfcopy introduced a native write");
        Check(controller.InitializeSequence(image, source, &Owner) == ClockResult::Ok, "reset actual source row");
        output = Sentinel();
        const auto unchanged = output;
        Verify(controller.ReadSequenceFrameFormat(image, source, &Owner, output) == ClockResult::NotInitialized && Same(output, unchanged) &&
                   controller.ReadSequenceFrameFormat(image, target, &Owner, output) == ClockResult::Ok && output.format == Format::Uncompressed,
               "Initialize failed to clear only its descriptor or erased a copied alias");
        Check(controller.CopySequenceProperties(image, target, tag + 160u, &Owner, &copied) == ClockResult::Ok, "copy actual unknown row");
        output = unchanged;
        Verify(controller.ReadSequenceFrameFormat(image, target, &Owner, output) == ClockResult::NotInitialized && Same(output, unchanged),
               "Unknown source copy retained target's old format");
        Check(controller.FreeFrames(image, generation, oldFrames, &Owner) == ClockResult::Ok &&
                  controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok &&
                  controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok,
              "retire every original allocation once after copy/reset");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 64, &Owner, frames) == ClockResult::Ok,
              "prepare physical shape/descriptor distinctions");
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        Accepted                              output = Sentinel();
        const auto                            untouched = output;
        const std::uint16_t                   rawFlags = 3, rawCount = 2;
        std::memcpy(reinterpret_cast<void*>(tag + 0x42), &rawFlags, 2);
        std::memcpy(reinterpret_cast<void*>(tag + 0x44), &rawCount, 2);
        std::memcpy(reinterpret_cast<void*>(tag + 0x48), &frames, sizeof(frames));
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::NotInitialized && Same(output, untouched),
               "raw flags/count/pointer fabricated accepted uncompressed authority");
        Check(controller.InitializeSequence(image, token, &Owner) == ClockResult::Ok &&
                  controller.PrepareFreshUncompressedSequence(image, token, 2, true, frames, &Owner) == ClockResult::Ok,
              "admit actual encoding after explicit fresh builder");
        for (std::uint16_t flag : {std::uint16_t{1}, std::uint16_t{7}})
        {
            std::memcpy(reinterpret_cast<void*>(tag + 0x42), &flag, 2);
            output = untouched;
            Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::BindingChanged && Same(output, untouched),
                   "root/compression mismatch was trusted or published failed output");
        }
        std::memcpy(reinterpret_cast<void*>(tag + 0x42), &rawFlags, 2);
        const std::uint16_t wrongCount = 3;
        std::memcpy(reinterpret_cast<void*>(tag + 0x44), &wrongCount, 2);
        output = untouched;
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::BindingChanged && Same(output, untouched),
               "native count mismatch bypassed accepted bounds");
        std::memcpy(reinterpret_cast<void*>(tag + 0x44), &rawCount, 2);
        const auto interior = frames + 1u;
        std::memcpy(reinterpret_cast<void*>(tag + 0x48), &interior, sizeof(interior));
        output = untouched;
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::BindingChanged && Same(output, untouched),
               "interior pointer replaced exact original Frame authority");
        std::memcpy(reinterpret_cast<void*>(tag + 0x48), &frames, sizeof(frames));
        output = untouched;
        Verify(controller.ReadSequenceFrameFormat(image, {generation, tag, 1}, &Owner, output) != ClockResult::Ok && Same(output, untouched) &&
                   controller.ReadSequenceFrameFormat(image, {generation + 1, tag, 0}, &Owner, output) != ClockResult::Ok && Same(output, untouched),
               "bad row/generation exposed a format");
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Verify(controller.ReadSequenceFrameFormat(image, token, &Owner, output) == ClockResult::WrongThread && Same(output, untouched),
               "wrong owner got a cached descriptor");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok &&
                  controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok,
              "retire tamper test originals");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0, replacement = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 60, &Owner, frames) == ClockResult::Ok &&
                  controller.PrepareFreshUncompressedSequence(image, {generation, tag, 0}, 3, false, frames, &Owner) == ClockResult::Ok,
              "prepare exact address reuse case");
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok, "consume original frame before reissue");
        reuseAddress = frames;
        Check(controller.AllocateFrames(image, generation, 60, &Owner, replacement, nullptr, &ReusingMalloc) == ClockResult::Ok && replacement == frames,
              "force actual new Frame allocation at old raw address");
        Accepted   output = Sentinel();
        const auto unchanged = output;
        Verify(controller.ReadSequenceFrameFormat(image, {generation, tag, 0}, &Owner, output) == ClockResult::NotInitialized && Same(output, unchanged),
               "retired encoding resurrected from a same-generation reused address");
        Check(controller.FreeFrames(image, generation, replacement, &Owner) == ClockResult::Ok &&
                  controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok,
              "free new Frame and original array once");
        Verify(host.freeCalls == 3 && !controller.IsRetained(), "reuse lost or duplicated an actual allocation journal");
    }
    reset();
    {
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 60, &Owner, frames) == ClockResult::Ok,
              "prepare fresh full-span refusal records");
        Controller::FrameBindReceipt          receipt;
        NativeAnimationSequenceReferenceToken token{generation, tag, 0};
        for (auto base : {std::uintptr_t{0}, frames + 1u, tag})
            Verify(
                controller.PrepareFreshUncompressedSequence(image, token, 3, false, base, &Owner, &receipt) != ClockResult::Ok && !receipt.entered && !writes,
                "null/interior/array tag adopted a Frame");
        for (auto count : {std::size_t{0}, (std::numeric_limits<std::size_t>::max)()})
            Verify(controller.PrepareFreshUncompressedSequence(image, token, count, false, frames, &Owner, &receipt) != ClockResult::Ok && !receipt.entered &&
                       !writes,
                   "zero/overflow full count narrowed before byte-span admission");
        Verify(controller.PrepareFreshUncompressedSequence(image, token, 3, true, frames, &Owner, &receipt) != ClockResult::Ok && !receipt.entered && !writes,
               "root32 accepted the shorter child20 span");
        Check(controller.FreeFrames(image, generation, frames, &Owner) == ClockResult::Ok &&
                  controller.FreeStorage(image, generation, tag, &Owner) == ClockResult::Ok,
              "retire refusal originals");
    }
    for (unsigned failure : {1u, 2u})
    {
        reset();
        Controller     controller;
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 60, &Owner, frames) == ClockResult::Ok,
              "prepare entered uncompressed failure records");
        Controller::FrameBindReceipt receipt;
        failureMode = failure;
        Verify(controller.PrepareFreshUncompressedSequence(image, {generation, tag, 0}, 3, false, frames, &Owner, &receipt) != ClockResult::Ok &&
                   receipt.entered && receipt.accepted == (failure == 2) && writes == 1 && controller.IsRetained() && controller.RecoveryPending() &&
                   !host.freeCalls,
               "entered unknown/accepted postflight loss lost original rows or Frame debt");
        failureMode = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(controller.PrepareFreshUncompressedSequence(image, {generation, tag, 0}, 3, false, frames, &Owner, &receipt) != ClockResult::Ok && writes == 1,
               "entered failure automatically replayed the binder");
        Controller::RetireProgress progress;
        Verify(controller.Retire(image, generation, 4, &Owner, progress) == ClockResult::Ok && !progress.retained && host.freeCalls == 2,
               "private storage recovery failed to retire each original allocation once");
    }
    Heap::active = nullptr;
    std::puts("Production fresh uncompressed sequence controller fixture completed");
    std::puts("Accepted sequence frame format authority fixture completed");
    std::printf("Passed %u fresh uncompressed sequence controller checks (64-bit; no native engine code executed)\n", localChecks);
    return 0;
}
