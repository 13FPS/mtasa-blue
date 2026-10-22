/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine classifier entries with actual owner/image/model guards
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelClassificationHistoricalRequestMain
#include "NativeStreamingRequestTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelClassificationCalls.h"
namespace
{
    mta::game_de::ClockResult InvokeClassification(mta::game_de::NativeModelClassificationCalls::NativeFunction, std::uint32_t, unsigned char&,
                                                   std::uint32_t&) noexcept;
}
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE ::InvokeClassification
#include "../../Client/game_de/NativeModelClassificationCalls.cpp"
#undef MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace MC = NativeModelClassificationCalls;
    namespace MP = NativeModelClassificationProfile;
    unsigned       classifierCalls = 0, classifierScenario = 0;
    unsigned char  nativeClassification = 0;
    std::uintptr_t observedClassifier = 0;
    std::uint32_t  observedKey = 0;
    ClockResult    InvokeClassification(MC::NativeFunction function, std::uint32_t model, unsigned char& result, std::uint32_t& exception) noexcept
    {
        ++classifierCalls;
        observedClassifier = reinterpret_cast<std::uintptr_t>(function);
        observedKey = model;
        result = nativeClassification;
        auto& fixture = *activeRequest;
        if (classifierScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (classifierScenario == 2)
            fixture.Global(SP::RootRva, std::uintptr_t{0});
        if (classifierScenario == 3)
            fixture.Slot(model, fixture.Address(fixture.metadata) + 0x80);
        if (classifierScenario == 4)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (classifierScenario == 5)
            fixture.Put(fixture.provider, SP::ProviderCountOffset, std::int32_t{17});
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "classifier evidence is workspace-owned");
    RequestFixture fixture(argv[1]);
    activeRequest = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    for (const auto& pin : MP::Code)
        fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const auto reset = [&]
    {
        fixture.Reset();
        classifierCalls = classifierScenario = 0;
        nativeClassification = 0;
        observedClassifier = 0;
        observedKey = 0;
    };
    MC::Receipt receipt;
    bool        output = true;
    for (const auto& entry : MP::Entries)
        for (unsigned char value : std::array<unsigned char, 2>{0, 1})
        {
            reset();
            nativeClassification = value;
            output = value == 0;
            Check(MC::Read(image, 17, entry.predicate, &PoolTestHost::Owner, output, &receipt) == ClockResult::Ok && output == (value != 0) &&
                      receipt.entered && receipt.returned && classifierCalls == 1 && observedClassifier == image + entry.rva && observedKey == 17 &&
                      receipt.modelInfo == fixture.Address(fixture.metadata) && !requestCalls,
                  "exact genuine classifier entry returns actual true/false once on current qualified metadata, without executing Request");
        }
    for (unsigned scenario = 1; scenario <= 6; ++scenario)
        for (bool previous : {false, true})
        {
            reset();
            classifierScenario = scenario;
            nativeClassification = scenario == 6 ? 2 : 1;
            output = previous;
            Check(MC::Read(image, 17, NativeModelVehiclePredicate::Boat, &PoolTestHost::Owner, output, &receipt) != ClockResult::Ok && receipt.entered &&
                      classifierCalls == 1 && output == previous,
                  "entered exception, lost owner/root/model/provider or malformed native boolean preserves caller output without another entry");
        }
    reset();
    output = true;
    Check(MC::Read(image, 17, NativeModelVehiclePredicate::Boat, nullptr, output, &receipt) == ClockResult::NotInitialized && output && !receipt.entered,
          "no current owner cannot turn a model query into false");
    for (const auto key : std::array<std::uint32_t, 4>{20000, 65535, 65536, 0xffffffff})
    {
        reset();
        output = true;
        Check(MC::Read(image, key, NativeModelVehiclePredicate::Car, &PoolTestHost::Owner, output, &receipt) == ClockResult::InvalidArgument && output &&
                  !receipt.entered && !classifierCalls,
              "full DWORD bounds reject sentinel/sign/high aliases before native entry");
    }
    reset();
    output = true;
    Check(MC::Read(image, 17, static_cast<NativeModelVehiclePredicate>(255), &PoolTestHost::Owner, output, &receipt) == ClockResult::InvalidArgument &&
              output && !receipt.entered && !classifierCalls,
          "unknown predicate cannot select an arbitrary virtual or native entry");
    reset();
    fixture.Slot(17, 0);
    Check(MC::Read(image, 17, NativeModelVehiclePredicate::Boat, &PoolTestHost::Owner, output, &receipt) == ClockResult::NotInitialized && output &&
              !receipt.entered,
          "absent metadata refuses this borrowed slice rather than publishing a model wrapper");
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = MC::Read(image, 17, NativeModelVehiclePredicate::Car, &PoolTestHost::Owner, output, &receipt); });
    other.join();
    Check(foreign != ClockResult::Ok && !receipt.entered && !classifierCalls && output, "foreign thread cannot borrow native model getter authority");
    for (const auto& pin : MP::Code)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(MC::Read(image, 17, NativeModelVehiclePredicate::Plane, &PoolTestHost::Owner, output, &receipt) != ClockResult::Ok && !receipt.entered &&
                  !classifierCalls && output,
              "changed genuine classifier implementation refuses before borrowed model access");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    PoolTestHost::failAt = 1;
    Check(MC::Read(image, 17, NativeModelVehiclePredicate::Heli, &PoolTestHost::Owner, output, &receipt) != ClockResult::Ok && output && !receipt.entered,
          "actual guarded copy failure preserves caller output");
    reset();
    for (unsigned char type : std::array<unsigned char, 8>{0, 1, 3, 5, 6, 7, 8, 255})
    {
        const std::array<unsigned char, 6> code{0xb8, type, 0, 0, 0, 0xc3};
        fixture.transform.image.Patch(0x1100010, code.data(), code.size(), PAGE_EXECUTE_READ);
        NativeModelTypeObservation observed{false, 99};
        Check(
            MC::ReadType(image, 17, &PoolTestHost::Owner, observed) == ClockResult::Ok && observed.hasModel && observed.nativeType == type && !classifierCalls,
            "current bank type getter is observed through the shared whole-leaf proof without invoking any native virtual");
    }
    const std::array<unsigned char, 6> impure{0x8b, 0x41, 0x20, 0xc3, 0xcc, 0xcc};
    fixture.transform.image.Patch(0x1100010, impure.data(), impure.size(), PAGE_EXECUTE_READ);
    NativeModelTypeObservation preserved{true, 77};
    Check(MC::ReadType(image, 17, &PoolTestHost::Owner, preserved) != ClockResult::Ok && preserved.hasModel && preserved.nativeType == 77 && !classifierCalls,
          "a getter needing native object dereference is not replaced by an invented pure type implementation");
    reset();
    fixture.Slot(17, 0);
    preserved = {true, 77};
    Check(MC::ReadType(image, 17, &PoolTestHost::Owner, preserved) == ClockResult::Ok && !preserved.hasModel && preserved.nativeType == 0 && !classifierCalls,
          "qualified current absent bank is a distinct successful observation, never a fabricated native type9");
    preserved = {true, 77};
    Check(MC::ReadType(image, 17, nullptr, preserved) == ClockResult::NotInitialized && preserved.hasModel && preserved.nativeType == 77,
          "owner absence cannot become absent-model success or overwrite old type observation");
    std::puts("Production native model type leaf reader fixture completed");
    std::puts("Production native model classification primitive fixture completed");
    std::printf("Passed %u native model-classification checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
