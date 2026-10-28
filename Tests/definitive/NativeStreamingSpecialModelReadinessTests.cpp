/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read real special-request prerequisites without native effects
 ****************************************************************************/
#include <cstdlib>
#include <new>
namespace ReadinessCpuAllocation
{
    bool failNext = false;
}
void* operator new(std::size_t bytes)
{
    if (ReadinessCpuAllocation::failNext)
    {
        ReadinessCpuAllocation::failNext = false;
        throw std::bad_alloc();
    }
    if (auto* value = std::malloc(bytes ? bytes : 1))
        return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept
{
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept
{
    std::free(value);
}

// Reuse only the existing owned PE/world/provider/cache fixture and real
// controller definitions. Its historical main is never executed here.
#pragma push_macro("main")
#undef main
#define main NativeSpecialReadinessSupportMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")

namespace
{
    void ChangeReadyProviderCount()
    {
        activeSpecial->Put(activeSpecial->provider, NativeStreamingRequestProfile::ProviderCountOffset, std::int32_t{19999});
    }
}

int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "readiness fixture remains in owned workspace scratch");
    SpecialFixture f(argv[1]);
    activeSpecial = &f;
    const auto image = f.transform.image.image;
    PoolTestHost::image = image;
    const auto reset = [&]
    {
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::failAt = PoolTestHost::revokeAt = 0;
        ReadinessCpuAllocation::failNext = false;
        f.Reset();
        specialCalls = 0;
        specialMode = 0;
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    };
    const auto ready = [&] { return SC::CheckReady(image, &PoolTestHost::Owner); };
    reset();
    Check(SC::CheckReady(image, nullptr) == ClockResult::NotInitialized && !PoolTestHost::reads && !PoolTestHost::ownerCalls && !specialCalls,
          "missing owner refuses before the loading gate or any native observation");
    reset();
    f.Global(NativeModelBankProfile::BankRva + std::uintptr_t{17} * NativeModelBankProfile::SlotBytes, std::uintptr_t{0});
    f.Put(f.metadata, 0x22, std::int16_t{123});
    Check(ready() == ClockResult::Ok && !specialCalls,
          "capability readiness does not require existing model metadata or inspect a future selected reference count");
    NativeStreamingModelContext::Value provider;
    Check(NativeStreamingModelContext::CaptureProvider(image, provider) == ClockResult::Ok && !provider.modelVtable && !provider.typeMethod &&
              !provider.animationMethod,
          "model-independent provider capture publishes no fabricated metadata/vtable authority");
    NativeModelBankReader::Value absent{17, image + NativeModelBankProfile::BankRva + std::uintptr_t{17} * NativeModelBankProfile::SlotBytes, 0};
    Check(NativeStreamingModelContext::Capture(image, absent, provider) == ClockResult::BindingChanged,
          "the actual per-effect capture still requires its selected real metadata");
    reset();
    f.Cache(true);
    Check(ready() == ClockResult::Ok && !specialCalls, "actual populated cache observations qualify without invoking their constructors or inserts");
    reset();
    Check(NativeStreamingCompletionContext::TryEnter(), "test owns the existing shared loading gate");
    Check(ready() == ClockResult::Busy && !specialCalls, "readiness cannot borrow a live special/loading operation's admission");
    NativeStreamingCompletionContext::Leave();
    Check(ready() == ClockResult::Ok && !specialCalls, "the shared gate is available after the original scope retires");
    for (unsigned fault = 0; fault != 7; ++fault)
    {
        reset();
        if (fault == 0)
            f.Global(XP::NameInitializedRva, static_cast<unsigned char>(0));
        if (fault == 1)
            f.Global(XP::NamePoolRva + 8, std::uint32_t{XP::NameBlockCount});
        if (fault == 2)
            f.Global(XP::CacheRva + 0x30, std::int32_t{0});
        if (fault == 3)
            f.Global(AP::HeapRva, std::uintptr_t{0});
        if (fault == 4)
            f.Put(f.provider, NativeStreamingRequestProfile::ProviderRowsOffset, std::uintptr_t{0});
        if (fault == 5)
            f.Put(f.provider, NativeStreamingRequestProfile::ProviderCountOffset, std::int32_t{-1});
        if (fault == 6)
            f.Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
        Check(ready() != ClockResult::Ok && !specialCalls,
              "current name initialization/cache/heap/provider/world refusal never becomes native work or cached readiness");
        reset();
        Check(ready() == ClockResult::Ok && !specialCalls, "each explicit repaired read freshly qualifies instead of retaining a failed/success state");
    }
    reset();
    PoolTestHost::watched = f.Address(f.provider) + NativeStreamingRequestProfile::ProviderCountOffset;
    PoolTestHost::after = &ChangeReadyProviderCount;
    Check(ready() == ClockResult::BindingChanged && !specialCalls, "two provider observations refuse a current header change without selecting metadata");
    reset();
    ReadinessCpuAllocation::failNext = true;
    const auto allocationResult = ready();
    Check(allocationResult == ClockResult::WriteFailure && !ReadinessCpuAllocation::failNext && !specialCalls,
          "temporary CPU snapshot allocation failure has an explicit result and performs no native allocator/request effect");
    Check(ready() == ClockResult::Ok && !specialCalls, "allocation refusal releases its transient completion gate");
    reset();
    const auto& pin = XP::Code[0];
    const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
    f.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
    Check(ready() == ClockResult::UnsupportedBuild && !specialCalls, "changed actual native special/name profile refuses before any engine entry");
    f.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    reset();
    Check(ready() == ClockResult::Ok, "measure only the new readiness read/owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (const auto fail : {1u, reads / 2, reads})
    {
        reset();
        PoolTestHost::failAt = fail;
        Check(ready() != ClockResult::Ok && !specialCalls, "guarded-copy failure never publishes readiness or invokes a native effect");
    }
    for (const auto fail : {1u, owners / 2, owners})
    {
        reset();
        PoolTestHost::revokeAt = fail;
        Check(ready() != ClockResult::Ok && !specialCalls, "owner revocation refuses with no cached lease or native effect");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = ready(); });
    worker.join();
    Check(foreign != ClockResult::Ok && !specialCalls, "foreign thread cannot borrow a current capability observation");
    reset();
    Check(ready() == ClockResult::Ok && !specialCalls, "the owner thread remains available after all read-only refusals");
    std::puts("Production native special-request readiness fixture completed");
    std::printf("Passed %u native special-request-readiness checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
