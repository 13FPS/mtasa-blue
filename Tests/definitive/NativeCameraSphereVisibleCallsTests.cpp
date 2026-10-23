/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One genuine camera entry over owned image/world storage, no ped dependency
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeCameraSphereVisibleHistoricalLoadMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeCameraSphereVisibleFixture.h"
#include "../../Client/game_de/NativeCameraSphereVisibleCalls.h"
#include "../../Client/game_de/NativeWorldGroundProfile.h"
namespace
{
    namespace CS = mta::game_de::NativeCameraSphereVisibleCalls;
    ClockResult SphereEntry(CS::NativeFunction, std::uintptr_t, const CVector&, float, bool&, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                           PoolTestHost::Module
#define ReadProcessMemory                          PoolTestHost::Read
#define MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE ::SphereEntry
#include "../../Client/game_de/NativeCameraSphereVisibleCalls.cpp"
#undef MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace CP = NativeCameraSphereVisibleProfile;
    namespace CD = NativeStreamingCompletionContext;
    struct SphereFixture : LoadFixture
    {
        explicit SphereFixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        void Reset()
        {
            LoadFixture::Reset();
            NativeCameraSphereVisibleFixture::Install(transform.image.image, &LoadFixture::Write, this);
        }
    };
    SphereFixture* activeSphere = nullptr;
    CVector        expectedCenter, sourceCenter;
    float          expectedRadius = 0, sourceRadius = 0;
    unsigned       sphereCalls = 0, sphereEffects = 0, sphereScenario = 0;
    bool           sphereValue = false;
    CVector*       mutateSource = nullptr;
    float*         mutateRadius = nullptr;
    ClockResult    nestedSphere = ClockResult::Ok, nestedSphereLoad = ClockResult::Ok;
    std::uint32_t  FloatBits(const float& value)
    {
        std::uint32_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
    ClockResult MutateInputsOwner() noexcept
    {
        if (mutateSource)
            *mutateSource = CVector(900, 901, 902);
        if (mutateRadius)
            *mutateRadius = 903;
        return PoolTestHost::Owner();
    }
    ClockResult SphereEntry(CS::NativeFunction function, std::uintptr_t camera, const CVector& center, float radius, bool& value,
                            std::uint32_t& exception) noexcept
    {
        ++sphereCalls;
        ++sphereEffects;
        auto&      f = *activeSphere;
        const auto image = f.transform.image.image;
        Check(reinterpret_cast<std::uintptr_t>(function) == image + CP::EntryRva && camera == image + CP::CameraRva,
              "invoke exactly the previously qualified sphere entry with the actual static native camera");
        Check(&center != &sourceCenter && !std::memcmp(&center, &expectedCenter, sizeof(center)) && FloatBits(radius) == FloatBits(expectedRadius),
              "entry receives a private pre-callback XYZ copy and the original raw radius bits");
        f.application[0x79] = static_cast<unsigned char>(sphereEffects);
        value = sphereValue;
        if (sphereScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (sphereScenario == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (sphereScenario == 3)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (sphereScenario == 4)
            f.Put(f.graph.items.data(), 0x10, std::uint32_t{999});
        if (sphereScenario == 5)
        {
            const auto& pin = CP::Pins.front();
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (sphereScenario == 6)
            f.Global(CP::CameraRva + 0x18, std::uintptr_t{1});
        if (sphereScenario == 7)
            f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (sphereScenario == 8)
        {
            f.Global(CP::CameraRva + 0x18, reinterpret_cast<std::uintptr_t>(f.transform.matrix));
            f.Global(CP::CameraRva + 0xa10, float{21.25f});
            f.Global(CP::DepthRva, float{29.5f});
            f.graph.world[LPF::WorldModeOffset] = 1;
        }
        if (sphereScenario == 9)
        {
            CS::Receipt nested;
            bool        output = true;
            nestedSphere = CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &nested);
            Check(!nested.entered && output, "recursive sphere request has no native prefix or output");
            LC::Receipt load;
            nestedSphereLoad = LC::Load(image, false, &PoolTestHost::Owner, &load);
            Check(!load.entered, "native loading cannot enter an active camera query");
        }
        if (sphereScenario == 10)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "camera evidence uses a workspace-owned explicit path");
    SphereFixture fixture(argv[1]);
    activeSphere = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    CS::Receipt receipt;
    bool        output = true;
    const auto  reset = [&]
    {
        fixture.Reset();
        sourceCenter = CVector(1.25f, -2.5f, 7.75f);
        sourceRadius = 3.25f;
        std::memcpy(&expectedCenter, &sourceCenter, sizeof(expectedCenter));
        std::memcpy(&expectedRadius, &sourceRadius, sizeof(expectedRadius));
        sphereCalls = sphereEffects = sphereScenario = 0;
        sphereValue = false;
        mutateSource = nullptr;
        mutateRadius = nullptr;
        nestedSphere = nestedSphereLoad = ClockResult::Ok;
        output = true;
    };
    reset();
    Check(CS::Read(image, sourceCenter, sourceRadius, output, nullptr, &receipt) == ClockResult::NotInitialized && output && !receipt.entered && !sphereCalls,
          "raw coordinates and an image do not create the callback owner");
    for (bool value : {false, true})
    {
        reset();
        sphereValue = value;
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == value &&
                  receipt.value == value && receipt.entered && receipt.returned && sphereCalls == 1 && sphereEffects == 1 &&
                  receipt.camera == image + CP::CameraRva && receipt.world == fixture.graph.World() &&
                  !std::memcmp(&receipt.center, &expectedCenter, sizeof(expectedCenter)) && FloatBits(receipt.radius) == FloatBits(expectedRadius),
              "actual native false and true are each successful one-call scalar results");
    }
    for (std::uint32_t bits : {0x00000000u, 0x80000000u, 0xbf800000u, 0x7f800000u, 0x7fcabcdeu})
    {
        reset();
        std::memcpy(&sourceRadius, &bits, sizeof(bits));
        std::memcpy(&expectedRadius, &bits, sizeof(bits));
        std::memcpy(&sourceCenter.fX, &bits, sizeof(bits));
        std::memcpy(&expectedCenter.fX, &bits, sizeof(bits));
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output && sphereCalls == 1,
              "radius and center float bits have no invented finite, sign, size or normalization policy");
    }
    reset();
    mutateSource = &sourceCenter;
    mutateRadius = &sourceRadius;
    Check(CS::Read(image, sourceCenter, sourceRadius, output, &MutateInputsOwner, &receipt) == ClockResult::Ok && !output && sourceCenter.fX == 900 &&
              sourceRadius == 903,
          "snapshot both caller inputs before an owner callback can mutate their storage");
    for (unsigned scenario : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 10u})
    {
        reset();
        sphereScenario = scenario;
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output && receipt.entered &&
                  sphereCalls == 1 && sphereEffects == 1 && fixture.application[0x79] == 1,
              "entered exception or lost owner/world/profile/camera/GC/read acknowledgement keeps output and effects without replay or rollback");
        if (scenario == 1)
            Check(!receipt.returned && receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION, "SEH result preserves its genuine failure code");
    }
    for (unsigned scenario : {8u, 9u})
    {
        reset();
        sphereScenario = scenario;
        fixture.graph.world[LPF::WorldModeOffset] = 2;
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output && sphereCalls == 1,
              "coherent matrix/plane/depth/world-mode changes and rejected nested work do not retire a valid result");
        if (scenario == 9)
            Check(nestedSphere == ClockResult::Busy && nestedSphereLoad == ClockResult::Busy, "camera and loading reuse the same serialized native gate");
    }
    for (unsigned failure = 0; failure != 10; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                fixture.Global(LPF::RootRva, std::uintptr_t{0});
                break;
            case 1:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 2:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 3:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
                break;
            case 4:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 5:
                fixture.Global(CP::CameraRva + 0x18, std::uintptr_t{1});
                break;
            case 6:
            {
                const std::uintptr_t absent = 0;
                fixture.transform.image.Patch(CP::CameraRva + 0x18, &absent, sizeof(absent), PAGE_READONLY);
                break;
            }
            case 7:
            {
                const std::uint32_t zero = 0;
                fixture.transform.image.Patch(CP::DepthRva, &zero, sizeof(zero), PAGE_NOACCESS);
                break;
            }
            case 8:
                fixture.Put(fixture.graph.items.data(), 8, std::uint32_t{0x10000000});
                break;
            case 9:
                PoolTestHost::failAt = 1;
                break;
        }
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output && !receipt.entered &&
                  !sphereCalls,
              "missing current world/GTGC/camera storage/authority refuses before native entry");
    }
    for (const auto& pin : CP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::UnsupportedBuild && output &&
                  !receipt.entered && !sphereCalls,
              "both previously qualified whole native camera bodies stay exact executable pins");
    }
    reset();
    fixture.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
    fixture.Slot(17, std::uintptr_t{1});
    fixture.Global(NativeWorldGroundProfile::FreeStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::ScanRva, std::uint16_t{0xffff});
    Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output && sphereCalls == 1,
          "camera predicate requires no live ped pool, model association or Ground scan/matrix allocator state");
    reset();
    Check(CD::TryEnter(), "hold the existing native work scope");
    {
        CD::Scope held;
        Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::Busy && output && !receipt.entered &&
                  !sphereCalls,
              "an already active completion operation excludes camera entry");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt); });
    thread.join();
    Check(foreign != ClockResult::Ok && output && !receipt.entered && !sphereCalls, "foreign thread cannot borrow the current engine camera operation");
    reset();
    Check(CS::Read(image, sourceCenter, sourceRadius, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output && sphereCalls == 1,
          "all refused and entered paths release the shared scope for a fresh admitted query");
    std::puts("Production native camera sphere visible operation fixture completed");
    std::printf("Passed %u native camera-sphere-visible checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
