/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the qualified ped animation field without a native call
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedModelAnimationHistoricalDefinitionsMain
#include "NativeModelAnimationTests.cpp"
#undef main
#pragma pop_macro("main")

namespace PedAnimationFixture
{
    namespace H = ModelAnimationFixture;
    namespace P = mta::game_de::NativeModelAnimationProfile;
    using Reader = mta::game_de::NativeModelAnimationReader;
    H::Fixture* changing = nullptr;
    unsigned    change = 0, fieldReads = 0;

    void Seed(H::Fixture& fixture)
    {
        // Independent saved leaf bytes seed only this owned SEC_IMAGE. They
        // are observations for the real reader, never executable test hooks.
        constexpr std::array<unsigned char, 5> getter{0x48, 0x8b, 0x41, 0x68, 0xc3};
        constexpr std::array<unsigned char, 3> type{0xb0, 0x07, 0xc3};
        fixture.transform.image.Patch(P::PedGetterRva, getter.data(), getter.size(), PAGE_EXECUTE_READ);
        fixture.transform.image.Patch(P::PedTypeGetterRva, type.data(), type.size(), PAGE_EXECUTE_READ);
        const auto method = fixture.transform.image.image + P::PedGetterRva;
        const auto typeMethod = fixture.transform.image.image + P::PedTypeGetterRva;
        fixture.transform.image.Patch(P::PedObservedVtableRva + P::GetterSlot, &method, sizeof(method), PAGE_READONLY);
        fixture.transform.image.Patch(P::PedObservedVtableRva + P::PedTypeSlot, &typeMethod, sizeof(typeMethod), PAGE_READONLY);
    }
    void Model(unsigned char* model, std::uintptr_t image, std::uint64_t value)
    {
        const auto table = image + P::PedObservedVtableRva;
        std::memcpy(model, &table, sizeof(table));
        std::memcpy(model + P::PedValueOffset, &value, sizeof(value));
    }
    void Mutate()
    {
        auto&      fixture = *changing;
        const auto image = fixture.transform.image.image;
        switch (change)
        {
            case 0:
            {
                const std::uint64_t highOnly = 0x2222222200000007;
                std::memcpy(fixture.object + P::PedValueOffset, &highOnly, sizeof(highOnly));
                break;
            }
            case 1:
                fixture.Slot(17, 0);
                break;
            case 2:
            {
                const auto table = image + H::Fixture::AlternateVtableRva;
                std::memcpy(fixture.object, &table, sizeof(table));
                break;
            }
            case 3:
            {
                const auto wrong = image + P::PedGetterRva + 1;
                fixture.transform.image.Patch(P::PedObservedVtableRva + P::GetterSlot, &wrong, sizeof(wrong), PAGE_READONLY);
                break;
            }
            case 4:
            {
                const auto wrong = image + P::PedTypeGetterRva + 1;
                fixture.transform.image.Patch(P::PedObservedVtableRva + P::PedTypeSlot, &wrong, sizeof(wrong), PAGE_READONLY);
                break;
            }
            case 5:
            {
                const unsigned char wrong = 6;
                fixture.transform.image.Patch(P::PedTypeGetterRva + 1, &wrong, sizeof(wrong), PAGE_EXECUTE_READ);
                break;
            }
            case 6:
            {
                const unsigned char wrong = 0x49;
                fixture.transform.image.Patch(P::PedGetterRva, &wrong, sizeof(wrong), PAGE_EXECUTE_READ);
                break;
            }
            case 7:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 8:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
        }
    }
    void FailNextRead()
    {
        PoolTestHost::failAt = PoolTestHost::reads + 1;
    }
    void MutateSecondFieldRead()
    {
        if (++fieldReads == 1)
            PoolTestHost::after = &MutateSecondFieldRead;
        else
            Mutate();
    }
}

int main(int argc, char** argv)
{
    namespace T = PedAnimationFixture;
    namespace P = T::P;
    using Reader = T::Reader;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "ped animation evidence stays in an explicit owned workspace folder");
    static_assert(P::PedReceiverBytes == 152 && P::PedValueOffset == 0x68);
    ModelAnimationFixture::Fixture fixture(argv[1]);
    T::changing = &fixture;
    const Reader::Binding binding{fixture.transform.image.image};
    PoolTestHost::image = binding.image;
    Reader        reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    std::uint32_t value = ModelAnimationFixture::sentinel;
    const auto    reset = [&]
    {
        fixture.Reset();
        T::Seed(fixture);
        T::Model(fixture.object, binding.image, 0x1111111100000007);
        T::Model(fixture.mapped, binding.image, 0x1111111100000007);
        T::fieldReads = 0;
        value = ModelAnimationFixture::sentinel;
    };
    reset();
    Check(unowned.Read(17, value) == ClockResult::NotInitialized && value == ModelAnimationFixture::sentinel,
          "the exact ped table and fields do not manufacture the synchronous native owner");
    for (const auto raw :
         std::array<std::uint64_t, 6>{0, 0xffffffffffffffffull, 0x89abcdef00000007ull, 0xffffffff00000000ull, 0x1234567887654321ull, 0x00000000ffffffffull})
    {
        reset();
        T::Model(fixture.object, binding.image, raw);
        Check(reader.Read(17, value) == ClockResult::Ok && value == static_cast<std::uint32_t>(raw),
              "the real ped getter observation preserves native EAX conversion after reading the entire QWORD68");
    }
    reset();
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(fixture.mapped));
    Check(reader.Read(17, value) == ClockResult::Ok && value == 7,
          "a current mapped ped receiver needs readable152 bytes and no allocation or loaded-state claim");
    reset();
    std::array<unsigned char, 152> staticPed{};
    T::Model(staticPed.data(), binding.image, 0x1234567887654321ull);
    fixture.transform.image.Patch(ModelAnimationFixture::Fixture::ImageReceiverRva, staticPed.data(), staticPed.size(), PAGE_READONLY);
    fixture.Slot(17, binding.image + ModelAnimationFixture::Fixture::ImageReceiverRva);
    Check(reader.Read(17, value) == ClockResult::Ok && value == 0x87654321u,
          "the same canonical ped class can be borrowed from readable image storage without an allocator lease");

    auto* boundary = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(boundary != nullptr, "allocate two owned pages for exact ped receiver bounds");
    DWORD prior = 0;
    Check(VirtualProtect(boundary + 4096, 4096, PAGE_NOACCESS, &prior) != FALSE, "guard the page immediately after the small receiver");
    reset();
    auto* exact = boundary + 4096 - P::PedReceiverBytes;
    T::Model(exact, binding.image, 0x89abcdef00000007ull);
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(exact));
    Check(reader.Read(17, value) == ClockResult::Ok && value == 7,
          "exactly152 readable bytes at a page boundary admit the ped path without the unrelated vehicle6c0 span");
    reset();
    auto* shortPed = boundary + 4096 - 0x80;
    T::Model(shortPed, binding.image, 7);
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(shortPed));
    Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel,
          "a readable vptr and QWORD68 alone cannot replace the complete proved152-byte receiver extent");
    reset();
    T::Model(exact, binding.image, 7);
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(exact));
    Check(VirtualProtect(boundary, 4096, PAGE_READWRITE | PAGE_GUARD, &prior) != FALSE, "guard the actual owned ped receiver");
    Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel, "guarded receiver bytes cannot publish an animation index");
    Check(VirtualProtect(boundary, 4096, PAGE_READWRITE, &prior) != FALSE, "restore only the owned page protection");
    Check(VirtualFree(boundary, 0, MEM_RELEASE) != FALSE, "release the owned boundary fixture");

    reset();
    const auto alternate = binding.image + ModelAnimationFixture::Fixture::AlternateVtableRva;
    const auto pedMethod = binding.image + P::PedGetterRva;
    fixture.transform.image.Patch(ModelAnimationFixture::Fixture::AlternateVtableRva + P::GetterSlot, &pedMethod, sizeof(pedMethod), PAGE_READONLY);
    std::memcpy(fixture.object, &alternate, sizeof(alternate));
    Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel,
          "an unproved class sharing the ped leaf cannot inherit the observed ped table and native type authority");
    const auto vehicleMethod = binding.image + P::GetterRva;
    fixture.transform.image.Patch(ModelAnimationFixture::Fixture::AlternateVtableRva + P::GetterSlot, &vehicleMethod, sizeof(vehicleMethod), PAGE_READONLY);

    for (T::change = 0; T::change != 9; ++T::change)
    {
        reset();
        PoolTestHost::watched = fixture.Address() + P::PedValueOffset;
        PoolTestHost::after = &T::Mutate;
        Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel,
              "ped QWORD high bits, bank, vptr, method/type cells or code and owner/GC changes cannot escape the observation bookends");
    }
    for (T::change = 3; T::change <= 6; ++T::change)
    {
        reset();
        PoolTestHost::watched = fixture.Address() + P::PedValueOffset;
        PoolTestHost::after = &T::MutateSecondFieldRead;
        Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel && T::fieldReads == 2,
              "current ped type/getter code and table cells still qualify after the second field sample before output publication");
    }
    reset();
    PoolTestHost::watched = fixture.Address() + P::PedValueOffset;
    PoolTestHost::after = &T::FailNextRead;
    Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel,
          "an OS read failure after the first ped field observation preserves the caller's previous output");
    for (unsigned failure = 0; failure != 5; ++failure)
    {
        reset();
        if (failure == 0)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (failure == 1)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (failure == 2)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (failure == 3)
            fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
        if (failure == 4)
            PoolTestHost::failAt = 1;
        Check(reader.Read(17, value) != ClockResult::Ok && value == ModelAnimationFixture::sentinel,
              "the new receiver variant preserves current owner, native thread, GC, gameplay state and read refusals");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.Read(17, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && value == ModelAnimationFixture::sentinel,
          "the ped profile does not broaden the actual native game-thread admission");
    for (unsigned broken = 0; broken != 3; ++broken)
    {
        reset();
        // The existing inherited vehicle leaf is a separate proven variant.
        // New ped-specific failures must not become a vehicle prerequisite.
        const auto table = broken == 2 ? alternate : binding.image + P::ObservedVtableRva;
        std::memcpy(fixture.object, &table, sizeof(table));
        ModelAnimationFixture::Fixture::Raw(fixture.object, 0x1234567887654321ull);
        if (broken == 0)
        {
            const unsigned char byte = 0x49;
            fixture.transform.image.Patch(P::PedGetterRva, &byte, 1, PAGE_EXECUTE_READ);
        }
        else
        {
            const auto wrong = binding.image + P::PedTypeGetterRva + 1;
            fixture.transform.image.Patch(P::PedObservedVtableRva + P::PedTypeSlot, &wrong, sizeof(wrong), PAGE_READONLY);
        }
        Check(reader.Read(17, value) == ClockResult::Ok && value == 0x87654321u,
              "existing primary or inherited vehicle getter authority stays independent of the new ped class qualifier");
    }
    std::puts("Production native ped model animation-file reader fixture completed");
    std::printf("Passed %u native ped model-animation checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
