/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Existing-slot rename admission with owned native effects and real guards
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeSpecialHistoricalRemovalMain
#include "NativeStreamingRemoveTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeStreamingLoadAllGraphFixture.h"
#include "../../Client/game_de/NativeStreamingSpecialModelCalls.h"
#include "../../Client/game_de/NativeStreamingSpecialModelProfile.h"
namespace
{
    ClockResult InvokeSpecial(NativeStreamingSpecialModelCalls::NativeFunction, std::uint32_t, const char*, std::uint32_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeStreamingCompletionContext.cpp"
#define MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE ::InvokeSpecial
#include "../../Client/game_de/NativeStreamingSpecialModelCalls.cpp"
#undef MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace SC = NativeStreamingSpecialModelCalls;
    namespace XP = NativeStreamingSpecialModelProfile;
    struct SpecialFixture : RemoveFixture
    {
        NativeStreamingLoadAllFixture::Graph graph;
        unsigned char *                      nameBlock = nullptr, *shardHashes = nullptr, *cacheRows = nullptr;
        static void                          Patch(void* host, std::uintptr_t rva, const void* bytes, std::size_t count, DWORD protection)
        {
            static_cast<SpecialFixture*>(host)->transform.image.Patch(rva, bytes, count, protection);
        }
        explicit SpecialFixture(const std::filesystem::path& directory)
            : RemoveFixture(directory), graph(transform.image.image, Address(application), &Patch, this)
        {
            nameBlock = static_cast<unsigned char*>(VirtualAlloc(nullptr, XP::NameBlockBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            shardHashes = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            cacheRows = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            Check(nameBlock && shardHashes && cacheRows, "own actual-shaped name pool blocks, sixteen shard hashes and two packed cache allocations");
            for (const auto& pin : XP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            transform.image.Patch(XP::CrcTableRva, XP::CrcTable, sizeof(XP::CrcTable), PAGE_READONLY);
            for (const auto& pin : NativeStreamingLoadAllProfile::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartupOwnerProfile::pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeStartupWorldProfile::pins)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            // Vtable slots must remain image read-only, just like the other
            // native allocator methods already used by the existing fixture.
            const auto allocate = transform.image.image + XP::HeapAllocateRva, proxyAllocate = transform.image.image + XP::ProxyAllocateRva;
            transform.image.Patch(AP::HeapVtableRva + 0x10, &allocate, sizeof(allocate), PAGE_READONLY);
            transform.image.Patch(AP::ProxyVtableRva + 0x10, &proxyAllocate, sizeof(proxyAllocate), PAGE_READONLY);
            const unsigned char ret = 0xc3;
            transform.image.Patch(XP::HeapAllocateRva, &ret, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(XP::ProxyAllocateRva, &ret, 1, PAGE_EXECUTE_READ);
            Reset();
        }
        void Cache(bool occupied)
        {
            std::memset(cacheRows, 0, 0x2000);
            for (std::size_t index = 0; index != 2; ++index)
            {
                std::array<unsigned char, XP::CacheHeaderBytes> header{};
                Put(header.data(), 0x2c, std::int32_t{128});
                Put(header.data(), 0x30, std::int32_t{-1});
                if (occupied)
                {
                    const auto address = Address(cacheRows) + index * 0x1000;
                    Put(header.data(), 0, address);
                    Put(header.data(), 8, std::int32_t{1});
                    Put(header.data(), 0xc, std::int32_t{4});
                    Put(header.data(), 0x10, std::uint32_t{1});
                    Put(header.data(), 0x28, std::int32_t{1});
                    Put(header.data(), 0x38, std::int32_t{0});
                    Put(header.data(), 0x48, std::int32_t{1});
                    auto* row = cacheRows + index * 0x1000;
                    if (!index)
                        Put(row, 0, std::uint32_t{0x12345678});
                    else
                        Put(row, 8, std::uint32_t{0x12345678});
                    Put(row, 0xc, std::int32_t{-1});
                }
                Global(index ? XP::ReverseCacheRva : XP::CacheRva, header);
            }
        }
        void Reset(bool loaded = false)
        {
            RemoveFixture::Reset();
            graph.Reset();
            // Merge real graph and removal/provider registries. The native
            // paths share one registry, not two fixture-specific authorities.
            std::memcpy(items, graph.items.data(), graph.items.size());
            Register(provider, 11, graph.classes[0].data());
            Put(provider, 0x20, Address(application));
            Register(ObjectClass(), 12, ClassClass());
            Register(ClassClass(), 13, ClassClass());
            Register(ActorClass(), 14, ClassClass());
            Register(Asset(), 15, ObjectClass());
            Register(ActorAsset(), 16, ClassClass());
            Put(Bundle(), 0x18, std::int32_t{15});
            Put(Bundle(), 0x1c, std::uint32_t{25});
            Put(Bundle(), 0x40, std::int32_t{16});
            Put(Bundle(), 0x44, std::uint32_t{26});
            Put(chunks, 0, Address(items));
            Global(0x5087380, Address(chunks));
            Global(0x5087394, std::int32_t{17});
            if (!loaded)
            {
                RM::StreamRow row{};
                row[12] = 0x40;
                Global(LP::ArrayRva + std::uintptr_t{17} * 16, row);
            }
            Put(metadata, 0x22, std::int16_t{0});
            std::memset(nameBlock, 0, XP::NameBlockBytes);
            std::memset(shardHashes, 0, 0x4000);
            std::vector<unsigned char> pool(XP::NamePoolBytes);
            Put(pool.data(), 0x10, Address(nameBlock));
            const auto poolAddress = transform.image.image + XP::NamePoolRva;
            for (std::size_t index = 0; index != XP::ShardCount; ++index)
            {
                auto* shard = pool.data() + XP::ShardOffset + index * XP::ShardStride;
                Put(shard, 0xc, std::uint32_t{255});
                Put(shard, 0x10, Address(shardHashes) + index * 0x400);
                Put(shard, 0x18, poolAddress);
            }
            transform.image.Patch(XP::NamePoolRva, pool.data(), pool.size(), PAGE_READWRITE);
            Global(XP::NameInitializedRva, static_cast<unsigned char>(1));
            Cache(false);
        }
        ~SpecialFixture()
        {
            for (auto* pointer : {nameBlock, shardHashes, cacheRows})
                if (pointer)
                    VirtualFree(pointer, 0, MEM_RELEASE);
        }
    };
    SpecialFixture*     activeSpecial = nullptr;
    unsigned            specialCalls = 0, specialMode = 0;
    std::uint32_t       specialModel = 0, specialChannel = 0;
    std::array<char, 9> specialName{};
    SC::NativeFunction  specialFunction = nullptr;
    ClockResult         specialNested = ClockResult::Ok;
    char*               mutateCallerName = nullptr;
    ClockResult         NameMutatingOwner() noexcept
    {
        if (mutateCallerName)
            mutateCallerName[0] = 'X';
        return PoolTestHost::Owner();
    }
    ClockResult InvokeSpecial(SC::NativeFunction function, std::uint32_t model, const char* name, std::uint32_t channel, std::uint32_t& exception) noexcept
    {
        ++specialCalls;
        specialFunction = function;
        specialModel = model;
        specialChannel = channel;
        std::memcpy(specialName.data(), name, std::strlen(name) + 1);
        auto& f = *activeSpecial;
        f.Put(f.metadata, 8, std::uint32_t{0x12345678});
        std::memcpy(f.metadata + 0xc, name, std::strlen(name) + 1);
        f.Stream(model, static_cast<unsigned char>(channel), 2);
        f.Cache(true);
        if (specialMode == 1)
        {
            exception = 0xe0570011;
            return ClockResult::OriginalException;
        }
        if (specialMode == 2)
            f.Global(SP::RootRva, std::uintptr_t{0});
        if (specialMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (specialMode == 4)
            f.metadata[0xc] ^= 1;
        if (specialMode == 5)
        {
            SC::Receipt nested;
            specialNested = SC::Request(f.transform.image.image, model, "other", channel, &PoolTestHost::Owner, &nested);
            if (nested.entered)
                std::abort();
        }
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "special-model fixture evidence is workspace-owned");
    SpecialFixture f(argv[1]);
    activeSpecial = &f;
    const auto image = f.transform.image.image;
    PoolTestHost::image = image;
    SC::Receipt receipt;
    const auto  reset = [&](bool loaded = false)
    {
        f.Reset(loaded);
        specialCalls = 0;
        specialMode = 0;
        specialName.fill(0);
        specialNested = ClockResult::Ok;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::reads = 0;
        PoolTestHost::failAt = 0;
        PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = 0;
    };
    const auto run = [&](const char* name = "special", std::uint32_t channel = 0x80000116u, std::uint32_t model = 17)
    { return SC::Request(image, model, name, channel, &PoolTestHost::Owner, &receipt); };
    reset();
    char original[] = "original";
    mutateCallerName = original;
    Check(SC::Request(image, 17, original, 6, &NameMutatingOwner, &receipt) == ClockResult::Ok && original[0] == 'X' &&
              std::strcmp(specialName.data(), "original") == 0 && std::strcmp(receipt.copiedName.data(), "original") == 0,
          "caller name is copied before arbitrary owner callbacks and only copied intent reaches the native entry");
    mutateCallerName = nullptr;
    for (bool loaded : {false, true})
        for (const auto channel : {0u, 1u, 6u, 0x16u, 0x80000000u, 0xffffffffu})
            for (const char* name : {"", "Mixed_2", "eight888"})
            {
                reset(loaded);
                const auto specialStatus = run(name, channel);
                const bool accepted = specialStatus == ClockResult::Ok && specialCalls == 1 && receipt.entered && receipt.returned &&
                                      specialFunction == reinterpret_cast<SC::NativeFunction>(image + XP::EntryRva) && specialModel == 17 &&
                                      specialChannel == channel && std::strcmp(specialName.data(), name) == 0 && receipt.keyAfter == 0x12345678 &&
                                      receipt.rawStateAfter == 2;
                // Preserve the exact failed boundary when a supported-case
                // fixture is incomplete, without weakening native admission.
                if (!accepted)
                    std::fprintf(stderr,
                                 "Special case loaded=%u channel=%08x name='%s' result=%u calls=%u entered=%u returned=%u before=%u after=%u key=%08x\n",
                                 static_cast<unsigned>(loaded), channel, name, static_cast<unsigned>(specialStatus), specialCalls,
                                 static_cast<unsigned>(receipt.entered), static_cast<unsigned>(receipt.returned), static_cast<unsigned>(receipt.rawStateBefore),
                                 static_cast<unsigned>(receipt.rawStateAfter), receipt.keyAfter);
                Check(accepted,
                      "genuine complete entry accepts existing empty/loaded slots, copies original bounded intent and preserves every channel bit once");
            }
    for (const auto refs : {std::int16_t{1}, std::int16_t{32767}, std::int16_t{-1}, std::int16_t{-32768}})
    {
        reset();
        f.Put(f.metadata, 0x22, refs);
        Check(run() == ClockResult::UnsupportedBuild && !receipt.entered && !specialCalls,
              "unqualified entity-retirement and negative-reference paths never enter");
    }
    for (const auto state : {2, 3, 4, 5, 255})
    {
        reset();
        f.Stream(17, 0x40, static_cast<unsigned char>(state));
        Check(run() != ClockResult::Ok && !receipt.entered && !specialCalls, "pending/unknown removal states refuse before destructive rename");
    }
    for (const auto model : {1000u, 1193u, 20000u, 0xffffu, 0xffffffffu})
    {
        reset();
        Check(run("special", 6, model) != ClockResult::Ok && !specialCalls, "upgrade/resource/special keys cannot inherit existing DFF rename authority");
    }
    const char high[] = {static_cast<char>(0x80), 0};
    for (const char* name : {static_cast<const char*>(nullptr), "123456789", high})
    {
        reset();
        Check(run(name) != ClockResult::Ok && !specialCalls, "unsupported/unreadable name domains refuse without normalization or effects");
    }
    for (unsigned fault = 0; fault != 11; ++fault)
    {
        reset();
        const auto shard = XP::NamePoolRva + XP::ShardOffset;
        if (fault == 0)
            f.Global(XP::NameInitializedRva, static_cast<unsigned char>(0));
        if (fault == 1)
            f.Global(XP::NamePoolRva + 8, std::uint32_t{8192});
        if (fault == 2)
            f.Global(XP::NamePoolRva + 0xc, std::uint32_t{0x20001});
        if (fault == 3)
            f.Global(XP::NamePoolRva + 0x10, std::uintptr_t{0});
        if (fault == 4)
            f.Global(shard + 0x18, std::uintptr_t{0});
        if (fault == 5)
            f.Global(shard + 0x10, std::uintptr_t{0});
        if (fault == 6)
            f.Global(shard + 0xc, std::uint32_t{256});
        if (fault == 7)
            f.Global(shard + 8, std::uint32_t{1});
        if (fault == 8)
            f.Global(XP::CacheRva + 0x30, std::int32_t{0});
        if (fault == 9)
            f.Global(XP::ReverseCacheRva + 0x34, std::int32_t{1});
        if (fault == 10)
            f.Global(AP::HeapRva, std::uintptr_t{0});
        Check(run() != ClockResult::Ok && !receipt.entered && !specialCalls,
              "name constructor guard, pool/shard publication, both caches and current allocator are mandatory");
    }
    reset();
    f.Cache(true);
    Check(run() == ClockResult::Ok && specialCalls == 1, "already-populated genuine packed sparse cache shape is admitted");
    for (unsigned fault = 0; fault != 5; ++fault)
    {
        reset();
        f.Cache(true);
        if (fault == 0)
            f.Put(f.cacheRows, 0xc, std::int32_t{0});
        if (fault == 1)
            f.Put(f.cacheRows, 0x10, std::int32_t{1});
        if (fault == 2)
            f.Global(XP::CacheRva + 0x10, std::uint32_t{0});
        if (fault == 3)
            f.Global(XP::ReverseCacheRva + 0x38, std::int32_t{1});
        if (fault == 4)
            f.Global(XP::ReverseCacheRva + 0xc, std::int32_t{0});
        Check(run() != ClockResult::Ok && !specialCalls,
              "cache cycles, wrong bucket/occupancy, dangling reverse index and capacity refuse before native insertion");
    }
    for (unsigned mode = 1; mode != 6; ++mode)
    {
        reset();
        specialMode = mode;
        const auto result = run();
        if (mode == 5)
            Check(result == ClockResult::Ok && specialNested == ClockResult::Busy, "same shared loading gate refuses recursive special-model reentry");
        else
            Check(result != ClockResult::Ok && receipt.entered && receipt.returned == (mode != 1) && specialCalls == 1 && f.Stream(17)[1] == 2,
                  "entered failure and post-owner/name loss retain native prefix without replay or rollback");
    }
    for (const auto& pin : XP::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        f.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(run() != ClockResult::Ok && !specialCalls, "changed actual operation/cache/FName protocol refuses before effects");
        f.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    Check(run() == ClockResult::Ok, "measure actual read/owner boundaries for copied faults");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (const auto fail : {1u, reads / 2, reads})
    {
        reset();
        PoolTestHost::failAt = fail;
        Check(run() != ClockResult::Ok && specialCalls <= 1, "copied byte failures never create completion or replay accepted prefix");
    }
    for (const auto fail : {1u, owners / 2, owners})
    {
        reset();
        PoolTestHost::revokeAt = fail;
        Check(run() != ClockResult::Ok && specialCalls <= 1, "callback revocation refuses before entry or retains entered effects once");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = run(); });
    worker.join();
    Check(foreign != ClockResult::Ok && !specialCalls, "foreign thread cannot borrow existing-slot rename authority");
    std::puts("Production native streaming special-model primitive fixture completed");
    std::printf("Passed %u native streaming-special-model checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
