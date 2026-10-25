/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Missing profile/state gates over actual private postprocess owners
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalPostProcessDependencyFixtureMain
#include "NativePreparedArrayDependencyTests.cpp"
#undef main
#pragma pop_macro("main")
namespace PostProcessStateHost
{
    using Parent = NativePrivateAnimationHierarchyController;
    using Children = NativeCustomAnimSequenceController;
    using Token = NativeAnimationHierarchyReferenceToken;
    using Receipt = NativePrivateAnimationHierarchyCalls::PostProcessReceipt;
    using Function = NativePrivateAnimationHierarchyCalls::NativePostProcessFunction;
    namespace Profile = NativePrivateAnimationHierarchyPostProcessProfile;
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    constexpr std::uint64_t Generation = 0x8ace02468ace0246ull;
    unsigned                localChecks = 0, calls = 0, mode = 0;
    std::uintptr_t          image = 0, target = 0, entryRva = 0;
    SpecialFixture*         active = nullptr;
    bool                    abi = true, gate = true;
    void                    Verify(bool value, const char* why)
    {
        ++localChecks;
        Check(value, why);
    }
    ClockResult Owner() noexcept
    {
        return PoolTestHost::Owner();
    }
    ClockResult Invoke(Function function, std::uintptr_t receiver, std::uint32_t& exception) noexcept
    {
        ++calls;
        abi = abi && reinterpret_cast<std::uintptr_t>(function) == image + entryRva && receiver == target;
        const bool held = !NativeStreamingCompletionContext::TryEnter();
        gate = gate && held;
        if (!held)
            NativeStreamingCompletionContext::Leave();
        // Delegate the exact saved CPU-only function. There is no substitute
        // arithmetic model and no GTA process/engine context in this fixture.
        const auto result = PrivateAnimationHierarchyDetail::NativePostProcessCall(function, receiver, exception);
        if (result != ClockResult::Ok)
            return result;
        if (mode == 1)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        if (mode == 2)
        {
            const auto& pin = Profile::Code[entryRva == Profile::QuaternionRva ? 0u : 1u];
            const auto  bad = static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1u);
            SpecialFixture::Patch(active, pin.rva + pin.size - 1, &bad, 1, PAGE_EXECUTE_READ);
        }
        return result;
    }
    ClockResult Run(Parent& parent, const Token& token, bool quaternion, Receipt& receipt)
    {
        target = token.locator;
        entryRva = quaternion ? Profile::QuaternionRva : Profile::TimeRva;
        return quaternion ? parent.RemoveQuaternionFlips(image, token, &Owner, &receipt, &Invoke)
                          : parent.CalculateTotalTime(image, token, &Owner, &receipt, &Invoke);
    }
    void Install(SpecialFixture& fixture)
    {
        PreparedDependencyHost::Install(fixture, image);
        for (const auto& pin : Profile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto& mask = Profile::SignMask;
        SpecialFixture::Patch(&fixture, mask.rva, mask.bytes, mask.size, PAGE_READONLY);
    }
}
int main(int argc, char** argv)
{
    using namespace PostProcessStateHost;
    Check(argc == 2, "postprocess state fixture owns its evidence directory");
    SpecialFixture fixture(argv[1]);
    Heap::State    host;
    Heap::active = &host;
    active = &fixture;
    image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        Install(fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        PoolTestHost::ownerResult = ClockResult::Ok;
        calls = mode = 0;
        target = entryRva = 0;
        abi = gate = true;
    };
    for (bool quaternion : {false, true})
    {
        reset();
        Parent parent;
        Token  token;
        Verify(parent.Create(image, Generation, &Owner, token) == ClockResult::Ok, "issue actual no-child CPU hierarchy");
        const auto& pin = Profile::Code[quaternion ? 0u : 1u];
        for (const auto offset : {std::size_t{0}, pin.size - 1})
        {
            const auto bad = static_cast<unsigned char>(pin.bytes[offset] ^ 1u);
            SpecialFixture::Patch(&fixture, pin.rva + offset, &bad, 1, PAGE_EXECUTE_READ);
            Receipt receipt;
            Verify(Run(parent, token, quaternion, receipt) != ClockResult::Ok && !receipt.entered && !calls,
                   "whole selected body first/last signature protects entry, not only a prefix");
            SpecialFixture::Patch(&fixture, pin.rva + offset, pin.bytes + offset, 1, PAGE_EXECUTE_READ);
        }
        PoolTestHost::ownerResult = ClockResult::WrongThread;
        Receipt receipt;
        Verify(Run(parent, token, quaternion, receipt) == ClockResult::WrongThread && !receipt.entered && !calls, "owner refusal enters no body");
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(Run(parent, token, quaternion, receipt) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.accepted && receipt.verified &&
                   calls == 1 && abi && gate,
               "genuine count0 entry retains this-only ABI and one held gate");
        Verify(parent.Destroy(image, token, &Owner) == ClockResult::Ok && !parent.IsRetained(),
               "successful payload operation leaves ordinary CPU retirement usable");
    }
    {
        reset();
        Parent  parent;
        Token   token;
        Receipt receipt;
        Check(parent.Create(image, Generation, &Owner, token) == ClockResult::Ok, "mask fixture parent");
        const auto& mask = Profile::SignMask;
        const auto  bad = static_cast<unsigned char>(mask.bytes[0] ^ 1u);
        SpecialFixture::Patch(&fixture, mask.rva, &bad, 1, PAGE_READONLY);
        Verify(Run(parent, token, true, receipt) != ClockResult::Ok && !receipt.entered && !calls,
               "quaternion sign literal is the reused exact readonly producer");
        Verify(Run(parent, token, false, receipt) == ClockResult::Ok && receipt.verified && calls == 1,
               "time has no artificial dependency on unused quaternion literal");
        SpecialFixture::Patch(&fixture, mask.rva, mask.bytes, mask.size, PAGE_READONLY);
        Check(parent.SetSequenceCount(image, token, 1, &Owner) == ClockResult::Ok, "source count-only write");
        const auto before = calls;
        Verify(Run(parent, token, false, receipt) != ClockResult::Ok && !receipt.entered && calls == before,
               "positive count without a real child command is not an empty time success");
        Verify(Run(parent, token, true, receipt) != ClockResult::Ok && !receipt.entered && calls == before,
               "positive count without actual array refuses quaternion entry too");
        Verify(parent.Destroy(image, token, &Owner) == ClockResult::Ok, "count-only receiver still owns no child resource");
    }
    {
        reset();
        Children       children;
        Parent         parent(&children);
        Token          token;
        std::uintptr_t array = 0, frames = 0;
        Check(children.Allocate(image, Generation, 1, &Owner, array) == ClockResult::Ok &&
                  children.AllocateFrames(image, Generation, 20, &Owner, frames) == ClockResult::Ok,
              "actual original arrays/frames issue native storage journals");
        const std::array<std::uint32_t, 5> nativeFrame{{0, 0, 0, 0x3f800000u, 0x3f800000u}};
        std::memcpy(reinterpret_cast<void*>(frames), nativeFrame.data(), sizeof(nativeFrame));
        Check(children.PrepareFreshUncompressedSequence(image, {Generation, array, 0}, 1, false, frames, &Owner) == ClockResult::Ok &&
                  parent.Create(image, Generation, &Owner, token) == ClockResult::Ok && parent.SetSequenceCount(image, token, 1, &Owner) == ClockResult::Ok &&
                  parent.SetSequences(image, token, array, &Owner) == ClockResult::Ok,
              "actual prepared uncompressed parent is acknowledged before payload");
        Receipt receipt;
        mode = 1;
        Verify(
            Run(parent, token, false, receipt) != ClockResult::Ok && receipt.entered && receipt.returned && receipt.accepted && !receipt.verified && calls == 1,
            "normal native return with lost acknowledgement keeps actual effects and blocks the command");
        mode = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Verify(parent.SetSequences(image, token, array, &Owner) == ClockResult::Ok, "real pointer acknowledgement remains possible for dependency cleanup");
        Verify(Run(parent, token, false, receipt) != ClockResult::Ok && !receipt.entered && calls == 1,
               "pointer acknowledgement cannot authorize automatic replay of failed time conversion");
        Verify(parent.SetSequences(image, token, 0, &Owner) == ClockResult::Ok && children.FreeFrames(image, Generation, frames, &Owner) == ClockResult::Ok &&
                   children.FreeStorage(image, Generation, array, &Owner) == ClockResult::Ok && parent.Destroy(image, token, &Owner) == ClockResult::Ok &&
                   !children.IsRetained() && !parent.IsRetained() && host.freeCalls == 2,
               "payload fault preserves matching detach and actual storage-only cleanup");
    }
    {
        reset();
        Parent  parent;
        Token   token;
        Receipt receipt;
        Check(parent.Create(image, Generation, &Owner, token) == ClockResult::Ok, "post-signature parent");
        mode = 2;
        Verify(Run(parent, token, false, receipt) != ClockResult::Ok && receipt.entered && receipt.returned && !receipt.verified && calls == 1,
               "full-code postflight loss retains a returned operation rather than replaying it");
        const auto& pin = Profile::Code[1];
        SpecialFixture::Patch(&fixture, pin.rva + pin.size - 1, pin.bytes + pin.size - 1, 1, PAGE_EXECUTE_READ);
        mode = 0;
        Verify(Run(parent, token, false, receipt) != ClockResult::Ok && !receipt.entered && calls == 1, "restoring raw code does not replay a blocked command");
        Verify(parent.Destroy(image, token, &Owner) == ClockResult::Ok && !parent.IsRetained(), "known no-child failed payload remains CPU-retireable");
    }
    std::cout << "Passed " << localChecks << " private hierarchy postprocess state checks (64-bit; saved CPU bodies only)\n";
    std::cout << "Production private hierarchy postprocess state fixture completed\n";
    std::cout << "Actual saved postprocessor CPU bodies executed without GTA\n";
    return 0;
}
