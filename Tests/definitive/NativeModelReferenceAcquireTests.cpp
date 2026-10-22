/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine acquisition orchestration with owned native-call results
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelAcquireHistoricalReferenceMain
#include "NativeModelReferenceTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelReferenceTransactions.h"
#include "../../Client/game_de/NativeModelReferenceTransactionProfile.h"
namespace
{
    NativeModelReferenceTransactions::ReservedRow* ReserveModel(std::uintptr_t, NativeModelReferenceTransactions::ReservedRow*);
    std::int32_t*                                  PublishModel(std::uintptr_t, std::int32_t*, std::int32_t, std::uintptr_t, std::int32_t);
}
#define GetModuleHandleW                      PoolTestHost::Module
#define ReadProcessMemory                     PoolTestHost::Read
#define MTA_DE_MODEL_REFERENCE_NATIVE_RESERVE ::ReserveModel
#define MTA_DE_MODEL_REFERENCE_NATIVE_PUBLISH ::PublishModel
#include "../../Client/game_de/NativeModelReferenceTransactions.cpp"
#undef MTA_DE_MODEL_REFERENCE_NATIVE_PUBLISH
#undef MTA_DE_MODEL_REFERENCE_NATIVE_RESERVE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace A = NativeModelReferenceTransactions;
    namespace AP = NativeModelReferenceTransactionProfile;
    struct AcquireFixture : ReferenceFixture
    {
        unsigned char *heap = nullptr, *proxy = nullptr;
        explicit AcquireFixture(const std::filesystem::path& path) : ReferenceFixture(path)
        {
            heap = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            proxy = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(heap && proxy, "own the constructor-shaped native heap and optional proxy objects");
            for (const auto& pin : AP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            transform.image.Patch(AP::ClassSizesRva, AP::ClassSizes.data(), sizeof(AP::ClassSizes), PAGE_READONLY);
            const auto reallocate = transform.image.image + AP::HeapReallocateRva, quantize = transform.image.image + AP::HeapQuantizeRva;
            const auto proxyReallocate = transform.image.image + AP::ProxyReallocateRva, proxyQuantize = transform.image.image + AP::ProxyQuantizeRva;
            transform.image.Patch(AP::HeapVtableRva + 0x20, &reallocate, sizeof(reallocate), PAGE_READONLY);
            transform.image.Patch(AP::HeapVtableRva + 0x38, &quantize, sizeof(quantize), PAGE_READONLY);
            transform.image.Patch(AP::ProxyVtableRva + 0x20, &proxyReallocate, sizeof(proxyReallocate), PAGE_READONLY);
            transform.image.Patch(AP::ProxyVtableRva + 0x38, &proxyQuantize, sizeof(proxyQuantize), PAGE_READONLY);
            Reset();
        }
        void SeedHeap(bool useProxy = false)
        {
            std::memset(heap, 0, 0x1000);
            std::memset(proxy, 0, 0x1000);
            Put(heap, 0, transform.image.image + AP::HeapVtableRva);
            Put(proxy, 0, transform.image.image + AP::ProxyVtableRva);
            Put(proxy, 8, Address(heap));
            Global(AP::HeapAuthorityRva, Address(heap));
            Global(AP::HeapRva, Address(useProxy ? proxy : heap));
            std::array<unsigned char, AP::LookupBytes> lookup{};
            std::array<std::uint16_t, AP::ClassCount>  reverse{};
            std::size_t                                category = 0;
            for (std::size_t i = 0; i != lookup.size(); ++i)
            {
                while (category < AP::ClassSizes.size() && AP::ClassSizes[category] < i * 16)
                    ++category;
                if (category == AP::ClassSizes.size())
                    Check(false, "native constructor source covers every small allocation class");
                lookup[i] = static_cast<unsigned char>(category);
            }
            for (std::size_t i = 0; i != reverse.size(); ++i)
                reverse[i] = AP::ClassSizes[reverse.size() - i - 1];
            transform.image.Patch(AP::LookupRva, lookup.data(), lookup.size(), PAGE_READWRITE);
            transform.image.Patch(AP::ReverseSizesRva, reverse.data(), sizeof(reverse), PAGE_READWRITE);
        }
        void Missing(bool empty = false, bool grow = false)
        {
            std::memset(Map(), 0, RP::HeaderBytes);
            std::memset(references, 0, 4 * RP::RowBytes);
            std::memset(relocated, 0, 0x1000);
            Header(0, empty ? std::uintptr_t{0} : Address(references));
            Header(8, empty ? std::int32_t{0} : std::int32_t{1});
            Header(0xc, empty ? std::int32_t{0} : grow ? std::int32_t{1} : std::int32_t{4});
            Header(0x10, empty ? std::uint32_t{0} : std::uint32_t{1});
            Header(0x28, empty ? std::int32_t{0} : std::int32_t{1});
            Header(0x2c, empty ? std::int32_t{0} : std::int32_t{128});
            Header(0x30, std::int32_t{-1});
            Header(0x38, empty ? std::int32_t{-1} : std::int32_t{0});
            Header(0x48, empty ? std::int32_t{0} : std::int32_t{1});
            if (!empty)
            {
                Row(0, 0, std::uint32_t{0});
                Row(0, 4, std::uint32_t{3});
                Row(0, 8, std::int32_t{-1});
                Row(0, 12, std::int32_t{0});
            }
        }
        std::uintptr_t RowsAddress() const
        {
            std::uintptr_t value = 0;
            std::memcpy(&value, Map(), sizeof(value));
            return value;
        }
        std::int32_t Integer(std::size_t at) const
        {
            std::int32_t value = 0;
            std::memcpy(&value, Map() + at, sizeof(value));
            return value;
        }
        std::uint32_t NativeCount(std::int32_t index) const
        {
            std::uint32_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(RowsAddress() + static_cast<std::uintptr_t>(index) * 16 + 4), 4);
            return value;
        }
        void Reset()
        {
            ReferenceFixture::Reset();
            SeedHeap();
            Missing();
        }
        ~AcquireFixture()
        {
            if (heap)
                VirtualFree(heap, 0, MEM_RELEASE);
            if (proxy)
                VirtualFree(proxy, 0, MEM_RELEASE);
        }
    };
    AcquireFixture* activeAcquire = nullptr;
    unsigned        reserveCalls = 0, publishCalls = 0, acquireMode = 0;
    std::uint32_t   acquireKey = 17;
    bool            publishAbiValid = false;
    A::ReservedRow* ReserveModel(std::uintptr_t map, A::ReservedRow* output)
    {
        ++reserveCalls;
        auto& f = *activeAcquire;
        Check(map == f.Address(f.Map()), "native reserve receives the current embedded model map, never a fabricated map owner");
        if (acquireMode == 1)
            RaiseException(0xe0580101, 0, 0, nullptr);
        auto       rows = f.RowsAddress();
        const auto free = f.Integer(0x34), num = f.Integer(8);
        if (free)
        {
            output->index = f.Integer(0x30);
            std::int32_t next = -1;
            std::memcpy(&next, reinterpret_cast<void*>(rows + static_cast<std::uintptr_t>(output->index) * 16 + 4), 4);
            f.Header(0x30, next);
            f.Header(0x34, free - 1);
            if (free > 1)
            {
                const std::int32_t previous = -1;
                std::memcpy(reinterpret_cast<void*>(rows + static_cast<std::uintptr_t>(next) * 16), &previous, 4);
            }
        }
        else
        {
            output->index = num;
            if (!rows || num + 1 > f.Integer(0xc))
            {
                if (rows)
                    std::memcpy(f.relocated, reinterpret_cast<void*>(rows), static_cast<std::size_t>(num) * 16);
                rows = f.Address(f.relocated);
                f.Header(0, rows);
                f.Header(0xc, std::int32_t{4});
            }
            f.Header(8, num + 1);
            f.Header(0x28, num + 1);
            if (!f.Integer(0x2c))
                f.Header(0x2c, std::int32_t{128});
        }
        std::uintptr_t external = 0;
        std::memcpy(&external, f.Map() + 0x20, 8);
        auto*         bits = external ? reinterpret_cast<unsigned char*>(external) : f.Map() + 0x10;
        std::uint32_t word = 0;
        std::memcpy(&word, bits + static_cast<std::size_t>(output->index / 32) * 4, 4);
        word |= std::uint32_t{1} << (output->index % 32);
        f.Put(bits, static_cast<std::size_t>(output->index / 32) * 4, word);
        output->row = rows + static_cast<std::uintptr_t>(output->index) * 16;
        if (acquireMode == 2)
            RaiseException(0xe0580102, 0, 0, nullptr);
        if (acquireMode == 3)
            f.Global(AP::HeapRva, std::uintptr_t{0});
        return output;
    }
    std::int32_t* PublishModel(std::uintptr_t map, std::int32_t* output, std::int32_t hash, std::uintptr_t row, std::int32_t reserved)
    {
        ++publishCalls;
        auto&         f = *activeAcquire;
        std::uint32_t key = 0, count = 99;
        std::int32_t  next = 0;
        std::memcpy(&key, reinterpret_cast<void*>(row), 4);
        std::memcpy(&count, reinterpret_cast<void*>(row + 4), 4);
        std::memcpy(&next, reinterpret_cast<void*>(row + 8), 4);
        publishAbiValid = map == f.Address(f.Map()) && hash == static_cast<std::int32_t>(acquireKey) && key == acquireKey && count == 0 && next == -1 &&
                          row == f.RowsAddress() + static_cast<std::uintptr_t>(reserved) * 16;
        Check(publishAbiValid, "production performs exact row initialization and supplies all five genuine native publish arguments");
        if (acquireMode == 5)
            RaiseException(0xe0580105, 0, 0, nullptr);
        const auto oldCount = f.Integer(0x48), buckets = oldCount ? oldCount : 1;
        f.Header(0x48, buckets);
        std::uintptr_t externalHash = 0;
        std::memcpy(&externalHash, f.Map() + 0x40, 8);
        auto* heads = externalHash ? reinterpret_cast<unsigned char*>(externalHash) : f.Map() + 0x38;
        std::memset(heads, 0xff, static_cast<std::size_t>(buckets) * 4);
        std::uintptr_t externalBits = 0;
        std::memcpy(&externalBits, f.Map() + 0x20, 8);
        auto* bits = externalBits ? reinterpret_cast<unsigned char*>(externalBits) : f.Map() + 0x10;
        for (std::int32_t index = 0; index != f.Integer(8); ++index)
        {
            std::uint32_t word = 0;
            std::memcpy(&word, bits + static_cast<std::size_t>(index / 32) * 4, 4);
            if (!(word & (std::uint32_t{1} << (index % 32))))
                continue;
            auto*         entry = reinterpret_cast<unsigned char*>(f.RowsAddress() + static_cast<std::uintptr_t>(index) * 16);
            std::uint32_t entryKey = 0;
            std::memcpy(&entryKey, entry, 4);
            const auto   bucket = static_cast<std::int32_t>(entryKey & static_cast<std::uint32_t>(buckets - 1));
            std::int32_t head = -1;
            std::memcpy(&head, heads + static_cast<std::size_t>(bucket) * 4, 4);
            f.Put(entry, 8, head);
            f.Put(entry, 12, bucket);
            f.Put(heads, static_cast<std::size_t>(bucket) * 4, index);
        }
        *output = acquireMode == 4 ? reserved + 1 : reserved;
        if (acquireMode == 6)
            f.Put(reinterpret_cast<unsigned char*>(f.RowsAddress()), 4, std::uint32_t{55});
        if (acquireMode == 7)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (acquireMode == 8)
            f.Slot(17, f.Address(f.metadata) + 0x80);
        if (acquireMode == 9)
        {
            const auto changed = static_cast<unsigned char>(AP::NativePublish[0] ^ 1);
            f.transform.image.Patch(AP::PublishRva, &changed, 1, PAGE_EXECUTE_READ);
        }
        if (acquireMode == 10)
            f.Global(AP::HeapAuthorityRva, std::uintptr_t{0});
        if (acquireMode == 11)
            f.Put(reinterpret_cast<unsigned char*>(row), 4, std::uint32_t{9});
        return output;
    }
    void ChangePublishedCount()
    {
        activeAcquire->Put(reinterpret_cast<unsigned char*>(activeAcquire->RowsAddress() + 16), 4, std::uint32_t{9});
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "acquisition evidence uses an explicit workspace-owned directory");
    AcquireFixture fixture(argv[1]);
    activeAcquire = &fixture;
    activeReference = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]()
    {
        fixture.Reset();
        reserveCalls = publishCalls = acquireMode = referenceCalls = referenceMode = 0;
        acquireKey = 17;
        publishAbiValid = false;
    };
    A::Receipt receipt;
    const auto acquire = [&]() { return A::Acquire(image, acquireKey, &PoolTestHost::Owner, &receipt); };
    for (const bool proxy : {false, true})
        for (const bool empty : {false, true})
            for (const bool growth : {false, true})
            {
                reset();
                fixture.SeedHeap(proxy);
                fixture.Missing(empty, growth);
                Check(acquire() == ClockResult::Ok && reserveCalls == 1 && publishCalls == 1 && publishAbiValid && receipt.entered && receipt.reserved &&
                          receipt.published && receipt.accepted && receipt.before == 0 && receipt.target == 1 &&
                          receipt.heap == fixture.Address(proxy ? fixture.proxy : fixture.heap) && fixture.NativeCount(receipt.publishedIndex) == 1,
                      "real production acquisition completes a new key through direct/proxy heap, empty maps and native row relocation");
            }
    reset();
    fixture.Shape(4, 1, false, 4);
    fixture.Row(1, 0, std::uint32_t{33});
    Check(acquire() == ClockResult::Ok && receipt.reservedIndex == 3 && fixture.NativeCount(3) == 1,
          "native reserve can recycle a coherent free row without treating allocator links as model counts");
    for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{0x7fffffff}, std::uint32_t{0xffffffff}})
    {
        reset();
        fixture.ReferenceFixture::Reset();
        fixture.Row(1, 4, count);
        fixture.Global(AP::HeapRva, std::uintptr_t{0});
        fixture.Global(AP::HeapAuthorityRva, std::uintptr_t{0});
        const auto expected = count == 0xffffffffu ? ClockResult::InvalidArgument : ClockResult::Ok;
        Check(acquire() == expected && reserveCalls == 0 && publishCalls == 0 && receipt.heap == 0 &&
                  referenceCalls == (expected == ClockResult::Ok ? 1u : 0u) && fixture.Count(1) == (expected == ClockResult::Ok ? count + 1 : count),
              "existing-key increment keeps its real current counter rules and gains no allocation or heap initialization prerequisite");
    }
    for (const auto key : {std::uint32_t{1000}, std::uint32_t{1099}, std::uint32_t{1193}})
    {
        reset();
        acquireKey = key;
        fixture.Row(0, 0, B::ModelCount - 1);
        fixture.Slot(key, fixture.Address(fixture.metadata));
        fixture.Stream(key, 0x40, 1);
        fixture.Global(NativeStreamingUpgradeProfile::LinkCountRva, std::int32_t{1});
        fixture.Global(NativeStreamingUpgradeProfile::FirstLinksRva, static_cast<std::uint16_t>(key));
        fixture.Global(NativeStreamingUpgradeProfile::SecondLinksRva, static_cast<std::uint16_t>(B::ModelCount - 1));
        Check(acquire() == ClockResult::Ok && receipt.reserved && receipt.published && receipt.accepted && fixture.NativeCount(0) == 3 &&
                  fixture.NativeCount(receipt.publishedIndex) == 1 && reserveCalls == 1 && publishCalls == 1,
              "upgrade acquisition creates only the selected keyed reference and preserves its companion's existing count");
        Check(acquire() == ClockResult::Ok && !receipt.reserved && !receipt.published && receipt.heap == 0 && fixture.NativeCount(0) == 3 &&
                  fixture.NativeCount(1) == 2 && reserveCalls == 1 && publishCalls == 1 && referenceCalls == 1,
              "repeated upgrade acquisition reuses the selected native counter without paired allocation or increment");
    }
    for (unsigned guard = 0; guard != 12; ++guard)
    {
        reset();
        if (guard == 0)
            fixture.Global(AP::HeapRva, std::uintptr_t{0});
        if (guard == 1)
            fixture.Global(AP::HeapAuthorityRva, std::uintptr_t{0});
        if (guard == 2)
            fixture.Put(fixture.heap, 0, image + AP::ProxyVtableRva);
        if (guard == 3)
        {
            fixture.SeedHeap(true);
            fixture.Put(fixture.proxy, 8, fixture.Address(fixture.proxy));
        }
        if (guard == 4)
        {
            fixture.SeedHeap(true);
            fixture.Put(fixture.proxy, 0, image + AP::HeapVtableRva);
        }
        if (guard == 5)
            fixture.Global(AP::LookupRva + 37, static_cast<unsigned char>(44));
        if (guard == 6)
            fixture.Global(AP::ReverseSizesRva + 14, std::uint16_t{0});
        if (guard == 7)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (guard == 8)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (guard == 9)
            fixture.Stream(17, 0x40, 0);
        if (guard == 10)
            fixture.Header(0x10, std::uint32_t{0});
        if (guard == 11)
            PoolTestHost::ownerResult = ClockResult::Expired;
        Check(acquire() != ClockResult::Ok && !receipt.entered && !reserveCalls && !publishCalls,
              "heap authority, completed native size tables, proxy owner, sparse occupancy and current native caller prerequisites refuse before reservation");
    }
    for (unsigned mode = 1; mode <= 11; ++mode)
    {
        reset();
        acquireMode = mode;
        Check(acquire() != ClockResult::Ok && receipt.entered && reserveCalls == 1 && publishCalls == (mode == 1 || mode == 2 ? 0u : 1u),
              "native exceptions and post-entry owner/data loss never replay reservation or publication");
        if (mode == 3 || (mode >= 6 && mode <= 10))
            Check(receipt.accepted && fixture.NativeCount(receipt.publishedIndex) == 1,
                  "accepted native reference survives post-entry failure without rollback");
        if (mode == 4 || mode == 11)
            Check(!receipt.accepted, "unexpected publication outputs and outside count intent cannot acknowledge an acquired reference");
        if (mode == 9)
            fixture.transform.image.Patch(AP::PublishRva, AP::NativePublish.data(), 1, PAGE_EXECUTE_READ);
    }
    reset();
    PoolTestHost::watched = fixture.Address(fixture.references) + 16;
    PoolTestHost::after = &ChangePublishedCount;
    Check(acquire() == ClockResult::Busy && receipt.entered && receipt.published && !receipt.accepted && receipt.observed == 9 && fixture.NativeCount(1) == 9 &&
              reserveCalls == 1 && publishCalls == 1,
          "one new-count compare/exchange preserves conflicting outside intent without replay");
    reset();
    Check(acquire() == ClockResult::Ok, "measure all actual acquisition copied-byte and owner boundaries once");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= reads; ++fail)
    {
        reset();
        PoolTestHost::failAt = fail;
        Check(acquire() != ClockResult::Ok && reserveCalls <= 1 && publishCalls <= 1 && receipt.entered == (reserveCalls != 0),
              "each actual copy refusal precedes native intent or accounts for the one entered transaction");
        if (receipt.published)
            Check(fixture.NativeCount(receipt.publishedIndex) == (receipt.accepted ? 1u : 0u),
                  "published/accepted native effects remain exact after a copied-byte failure");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(acquire() != ClockResult::Ok && reserveCalls <= 1 && publishCalls <= 1,
              "each retained owner boundary can revoke without replaying native acquisition");
    }
    for (const auto& pin : AP::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(acquire() != ClockResult::Ok && !receipt.entered && !reserveCalls,
              "changed publisher, allocator or heap constructor protocol refuses before reservation");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread foreignThread(
        [&]()
        {
            A::Receipt other;
            foreign = A::Acquire(image, 17, &PoolTestHost::Owner, &other);
        });
    foreignThread.join();
    Check(foreign != ClockResult::Ok && !reserveCalls && !publishCalls,
          "an actual producer thread cannot borrow native GameThread authority for model allocation");
    reset();
    const A::Calls missingCalls{};
    Check(A::Acquire(image, 17, &PoolTestHost::Owner, &receipt, &missingCalls) == ClockResult::InvalidArgument && !receipt.entered && !PoolTestHost::reads,
          "incomplete final native-call seams refuse before reading allocation or callback state");
    reset();
    Check(A::Acquire(image, B::ModelCount, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !PoolTestHost::reads,
          "full unsigned key bounds refuse before any native owner or allocation read");
    Check(A::Acquire(image, 17, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered, "a readable heap cannot supply callback ownership");
    std::puts("Production native model-reference acquisition transaction fixture completed");
    std::printf("Passed %u native model-acquisition checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
