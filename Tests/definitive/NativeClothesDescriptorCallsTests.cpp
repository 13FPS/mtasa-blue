/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CPU-owned initialization over the existing native name/world host
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalDescriptorNameDefinitionsMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeClothesDescriptorFixture.h"
#include "../../Client/game_de/NativeClothesDescriptorCalls.h"
#include <memory>
#include <type_traits>
namespace DescriptorInitializerTests
{
    namespace DC = mta::game_de::NativeClothesDescriptorCalls;
    ClockResult DescriptorEntry(DC::NativeFunction, std::uintptr_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                        PoolTestHost::Module
#define ReadProcessMemory                       PoolTestHost::Read
#define MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE ::DescriptorInitializerTests::DescriptorEntry
#include "../../Client/game_de/NativeClothesDescriptorCalls.cpp"
#undef MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace DescriptorInitializerTests
{
    namespace DP = NativeClothesDescriptorProfile;
    SpecialFixture*                 descriptorFixture = nullptr;
    unsigned                        descriptorCalls = 0, descriptorEffects = 0, descriptorScenario = 0;
    std::uintptr_t                  passedDescriptor = 0;
    DC::NativeFunction              passedDescriptorFunction = nullptr;
    NativeClothesDescriptorStorage* nestedStorage = nullptr;
    ClockResult                     nestedDescriptor = ClockResult::NotInitialized;
    ClockResult                     DescriptorEntry(DC::NativeFunction function, std::uintptr_t descriptor, std::uint32_t& exception) noexcept
    {
        ++descriptorCalls;
        passedDescriptorFunction = function;
        passedDescriptor = descriptor;
        auto& fixture = *descriptorFixture;
        // A failing native initializer may have changed both CPU bytes and
        // shared caches. The caller must preserve effects without publishing
        // a descriptor or replaying this same one-use CPU storage.
        std::memset(reinterpret_cast<void*>(descriptor), descriptorScenario == 3 ? 0x5a : 0, DP::DescriptorBytes);
        fixture.Cache(true);
        ++descriptorEffects;
        if (descriptorScenario == 1)
        {
            exception = 0xc0000005;
            return ClockResult::OriginalException;
        }
        if (descriptorScenario == 2)
            PoolTestHost::ownerResult = ClockResult::BindingChanged;
        if (descriptorScenario == 4)
            fixture.Global(DP::Names[0].rva, static_cast<unsigned char>('!'));
        if (descriptorScenario == 5)
            fixture.Put(fixture.items, 0x10, std::uint32_t{999});
        if (descriptorScenario == 6)
            nestedDescriptor = DC::Initialize(fixture.transform.image.image, *nestedStorage, &PoolTestHost::Owner);
        if (descriptorScenario == 7)
            fixture.Global(NativeModelReferenceTransactionProfile::HeapAuthorityRva, std::uintptr_t{0});
        if (descriptorScenario == 8)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace DescriptorInitializerTests;
    if (argc != 2)
        return 2;
    static_assert(!std::is_copy_constructible_v<NativeClothesDescriptorStorage> && !std::is_move_constructible_v<NativeClothesDescriptorStorage>);
    static_assert(sizeof(NativeClothesDescriptorStorage) >= DP::DescriptorBytes && alignof(NativeClothesDescriptorStorage) == alignof(std::uintptr_t));
    SpecialFixture fixture(argv[1]);
    descriptorFixture = &fixture;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        // SpecialFixture definitions do not run their historical main. Publish
        // the same owned module image before actual native guards borrow it;
        // otherwise Module(nullptr) selects this test EXE instead of the host.
        PoolTestHost::image = image;
        NativeClothesDescriptorFixture::Install(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        descriptorCalls = descriptorEffects = descriptorScenario = 0;
        passedDescriptor = 0;
        passedDescriptorFunction = nullptr;
        nestedStorage = nullptr;
        nestedDescriptor = ClockResult::NotInitialized;
    };
    reset();
    NativeClothesDescriptorStorage storage;
    DC::Receipt                    receipt;
    Check(!storage.IsInitialized(), "CPU owner starts without a published/initialized descriptor");
    const auto initialResult = DC::Initialize(image, storage, &PoolTestHost::Owner, &receipt);
    const bool normalAccepted = initialResult == ClockResult::Ok && storage.IsInitialized() && receipt.entered && receipt.returned && descriptorCalls == 1 &&
                                descriptorEffects == 1 && receipt.entryRva == DP::InitializeRva && receipt.descriptor == passedDescriptor && passedDescriptor &&
                                passedDescriptorFunction == reinterpret_cast<DC::NativeFunction>(image + DP::InitializeRva);
    if (!normalAccepted)
    {
        unsigned char namesInitialized = 0;
        std::memcpy(&namesInitialized, reinterpret_cast<void*>(image + XP::NameInitializedRva), sizeof(namesInitialized));
        std::fprintf(stderr,
                     "descriptor normal result=%u entered=%u returned=%u initialized=%u calls=%u effects=%u exception=%08x "
                     "host=%llx expected=%llx root=%llx namesInitialized=%u reads=%u ownerCalls=%u\n",
                     static_cast<unsigned>(initialResult), static_cast<unsigned>(receipt.entered), static_cast<unsigned>(receipt.returned),
                     static_cast<unsigned>(storage.IsInitialized()), descriptorCalls, descriptorEffects, receipt.exceptionCode,
                     static_cast<unsigned long long>(PoolTestHost::image), static_cast<unsigned long long>(image),
                     static_cast<unsigned long long>(receipt.root), static_cast<unsigned>(namesInitialized), PoolTestHost::reads, PoolTestHost::ownerCalls);
        std::fflush(stderr);
    }
    Check(normalAccepted, "one real initializer ABI uses only the module CPU buffer and accepts shared native cache effects");
    const auto                                     completeReadCount = PoolTestHost::reads;
    std::array<unsigned char, DP::DescriptorBytes> zero{};
    Check(std::memcmp(reinterpret_cast<void*>(receipt.descriptor), zero.data(), zero.size()) == 0,
          "complete native descriptor defaults are verified after real initialization");
    DC::Receipt repeated;
    Check(DC::Initialize(image, storage, &PoolTestHost::Owner, &repeated) == ClockResult::Busy && !repeated.entered && !repeated.descriptor &&
              descriptorCalls == 1,
          "initialized storage cannot silently replay default initialization or expose another pointer");
    reset();
    NativeClothesDescriptorStorage ownerless;
    Check(DC::Initialize(image, ownerless, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.descriptor && !receipt.entered &&
              !ownerless.IsInitialized() && !descriptorCalls,
          "CPU storage alone does not create native owner admission or publish a pointer");
    for (unsigned scenario = 0; scenario != 8; ++scenario)
    {
        reset();
        NativeClothesDescriptorStorage refused;
        if (scenario == 0)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
        else if (scenario == 1)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        else if (scenario == 2)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        else if (scenario == 3)
            fixture.Global(DP::InitializeRva, static_cast<unsigned char>(0xcc));
        else if (scenario == 4)
            fixture.Global(DP::Names[0].rva, static_cast<unsigned char>('!'));
        else if (scenario == 5)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        else if (scenario == 6)
            fixture.Global(XP::NameInitializedRva, static_cast<unsigned char>(0));
        else
            PoolTestHost::failAt = 1;
        const auto result = DC::Initialize(image, refused, &PoolTestHost::Owner, &receipt);
        Check(result != ClockResult::Ok && !receipt.entered && !receipt.descriptor && !refused.IsInitialized() && !descriptorCalls,
              "native state/thread/GC/code/literals/owner/names/read refusals leave CPU storage unattached and unconsumed");
    }
    reset();
    NativeClothesDescriptorStorage retry;
    PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    Check(DC::Initialize(image, retry, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered,
          "pre-entry owner refusal does not consume the ordinary CPU buffer");
    PoolTestHost::ownerResult = ClockResult::Ok;
    Check(DC::Initialize(image, retry, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && retry.IsInitialized() && descriptorCalls == 1,
          "a fresh CPU buffer may be admitted after a prior pre-entry refusal");
    // Shared names/world/heap readers already have their own broad fixtures.
    // Cover only this operation's first and measured last distinct read edge,
    // rather than repeating all nested dependency faults in a new suite.
    for (const auto failAt : {1u, completeReadCount})
    {
        reset();
        NativeClothesDescriptorStorage faulted;
        PoolTestHost::failAt = failAt;
        const auto result = DC::Initialize(image, faulted, &PoolTestHost::Owner, &receipt);
        Check(result != ClockResult::Ok && !receipt.descriptor && !faulted.IsInitialized() &&
                  (failAt == 1 ? !receipt.entered && !descriptorCalls : receipt.entered && descriptorCalls == 1),
              "first and last actual copied-byte faults refuse publication on their respective pre/post native boundaries");
    }
    for (unsigned scenario = 1; scenario <= 8; ++scenario)
    {
        reset();
        auto                           owned = std::make_unique<NativeClothesDescriptorStorage>();
        NativeClothesDescriptorStorage nested;
        nestedStorage = &nested;
        descriptorScenario = scenario;
        const auto result = DC::Initialize(image, *owned, &PoolTestHost::Owner, &receipt);
        if (scenario == 6)
        {
            Check(
                result == ClockResult::Ok && owned->IsInitialized() && nestedDescriptor == ClockResult::Busy && !nested.IsInitialized() && descriptorCalls == 1,
                "the existing shared native gate rejects reentrant initialization without consuming another CPU buffer");
        }
        else
        {
            if (!(result != ClockResult::Ok && receipt.entered && !receipt.descriptor && !owned->IsInitialized() && descriptorCalls == 1 &&
                  descriptorEffects == 1))
                std::fprintf(stderr, "descriptor scenario=%u result=%u entered=%u returned=%u calls=%u effects=%u exception=%08x\n", scenario,
                             static_cast<unsigned>(result), receipt.entered, receipt.returned, descriptorCalls, descriptorEffects, receipt.exceptionCode);
            Check(result != ClockResult::Ok && receipt.entered && !receipt.descriptor && !owned->IsInitialized() && descriptorCalls == 1 &&
                      descriptorEffects == 1,
                  "entered native/postflight failure preserves actual effects and protects the uninitialized descriptor address");
            const auto calls = descriptorCalls;
            Check(DC::Initialize(image, *owned, &PoolTestHost::Owner, &receipt) == ClockResult::Busy && !receipt.entered && !receipt.descriptor &&
                      descriptorCalls == calls,
                  "an entered failure cannot reinitialize the same private CPU storage");
        }
        // No native free callback exists or is needed: the complete initializer
        // never stores/passes this unattached address outside its value writes.
        owned.reset();
        Check(descriptorCalls == 1 && descriptorEffects == 1, "ordinary module CPU destruction performs no native allocator/destructor operation");
    }
    reset();
    NativeClothesDescriptorStorage final;
    Check(DC::Initialize(image, final, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && final.IsInitialized(),
          "all terminal and shared-gate paths release the operation scope for a fresh ordinary CPU object");
    std::printf("Passed %u native clothes-descriptor checks (64-bit; no native engine code executed)\n", checks);
    std::puts("Production module-owned clothes initializer fixture completed");
}
