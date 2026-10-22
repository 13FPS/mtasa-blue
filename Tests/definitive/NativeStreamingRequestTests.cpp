/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real model request guards on owned native-shaped registry/data pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeStreamingRequestTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelBankReader.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include "../../Client/game_de/NativeModelLoadedReader.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include "../../Client/game_de/NativeStreamingRequestCalls.h"
#include "../../Client/game_de/NativeStreamingRequestProfile.h"
#include <thread>

namespace
{
    mta::game_de::ClockResult InvokeRequest(mta::game_de::NativeStreamingRequestCalls::NativeFunction, std::uint32_t, std::uint32_t, std::uint32_t&) noexcept;
}
// Only current module selection, actual copied-byte faults and the final native
// entry are substituted. The production owner/image/registry/data guards run.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativeModelBankReader.cpp"
#include "../../Client/game_de/NativeModelLoadedReader.cpp"
#include "../../Client/game_de/NativeStreamingModelContext.cpp"
#define MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE ::InvokeRequest
#include "../../Client/game_de/NativeStreamingRequestCalls.cpp"
#undef MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    namespace B = NativeModelBankProfile;
    using BankReader = NativeModelBankReader;
    struct BankFixture
    {
        TransformFixture transform;
        explicit BankFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundaries{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t index = 0; index != sections.size(); ++index)
            {
                auto& section = sections[index];
                std::memcpy(section.Name, index == 0 ? ".text" : index == 1 ? ".rdata" : ".data", index == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundaries[index]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundaries[index + 1] - boundaries[index]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + index * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (index == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                           : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (index == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : B::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        void Slot(std::uint32_t index, std::uintptr_t value) { Global(B::BankRva + static_cast<std::uintptr_t>(index) * B::SlotBytes, value); }
        void Reset()
        {
            transform.Reset(false);
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            Slot(0, 0);
            Slot(B::ModelCount - 1, 0);
            Slot(17, 0x1231);
        }
    };
    namespace SR = NativeStreamingRequestCalls;
    namespace SP = NativeStreamingRequestProfile;
    namespace LP = NativeModelLoadedProfile;
    struct RequestFixture : BankFixture
    {
        unsigned char *application = nullptr, *provider = nullptr, *rows = nullptr, *metadata = nullptr, *chunks = nullptr, *items = nullptr;
        explicit RequestFixture(const std::filesystem::path& directory) : BankFixture(directory)
        {
            const auto allocate = [](std::size_t bytes)
            { return static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)); };
            application = allocate(0x1000);
            provider = allocate(0x1000);
            rows = allocate(B::ModelCount * SP::ProviderRowStride);
            metadata = allocate(0x1000);
            chunks = allocate(0x1000);
            items = allocate(65536 * 24);
            Check(application && provider && rows && metadata && chunks && items,
                  "own application,provider,complete modelrows,metadata and genuine registry-shaped allocations");
            for (const auto& pin : LP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : SP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeUObjectMembershipProfile::pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto          type = transform.image.image + std::uintptr_t{0x1100010}, animation = transform.image.image + std::uintptr_t{0x1100020};
            const unsigned char ret = 0xc3;
            transform.image.Patch(0x1100010, &ret, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(0x1100020, &ret, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(0x4301000 + SP::TypeMethodSlot, &type, sizeof(type), PAGE_READONLY);
            transform.image.Patch(0x4301000 + SP::AnimationMethodSlot, &animation, sizeof(animation), PAGE_READONLY);
            Reset();
        }
        template <class T>
        void Put(unsigned char* object, std::size_t at, const T& value)
        {
            std::memcpy(object + at, &value, sizeof(value));
        }
        std::uintptr_t Address(unsigned char* object) const { return reinterpret_cast<std::uintptr_t>(object); }
        void           Stream(std::uint32_t model, unsigned char flags, unsigned char state)
        {
            Global(LP::ArrayRva + static_cast<std::uintptr_t>(model) * LP::RowStride + LP::FlagsOffset, std::array<unsigned char, 2>{flags, state});
        }
        std::array<unsigned char, 2> Stream(std::uint32_t model) const
        {
            std::array<unsigned char, 2> value{};
            std::memcpy(value.data(),
                        reinterpret_cast<void*>(transform.image.image + LP::ArrayRva + static_cast<std::uintptr_t>(model) * LP::RowStride + LP::FlagsOffset),
                        2);
            return value;
        }
        void Reset()
        {
            BankFixture::Reset();
            std::memset(application, 0, 0x1000);
            std::memset(provider, 0, 0x1000);
            std::memset(metadata, 0, 0x1000);
            std::memset(items, 0, 3 * 24);
            Put(application, 0, transform.image.image + SP::RootVtableRva);
            Put(application, 0xc, std::int32_t{1});
            Put(application, 0x10, Address(application) + 0x600);
            Put(application, SP::ProviderOffset, Address(provider));
            Put(provider, 0, transform.image.image + std::uintptr_t{0x4301010});
            Put(provider, 0xc, std::int32_t{2});
            Put(provider, 0x10, Address(provider) + 0x200);
            Put(provider, 0x20, Address(application));
            Put(provider, SP::ProviderRowsOffset, Address(rows));
            Put(provider, SP::ProviderCountOffset, std::int32_t{20000});
            Put(chunks, 0, Address(items));
            Put(items, 24, Address(application));
            Put(items, 24 + 0x10, std::uint32_t{11});
            Put(items, 48, Address(provider));
            Put(items, 48 + 0x10, std::uint32_t{12});
            Global(0x5087380, Address(chunks));
            Global(0x5087394, std::int32_t{3});
            Global(SP::RootRva, Address(application));
            Put(metadata, 0, transform.image.image + std::uintptr_t{0x4301000});
            for (const auto model : {std::uint32_t{0}, std::uint32_t{17}, B::ModelCount - 1})
            {
                Slot(model, Address(metadata));
                Stream(model, 0, 0);
            }
        }
        ~RequestFixture()
        {
            for (auto* object : {application, provider, rows, metadata, chunks, items})
                if (object)
                    VirtualFree(object, 0, MEM_RELEASE);
        }
    };
    RequestFixture*    activeRequest = nullptr;
    unsigned           requestCalls = 0, requestMode = 0;
    std::uint32_t      passedModel = 0, passedFlags = 0;
    SR::NativeFunction passedRequest = nullptr;
    ClockResult        InvokeRequest(SR::NativeFunction function, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept
    {
        ++requestCalls;
        passedModel = model;
        passedFlags = flags;
        passedRequest = function;
        auto& fixture = *activeRequest;
        if (requestMode == 1)
        {
            exception = 0xe0570001;
            return ClockResult::OriginalException;
        }
        const auto old = fixture.Stream(model);
        fixture.Stream(model, static_cast<unsigned char>(old[0] | flags), 2);
        if (requestMode == 2)
        {
            exception = 0xe0570002;
            return ClockResult::OriginalException;
        }
        if (requestMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (requestMode == 4)
            fixture.Global(SP::RootRva, std::uintptr_t{0});
        if (requestMode == 5)
            fixture.Slot(model, fixture.Address(fixture.metadata) + 0x80);
        if (requestMode == 6)
            fixture.Put(fixture.provider, SP::ProviderRowsOffset, fixture.Address(fixture.rows) + SP::ProviderRowStride);
        if (requestMode == 7)
            fixture.Put(fixture.items, 24 + 0x10, std::uint32_t{99});
        if (requestMode == 8)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (requestMode == 9)
        {
            const auto changed = static_cast<unsigned char>(SP::RequestEntry[0] ^ 1);
            fixture.transform.image.Patch(SP::RequestRva, &changed, 1, PAGE_EXECUTE_READ);
        }
        return ClockResult::Ok;
    }
    void ChangeProviderCount()
    {
        activeRequest->Put(activeRequest->provider, SP::ProviderCountOffset, std::int32_t{17});
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "request mutation evidence uses an explicit workspace-owned directory");
    RequestFixture fixture(argv[1]);
    activeRequest = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]()
    {
        fixture.Reset();
        requestCalls = requestMode = 0;
        passedRequest = nullptr;
        passedModel = passedFlags = 0;
    };
    SR::Receipt receipt;
    for (const auto model : {std::uint32_t{0}, std::uint32_t{17}, B::ModelCount - 1})
        for (const auto flags : {std::uint32_t{0}, std::uint32_t{0x06}, std::uint32_t{0x16}, std::uint32_t{0x106}, std::uint32_t{0xffff0016}})
            for (unsigned char state : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(2),
                                        static_cast<unsigned char>(3), static_cast<unsigned char>(4)})
            {
                reset();
                fixture.Stream(model, 0x40, state);
                Check(SR::Request(image, model, flags, &PoolTestHost::Owner, &receipt, &InvokeRequest) == ClockResult::Ok && requestCalls == 1 &&
                          receipt.entered && receipt.returned && receipt.entryRva == SP::RequestRva && receipt.modelIndex == model && receipt.flags == flags &&
                          receipt.rawFlagsBefore == 0x40 && receipt.rawStateBefore == state &&
                          receipt.rawFlagsAfter == static_cast<unsigned char>(0x40 | flags) && receipt.rawStateAfter == 2 &&
                          receipt.root == fixture.Address(fixture.application) && receipt.provider == fixture.Address(fixture.provider) &&
                          receipt.modelInfo == fixture.Address(fixture.metadata) &&
                          passedRequest == reinterpret_cast<SR::NativeFunction>(image + SP::RequestRva) && passedModel == model && passedFlags == flags,
                      "ordinary first/final keys,raw32bit flags and each admitted state enter once without requiring async loaded1");
            }
    for (const auto bad : {B::ModelCount, std::uint32_t{65535}, std::uint32_t{65536}, 0xffffffffu, std::uint32_t{1000}, std::uint32_t{1193}})
    {
        reset();
        const auto expected = bad >= B::ModelCount ? ClockResult::InvalidArgument : ClockResult::UnsupportedBuild;
        Check(SR::Request(image, bad, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) == expected && !receipt.entered && requestCalls == 0 &&
                  PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
              "full-key bounds and actual shared upgrade dispatch refuse before any read or native request");
    }
    reset();
    Check(SR::Request(image, 17, 0x16, nullptr, &receipt, &InvokeRequest) == ClockResult::NotInitialized && !receipt.entered && requestCalls == 0,
          "readable model assets never replace the existing callback owner");
    for (unsigned guard = 0; guard < 16; ++guard)
    {
        reset();
        if (guard == 0)
            fixture.Slot(17, 0);
        if (guard == 1)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (guard == 2)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (guard == 3)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (guard == 4)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (guard == 5)
            fixture.Global(SP::RootRva, std::uintptr_t{0});
        if (guard == 6)
            fixture.Put(fixture.application, 0, image + std::uintptr_t{0x4301000});
        if (guard == 7)
            fixture.Put(fixture.application, SP::ProviderOffset, std::uintptr_t{0});
        if (guard == 8)
            fixture.Put(fixture.provider, SP::ProviderRowsOffset, std::uintptr_t{0});
        if (guard == 9)
            fixture.Put(fixture.provider, SP::ProviderCountOffset, std::int32_t{17});
        if (guard == 10)
            fixture.Put(fixture.items, 24 + 8, std::uint32_t{0x30000000});
        if (guard == 11)
            fixture.Put(fixture.items, 48, fixture.Address(fixture.application));
        if (guard == 12)
            fixture.Put(fixture.metadata, 0, std::uintptr_t{0});
        if (guard == 13)
        {
            const auto wrong = fixture.Address(fixture.application);
            fixture.transform.image.Patch(0x4301000 + SP::TypeMethodSlot, &wrong, sizeof(wrong), PAGE_READONLY);
        }
        if (guard == 14)
            fixture.Stream(17, 0, 255);
        if (guard == 15)
            fixture.Put(fixture.provider, SP::ProviderCountOffset, std::int32_t{-1});
        Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) != ClockResult::Ok && !receipt.entered && requestCalls == 0 &&
                  fixture.Stream(17)[1] != (unsigned char)2,
              "model,owner,GT,GC,state,application,provider,registry,metadata methods and unknown stream state all refuse before mutation");
        if (guard == 13)
        {
            const auto right = image + std::uintptr_t{0x1100010};
            fixture.transform.image.Patch(0x4301000 + SP::TypeMethodSlot, &right, sizeof(right), PAGE_READONLY);
        }
    }
    reset();
    PoolTestHost::watched = fixture.Address(fixture.provider) + SP::ProviderCountOffset;
    PoolTestHost::after = &ChangeProviderCount;
    Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) != ClockResult::Ok && !receipt.entered && requestCalls == 0,
          "provider selection changing during preflight cannot authorize a request through an earlier model-row snapshot");
    for (unsigned mode = 1; mode <= 9; ++mode)
    {
        reset();
        requestMode = mode;
        Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) != ClockResult::Ok && receipt.entered &&
                  receipt.returned == (mode != 1 && mode != 2) && requestCalls == 1,
              "entered exception,owner/root/bank/provider/registry/state/code loss never repeats native request");
        if (mode != 1)
            Check(fixture.Stream(17)[1] == 2 && fixture.Stream(17)[0] == 0x16, "uncertain native request retains its accepted async state and flag effects");
        if (mode == 1 || mode == 2)
            Check(receipt.exceptionCode == 0xe0570000 + mode, "request receipt retains exact native failure identity");
        if (mode == 9)
            fixture.transform.image.Patch(SP::RequestRva, SP::RequestEntry.data(), SP::RequestEntry.size(), PAGE_EXECUTE_READ);
    }
    reset();
    Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) == ClockResult::Ok,
          "measure actual production request copy/owner boundaries once");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= reads; ++fail)
    {
        reset();
        PoolTestHost::failAt = fail;
        Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) != ClockResult::Ok && requestCalls <= 1 &&
                  receipt.entered == (requestCalls != 0),
              "every actual OS-copy failure either refuses cleanly or accounts for one entered native request");
        Check(fixture.Stream(17)[1] == (receipt.entered ? 2 : 0), "preflight faults have no effects and post-call faults never rewind accepted async state");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) != ClockResult::Ok && requestCalls <= 1 &&
                  receipt.entered == (requestCalls != 0),
              "each genuine callback-owner boundary may revoke without native replay");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker(
        [&]()
        {
            SR::Receipt other;
            foreign = SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &other, &InvokeRequest);
        });
    worker.join();
    Check(foreign != ClockResult::Ok && requestCalls == 0, "a real producer thread cannot request assets through borrowed native GameThread authority");
    reset();
    for (const auto& pin : SP::Code)
    {
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(SR::Request(image, 17, 0x16, &PoolTestHost::Owner, &receipt, &InvokeRequest) != ClockResult::Ok && !receipt.entered && requestCalls == 0,
              "changed request implementation or independent ABI caller cannot inherit native call qualification");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    std::puts("Production native streaming request primitive fixture completed");
    std::printf("Passed %u native streaming-request checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
