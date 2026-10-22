/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real keyed native-map admission around one owned-memory exchange
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelReferenceHistoricalRequestMain
#include "NativeStreamingRequestTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelReferenceMemory.h"
#include "../../Client/game_de/NativeModelReferenceProfile.h"
#include <utility>
namespace
{
    ClockResult ExchangeReference(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                PoolTestHost::Module
#define ReadProcessMemory               PoolTestHost::Read
#define MTA_DE_MODEL_REFERENCE_EXCHANGE ::ExchangeReference
#include "../../Client/game_de/NativeModelReferenceMemory.cpp"
#undef MTA_DE_MODEL_REFERENCE_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace R = NativeModelReferenceMemory;
    namespace RP = NativeModelReferenceProfile;
    struct ReferenceFixture : RequestFixture
    {
        unsigned char *references = nullptr, *relocated = nullptr, *allocationBits = nullptr, *buckets = nullptr;
        std::int32_t   used = 0;
        explicit ReferenceFixture(const std::filesystem::path& path) : RequestFixture(path)
        {
            references = static_cast<unsigned char*>(VirtualAlloc(nullptr, RP::MaximumRows * RP::RowBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            relocated = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            allocationBits = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            buckets = static_cast<unsigned char*>(VirtualAlloc(nullptr, RP::MaximumBuckets * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(references && relocated && allocationBits && buckets,
                  "own the native-shaped sparse rows, relocation buffer, allocation bits and hash buckets");
            for (const auto& pin : RP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        unsigned char* Map() const { return application + RP::MapOffset; }
        template <class T>
        void Header(std::size_t offset, const T& value)
        {
            Put(Map(), offset, value);
        }
        template <class T>
        void Row(std::int32_t index, std::size_t offset, const T& value)
        {
            Put(references, static_cast<std::size_t>(index) * 16 + offset, value);
        }
        std::uint32_t Count(std::int32_t index) const
        {
            std::uint32_t result = 0;
            std::memcpy(&result, references + static_cast<std::size_t>(index) * 16 + 4, 4);
            return result;
        }
        void Shape(std::int32_t count, std::int32_t selected, bool externalBits, std::int32_t hashCount)
        {
            used = count;
            std::memset(Map(), 0, RP::HeaderBytes);
            std::memset(references, 0, static_cast<std::size_t>(count) * 16);
            std::memset(allocationBits, 0, 0x1000);
            std::memset(buckets, 0xff, static_cast<std::size_t>(hashCount) * 4);
            Header(0, Address(references));
            Header(8, count);
            Header(0xc, count + 4);
            Header(0x20, externalBits ? Address(allocationBits) : std::uintptr_t{0});
            Header(0x28, count);
            Header(0x2c, externalBits ? (count + 31) / 32 * 32 : std::int32_t{128});
            Header(0x34, count - 3);
            Header(0x38, std::int32_t{-1});
            Header(0x40, hashCount == 1 ? std::uintptr_t{0} : Address(buckets));
            Header(0x48, hashCount);
            auto*                                                       bits = externalBits ? allocationBits : Map() + 0x10;
            auto*                                                       hash = hashCount == 1 ? Map() + 0x38 : buckets;
            const std::array<std::pair<std::int32_t, std::uint32_t>, 3> live{{{0, 0}, {selected, 17}, {2, B::ModelCount - 1}}};
            for (const auto& entry : live)
            {
                const auto    index = entry.first;
                const auto    key = entry.second;
                const auto    bucket = static_cast<std::int32_t>(key & static_cast<std::uint32_t>(hashCount - 1));
                std::uint32_t word = 0;
                std::memcpy(&word, bits + static_cast<std::size_t>(index / 32) * 4, 4);
                word |= std::uint32_t{1} << (index % 32);
                Put(bits, static_cast<std::size_t>(index / 32) * 4, word);
                std::int32_t head = -1;
                std::memcpy(&head, hash + static_cast<std::size_t>(bucket) * 4, 4);
                Row(index, 0, key);
                Row(index, 4, std::uint32_t{7});
                Row(index, 8, head);
                Row(index, 12, bucket);
                Put(hash, static_cast<std::size_t>(bucket) * 4, index);
            }
            std::int32_t previous = -1, head = -1;
            for (std::int32_t index = 0; index != count; ++index)
            {
                if (index == 0 || index == selected || index == 2)
                    continue;
                if (head == -1)
                    head = index;
                Row(index, 0, previous);
                Row(index, 4, std::int32_t{-1});
                Row(index, 8, std::int32_t{123});  // Stale hash metadata on free rows is legitimate.
                Row(index, 12, std::int32_t{456});
                if (previous != -1)
                    Row(previous, 4, index);
                previous = index;
            }
            Header(0x30, head);
        }
        void Reset()
        {
            RequestFixture::Reset();
            Shape(4, 1, false, 4);
            for (const auto model : {std::uint32_t{0}, std::uint32_t{17}, B::ModelCount - 1})
                Stream(model, 0x40, 1);
        }
        ~ReferenceFixture()
        {
            for (auto* pointer : {references, relocated, allocationBits, buckets})
                if (pointer)
                    VirtualFree(pointer, 0, MEM_RELEASE);
        }
    };
    ReferenceFixture* activeReference = nullptr;
    unsigned          referenceCalls = 0, referenceMode = 0;
    ClockResult       ExchangeReference(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed,
                                        std::uint32_t& exception) noexcept
    {
        ++referenceCalls;
        auto& f = *activeReference;
        if (referenceMode == 1)
        {
            exception = 0xe0580001;
            return ClockResult::OriginalException;
        }
        if (referenceMode == 2)
        {
            observed = expected + 2;
            std::memcpy(reinterpret_cast<void*>(address), &observed, 4);
            return ClockResult::Ok;
        }
        LONG e = 0, t = 0;
        std::memcpy(&e, &expected, 4);
        std::memcpy(&t, &target, 4);
        const auto result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), t, e);
        std::memcpy(&observed, &result, 4);
        if (referenceMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (referenceMode == 4)
            f.Global(SP::RootRva, std::uintptr_t{0});
        if (referenceMode == 5)
            f.Row(0, 4, std::uint32_t{55});
        if (referenceMode == 6)
            f.Header(0xc, f.used + 9);
        if (referenceMode == 7)
            f.Put(f.items, 24 + 0x10, std::uint32_t{777});
        if (referenceMode == 8)
            f.Stream(17, 0x40, 2);
        if (referenceMode == 9)
        {
            const auto changed = static_cast<unsigned char>(RP::NativeAssignment[0] ^ 1);
            f.transform.image.Patch(RP::Code[0].rva, &changed, 1, PAGE_EXECUTE_READ);
        }
        return ClockResult::Ok;
    }
    void FreeSelectedWhileReading()
    {
        activeReference->Put(activeReference->Map(), 0x10, std::uint32_t{5});
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "reference fixture evidence uses a workspace-owned directory");
    ReferenceFixture fixture(argv[1]);
    activeReference = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]()
    {
        fixture.Reset();
        referenceCalls = referenceMode = 0;
    };
    const auto adjust = [&](std::uint32_t key, int delta, R::Receipt& receipt) { return R::Adjust(image, key, delta, &PoolTestHost::Owner, &receipt); };
    R::Receipt receipt;
    for (const auto key : {std::uint32_t{0}, std::uint32_t{17}, B::ModelCount - 1})
        for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{2}, std::uint32_t{0x7fffffff}, std::uint32_t{0x80000000},
                                 std::uint32_t{0xfffffffe}, std::uint32_t{0xffffffff}})
            for (const int delta : {-1, 1})
            {
                reset();
                const std::int32_t index = key == 0 ? 0 : key == 17 ? 1 : 2;
                fixture.Row(index, 4, count);
                const auto expected = (delta < 0 && count == 0) || (delta > 0 && count == 0xffffffffu) ? ClockResult::InvalidArgument
                                      : delta < 0 && count == 1                                        ? ClockResult::UnsupportedBuild
                                                                                                       : ClockResult::Ok;
                const auto target = expected == ClockResult::Ok ? delta > 0 ? count + 1 : count - 1 : count;
                Check(adjust(key, delta, receipt) == expected && fixture.Count(index) == target && referenceCalls == (expected == ClockResult::Ok ? 1u : 0u) &&
                          receipt.entered == (expected == ClockResult::Ok) && receipt.accepted == (expected == ClockResult::Ok),
                      "full unsigned first/final model keys and DWORD arithmetic preserve sign-bit transitions while refusing zero continuations and wrap");
            }
    for (const auto external : {false, true})
        for (const std::int32_t hashCount : {1, 2, 4, 8})
        {
            reset();
            fixture.Shape(external ? 160 : 4, external ? 129 : 1, external, hashCount);
            Check(adjust(17, 1, receipt) == ClockResult::Ok && receipt.accepted && referenceCalls == 1 && fixture.Count(external ? 129 : 1) == 8,
                  "inline/dynamic allocation bits and hash buckets follow the native allocator/free-chain representation");
        }
    reset();
    Check(adjust(17, 1, receipt) == ClockResult::Ok && adjust(17, -1, receipt) == ClockResult::Ok && referenceCalls == 2 && fixture.Count(1) == 7,
          "each call reacquires the key and uses the current accepted count without caching a native row across calls");
    reset();
    Check(adjust(17, 1, receipt) == ClockResult::Ok, "first adjustment observes the original sparse row allocation");
    std::memcpy(fixture.relocated, fixture.references, 4 * RP::RowBytes);
    fixture.Header(0, fixture.Address(fixture.relocated));
    Check(adjust(17, -1, receipt) == ClockResult::Ok && receipt.row == fixture.Address(fixture.relocated) + RP::RowBytes && fixture.Count(1) == 8,
          "a later call reacquires the replaced native row allocation instead of mutating its earlier accepted address");
    reset();
    DWORD protection = 0, discarded = 0;
    Check(VirtualProtect(fixture.references, 0x1000, PAGE_READONLY, &protection) != 0, "make the actual row page read-only for the admission check");
    const auto readonlyResult = adjust(17, 1, receipt);
    Check(VirtualProtect(fixture.references, 0x1000, protection, &discarded) != 0, "restore owned row page protection after the check");
    Check(readonlyResult != ClockResult::Ok && !receipt.entered && referenceCalls == 0 && fixture.Count(1) == 7,
          "a readable native-shaped page is not writable counter authority");
    for (unsigned guard = 0; guard != 25; ++guard)
    {
        reset();
        if (guard == 0)
            fixture.Header(0, std::uintptr_t{0});
        if (guard == 1)
            fixture.Header(8, std::int32_t{-1});
        if (guard == 2)
            fixture.Header(0xc, std::int32_t{3});
        if (guard == 3)
            fixture.Header(0x28, std::int32_t{3});
        if (guard == 4)
            fixture.Header(0x2c, std::int32_t{256});
        if (guard == 5)
            fixture.Header(0x34, std::int32_t{0});
        if (guard == 6)
            fixture.Header(0x30, std::int32_t{-1});
        if (guard == 7)
            fixture.Header(0x48, std::int32_t{3});
        if (guard == 8)
            fixture.Header(0x40, std::uintptr_t{0});
        if (guard == 9)
            fixture.Row(1, 8, std::int32_t{1});
        if (guard == 10)
            fixture.Row(1, 12, std::int32_t{0});
        if (guard == 11)
            fixture.Header(0x10, std::uint32_t{5});
        if (guard == 12)
            fixture.Row(3, 0, std::int32_t{17});
        if (guard == 13)
            fixture.Row(3, 4, std::int32_t{3});
        if (guard == 14)
            fixture.Put(fixture.buckets, 4, std::int32_t{3});
        if (guard == 15)
            fixture.Row(1, 0, std::uint32_t{33});
        if (guard == 16)
            fixture.Header(0, fixture.Address(fixture.provider));
        if (guard == 17)
            fixture.Stream(17, 0, 2);
        if (guard == 18)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (guard == 19)
            fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
        if (guard == 20)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (guard == 21)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (guard == 22)
            fixture.Put(fixture.items, 48 + 8, std::uint32_t{0x30000000});
        if (guard == 23)
            fixture.Slot(17, 0);
        if (guard == 24)
            fixture.Row(1, 8, std::int32_t{4});
        Check(adjust(17, 1, receipt) != ClockResult::Ok && !receipt.entered && referenceCalls == 0 && fixture.Count(1) == 7,
              "malformed allocation/free/hash/key metadata and current native owner/model/loading prerequisites refuse before exchange");
    }
    reset();
    fixture.Row(2, 0, std::uint32_t{17});
    fixture.Row(2, 12, std::int32_t{1});
    fixture.Row(2, 8, std::int32_t{1});
    fixture.Put(fixture.buckets, 4, std::int32_t{2});
    fixture.Put(fixture.buckets, 12, std::int32_t{-1});
    Check(adjust(17, 1, receipt) == ClockResult::BindingChanged && !receipt.entered, "duplicate occupied keys cannot choose an arbitrary native reference row");
    for (const auto key : {B::ModelCount, std::uint32_t{65535}, std::uint32_t{65536}, std::uint32_t{0xffffffff}, std::uint32_t{1000}, std::uint32_t{1193}})
    {
        reset();
        Check(adjust(key, 1, receipt) != ClockResult::Ok && !receipt.entered && referenceCalls == 0 && PoolTestHost::reads == 0,
              "unsigned out-of-range keys and unimplemented upgrade reference policy never alias another model");
    }
    for (const auto delta : {0, 2, -2})
    {
        reset();
        Check(adjust(17, delta, receipt) == ClockResult::InvalidArgument && !receipt.entered && PoolTestHost::reads == 0,
              "only one-step native intent is admitted");
    }
    reset();
    Check(R::Adjust(image, 17, 1, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.entered, "map readability cannot invent a callback owner");
    reset();
    PoolTestHost::watched = fixture.Address(fixture.Map());
    PoolTestHost::after = &FreeSelectedWhileReading;
    Check(adjust(17, 1, receipt) != ClockResult::Ok && !receipt.entered && referenceCalls == 0,
          "map occupancy changes during capture cannot authorize the old keyed row");
    for (unsigned mode = 1; mode <= 9; ++mode)
    {
        reset();
        referenceMode = mode;
        Check(adjust(17, 1, receipt) != ClockResult::Ok && receipt.entered && referenceCalls == 1 && receipt.accepted == (mode >= 3),
              "one exchange or comparison failure is accounted without retry, including post-write owner/root/map/registry/loaded/code loss");
        Check(fixture.Count(1) == (mode == 1   ? 7u
                                   : mode == 2 ? 9u
                                               : 8u),
              "accepted native count effects and conflicting outside intent survive failure without rollback");
        if (mode == 9)
            fixture.transform.image.Patch(RP::Code[0].rva, RP::Code[0].bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    Check(adjust(17, 1, receipt) == ClockResult::Ok, "measure actual reference copy/owner boundaries once");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned failed = 1; failed <= reads; ++failed)
    {
        reset();
        PoolTestHost::failAt = failed;
        Check(adjust(17, 1, receipt) != ClockResult::Ok && referenceCalls <= 1 && receipt.entered == (referenceCalls != 0) &&
                  fixture.Count(1) == (receipt.accepted ? 8u : 7u),
              "every actual copied-byte fault refuses before entry or retains the single accepted count effect");
    }
    for (unsigned revoked = 1; revoked <= owners; ++revoked)
    {
        reset();
        PoolTestHost::revokeAt = revoked;
        Check(adjust(17, 1, receipt) != ClockResult::Ok && referenceCalls <= 1 && fixture.Count(1) == (receipt.accepted ? 8u : 7u),
              "every genuine callback owner boundary can revoke without replay or rollback");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread(
        [&]()
        {
            R::Receipt other;
            foreign = adjust(17, 1, other);
        });
    thread.join();
    Check(foreign != ClockResult::Ok && referenceCalls == 0, "a real producer thread cannot borrow native GT authority to mutate model counts");
    reset();
    for (const auto& pin : RP::Code)
    {
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(adjust(17, 1, receipt) != ClockResult::Ok && !receipt.entered && referenceCalls == 0,
              "changed native count/map/allocator semantics refuse before exchange");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    std::puts("Production native existing model-reference adjustment fixture completed");
    std::printf("Passed %u native model-reference checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
