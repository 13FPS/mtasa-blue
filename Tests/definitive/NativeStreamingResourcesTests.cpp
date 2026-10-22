/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual resource admission on one owned image and current native owner
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalResourceRequestFixtureMain
#include "NativeStreamingRequestTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingResourceCalls.h"
#include "../../Client/game_de/NativeStreamingResourceProfile.h"
namespace
{
    mta::game_de::ClockResult InvokeResource(mta::game_de::NativeStreamingResourceCalls::NativeFunction, std::uint32_t, std::uint32_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeStreamingResourceContext.cpp"
#define MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE ::InvokeResource
#include "../../Client/game_de/NativeStreamingResourceCalls.cpp"
#undef MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace RP = NativeStreamingResourceProfile;
    namespace RC = NativeStreamingResourceCalls;
    namespace Context = NativeStreamingResourceContext;
    struct ResourceFixture : RequestFixture
    {
        unsigned char *img = nullptr, *fileRows = nullptr, *wideName = nullptr, *buffer = nullptr, *cache = nullptr, *dat = nullptr, *datRows = nullptr;
        explicit ResourceFixture(const std::filesystem::path& directory) : RequestFixture(directory)
        {
            const auto allocate = []() { return static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)); };
            img = allocate();
            fileRows = allocate();
            wideName = allocate();
            buffer = allocate();
            cache = allocate();
            dat = allocate();
            datRows = allocate();
            Check(img && fileRows && wideName && buffer && cache && dat && datRows, "own typed IMGFile,filename/buffer/cache and DAT provider allocations");
            Reset();
        }
        void Links(std::uint32_t model, std::int16_t next, std::int16_t previous)
        {
            Global(LP::ArrayRva + static_cast<std::uintptr_t>(model) * LP::RowStride + 8, std::array<std::int16_t, 2>{next, previous});
        }
        void Name(const char* ascii)
        {
            const auto length = std::strlen(ascii);
            for (std::size_t index = 0; index <= length; ++index)
                Put(wideName, index * 2, static_cast<std::uint16_t>(static_cast<unsigned char>(ascii[index])));
            Put(fileRows, 0, Address(wideName));
            Put(fileRows, 8, static_cast<std::int32_t>(length + 1));
            Put(fileRows, 12, static_cast<std::int32_t>(length + 1));
        }
        void Registry(unsigned char* object, std::int32_t index, std::uintptr_t vtable, std::uintptr_t type)
        {
            Put(object, 0, vtable);
            Put(object, 0xc, index);
            Put(object, 0x10, type);
            Put(object, 0x20, Address(application));
            Put(items, static_cast<std::size_t>(index) * 24, Address(object));
            // The parent fixture resets only its own first three records.
            // New IMG/DAT records must not retain a previous rejection flag.
            Put(items, static_cast<std::size_t>(index) * 24 + NativeStartupProvidersProfile::itemFlagsOffset, std::uint32_t{0});
            Put(items, static_cast<std::size_t>(index) * 24 + 0x10, std::uint32_t{0});
        }
        void Built(const char* name)
        {
            Name(name);
            Put(img, 0x58, Address(cache));
            Put(img, 0x60, std::int32_t{1});
            Put(img, 0x64, std::int32_t{1});
            Put(img, 0x68, std::uint32_t{1});
            Put(img, 0x80, std::int32_t{1});
            Put(img, 0x84, std::int32_t{128});
            Put(img, 0x88, std::int32_t{-1});
            Put(img, 0x90, std::int32_t{0});
            Put(img, 0xa0, std::int32_t{1});
            Put(cache, 0, Context::ResourceContextDetail::Hash(name));
            Put(cache, 8, Address(fileRows));
            Put(cache, 0x10, std::int32_t{-1});
        }
        void Queue(std::uint32_t model)
        {
            const auto head = static_cast<std::int16_t>(RP::ResourceEnd + 2), tail = static_cast<std::int16_t>(RP::ResourceEnd + 3);
            Links(static_cast<std::uint32_t>(head), static_cast<std::int16_t>(model), -1);
            Links(model, tail, head);
            Links(static_cast<std::uint32_t>(tail), -1, static_cast<std::int16_t>(model));
            Global(RP::RequestCountRva, std::int32_t{1});
        }
        void Reset()
        {
            RequestFixture::Reset();
            for (const auto& pin : RP::Pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (auto* memory : {img, fileRows, wideName, buffer, cache, dat, datRows})
                std::memset(memory, 0, 0x1000);
            const auto image = transform.image.image;
            Global(RP::ArrayPointerRva, image + LP::ArrayRva);
            const std::array<std::uintptr_t, 4> globals{RP::LoadedHeadRva, RP::LoadedTailRva, RP::RequestHeadRva, RP::RequestTailRva};
            for (std::size_t index = 0; index < globals.size(); ++index)
                Global(globals[index], image + LP::ArrayRva + (RP::ResourceEnd + index) * LP::RowStride);
            for (auto head : {RP::ResourceEnd, RP::ResourceEnd + 2})
            {
                Links(head, static_cast<std::int16_t>(head + 1), -1);
                Links(head + 1, -1, static_cast<std::int16_t>(head));
            }
            Global(RP::RequestCountRva, std::int32_t{0});
            Global(RP::PriorityCountRva, std::int32_t{0});
            for (auto model : {RP::ResourceFirst, RP::ColFirst - 1, RP::ColFirst, RP::IplFirst - 1, RP::IplFirst, RP::DatFirst - 1, RP::DatFirst,
                               RP::IfpFirst - 1, RP::IfpFirst, RP::RrrFirst - 1, RP::RrrFirst, RP::ScmFirst - 1, RP::ScmFirst, RP::ResourceEnd - 1})
            {
                Stream(model, 0x40, 0);
                Links(model, -1, -1);
            }
            Registry(img, 3, image + RP::ImgVtableRva, Address(metadata) + 0x500);
            Registry(dat, 4, image + std::uintptr_t{0x4301010}, Address(metadata) + 0x600);
            Global(0x5087394, std::int32_t{5});
            Global(RP::ImgClassCacheRva, Address(metadata) + 0x500);
            Put(application, RP::FileArrayOffset, Address(application) + 0x800);
            Put(application, RP::FileCountOffset, std::int32_t{1});
            Put(application, 0x800, Address(img));
            Put(img, 0x30, Address(fileRows));
            Put(img, 0x38, std::int32_t{1});
            Put(img, 0x3c, std::int32_t{1});
            Name("test.col");
            Put(fileRows, 0x10, Address(buffer));
            Put(fileRows, 0x18, std::int32_t{16});
            Put(fileRows, 0x1c, std::int32_t{16});
            Global(RP::FallbackRva, std::uintptr_t{0});
            Global(RP::FallbackRva + 8, std::int32_t{0});
            Put(application, RP::DatProviderOffset, Address(dat));
            Put(dat, 0x58, Address(datRows));
            Put(dat, 0x60, std::int32_t{64});
            Put(dat, 0x64, std::int32_t{64});
            for (std::size_t index = 0; index < 64; ++index)
                Put(datRows, index * 40 + 0x10, std::uint64_t{1});
            // The selected store observes exactly the same native CPool header
            // and allocated generation flag as the actual filename consumer.
            for (unsigned kind = 0; kind < 2; ++kind)
            {
                const auto        first = kind ? RP::IplFirst : RP::ColFirst, end = kind ? RP::DatFirst : RP::IplFirst;
                const std::size_t stride = kind ? 52 : 44;
                auto*             pool = metadata + 0x800 + kind * 0x40;
                auto*             data = rows + kind * 0x10000;
                auto*             flags = rows + 0x20000 + kind * 0x1000;
                std::memset(data, 0, static_cast<std::size_t>(end - first) * stride);
                std::memset(flags, 0, static_cast<std::size_t>(end - first));
                Put(pool, 0, Address(data));
                Put(pool, 8, Address(flags));
                Put(pool, 0x10, static_cast<std::int32_t>(end - first));
                Global(kind ? RP::IplPoolRva : RP::ColPoolRva, Address(pool));
                for (auto model : {first, end - 1})
                    std::memcpy(data + static_cast<std::size_t>(model - first) * stride + 0x10, "test", 5);
            }
            for (auto model : {RP::IfpFirst, RP::RrrFirst - 1})
                transform.image.Patch(RP::IfpRowsRva + static_cast<std::uintptr_t>(model - RP::IfpFirst) * 32, "test", 5, PAGE_READWRITE);
            for (auto model : {RP::RrrFirst, RP::ScmFirst - 1})
                Global(RP::RrrRowsRva + static_cast<std::uintptr_t>(model - RP::RrrFirst) * 24, std::int32_t{7});
            for (auto model : {RP::ScmFirst, RP::ResourceEnd - 1})
            {
                const auto row = RP::ScmRowsRva + static_cast<std::uintptr_t>(model - RP::ScmFirst) * 40;
                transform.image.Patch(row + 0xc, "test", 5, PAGE_READWRITE);
                Global(row + 0x20, std::int32_t{16});
            }
        }
        ~ResourceFixture()
        {
            for (auto* memory : {img, fileRows, wideName, buffer, cache, dat, datRows})
                if (memory)
                    VirtualFree(memory, 0, MEM_RELEASE);
        }
    };
    ResourceFixture*   activeResource = nullptr;
    unsigned           resourceCalls = 0, resourceMode = 0;
    std::uint32_t      passedResource = 0, passedResourceFlags = 0;
    RC::NativeFunction resourceFunction = nullptr;
    ClockResult        InvokeResource(RC::NativeFunction function, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept
    {
        ++resourceCalls;
        resourceFunction = function;
        passedResource = model;
        passedResourceFlags = flags;
        auto& fixture = *activeResource;
        if (resourceMode == 1)
        {
            exception = 0xe0590001;
            return ClockResult::OriginalException;
        }
        auto old = fixture.Stream(model);
        if (!(old[0] & 0x80))
        {
            const auto effective = old[1] && old[1] != 2 ? flags & ~std::uint32_t{0x10} : flags;
            auto       state = old[1];
            if (!state)
                state = model < RP::ColFirst ? 1 : model >= RP::DatFirst && model < RP::IfpFirst ? 3 : resourceMode == 9 ? 2 : 1;
            if (state == 4)
                state = 2;
            fixture.Stream(model, static_cast<unsigned char>(old[0] | effective), state);
            if (resourceMode == 9)
                fixture.Queue(model);
        }
        if (resourceMode == 2)
        {
            exception = 0xe0590002;
            return ClockResult::OriginalException;
        }
        if (resourceMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (resourceMode == 4)
            fixture.Global(RP::RootRva, std::uintptr_t{0});
        if (resourceMode == 5)
            fixture.Put(fixture.items, 3 * 24 + NativeStartupProvidersProfile::itemFlagsOffset, NativeStartupProvidersProfile::itemRejectMask);
        if (resourceMode == 6)
            fixture.Put(fixture.img, 0x38, std::int32_t{2});
        if (resourceMode == 7)
            fixture.Links(RP::ResourceEnd, 0, -1);
        if (resourceMode == 8)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (resourceMode == 10)
            fixture.Built("test.col");
        if (resourceMode == 11)
        {
            fixture.Built("test.col");
            fixture.Put(fixture.cache, 8, fixture.Address(fixture.fileRows) + 1);
        }
        return ClockResult::Ok;
    }
    const char* ResourceName(std::uint32_t model)
    {
        return model < RP::IplFirst   ? "test.col"
               : model < RP::DatFirst ? "test.ipl"
               : model < RP::RrrFirst ? "test.ifp"
               : model < RP::ScmFirst ? "carrec007.rrr"
                                      : "test.scm";
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "resource evidence stays in the explicit owned workspace directory");
    ResourceFixture fixture(argv[1]);
    activeResource = &fixture;
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    const auto reset = [&]()
    {
        fixture.Reset();
        resourceCalls = resourceMode = 0;
        resourceFunction = nullptr;
        passedResource = passedResourceFlags = 0;
    };
    RC::Receipt receipt;
    Check(Context::QualifyProfile(image), "exact request,selector,class,constructor,cache,decoder and readonly format profiles qualify");
    for (auto bad :
         {std::uint32_t{0}, std::uint32_t{19999}, RP::ResourceEnd, RP::ResourceEnd + 1, LP::ModelCount - 1, LP::ModelCount, std::uint32_t{65535}, 0xffffffffu})
    {
        reset();
        Check(RC::Request(image, bad, 0x16, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && resourceCalls == 0 &&
                  PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
              "DFF,sentinel,fullunsigned out-of-file bounds refuse before any native input or owner read");
    }
    for (auto model : {RP::ResourceFirst, RP::ColFirst - 1, RP::ColFirst, RP::IplFirst - 1, RP::IplFirst, RP::DatFirst - 1, RP::DatFirst, RP::IfpFirst - 1,
                       RP::IfpFirst, RP::RrrFirst - 1, RP::RrrFirst, RP::ScmFirst - 1, RP::ScmFirst, RP::ResourceEnd - 1})
        for (auto flags : {std::uint32_t{6}, std::uint32_t{0x16}, std::uint32_t{0x100}, std::uint32_t{0xffff0016}})
            for (unsigned char state : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(2),
                                        static_cast<unsigned char>(3), static_cast<unsigned char>(4)})
            {
                reset();
                fixture.Name(ResourceName(model));
                fixture.Stream(model, 0x40, state);
                Check(RC::Request(image, model, flags, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && resourceCalls == 1 && receipt.entered &&
                          receipt.returned && receipt.root == fixture.Address(fixture.application) && receipt.entryRva == SP::RequestRva &&
                          receipt.modelIndex == model && receipt.flags == flags &&
                          resourceFunction == reinterpret_cast<RC::NativeFunction>(image + SP::RequestRva) && passedResource == model &&
                          passedResourceFlags == flags && receipt.rawFlagsBefore == 0x40 && receipt.rawStateBefore == state &&
                          receipt.rawStateAfter == fixture.Stream(model)[1],
                      "every category's real first/final file IDs and admitted states call the genuine request once with unchanged32bit flags");
                if (!state && model >= RP::DatFirst && model < RP::IfpFirst)
                    Check(receipt.datProvider == fixture.Address(fixture.dat) && receipt.imgFileCount == 0 && receipt.rawStateAfter == 3,
                          "DAT first load uses actual provider soft path and async3 rather than a DFF model or fake loaded result");
                if (!state && model < RP::ColFirst)
                    Check(receipt.rawStateAfter == 1, "native TXD first-load state1 path needs no metadata bank or IMG lookup");
            }
    for (auto mode : {0u, 10u})
    {
        reset();
        resourceMode = mode;
        if (!mode)
            fixture.Built("TEST.COL");
        Check(RC::Request(image, RP::ColFirst, 0x16, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.imgFileCount == 1 && resourceCalls == 1,
              "existing case-insensitive cache and native lazy cache rebuild remain real engine operations under the same typed file owner");
    }
    reset();
    resourceMode = 9;
    fixture.Put(fixture.fileRows, 0x18, std::int32_t{0});
    Check(RC::Request(image, RP::ColFirst, 0x16, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.rawStateAfter == 2 && resourceCalls == 1,
          "missing data may perform a genuine pending-list insertion; the guard requalifies rather than restores native list mutations");
    for (unsigned mode = 1; mode <= 8; ++mode)
    {
        reset();
        resourceMode = mode;
        Check(RC::Request(image, RP::ColFirst, 0x16, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.returned == (mode > 2) &&
                  resourceCalls == 1,
              "entered exception or lost actual file/root/GC/list/owner bookend is reported once for enclosing Stop");
        if (mode >= 2)
            Check(fixture.Stream(RP::ColFirst)[0] == 0x56 && fixture.Stream(RP::ColFirst)[1] == 1,
                  "entered failures never rewind flags,state or replay native decode/retry");
    }
    reset();
    resourceMode = 11;
    Check(RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && receipt.entered && receipt.returned && resourceCalls == 1,
          "post-entry native cache replacement is allowed only while its current typed buffer route remains valid");
    for (unsigned mode = 0; mode < 12; ++mode)
    {
        reset();
        if (mode == 0)
            fixture.Global(RP::ArrayPointerRva, image + LP::ArrayRva + 16);
        if (mode == 1)
            fixture.Links(RP::ResourceEnd, 0, -1);
        if (mode == 2)
            fixture.Global(RP::PriorityCountRva, std::int32_t{-1});
        if (mode == 3)
            fixture.Put(fixture.img, 0, fixture.Address(fixture.img));
        if (mode == 4)
            fixture.Put(fixture.items, 3 * 24 + NativeStartupProvidersProfile::itemFlagsOffset, NativeStartupProvidersProfile::itemRejectMask);
        if (mode == 5)
            fixture.Put(fixture.img, 0x38, std::int32_t{2});
        if (mode == 6)
            fixture.Put(fixture.fileRows, 8, std::int32_t{-1});
        if (mode == 7)
            fixture.Put(fixture.fileRows, 0x18, std::int32_t{17});
        if (mode == 8)
            fixture.Put(fixture.fileRows, 0x10, std::uintptr_t{1});
        if (mode == 9)
        {
            fixture.Built("test.col");
            fixture.Put(fixture.cache, 8, fixture.Address(fixture.fileRows) + 1);
        }
        if (mode == 10)
        {
            fixture.Built("test.col");
            fixture.Put(fixture.cache, 0, std::uint32_t{0});
            fixture.Put(fixture.cache, 0x10, std::int32_t{0});
        }
        if (mode == 11)
        {
            fixture.Built("test.col");
            fixture.Put(fixture.img, 0x68, std::uint32_t{0});
        }
        Check(RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && resourceCalls == 0,
              "malformed native list,IMGFile membership,source/header/buffer,cache pointer/free-slot/cycle refuses before the first native effect");
    }
    reset();
    fixture.Put(fixture.datRows, 0x10, std::uint64_t{0});
    Check(RC::Request(image, RP::DatFirst, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered,
          "unpublished DAT soft asset name cannot be replaced with a metadata model or anonymous object pointer");
    reset();
    fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    Check(RC::Request(image, RP::ResourceFirst, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered,
          "native GC admission precedes even root-free TXD's native write");
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&]() { foreign = RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt); });
    other.join();
    Check(foreign == ClockResult::WrongThread && !receipt.entered && !resourceCalls,
          "a producer thread cannot substitute for actual initialized native GameThread");
    reset();
    Check(RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt) == ClockResult::Ok, "measure real new resource read/owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned part = 0; part < 16; ++part)
    {
        reset();
        PoolTestHost::failAt = 1 + (reads - 1) * part / 15;
        const auto result = RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt);
        Check(result != ClockResult::Ok && resourceCalls <= 1 && receipt.entered == (resourceCalls == 1),
              "distributed real copied-byte faults preserve pre-entry refusal versus non-replayed entered failure");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && resourceCalls <= 1 &&
                  receipt.entered == (resourceCalls == 1),
              "every actual callback-owner bookend can revoke without inventing a fallback or duplicate resource request");
    }
    for (const auto& pin : RP::Pins)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        fixture.transform.image.Patch(pin.rva, &changed, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        Check(RC::Request(image, RP::ColFirst, 6, &PoolTestHost::Owner, &receipt) != ClockResult::Ok && !receipt.entered && !resourceCalls,
              "each exact native class/cache/decoder/format/hash/profile pin is mandatory before entry");
    }
    std::puts("Production native streaming resources primitive fixture completed");
    std::printf("Passed %u native streaming-resources checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
