/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bookended raw vehicle subtype observation on owned model data
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeVehicleSubtypeHistoricalRequestMain
#include "NativeStreamingRequestTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelClassificationCalls.h"
namespace
{
    unsigned                  subtypeNativeEntries = 0;
    mta::game_de::ClockResult NoSubtypeNativeEntry(mta::game_de::NativeModelClassificationCalls::NativeFunction, std::uint32_t, unsigned char&,
                                                   std::uint32_t&) noexcept
    {
        ++subtypeNativeEntries;
        return mta::game_de::ClockResult::UnsupportedBuild;
    }
}
#define GetModuleHandleW                          PoolTestHost::Module
#define ReadProcessMemory                         PoolTestHost::Read
#define MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE ::NoSubtypeNativeEntry
#include "../../Client/game_de/NativeModelClassificationCalls.cpp"
#undef MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace VehicleSubtypeFixture
{
    namespace MC = mta::game_de::NativeModelClassificationCalls;
    namespace MP = mta::game_de::NativeModelClassificationProfile;
    constexpr std::uint32_t sentinel = 0xaabbccdd;
    constexpr std::size_t   subtypeOffset = 0x84;
    struct Fixture final : RequestFixture
    {
        unsigned char* split = nullptr;
        explicit Fixture(const std::filesystem::path& path) : RequestFixture(path)
        {
            split = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(split != nullptr, "allocate only owned split-prefix model pages");
            Reset();
        }
        ~Fixture()
        {
            if (split)
                VirtualFree(split, 0, MEM_RELEASE);
        }
        void Reset()
        {
            DWORD ignored = 0;
            Check(VirtualProtect(metadata, 4096, PAGE_READWRITE, &ignored) != FALSE, "restore owned receiver page before resetting test data");
            Check(VirtualProtect(split, 8192, PAGE_READWRITE, &ignored) != FALSE, "restore owned split pages before resetting test data");
            RequestFixture::Reset();
            std::memset(split, 0, 8192);
            for (const auto& pin : MP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Leaf(6);
            Put(metadata, subtypeOffset, std::uint32_t{7});
            subtypeNativeEntries = requestCalls = requestMode = 0;
        }
        void Leaf(unsigned char type)
        {
            const std::array<unsigned char, 6> bytes{0xb8, type, 0, 0, 0, 0xc3};
            transform.image.Patch(0x1100010, bytes.data(), bytes.size(), PAGE_EXECUTE_READ);
        }
    };
    Fixture* current = nullptr;
    unsigned mutation = 0;
    void     Mutate()
    {
        auto&      f = *current;
        const auto image = f.transform.image.image;
        switch (mutation)
        {
            case 0:
                f.Put(f.metadata, subtypeOffset, std::uint32_t{0x12340007});
                break;
            case 1:
                f.Slot(17, 0);
                break;
            case 2:
                f.Put(f.metadata, 0, image + std::uintptr_t{0x4301010});
                break;
            case 3:
                f.Leaf(8);
                break;
            case 4:
                f.Global(SP::RootRva, std::uintptr_t{0});
                break;
            case 5:
                f.Put(f.provider, SP::ProviderCountOffset, std::int32_t{17});
                break;
            case 6:
                f.Put(f.items, 24 + 0x10, std::uint32_t{99});
                break;
            case 7:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 8:
                f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 9:
                f.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 10:
            {
                const auto bad = static_cast<unsigned char>(MP::Code[0].bytes[0] ^ 1);
                f.transform.image.Patch(MP::Code[0].rva, &bad, 1, PAGE_EXECUTE_READ);
                break;
            }
            case 11:
                f.Put(f.provider, SP::ProviderRowsOffset, f.Address(f.rows) + SP::ProviderRowStride);
                break;
            default:
                Check(false, "unknown bounded subtype mutation");
        }
    }
}
int main(int argc, char** argv)
{
    namespace H = VehicleSubtypeFixture;
    namespace MC = H::MC;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "raw subtype fixture has explicit workspace-owned evidence directory");
    H::Fixture fixture(argv[1]);
    H::current = &fixture;
    activeRequest = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    std::uint32_t output = H::sentinel;
    const auto    reset = [&]
    {
        fixture.Reset();
        output = H::sentinel;
    };
    const auto read = [&](std::uint32_t model = 17) { return MC::ReadVehicleSubtype(image, model, &PoolTestHost::Owner, output); };
    for (const auto raw : std::array<std::uint32_t, 7>{0, 1, 7, 0xffffffffu, 0x12340007, 0x80000000, 0x87654321})
    {
        reset();
        fixture.Put(fixture.metadata, H::subtypeOffset, raw);
        Check(read() == ClockResult::Ok && output == raw, "raw fullDWORD subtype including unknown/high bits is preserved without enum/sentinel inference");
        Check(!subtypeNativeEntries && !requestCalls, "read-only subtype observation never invokes classifier, virtual getter or streaming request");
    }
    for (const auto key : std::array<std::uint32_t, 4>{B::ModelCount, 65535, 65536, 0xffffffffu})
    {
        reset();
        Check(read(key) == ClockResult::InvalidArgument && output == H::sentinel && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
              "full32 invalid key refuses before owner/data observations without truncation");
    }
    reset();
    const auto streamRow = image + LP::ArrayRva + 17 * LP::RowStride;
    DWORD      streamProtection = 0, streamIgnored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(streamRow), LP::RowStride, PAGE_NOACCESS, &streamProtection) != FALSE,
          "hide only owned unrelated stream row to verify independent raw metadata observation");
    Check(read() == ClockResult::Ok && output == 7, "vehicle subtype observation never reads or requires loaded-state row");
    Check(VirtualProtect(reinterpret_cast<void*>(streamRow), LP::RowStride, streamProtection, &streamIgnored) != FALSE,
          "restore owned stream row before fixture reuse");
    reset();
    Check(MC::ReadVehicleSubtype(image, 17, nullptr, output) == ClockResult::NotInitialized && output == H::sentinel,
          "readable vehicle metadata never replaces callback owner");
    for (const auto word : std::array<std::uintptr_t, 3>{0, 1, (std::numeric_limits<std::uintptr_t>::max)()})
    {
        reset();
        fixture.Slot(17, word);
        Check(read() != ClockResult::Ok && output == H::sentinel, "null/poison bank entries cannot authorize raw subtype or manufacture ff");
    }
    for (unsigned char type : std::array<unsigned char, 4>{0, 5, 8, 255})
    {
        reset();
        fixture.Leaf(type);
        Check(read() != ClockResult::Ok && output == H::sentinel, "native raw reader independently requires a current proved vehicle AL6 receiver");
    }
    reset();
    const std::array<unsigned char, 6> impure{0x8b, 0x41, 0x20, 0xc3, 0xcc, 0xcc};
    fixture.transform.image.Patch(0x1100010, impure.data(), impure.size(), PAGE_EXECUTE_READ);
    Check(read() != ClockResult::Ok && output == H::sentinel, "an object-dependent type getter is refused instead of called or guessed");
    for (const auto& pin : H::MP::Code)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(read() != ClockResult::Ok && output == H::sentinel, "every exact saved classifier code pin remains required to interpret DWORD84");
    }
    for (DWORD access : std::array<DWORD, 3>{PAGE_NOACCESS, PAGE_READWRITE | PAGE_GUARD, PAGE_EXECUTE_READWRITE})
    {
        reset();
        DWORD old = 0;
        Check(VirtualProtect(fixture.metadata, 4096, access, &old) != FALSE, "set only owned receiver page permission");
        Check(read() != ClockResult::Ok && output == H::sentinel, "unreadable/guard/executable receiver cannot grant raw data authority");
    }
    reset();
    auto* atBoundary = fixture.split + 4096 - 0x80;
    std::memcpy(atBoundary, fixture.metadata, 0x88);
    DWORD ignored = 0;
    Check(VirtualProtect(fixture.split + 4096, 4096, PAGE_NOACCESS, &ignored) != FALSE, "guard only owned page following known type context prefix");
    fixture.Slot(17, fixture.Address(atBoundary));
    Check(read() != ClockResult::Ok && output == H::sentinel, "readable old80-byte context does not authorize field84 on a refused next page");
    for (H::mutation = 0; H::mutation != 12; ++H::mutation)
    {
        reset();
        PoolTestHost::watched = fixture.Address(fixture.metadata) + H::subtypeOffset;
        PoolTestHost::after = &H::Mutate;
        Check(read() != ClockResult::Ok && output == H::sentinel && PoolTestHost::after == nullptr,
              "changed fullDWORD/type/model/vtable/root/provider/membership/profile/owner refuses after first actual field sample");
        Check(!subtypeNativeEntries && !requestCalls, "bookend refusal never enters native code or retries a streaming effect");
    }
    for (unsigned state : std::array<unsigned, 2>{6, 7})
    {
        reset();
        fixture.Global(NativeGameStateProfile::stateRva, static_cast<std::uint32_t>(state));
        Check(read() != ClockResult::Ok && output == H::sentinel, "nonplaying state refuses the independent native observer");
    }
    reset();
    fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    Check(read() != ClockResult::Ok && output == H::sentinel, "native GC marker prevents raw subtype publication");
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = read(); });
    other.join();
    Check(foreign != ClockResult::Ok && output == H::sentinel && !subtypeNativeEntries, "foreign thread never borrows native subtype authority");
    reset();
    PoolTestHost::revokeAt = 1;
    Check(read() != ClockResult::Ok && output == H::sentinel, "earliest callback-owner revocation preserves caller output");
    reset();
    Check(read() == ClockResult::Ok && output == 7 && !subtypeNativeEntries && !requestCalls, "final restored fixture remains reusable without native entry");
    std::puts("Production native raw vehicle subtype reader fixture completed");
    std::printf("Passed %u native vehicle-subtype checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
