/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual native orientation phases over owned ped/world/actor storage
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedTeleportFixtureMain
#include "NativePedTeleportTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedOrientationMemory.h"
#include "../../Client/game_de/NativePedOrientationProfile.h"
#include "../../Client/game_de/NativePedOrientationAccess.h"
namespace OrientationTestHost
{
    using Memory = mta::game_de::NativePedOrientationMemory;
    TeleportFixture*              fixture = nullptr;
    Memory*                       active = nullptr;
    unsigned                      calls = 0, failAt = 99, mutateAt = 99, mode = 0;
    bool                          lists = false, reenter = false;
    std::array<Memory::Phase, 3>  phases{};
    std::array<CVector, 3>        arguments{};
    std::array<std::uintptr_t, 3> entries{}, receivers{};
    mta::game_de::ClockResult     nested = mta::game_de::ClockResult::Ok, teleportNested = mta::game_de::ClockResult::Ok;
    Memory::Receipt               nestedReceipt;
    void                          Reset(TeleportFixture& value, bool linked = true)
    {
        value.Reset(linked);
        fixture = &value;
        calls = 0;
        failAt = mutateAt = 99;
        mode = 0;
        lists = reenter = false;
        phases = {};
        arguments = {};
        entries = {};
        receivers = {};
    }
    mta::game_de::ClockResult Invoke(Memory::Phase phase, const Memory::Entries& functions, void* ped, const CVector& input, std::uint32_t& exception) noexcept
    {
        const auto index = calls++;
        if (index >= 3)
            std::abort();
        phases[index] = phase;
        arguments[index] = input;
        receivers[index] = reinterpret_cast<std::uintptr_t>(ped);
        entries[index] = phase == Memory::Phase::Remove   ? reinterpret_cast<std::uintptr_t>(functions.remove)
                         : phase == Memory::Phase::Rotate ? reinterpret_cast<std::uintptr_t>(functions.rotate)
                                                          : reinterpret_cast<std::uintptr_t>(functions.add);
        if (reenter)
        {
            reenter = false;
            nested = active->Apply(1, input, &nestedReceipt, &Invoke);
            NativePedTeleportMemory          teleport({fixture->transform.image.image}, &PoolTestHost::Owner);
            NativePedTeleportMemory::Receipt receipt;
            teleportNested = teleport.Apply(1, CVector(1, 2, 3), &receipt, &TeleportTestHost::Invoke);
        }
        if (phase == Memory::Phase::Remove && lists)
        {
            // Only the final boundary supplies a visible list retirement. Old
            // nodes remain in the real owned arena but are now free/poisoned;
            // fresh phase2 admission must not fence their old bytes again.
            fixture->transform.Store(0xe8, std::uintptr_t{0});
            fixture->transform.Store(0xf0, std::uintptr_t{0});
            fixture->Global(0x53d3b18, std::uintptr_t{0});
            fixture->Global(0x5423ea0 + 8, std::uintptr_t{0});
            fixture->pointers.flags[0] = fixture->pointers.flags[1] = 0x80;
            fixture->entries.flags[0] = 0x80;
            std::memset(fixture->pointers.objects, 0xcd, 0x30);
            std::memset(fixture->entries.objects, 0xcd, 0x28);
        }
        if (phase == Memory::Phase::Rotate)
        {
            std::uintptr_t matrix = 0;
            std::memcpy(&matrix, static_cast<unsigned char*>(ped) + 0x18, sizeof(matrix));
            // This marker acknowledges one genuine native-call boundary. It
            // does not emulate trig, actor movement or assert native rotation
            // output; raw arguments and persistent prefix are what is tested.
            if (matrix)
                std::memcpy(reinterpret_cast<void*>(matrix), &input, sizeof(input));
            else
                std::memcpy(static_cast<unsigned char*>(ped) + 0x14, &input.fZ, sizeof(float));
        }
        if (phase == Memory::Phase::Add && lists)
        {
            // Fresh native-style nodes initialize every link before their
            // occupied generation is published. The retired rows above were
            // deliberately poisoned; reusing their old links is corruption.
            std::memset(fixture->pointers.objects, 0, 2 * fixture->pointers.stride);
            std::memset(fixture->entries.objects, 0, fixture->entries.stride);
            fixture->Lists();
            fixture->pointers.flags[0] = 8;
            fixture->pointers.flags[1] = 9;
            fixture->entries.flags[0] = 10;
        }
        if (index == mutateAt)
        {
            if (mode == 1)
                PoolTestHost::ownerResult = mta::game_de::ClockResult::WrongThread;
            if (mode == 2)
                fixture->transform.pool.flags[0] = 2;
            if (mode == 3)
                TeleportFixture::Store(fixture->items, fixture->actorIndex * NativeStartupWorldProfile::objectItemBytes + 8, std::uint32_t{0x30000000});
        }
        if (index == failAt)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return mta::game_de::ClockResult::Ok;
    }
}
#define MTA_DE_PED_ORIENTATION_NATIVE_INVOKE OrientationTestHost::Invoke
#include "../../Client/game_de/NativePedOrientationMemory.cpp"
#undef MTA_DE_PED_ORIENTATION_NATIVE_INVOKE
int main(int argc, char** argv)
{
    namespace H = OrientationTestHost;
    namespace P = NativePedOrientationProfile;
    using Memory = NativePedOrientationMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "orientation evidence remains in an explicit workspace directory");
    TeleportFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    auto& image = fixture.transform.image;
    for (const auto& pin : P::Code)
        image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const Memory::Binding binding{image.image};
    Memory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    H::active = &memory;
    const CVector   radians(0.25f, -0.75f, 2.5f);
    Memory::Receipt receipt;
    const auto      reset = [&](bool linked = true) { H::Reset(fixture, linked); };
    const auto      apply = [&] { return memory.Apply(1, radians, &receipt); };
    const auto      complete = [&]
    {
        const auto result = apply();
        if (result != ClockResult::Ok)
            std::fprintf(stderr, "Orientation result=%u calls=%u entered=%u returned=%u observed=%u\n", static_cast<unsigned>(result), H::calls,
                         static_cast<unsigned>(receipt.enteredMask), static_cast<unsigned>(receipt.returnedMask), static_cast<unsigned>(receipt.afterObserved));
        Check(result == ClockResult::Ok && H::calls == 3 && receipt.entered && receipt.enteredMask == 7 && receipt.returnedMask == 7 && receipt.afterObserved,
              "one qualified ped/world performs genuine Remove, Rotate and Add once in order with fresh post-call acknowledgements");
        for (unsigned index = 0; index < 3; ++index)
        {
            Check(static_cast<unsigned>(H::phases[index]) == index && H::receivers[index] == reinterpret_cast<std::uintptr_t>(fixture.transform.pool.objects) &&
                      !std::memcmp(&H::arguments[index], &radians, sizeof(radians)),
                  "typed phase preserves the same ped and unconverted SDK radians");
            Check(H::entries[index] == image.image + (index == 0   ? P::RemoveRva
                                                      : index == 1 ? P::RotateRva
                                                                   : P::AddRva),
                  "typed phase selects only its exact proved native target");
        }
    };
    reset();
    struct DefaultAccess final : NativePedOrientationAccess
    {
    } absent;
    Check(absent.SetPedOrientation({}, radians) == ClockResult::NotInitialized,
          "default access provides no owner, player substitution or fabricated native action");
    Check(unowned.Apply(1, radians, &receipt) == ClockResult::NotInitialized && !receipt.entered && !H::calls,
          "a disk image and native handle cannot grant callback authority");
    {
        NativePedPoseMutation gate;
        Check(gate && apply() == ClockResult::Busy && !receipt.entered && !H::calls,
              "existing placement/Teleport pose gate also excludes orientation before entry");
    }
    reset();
    complete();
    reset(false);
    complete();
    float heading = 0;
    std::memcpy(&heading, fixture.transform.pool.objects + P::InlineHeadingOffset, sizeof(heading));
    Check(receipt.matrixBefore == 0 && receipt.matrixAfter == 0 && !std::memcmp(&heading, &radians.fZ, sizeof(heading)),
          "proved absent-matrix branch reaches native inline heading without allocation or an identity matrix substitute");
    reset();
    const auto player = image.image + NativePedTransformProfile::PlayerVtableRva;
    fixture.transform.Store(0, player);
    complete();
    reset();
    fixture.Lists();
    H::lists = true;
    complete();
    Check(fixture.pointers.flags[0] == 8 && fixture.pointers.flags[1] == 9 && fixture.entries.flags[0] == 10,
          "legitimate native Remove may retire old list identities and Add may publish fresh generations");
    reset();
    H::reenter = true;
    complete();
    Check(H::nested == ClockResult::Busy && !H::nestedReceipt.entered && H::teleportNested == ClockResult::Busy && !TeleportTestHost::calls,
          "all three native phases retain the same shared pose gate against orientation and Teleport recursion");
    for (const auto& pin : P::Code)
        for (const auto offset : {std::size_t{0}, pin.size - 1})
        {
            reset();
            const unsigned char corrupt = pin.bytes[offset] ^ 1;
            image.Patch(pin.rva + offset, &corrupt, 1, PAGE_EXECUTE_READ);
            Check(apply() != ClockResult::Ok && !receipt.entered && !H::calls,
                  "every new actual producer/consumer pin endpoint refuses before any native phase");
            image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        }
    for (unsigned mode = 0; mode < 5; ++mode)
    {
        reset();
        if (mode == 0)
            fixture.transform.pool.flags[0] = 2;
        if (mode == 1)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (mode == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 3)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (mode == 4)
            TeleportFixture::Store(fixture.actor, 0x10, TeleportFixture::Address(fixture.derivedActorClass));
        Check(apply() != ClockResult::Ok && !receipt.entered && !H::calls,
              "current pool, state, GC, native thread and actual typed actor prerequisites refuse before effects");
    }
    for (unsigned phase = 0; phase < 3; ++phase)
    {
        reset();
        H::failAt = phase;
        Check(apply() == ClockResult::OriginalException && H::calls == phase + 1 && receipt.enteredMask == ((1u << (phase + 1)) - 1) &&
                  receipt.returnedMask == ((1u << phase) - 1) && !receipt.afterObserved,
              "native exception retains exactly the entered prefix and never compensates Add or retries a phase");
        if (phase >= 1)
            Check(!std::memcmp(fixture.transform.matrix, &radians, sizeof(radians)), "a possibly accepted native rotation remains after lost acknowledgement");
        for (unsigned mode = 1; mode <= 3; ++mode)
        {
            reset();
            H::mutateAt = phase;
            H::mode = mode;
            Check(apply() != ClockResult::Ok && H::calls == phase + 1 && receipt.entered && receipt.returnedMask == ((1u << (phase + 1)) - 1) &&
                      !receipt.afterObserved,
                  "post-native owner, pool generation and registry loss stop the same prefix before any later call");
        }
    }
    reset();
    complete();
    for (const std::array<std::uint32_t, 3>& bits :
         {std::array<std::uint32_t, 3>{0x80000000u, 0x7f800000u, 0x7fc12345u}, std::array<std::uint32_t, 3>{0xff800000u, 0xffc54321u, 0x00000001u}})
    {
        reset();
        CVector raw;
        std::memcpy(&raw, bits.data(), sizeof(raw));
        Check(memory.Apply(1, raw, &receipt) == ClockResult::Ok && H::calls == 3 && !std::memcmp(&receipt.requested, &raw, sizeof(raw)),
              "actual raw SDK angle storage is forwarded without a new finite gate, normalization or degree conversion");
        for (const auto& observed : H::arguments)
            Check(!std::memcmp(&observed, &raw, sizeof(raw)), "each genuine native boundary receives original binary32 input bits");
    }
    std::puts("Production native ped three-phase orientation fixture completed");
    std::printf("Passed %u native ped-orientation checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
