/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real composed query ordering and admission with only final native entries replaced
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedOnScreenHistoricalLoadMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativePedOnScreenFixture.h"
#include "../../Client/game_de/NativePedOnScreenCalls.h"
#include "../../Client/game_de/NativeWorldGroundProfile.h"
namespace
{
    namespace OS = mta::game_de::NativePedOnScreenCalls;
    ClockResult OnScreenEntry(OS::RadiusFunction, std::uintptr_t, float&, std::uint32_t&) noexcept;
    ClockResult OnScreenEntry(OS::CenterFunction, std::uintptr_t, CVector&, CVector*&, std::uint32_t&) noexcept;
    ClockResult OnScreenEntry(OS::ScreenFunction, std::uintptr_t, const CVector&, float, bool&, std::uint32_t&) noexcept;
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_ONSCREEN_NATIVE_INVOKE ::OnScreenEntry
#include "../../Client/game_de/NativePedOnScreenCalls.cpp"
#undef MTA_DE_PED_ONSCREEN_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace OP = NativePedOnScreenProfile;
    namespace OD = NativeStreamingCompletionContext;
    struct OnScreenFixture : LoadFixture
    {
        explicit OnScreenFixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        template <class T>
        void PedAt(std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + offset, &value, sizeof(value));
        }
        void Reset()
        {
            LoadFixture::Reset();
            NativePedOnScreenFixture::Install(transform.image.image, &LoadFixture::Write, this);
            PedAt(0, transform.image.image + NativePedMetadataProfile::PedVtableRva);
            PedAt(NativePedMetadataProfile::ModelOffset, std::uint16_t{17});
            PedAt(NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(3));
        }
        OS::Ped Ped()
        {
            OS::Ped value;
            Check(NativePedPoolReader({transform.image.image}, &PoolTestHost::Owner).Resolve(1, value) == ClockResult::Ok,
                  "new query receives one current occupied native ped observation");
            return value;
        }
    };
    OnScreenFixture*        activeOnScreen = nullptr;
    OS::Ped*                mutableCaller = nullptr;
    std::array<unsigned, 3> nativeCalls{};
    std::array<unsigned, 3> nativeOrder{};
    unsigned                orderCount = 0, effects = 0, scenario = 0, actionStage = 0;
    bool                    nativeValue = false;
    float                   nativeRadius = 3.25f;
    CVector                 nativeCenter(1.25f, -2.5f, 7.75f);
    CVector*                privateCenter = nullptr;
    ClockResult             nestedOnScreen = ClockResult::Ok, nestedLoader = ClockResult::Ok;
    std::uint32_t           Bits(const float& value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    ClockResult MutatingCallerOwner() noexcept
    {
        if (mutableCaller)
            *mutableCaller = {};
        return PoolTestHost::Owner();
    }
    ClockResult Stage(unsigned stage, std::uint32_t& exception)
    {
        ++nativeCalls[stage];
        Check(orderCount < nativeOrder.size(), "no native helper can be replayed");
        nativeOrder[orderCount++] = stage;
        ++effects;
        activeOnScreen->application[0x79] = static_cast<unsigned char>(effects);
        if (stage != actionStage)
            return ClockResult::Ok;
        auto&      f = *activeOnScreen;
        const auto image = f.transform.image.image;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (scenario == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (scenario == 3)
            f.Global(LPF::RootRva, std::uintptr_t{0});
        if (scenario == 4)
            f.PedAt(NativePedMetadataProfile::ModelOffset, std::uint16_t{18});
        if (scenario == 5)
            f.Slot(17, std::uintptr_t{0x1231});
        if (scenario == 6)
        {
            const auto& pin = OP::Pins.front();
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (scenario == 7)
        {
            f.Global(OP::CameraRva + 0x18, reinterpret_cast<std::uintptr_t>(f.transform.matrix));
            f.graph.world[LPF::WorldModeOffset] = 1;
        }
        if (scenario == 8)
        {
            OS::Receipt nested;
            bool        value = false;
            const auto  ped = f.Ped();
            nestedOnScreen = OS::Read(image, ped, value, &PoolTestHost::Owner, &nested);
            Check(!nested.entered, "recursive query cannot acquire the outer native operation");
            LC::Receipt load;
            nestedLoader = LC::Load(image, false, &PoolTestHost::Owner, &load);
            Check(!load.entered, "loading shares the same active native-entry gate");
        }
        return ClockResult::Ok;
    }
    ClockResult OnScreenEntry(OS::RadiusFunction function, std::uintptr_t ped, float& value, std::uint32_t& exception) noexcept
    {
        Check(reinterpret_cast<std::uintptr_t>(function) == activeOnScreen->transform.image.image + OP::RadiusRva &&
                  ped == reinterpret_cast<std::uintptr_t>(activeOnScreen->transform.pool.objects),
              "radius entry receives exact real resolved receiver, not current player or another pool slot");
        const auto result = Stage(0, exception);
        std::memcpy(&value, &nativeRadius, sizeof(value));
        return result;
    }
    ClockResult OnScreenEntry(OS::CenterFunction function, std::uintptr_t ped, CVector& center, CVector*& returned, std::uint32_t& exception) noexcept
    {
        Check(reinterpret_cast<std::uintptr_t>(function) == activeOnScreen->transform.image.image + OP::CenterRva &&
                  ped == reinterpret_cast<std::uintptr_t>(activeOnScreen->transform.pool.objects),
              "center entry follows radius using the same qualified native ped");
        const auto result = Stage(1, exception);
        std::memcpy(&center, &nativeCenter, sizeof(center));
        privateCenter = &center;
        returned = scenario == 9 ? &nativeCenter : &center;
        return result;
    }
    ClockResult OnScreenEntry(OS::ScreenFunction function, std::uintptr_t camera, const CVector& center, float radius, bool& value,
                              std::uint32_t& exception) noexcept
    {
        Check(reinterpret_cast<std::uintptr_t>(function) == activeOnScreen->transform.image.image + OP::ScreenRva &&
                  camera == activeOnScreen->transform.image.image + OP::CameraRva && &center == privateCenter &&
                  !std::memcmp(&center, &nativeCenter, sizeof(center)) && Bits(radius) == Bits(nativeRadius),
              "camera entry consumes only exact private native bound outputs with the real camera root and unchanged raw floats");
        const auto result = Stage(2, exception);
        value = nativeValue;
        return result;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "onscreen evidence uses explicit workspace-owned path");
    OnScreenFixture fixture(argv[1]);
    activeOnScreen = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    OS::Receipt receipt;
    bool        output = true;
    const auto  reset = [&]
    {
        fixture.Reset();
        nativeCalls = {};
        nativeOrder = {};
        orderCount = effects = scenario = actionStage = 0;
        nativeValue = false;
        nativeRadius = 3.25f;
        nativeCenter = CVector(1.25f, -2.5f, 7.75f);
        privateCenter = nullptr;
        nestedOnScreen = nestedLoader = ClockResult::Ok;
        mutableCaller = nullptr;
        output = true;
    };
    reset();
    auto ped = fixture.Ped();
    Check(OS::Read(image, ped, output, nullptr, &receipt) == ClockResult::NotInitialized && output && !receipt.entered && !effects,
          "a resolved raw ped is not native entry authority");
    for (bool value : {false, true})
    {
        reset();
        ped = fixture.Ped();
        nativeValue = value;
        Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && output == value && receipt.value == value && receipt.entered &&
                  receipt.returned && receipt.completedMask == 7 && nativeCalls == std::array<unsigned, 3>{1, 1, 1} &&
                  nativeOrder == std::array<unsigned, 3>{0, 1, 2} && receipt.nativePed == ped.nativePed && receipt.modelIndex == 17 &&
                  receipt.camera == image + OP::CameraRva && receipt.world == fixture.graph.World() &&
                  !std::memcmp(&receipt.center, &nativeCenter, sizeof(nativeCenter)) && Bits(receipt.radius) == Bits(nativeRadius),
              "exact radius-center-camera composition returns actual true or false once each with owned scalar/vector results");
    }
    reset();
    ped = fixture.Ped();
    const std::uint32_t nan = 0x7fcabcde, negativeZero = 0x80000000;
    std::memcpy(&nativeRadius, &nan, sizeof(nan));
    std::memcpy(&nativeCenter.fX, &negativeZero, sizeof(negativeZero));
    Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output,
          "composition forwards raw native float results without invented finite, radius or endpoint policy");
    for (unsigned selectedStage = 0; selectedStage != 3; ++selectedStage)
        for (unsigned selectedScenario = 1; selectedScenario != 7; ++selectedScenario)
        {
            reset();
            ped = fixture.Ped();
            scenario = selectedScenario;
            actionStage = selectedStage;
            Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output && receipt.entered &&
                      effects == selectedStage + 1 && orderCount == selectedStage + 1 && fixture.application[0x79] == effects,
                  "each entered prefix fault or lost owner/world/model/provenance stops before the next helper without replay or rollback");
        }
    reset();
    ped = fixture.Ped();
    scenario = 9;
    Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::BindingChanged && output && receipt.entered &&
              receipt.completedMask == 3 && nativeCalls[2] == 0,
          "unexpected returned center pointer never reaches native camera or caller");
    for (unsigned selectedScenario : {7u, 8u})
    {
        reset();
        ped = fixture.Ped();
        scenario = selectedScenario;
        fixture.graph.world[LPF::WorldModeOffset] = 2;
        Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output && effects == 3,
              "legitimate current camera matrix/worldmode change and synchronous nested refusal preserve the operation");
        if (selectedScenario == 8)
            Check(nestedOnScreen == ClockResult::Busy && nestedLoader == ClockResult::Busy, "query and loading reject shared-gate reentry");
    }
    for (unsigned failure = 0; failure != 11; ++failure)
    {
        reset();
        ped = fixture.Ped();
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
                fixture.transform.pool.flags[0] ^= 1;
                break;
            case 6:
                fixture.PedAt(NativePedMetadataProfile::ModelOffset, std::uint16_t{0xffff});
                break;
            case 7:
                fixture.PedAt(NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(2));
                break;
            case 8:
                fixture.Global(OP::CameraRva + 0x18, std::uintptr_t{1});
                break;
            case 9:
                fixture.transform.image.Patch(OP::RemapRva, &failure, 1, PAGE_READONLY);
                break;
            case 10:
                fixture.Put(fixture.graph.items.data(), 8, std::uint32_t{0x10000000});
                break;
        }
        Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output && !receipt.entered && !effects,
              "missing current native owner/world/ped/model/camera/selector refuses before any genuine helper");
    }
    for (const auto& pin : OP::Pins)
    {
        reset();
        ped = fixture.Ped();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && output && !receipt.entered && !effects,
              "every exact named/helper/support/literal provenance pin remains mandatory");
    }
    reset();
    ped = fixture.Ped();
    mutableCaller = &ped;
    Check(OS::Read(image, ped, output, &MutatingCallerOwner, &receipt) == ClockResult::Ok && !output && ped.nativePed == 0 && effects == 3,
          "copy the resolved caller receiver before any supplied owner callback can mutate it");
    reset();
    ped = fixture.Ped();
    fixture.Global(NativeWorldGroundProfile::FreeStartRva + NativeWorldGroundProfile::NextOffset, std::uintptr_t{0});
    fixture.Global(NativeWorldGroundProfile::ScanRva, std::uint16_t{0xffff});
    Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output &&
              *reinterpret_cast<const std::uint16_t*>(image + NativeWorldGroundProfile::ScanRva) == 0xffff,
          "bounds and camera query does not need Ground sector/matrix allocation roots or mutate native scan");
    reset();
    ped = fixture.Ped();
    Check(OD::TryEnter(), "hold an unrelated native loading operation");
    {
        OD::Scope held;
        Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::Busy && !receipt.entered && !effects,
              "new query uses the existing serialized native-entry gate");
    }
    reset();
    ped = fixture.Ped();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt); });
    thread.join();
    Check(foreign != ClockResult::Ok && output && !receipt.entered && !effects, "foreign thread cannot borrow a registered-ped native query");
    reset();
    ped = fixture.Ped();
    Check(OS::Read(image, ped, output, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && !output && effects == 3,
          "scope retires on every previous path and a fresh operation remains usable");
    std::puts("Production native ped onscreen operation fixture completed");
    std::printf("Passed %u native ped-onscreen checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
