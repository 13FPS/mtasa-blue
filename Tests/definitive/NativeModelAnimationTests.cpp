/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual qualified animation slot/whole-QWORD observation, no invocation
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelAnimationHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelAnimationReader.h"
#include "../../Client/game_de/NativeModelAnimationProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeModelAnimationReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace ModelAnimationFixture
{
    using Reader = mta::game_de::NativeModelAnimationReader;
    namespace P = mta::game_de::NativeModelAnimationProfile;
    constexpr std::uint32_t sentinel = 0xaabbccdd;
    struct Fixture final : BankFixture
    {
        static constexpr std::uintptr_t AlternateVtableRva = 0x4317300, ImageReceiverRva = 0x5318000;
        unsigned char*                  object = nullptr;
        HANDLE                          mapping = nullptr;
        unsigned char*                  mapped = nullptr;
        explicit Fixture(const std::filesystem::path& path) : BankFixture(path)
        {
            object = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
            if (mapping)
                mapped = static_cast<unsigned char*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 4096));
            Check(object && mapped, "allocate only owned model getter receiver fixtures");
            for (const auto& pin : P::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto method = transform.image.image + P::GetterRva;
            transform.image.Patch(P::ObservedVtableRva + P::GetterSlot, &method, sizeof(method), PAGE_READONLY);
            transform.image.Patch(AlternateVtableRva + P::GetterSlot, &method, sizeof(method), PAGE_READONLY);
            Reset();
        }
        ~Fixture()
        {
            if (mapped)
                UnmapViewOfFile(mapped);
            if (mapping)
                CloseHandle(mapping);
            if (object)
                VirtualFree(object, 0, MEM_RELEASE);
        }
        std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(object); }
        static void    Raw(unsigned char* model, std::uint64_t value) { std::memcpy(model + P::ValueOffset, &value, sizeof(value)); }
        void           Reset()
        {
            BankFixture::Reset();
            std::memset(object, 0xcc, 4096);
            std::memset(mapped, 0xdd, 4096);
            const auto table = transform.image.image + P::ObservedVtableRva;
            std::memcpy(object, &table, sizeof(table));
            std::memcpy(mapped, &table, sizeof(table));
            Raw(object, 0x1111111100000007);
            Raw(mapped, 0x1111111100000007);
            Slot(17, Address());
        }
    };
    Fixture* changing = nullptr;
    unsigned change = 0;
    void     Mutate()
    {
        const auto image = changing->transform.image.image;
        switch (change)
        {
            case 0:
                Fixture::Raw(changing->object, 0x2222222200000007);
                break;
            case 1:
                changing->Slot(17, 0);
                break;
            case 2:
            {
                const auto alternate = image + Fixture::AlternateVtableRva;
                std::memcpy(changing->object, &alternate, sizeof(alternate));
                break;
            }
            case 3:
            {
                const auto wrong = image + P::GetterRva + 1;
                changing->transform.image.Patch(P::ObservedVtableRva + P::GetterSlot, &wrong, sizeof(wrong), PAGE_READONLY);
                break;
            }
            case 4:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 5:
                changing->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 6:
            {
                const auto wrong = static_cast<unsigned char>(P::Getter[0] ^ 1);
                changing->transform.image.Patch(P::GetterRva, &wrong, 1, PAGE_EXECUTE_READ);
                break;
            }
        }
    }
}
int main(int argc, char** argv)
{
    namespace H = ModelAnimationFixture;
    using Reader = H::Reader;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "animation getter evidence has an explicit owned workspace directory");
    Reader::Binding refused{123};
    Check(!Reader::Qualify(refused) && refused.image == 123, "shipping getter qualification refuses a non-game executable");
    H::Fixture fixture(argv[1]);
    H::changing = &fixture;
    const Reader::Binding binding{fixture.transform.image.image};
    PoolTestHost::image = binding.image;
    Reader        reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    std::uint32_t value = H::sentinel;
    const auto    reset = [&]
    {
        fixture.Reset();
        value = H::sentinel;
    };
    Check(Reader::Revalidate(binding), "exact whole8B getter, EAX consumer and readonly disk slot reference qualify");
    Check(unowned.Read(17, value) == ClockResult::NotInitialized && value == H::sentinel, "model bytes and vtable do not grant owner admission");
    const std::array<std::uint64_t, 6> values{{0, 0xffffffffffffffff, 0x89abcdef00000007, 0xffffffff00000000, 0x1234567887654321, 0x00000000ffffffff}};
    for (const auto raw : values)
    {
        reset();
        H::Fixture::Raw(fixture.object, raw);
        Check(reader.Read(17, value) == ClockResult::Ok && value == static_cast<std::uint32_t>(raw),
              "wholeQWORD samples preserve exact native EAX low32 intent without following any highbits or pointee");
    }
    reset();
    const auto alternate = binding.image + H::Fixture::AlternateVtableRva;
    std::memcpy(fixture.object, &alternate, sizeof(alternate));
    Check(reader.Read(17, value) == ClockResult::Ok && value == 7,
          "another current readonly table may share the exact inherited getter without a hidden IsVehicle/type predicate");
    reset();
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(fixture.mapped));
    Check(reader.Read(17, value) == ClockResult::Ok && value == 7, "mapped current receiver supports the exact getter field span");
    reset();
    std::array<unsigned char, H::P::ReceiverBytes> imageObject{};
    const auto                                     table = binding.image + H::P::ObservedVtableRva;
    const std::uint64_t                            rawImage = 0x87654321abcdef01;
    std::memcpy(imageObject.data(), &table, sizeof(table));
    std::memcpy(imageObject.data() + H::P::ValueOffset, &rawImage, sizeof(rawImage));
    fixture.transform.image.Patch(H::Fixture::ImageReceiverRva, imageObject.data(), imageObject.size(), PAGE_READONLY);
    fixture.Slot(17, binding.image + H::Fixture::ImageReceiverRva);
    Check(reader.Read(17, value) == ClockResult::Ok && value == static_cast<std::uint32_t>(rawImage),
          "actual readable image-backed static receiver field is admitted without native allocation rights");
    for (const auto model : {B::ModelCount, std::uint32_t{65535}, std::uint32_t{65536}, std::uint32_t{0xffffffff}})
    {
        reset();
        Check(reader.Read(model, value) == ClockResult::InvalidArgument && value == H::sentinel && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
              "full unsigned bank keys refuse outside native domain before callback or data reads");
    }
    reset();
    fixture.Slot(17, 0);
    Check(reader.Read(17, value) == ClockResult::NotInitialized && value == H::sentinel, "null model is refusal, never manufactured index0 or FFFFFFFF");
    reset();
    fixture.Slot(17, 1);
    Check(reader.Read(17, value) != ClockResult::Ok && value == H::sentinel, "opaque poison bank word cannot grant getter receiver authority");
    for (const auto& pin : H::P::Code)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(reader.Read(17, value) != ClockResult::Ok && value == H::sentinel, "both full getter and exact native EAX consumer profiles remain required");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    std::memcpy(fixture.object, &alternate, sizeof(alternate));
    const auto wrong = binding.image + H::P::GetterRva + 1;
    fixture.transform.image.Patch(H::Fixture::AlternateVtableRva + H::P::GetterSlot, &wrong, sizeof(wrong), PAGE_READONLY);
    Check(reader.Read(17, value) != ClockResult::Ok && value == H::sentinel,
          "an executable current slot target is insufficient when it is not the qualified leaf");
    const auto method = binding.image + H::P::GetterRva;
    fixture.transform.image.Patch(H::Fixture::AlternateVtableRva + H::P::GetterSlot, &method, sizeof(method), PAGE_READONLY);
    for (H::change = 0; H::change != 7; ++H::change)
    {
        reset();
        PoolTestHost::watched = fixture.Address() + H::P::ValueOffset;
        PoolTestHost::after = &H::Mutate;
        Check(reader.Read(17, value) != ClockResult::Ok && value == H::sentinel,
              "even high32-only changes with equalEAX, bank/vtable/slot/code changes or ownerGC loss cannot publish a torn index");
        if (H::change == 3)
            fixture.transform.image.Patch(H::P::ObservedVtableRva + H::P::GetterSlot, &method, sizeof(method), PAGE_READONLY);
        if (H::change == 6)
            fixture.transform.image.Patch(H::P::GetterRva, H::P::Getter.data(), 1, PAGE_EXECUTE_READ);
    }
    for (unsigned failure = 0; failure != 5; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 1:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 2:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 3:
                fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
                break;
            case 4:
                PoolTestHost::failAt = 1;
                break;
        }
        Check(reader.Read(17, value) != ClockResult::Ok && value == H::sentinel, "current frameowner/GT/GC/state/read refusals preserve prior output");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.Read(17, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && value == H::sentinel, "foreign thread cannot borrow the actual native game thread");
    reset();
    Check(reader.Read(17, value) == ClockResult::Ok && value == 7, "no loaded state, type, provider, bundle or target schema exists in this positive fixture");
    std::puts("Production native model animation-file reader fixture completed");
    std::printf("Passed %u native model-animation checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
