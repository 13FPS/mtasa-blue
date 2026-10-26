/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Focused real bank/field observations without render-target authority
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelObjectHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelObjectReader.h"
#include "../../Client/game_de/NativeModelObjectProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeModelObjectReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace ModelObjectFixture
{
    using Reader = mta::game_de::NativeModelObjectReader;
    namespace P = mta::game_de::NativeModelObjectProfile;
    constexpr std::uintptr_t sentinel = 0xfedcba9876543210;
    struct Fixture final : BankFixture
    {
        unsigned char*                  object = nullptr;
        unsigned char*                  mapped = nullptr;
        HANDLE                          mapping = nullptr;
        static constexpr std::uintptr_t ImageReceiverRva = 0x5318000;
        explicit Fixture(const std::filesystem::path& path) : BankFixture(path)
        {
            object = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
            if (mapping)
                mapped = static_cast<unsigned char*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 4096));
            Check(object && mapped, "create only owned private and mapped receiver storage");
            for (const auto& pin : P::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
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
        static void    Field(unsigned char* at, std::uintptr_t value) { std::memcpy(at + P::RenderOffset, &value, sizeof(value)); }
        void           Reset()
        {
            BankFixture::Reset();
            std::memset(object, 0xcc, 128);
            std::memset(mapped, 0xdd, 128);
            Field(object, 0x123456789abcdef0);
            Field(mapped, 0x123456789abcdef0);
            Slot(17, Address());
        }
    };
    Fixture* changing = nullptr;
    unsigned change = 0;
    void     Mutate()
    {
        switch (change)
        {
            case 0:
                Fixture::Field(changing->object, 0x8877665544332211);
                break;
            case 1:
                changing->Slot(17, reinterpret_cast<std::uintptr_t>(changing->mapped));
                break;
            case 2:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 3:
                changing->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
            {
                const auto bad = static_cast<unsigned char>(P::RenderConsumer[0] ^ 1);
                changing->transform.image.Patch(P::RenderConsumerRva, &bad, 1, PAGE_EXECUTE_READ);
                break;
            }
        }
    }
}
int main(int argc, char** argv)
{
    namespace H = ModelObjectFixture;
    using Reader = H::Reader;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "model object evidence uses an explicit workspace-owned directory");
    Reader::Binding refused{123};
    Check(!Reader::Qualify(refused) && refused.image == 123, "shipping model object qualification refuses non-game host without output");
    H::Fixture fixture(argv[1]);
    H::changing = &fixture;
    const Reader::Binding binding{fixture.transform.image.image};
    PoolTestHost::image = binding.image;
    Reader         reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    std::uintptr_t value = H::sentinel;
    const auto     reset = [&]
    {
        fixture.Reset();
        value = H::sentinel;
    };
    Check(Reader::Revalidate(binding) && Reader::RevalidateRender(binding), "existing bank authority and exact127-byte field consumer qualify independently");
    Check(unowned.ReadInterface(17, value) == ClockResult::NotInitialized && value == H::sentinel &&
              unowned.ReadRender(17, value) == ClockResult::NotInitialized && value == H::sentinel,
          "bytes alone cannot manufacture callback ownership");
    for (const auto model : {std::uint32_t{0}, std::uint32_t{17}, B::ModelCount - 1})
        for (const auto tag : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0x1231}, (std::numeric_limits<std::uintptr_t>::max)()})
        {
            reset();
            fixture.Slot(model, tag);
            Check(reader.ReadInterface(model, value) == ClockResult::Ok && value == tag,
                  "interface copies exact current opaque bank bits without querying any target");
        }
    for (const auto model : {B::ModelCount, std::uint32_t{65535}, std::uint32_t{65536}, std::uint32_t{0xffffffff}})
    {
        reset();
        Check(reader.ReadInterface(model, value) == ClockResult::InvalidArgument && value == H::sentinel &&
                  reader.ReadRender(model, value) == ClockResult::InvalidArgument && value == H::sentinel && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
              "full unsigned keys refuse outside native bank without narrowing, callback or data reads");
    }
    reset();
    fixture.Slot(17, 0);
    Check(reader.ReadRender(17, value) == ClockResult::Ok && value == 0, "current absent model shortcircuits render observation to legitimate null");
    for (const auto tag : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0x1231}, (std::numeric_limits<std::uintptr_t>::max)()})
    {
        reset();
        H::Fixture::Field(fixture.object, tag);
        Check(reader.ReadRender(17, value) == ClockResult::Ok && value == tag,
              "raw null/poison render bits survive without target page/schema/type/loaded or bundle58 predicates");
    }
    reset();
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(fixture.mapped));
    Check(reader.ReadRender(17, value) == ClockResult::Ok && value == 0x123456789abcdef0,
          "a current mapped receiver qualifies from actual readable field extent");
    reset();
    std::array<unsigned char, 128> imageObject{};
    imageObject.fill(0xee);
    const std::uintptr_t imageTag = 0x8877665544332211;
    std::memcpy(imageObject.data() + H::P::RenderOffset, &imageTag, sizeof(imageTag));
    fixture.transform.image.Patch(H::Fixture::ImageReceiverRva, imageObject.data(), imageObject.size(), PAGE_READONLY);
    fixture.Slot(17, binding.image + H::Fixture::ImageReceiverRva);
    Check(reader.ReadRender(17, value) == ClockResult::Ok && value == imageTag,
          "current readable image-backed static metadata is valid receiver storage, without allocation adoption");
    for (const auto protect : {DWORD{PAGE_NOACCESS}, DWORD{PAGE_EXECUTE_READ}})
    {
        reset();
        DWORD prior = 0, ignored = 0;
        Check(VirtualProtect(fixture.object, 4096, protect, &prior) != FALSE, "protect only owned receiver page");
        Check(reader.ReadRender(17, value) != ClockResult::Ok && value == H::sentinel,
              "non-readable or executable receiver pages do not grant field authority");
        Check(VirtualProtect(fixture.object, 4096, prior, &ignored) != FALSE, "restore only owned receiver page");
    }
    reset();
    const auto bad = static_cast<unsigned char>(H::P::RenderConsumer[0] ^ 1);
    fixture.transform.image.Patch(H::P::RenderConsumerRva, &bad, 1, PAGE_EXECUTE_READ);
    Check(reader.ReadInterface(17, value) == ClockResult::Ok && value == fixture.Address(),
          "interface bank domain does not acquire render consumer profile dependency");
    value = H::sentinel;
    Check(reader.ReadRender(17, value) != ClockResult::Ok && value == H::sentinel, "render read requires complete captured field consumer");
    fixture.transform.image.Patch(H::P::RenderConsumerRva, H::P::RenderConsumer.data(), 1, PAGE_EXECUTE_READ);
    for (H::change = 0; H::change != 5; ++H::change)
    {
        reset();
        PoolTestHost::watched = fixture.Address() + H::P::RenderOffset;
        PoolTestHost::after = &H::Mutate;
        Check(reader.ReadRender(17, value) != ClockResult::Ok && value == H::sentinel,
              "field/bank/profile mutation or owner/GC loss cannot publish hybrid render state");
        if (H::change == 4)
            fixture.transform.image.Patch(H::P::RenderConsumerRva, H::P::RenderConsumer.data(), 1, PAGE_EXECUTE_READ);
    }
    for (const auto& pin : B::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(reader.ReadInterface(17, value) != ClockResult::Ok && value == H::sentinel && reader.ReadRender(17, value) != ClockResult::Ok &&
                  value == H::sentinel,
              "both operations retain genuine bank profile requirements");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 1:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 2:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 3:
                fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
                break;
            case 4:
                PoolTestHost::failAt = 1;
                break;
            case 5:
                fixture.Slot(17, 1);
                break;
        }
        Check(reader.ReadRender(17, value) != ClockResult::Ok && value == H::sentinel, "owner/GT/GC/state/read/invalid receiver refusals preserve output");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.ReadRender(17, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && value == H::sentinel, "foreign thread cannot adopt actual native game-thread field authority");
    reset();
    Check(reader.ReadRender(17, value) == ClockResult::Ok, "read-only borrowed scope remains usable after unrelated refusals");
    std::puts("Production native model interface and render reader fixture completed");
    std::printf("Passed %u native model-object checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
