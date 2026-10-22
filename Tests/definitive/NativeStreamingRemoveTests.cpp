/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine removal admission/effects with owned final native entry results
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeRemoveHistoricalAcquireMain
#include "NativeModelReferenceAcquireTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStreamingRemoveCalls.h"
#include "../../Client/game_de/NativeStreamingRemoveProfile.h"
namespace
{
    ClockResult InvokeRemove(mta::game_de::NativeStreamingRemoveCalls::NativeFunction, std::uint32_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                      PoolTestHost::Module
#define ReadProcessMemory                     PoolTestHost::Read
#define MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE ::InvokeRemove
#include "../../Client/game_de/NativeStreamingRemoveCalls.cpp"
#undef MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace RC = NativeStreamingRemoveCalls;
    namespace MP = NativeStreamingRemoveProfile;
    namespace RM = NativeStreamingRemoveMemory;
    struct RemoveFixture : AcquireFixture
    {
        unsigned char* assetRows = nullptr;
        std::uint32_t  selectedKey = 17;
        bool           shared = false;
        explicit RemoveFixture(const std::filesystem::path& path) : AcquireFixture(path)
        {
            assetRows = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(assetRows != nullptr, "own separate genuine-shaped sparse asset rows");
            for (const auto& pin : MP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            transform.image.Patch(NativeStreamingFileIdsProfile::initializeRva, NativeStreamingFileIdsProfile::initialize.data(),
                                  NativeStreamingFileIdsProfile::initialize.size(), PAGE_EXECUTE_READ);
            const auto          deletion = transform.image.image + std::uintptr_t{0x1100030}, setter = transform.image.image + std::uintptr_t{0x1100040};
            const unsigned char ret = 0xc3;
            const std::array<unsigned char, 5> code{0x48, 0x89, 0x51, 0x58, 0xc3};
            transform.image.Patch(0x1100030, &ret, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(0x1100040, code.data(), code.size(), PAGE_EXECUTE_READ);
            transform.image.Patch(0x4301000 + 0x50, &deletion, 8, PAGE_READONLY);
            transform.image.Patch(0x4301000 + 0x80, &setter, 8, PAGE_READONLY);
            Reset();
        }
        template <class T>
        T Get(const unsigned char* object, std::size_t at) const
        {
            T value{};
            std::memcpy(&value, object + at, sizeof(value));
            return value;
        }
        unsigned char* AssetMap() const { return application + MP::AssetMapOffset; }
        unsigned char* Bundle() const { return rows + static_cast<std::size_t>(selectedKey) * SP::ProviderRowStride; }
        unsigned char* ObjectClass() const { return metadata + 0x200; }
        unsigned char* ClassClass() const { return metadata + 0x300; }
        unsigned char* ActorClass() const { return metadata + 0x400; }
        unsigned char* Asset() const { return metadata + 0x500; }
        unsigned char* ActorAsset() const { return metadata + 0x600; }
        void           Type(unsigned char value)
        {
            const std::array<unsigned char, 6> code{0xb0, value, 0xc3, 0xcc, 0xcc, 0xcc};
            transform.image.Patch(0x1100010, code.data(), code.size(), PAGE_EXECUTE_READ);
        }
        void Register(unsigned char* object, std::int32_t index, unsigned char* type)
        {
            Put(object, 0, transform.image.image + std::uintptr_t{0x4301020});
            Put(object, 0xc, index);
            Put(object, 0x10, Address(type));
            Put(items, static_cast<std::size_t>(index) * 24, Address(object));
            Put(items, static_cast<std::size_t>(index) * 24 + 0x10, static_cast<std::uint32_t>(10 + index));
        }
        void Shape(std::int32_t bucketsCount = 1, bool external = false, std::uint16_t firstCount = 2, std::uint16_t secondCount = 1)
        {
            std::memset(AssetMap(), 0, MP::HeaderBytes);
            std::memset(assetRows, 0, 0x1000);
            std::memset(allocationBits, 0, 0x1000);
            std::memset(buckets, 0xff, static_cast<std::size_t>(bucketsCount) * 4);
            Put(AssetMap(), 0, Address(assetRows));
            Put(AssetMap(), 8, std::int32_t{3});
            Put(AssetMap(), 0xc, std::int32_t{4});
            Put(AssetMap(), 0x20, external ? Address(allocationBits) : std::uintptr_t{0});
            Put(AssetMap(), 0x28, std::int32_t{3});
            Put(AssetMap(), 0x2c, external ? std::int32_t{32} : std::int32_t{128});
            Put(AssetMap(), 0x30, std::int32_t{-1});
            Put(AssetMap(), 0x40, bucketsCount == 1 ? std::uintptr_t{0} : Address(buckets));
            Put(AssetMap(), 0x48, bucketsCount);
            auto* bits = external ? allocationBits : AssetMap() + 0x10;
            Put(bits, 0, std::uint32_t{7});
            auto* heads = bucketsCount == 1 ? AssetMap() + 0x38 : buckets;
            Put(heads, 0, std::int32_t{-1});
            const std::array<std::uintptr_t, 3> keys{Address(Asset()), Address(ActorAsset()), std::uintptr_t{0xaaa0}};
            const std::array<std::uint16_t, 3>  counts{firstCount, secondCount, 7};
            for (std::size_t i = 0; i < keys.size(); ++i)
            {
                const auto bucket = static_cast<std::int32_t>(NativeStreamingRemoveAssets::Bucket(keys[i], static_cast<std::uint32_t>(bucketsCount - 1)));
                const auto head = Get<std::int32_t>(heads, static_cast<std::size_t>(bucket) * 4);
                Put(assetRows, i * 24, keys[i]);
                Put(assetRows, i * 24 + 8, counts[i]);
                Put(assetRows, i * 24 + 0x10, head);
                Put(assetRows, i * 24 + 0x14, bucket);
                Put(heads, static_cast<std::size_t>(bucket) * 4, static_cast<std::int32_t>(i));
            }
        }
        void Reset(std::uint32_t key = 17)
        {
            AcquireFixture::Reset();
            selectedKey = key;
            shared = false;
            Slot(key, Address(metadata));
            std::memset(items + 72, 0, 7 * 24);
            Global(0x5087394, std::int32_t{8});
            Register(ObjectClass(), 3, ClassClass());
            Register(ClassClass(), 4, ClassClass());
            Register(ActorClass(), 5, ClassClass());
            Register(Asset(), 6, ObjectClass());
            Register(ActorAsset(), 7, ClassClass());
            Put(ObjectClass(), 0x30, Address(metadata) + 0x900);
            Put(ObjectClass(), 0x38, std::int32_t{0});
            Put(ClassClass(), 0x30, Address(metadata) + 0x920);
            Put(ClassClass(), 0x38, std::int32_t{1});
            Put(ActorClass(), 0x30, Address(metadata) + 0x940);
            Put(ActorClass(), 0x38, std::int32_t{1});
            Put(ActorAsset(), 0x30, Address(metadata) + 0x960);
            Put(ActorAsset(), 0x38, std::int32_t{2});
            for (const auto at : {std::size_t{0x900}, std::size_t{0x920}, std::size_t{0x940}, std::size_t{0x960}})
                Put(metadata, at, Address(ObjectClass()) + 0x30);
            Put(metadata, 0x928, Address(ClassClass()) + 0x30);
            Put(metadata, 0x948, Address(ActorClass()) + 0x30);
            Put(metadata, 0x968, Address(ActorClass()) + 0x30);
            Put(metadata, 0x970, Address(ActorAsset()) + 0x30);
            Global(MP::ObjectClassRva, Address(ObjectClass()));
            Global(MP::MetaClassRva, Address(ClassClass()));
            Global(MP::ActorClassRva, Address(ActorClass()));
            Type(7);
            std::memset(Bundle(), 0, SP::ProviderRowStride);
            Put(Bundle(), 0x18, std::int32_t{6});
            Put(Bundle(), 0x1c, std::uint32_t{16});
            Put(Bundle(), 0x40, std::int32_t{7});
            Put(Bundle(), 0x44, std::uint32_t{17});
            Put(metadata, 0x58, Address(Bundle()));
            RM::StreamRow row{};
            Put(row.data(), 0, Address(Bundle()));
            Put(row.data(), 8, std::int16_t{-1});
            Put(row.data(), 10, std::int16_t{-1});
            row[12] = 0x40;
            row[13] = 1;
            Global(LP::ArrayRva + static_cast<std::uintptr_t>(key) * 16, row);
            Global(MP::ListBaseRva, transform.image.image + LP::ArrayRva);
            std::array<std::int32_t, 24> ids{};
            ids.fill(-1);
            ids[2] = static_cast<std::int32_t>(key);
            ids[18] = static_cast<std::int32_t>(key);
            Global(0x5300dc0, ids);
            Global(0x5300d68, std::uint32_t{2});
            Global(0x572bec8, std::uint32_t{6});
            Global(0x572bec4, std::uint32_t{4});
            for (std::uint32_t model = 290; model <= 319; ++model)
                Slot(model, model == key ? Address(metadata) : std::uintptr_t{0});
            Shape();
        }
        void Linked(bool same = false)
        {
            const std::int16_t previous = 26312, next = same ? previous : std::int16_t{26313};
            RM::StreamRow      a{}, b{};
            Put(a.data(), 8, std::int16_t{-1});
            Put(a.data(), 10, static_cast<std::int16_t>(selectedKey));
            Put(b.data(), 8, static_cast<std::int16_t>(selectedKey));
            Put(b.data(), 10, std::int16_t{-1});
            if (same)
                Put(a.data(), 8, static_cast<std::int16_t>(selectedKey));
            Global(LP::ArrayRva + static_cast<std::uintptr_t>(previous) * 16, a);
            if (!same)
                Global(LP::ArrayRva + static_cast<std::uintptr_t>(next) * 16, b);
            Global(LP::ArrayRva + static_cast<std::uintptr_t>(selectedKey) * 16 + 8, std::array<std::int16_t, 2>{previous, next});
        }
        void Share()
        {
            shared = true;
            auto* other = metadata + 0xa00;
            std::memset(other, 0, 0x80);
            Put(other, 0x58, Address(Bundle()));
            Slot(301, Address(other));
        }
        ~RemoveFixture()
        {
            if (assetRows)
                VirtualFree(assetRows, 0, MEM_RELEASE);
        }
    };
    RemoveFixture*     activeRemove = nullptr;
    unsigned           removeCalls = 0, removeMode = 0;
    RC::NativeFunction passedRemove = nullptr;
    void               DropOwnedAsset(RemoveFixture& f, std::uintptr_t key)
    {
        // Owned final-entry result, independent from production observation.
        // Native engine code and its destructor/allocator never execute here.
        auto*      map = f.AssetMap();
        const auto count = f.Get<std::int32_t>(map, 8);
        auto*      bits = f.Get<std::uintptr_t>(map, 0x20) ? f.allocationBits : map + 0x10;
        auto*      heads = f.Get<std::uintptr_t>(map, 0x40) ? f.buckets : map + 0x38;
        for (std::int32_t i = 0; i < count; ++i)
        {
            if (!(f.Get<std::uint32_t>(bits, static_cast<std::size_t>(i / 32) * 4) & (std::uint32_t{1} << (i % 32))) ||
                f.Get<std::uintptr_t>(f.assetRows, static_cast<std::size_t>(i) * 24) != key)
                continue;
            auto*      row = f.assetRows + static_cast<std::size_t>(i) * 24;
            const auto remaining = static_cast<std::uint16_t>(f.Get<std::uint16_t>(row, 8) - 1);
            f.Put(row, 8, remaining);
            if (remaining)
                return;
            auto* link = heads + static_cast<std::size_t>(f.Get<std::int32_t>(row, 0x14)) * 4;
            while (f.Get<std::int32_t>(link, 0) != i)
                link = f.assetRows + static_cast<std::size_t>(f.Get<std::int32_t>(link, 0)) * 24 + 0x10;
            f.Put(link, 0, f.Get<std::int32_t>(row, 0x10));
            const auto free = f.Get<std::int32_t>(map, 0x34), head = f.Get<std::int32_t>(map, 0x30);
            if (free)
                f.Put(f.assetRows, static_cast<std::size_t>(head) * 24, i);
            f.Put(row, 0, std::int32_t{-1});
            f.Put(row, 4, free ? head : std::int32_t{-1});
            f.Put(map, 0x30, i);
            f.Put(map, 0x34, free + 1);
            f.Put(bits, static_cast<std::size_t>(i / 32) * 4,
                  f.Get<std::uint32_t>(bits, static_cast<std::size_t>(i / 32) * 4) & ~(std::uint32_t{1} << (i % 32)));
            return;
        }
    }
    ClockResult InvokeRemove(RC::NativeFunction function, std::uint32_t key, std::uint32_t& exception) noexcept
    {
        ++removeCalls;
        passedRemove = function;
        auto& f = *activeRemove;
        auto  state = f.Stream(key);
        if (removeMode == 1)
        {
            exception = 0xe05a0001;
            return ClockResult::OriginalException;
        }
        if (!state[1])
            return ClockResult::Ok;
        if (removeMode == 6)
            return ClockResult::Ok;
        auto       row = reinterpret_cast<unsigned char*>(f.transform.image.image + LP::ArrayRva + static_cast<std::uintptr_t>(key) * 16);
        const auto previous = f.Get<std::int16_t>(row, 8), next = f.Get<std::int16_t>(row, 10);
        if (previous != -1)
        {
            f.Global(LP::ArrayRva + static_cast<std::uintptr_t>(previous) * 16 + 10, next);
            f.Global(LP::ArrayRva + static_cast<std::uintptr_t>(next) * 16 + 8, previous);
            f.Global(LP::ArrayRva + static_cast<std::uintptr_t>(key) * 16 + 8, std::uint32_t{0xffffffff});
        }
        f.Stream(key, state[0], 0);
        if (removeMode == 2)
        {
            exception = 0xe05a0002;
            return ClockResult::OriginalException;
        }
        const auto type = f.Get<unsigned char>(reinterpret_cast<unsigned char*>(f.transform.image.image + 0x1100010), 1);
        if (type == 7)
        {
            auto* ids = reinterpret_cast<unsigned char*>(f.transform.image.image + 0x5300dc0);
            auto  total = f.Get<std::uint32_t>(reinterpret_cast<unsigned char*>(f.transform.image.image), 0x5300d68);
            for (std::size_t i = 0; i < 24; ++i)
                if (f.Get<std::int32_t>(ids, i * 4) == static_cast<std::int32_t>(key))
                {
                    f.Global(0x5300dc0 + i * 4, std::int32_t{-1});
                    --total;
                }
            f.Global(0x5300d68, total);
        }
        const auto bundle = f.Get<std::uintptr_t>(row, 0);
        if (bundle)
        {
            if (!f.shared)
            {
                const auto first = f.Get<std::int32_t>(f.Bundle(), 0x18), second = f.Get<std::int32_t>(f.Bundle(), 0x40);
                if (first >= 0)
                    DropOwnedAsset(f, f.Get<std::uintptr_t>(f.items, static_cast<std::size_t>(first) * 24));
                if (second >= 0)
                    DropOwnedAsset(f, f.Get<std::uintptr_t>(f.items, static_cast<std::size_t>(second) * 24));
                if (type == 7)
                    f.Global(0x572bec8, f.Get<std::uint32_t>(reinterpret_cast<unsigned char*>(f.transform.image.image), 0x572bec8) - 1);
                if (type == 6)
                    f.Global(0x572bec4, f.Get<std::uint32_t>(reinterpret_cast<unsigned char*>(f.transform.image.image), 0x572bec4) - 1);
            }
            f.Global(LP::ArrayRva + static_cast<std::uintptr_t>(key) * 16, std::uintptr_t{0});
            f.Put(f.metadata, 0x58, std::uintptr_t{0});
        }
        if (removeMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (removeMode == 4)
            f.Global(SP::RootRva, std::uintptr_t{0});
        if (removeMode == 5)
            f.Put(f.assetRows, 2 * 24 + 8, std::uint16_t{99});
        if (removeMode == 7)
            f.Put(f.items, 7 * 24 + 0x10, std::uint32_t{99});
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2, "durable removal fixture evidence directory supplied");
    RemoveFixture f(argv[1]);
    activeRemove = &f;
    const auto image = f.transform.image.image;
    PoolTestHost::image = image;
    RC::Receipt receipt;
    const auto  reset = [&](std::uint32_t key = 17)
    {
        f.Reset(key);
        removeCalls = removeMode = 0;
        passedRemove = nullptr;
    };
    const auto run = [&](std::uint32_t key = 17) { return RC::Remove(image, key, &PoolTestHost::Owner, &receipt); };
    reset();
    const auto initial = run();
    if (initial != ClockResult::Ok)
    {
        std::fprintf(stderr, "Initial removal status=%u entered=%u returned=%u calls=%u\n", static_cast<unsigned>(initial),
                     static_cast<unsigned>(receipt.entered), static_cast<unsigned>(receipt.returned), removeCalls);
    }
    Check(initial == ClockResult::Ok && receipt.entered && receipt.returned && removeCalls == 1 &&
              passedRemove == reinterpret_cast<RC::NativeFunction>(image + MP::RemoveRva),
          "one real removal ABI entry with complete typed current DFF dependencies");
    Check(receipt.rawStateBefore == 1 && receipt.rawStateAfter == 0 && f.Stream(17)[1] == 0 && f.Get<std::uint16_t>(f.assetRows, 8) == 1 &&
              f.Get<std::int32_t>(f.AssetMap(), 0x30) == 1 && f.Get<std::uint32_t>(f.AssetMap(), 0x10) == 5,
          "WORD asset decrement and zero-reaching sparse release survive beside actual unloaded selected row");
    Check(f.Get<std::uint32_t>(f.references, 4) == 3, "direct streaming RemoveModel does not silently decrement unrelated DWORD model reference ownership");
    for (const auto key : {std::uint32_t{0}, std::uint32_t{1000}, std::uint32_t{1193}, std::uint32_t{19999}})
    {
        reset(key);
        Check(run(key) == ClockResult::Ok && removeCalls == 1, "ordinary selected-key removal includes upgrades without paired decrements");
    }
    reset();
    f.Shape(4, true, 1, 1);
    Check(run() == ClockResult::Ok && f.Get<std::int32_t>(f.AssetMap(), 0x34) == 2, "two final asset rows release through external occupancy/hash allocations");
    reset();
    f.Put(f.Bundle(), 0x18, std::int32_t{7});
    f.Put(f.Bundle(), 0x1c, std::uint32_t{17});
    f.Shape(1, false, 2, 2);
    Check(run() == ClockResult::Ok && f.Get<std::int32_t>(f.AssetMap(), 0x34) == 1,
          "same typed asset reached twice decrements twice and removes precisely at zero");
    for (const bool same : {false, true})
    {
        reset();
        f.Linked(same);
        Check(run() == ClockResult::Ok, "reciprocal ordinary and shared-sentinel intrusive links unlink exactly");
    }
    reset(300);
    f.Share();
    const auto oldCount = f.Get<std::uint16_t>(f.assetRows, 8);
    Check(run(300) == ClockResult::Ok && f.Get<std::uint16_t>(f.assetRows, 8) == oldCount,
          "native290..319 bundle sharing suppresses asset release but detaches selected metadata");
    reset();
    f.Global(LP::ArrayRva + 17 * 16, std::uintptr_t{0});
    f.Put(f.metadata, 0x58, std::uintptr_t{0});
    Check(run() == ClockResult::Ok, "loaded native model with no associated bundle uses genuine deletion and state change");
    for (const auto key : {std::uint32_t{17}, std::uint32_t{20000}, NativeModelLoadedProfile::ModelCount - 1})
    {
        reset();
        f.Stream(key, 0x40, 0);
        f.Global(SP::RootRva, std::uintptr_t{0});
        f.Global(AP::HeapRva, std::uintptr_t{0});
        Check(run(key) == ClockResult::Ok && removeCalls == 1 && receipt.root == 0,
              "genuine state0 no-op acquires no model/root/heap allocation prerequisites");
    }
    for (unsigned mode = 1; mode <= 7; ++mode)
    {
        reset();
        removeMode = mode;
        const auto result = run();
        Check(result != ClockResult::Ok && receipt.entered && removeCalls == 1, "entered failure retains receipt and never retries native removal");
        if (mode == 2)
            Check(f.Stream(17)[1] == 0 && f.Get<std::uint16_t>(f.assetRows, 8) == 2, "early partial native unloaded effect is retained without rollback");
    }
    for (const auto state : {2, 3, 4, 255})
    {
        reset();
        f.Stream(17, 0x40, static_cast<unsigned char>(state));
        Check(run() == ClockResult::UnsupportedBuild && !receipt.entered && !removeCalls,
              "unqualified pending/cancellation state is not transformed into an invented unload");
    }
    for (const auto key : {NativeModelLoadedProfile::ModelCount, std::uint32_t{0xffff}, std::uint32_t{0xffffffff}, std::uint32_t{0x1000011}})
    {
        reset();
        Check(run(key) == ClockResult::InvalidArgument && !receipt.entered && !removeCalls, "full DWORD resource bounds precede all native entry");
    }
    reset();
    f.Stream(20000, 0, 1);
    Check(run(20000) == ClockResult::UnsupportedBuild && !receipt.entered, "loaded resource destructors remain distinct from DFF removal");
    for (unsigned fault = 0; fault < 17; ++fault)
    {
        reset();
        switch (fault)
        {
            case 0:
                f.Put(f.metadata, 0x58, f.Address(f.Bundle()) + 1);
                break;
            case 1:
                f.Global(LP::ArrayRva + 17 * 16, f.Address(f.rows) + 1);
                f.Put(f.metadata, 0x58, f.Address(f.rows) + 1);
                break;
            case 2:
                f.Global(LP::ArrayRva + 17 * 16 + 8, std::array<std::int16_t, 2>{-1, 26312});
                break;
            case 3:
                f.Linked();
                f.Global(LP::ArrayRva + 26312 * 16 + 10, std::int16_t{18});
                break;
            case 4:
                f.Global(MP::ListBaseRva, image + LP::ArrayRva + 16);
                break;
            case 5:
                f.Put(f.Bundle(), 0x1c, std::uint32_t{99});
                break;
            case 6:
                f.Put(f.Bundle(), 0x18, std::int32_t{-1});
                f.Put(f.Bundle(), 0x1c, std::uint32_t{0});
                f.Put(f.Bundle(), 0x28, std::uintptr_t{1});
                break;
            case 7:
                f.Put(f.assetRows, 8, std::uint16_t{0});
                break;
            case 8:
                f.Put(f.AssetMap(), 0x10, std::uint32_t{3});
                break;
            case 9:
                f.Put(f.assetRows, 2 * 24 + 0x10, std::int32_t{2});
                break;
            case 10:
                f.Global(MP::ActorClassRva, std::uintptr_t{0});
                break;
            case 11:
                f.Global(0x572bec8, std::uint32_t{0});
                break;
            case 12:
                f.Global(0x5300d68, std::uint32_t{1});
                break;
            case 13:
                f.Type(6);
                f.Slot(409, f.Address(f.metadata));
                f.Stream(409, 0, 1);
                break;
            case 14:
                f.Put(f.ActorAsset(), 0x38, std::int32_t{64});
                break;
            case 15:
                f.Put(f.assetRows, 0x14, std::int32_t{7});
                break;
            case 16:
                f.Global(AP::HeapAuthorityRva, std::uintptr_t{0});
                break;
        }
        Check(run(fault == 13 ? 409 : 17) != ClockResult::Ok && !receipt.entered && !removeCalls,
              "malformed bank/list/weak/type/map/counter/heap admission fails before any native effect");
    }
    reset();
    std::array<unsigned char, 6> bytes{};
    bytes.fill(0x5a);
    std::uint8_t type = 99;
    for (const auto value : {0, 6, 7, 8, 255})
    {
        f.Type(static_cast<unsigned char>(value));
        Check(NativeModelTypeLeaf::Read(image, image + 0x1100010, bytes, type) && type == value,
              "shared native type leaf preserves the entire raw AL domain including native8");
    }
    const auto                         preserved = bytes;
    const std::array<unsigned char, 6> invalid{0x48, 0x8b, 0x01, 0xff, 0x50, 0x20};
    f.transform.image.Patch(0x1100010, invalid.data(), 6, PAGE_EXECUTE_READ);
    Check(!NativeModelTypeLeaf::Read(image, image + 0x1100010, bytes, type) && bytes == preserved && type == 255,
          "non-leaf type path refuses without fabricated type/output changes");
    for (const auto& pin : MP::Code)
    {
        reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        f.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        Check(run() != ClockResult::Ok && !receipt.entered && !removeCalls, "changed actual removal/class/asset protocol cannot inherit native call authority");
        f.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    reset();
    Check(run() == ClockResult::Ok, "successful final snapshot establishes concrete read/owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (const auto fail : {1u, reads / 2, reads})
    {
        reset();
        PoolTestHost::failAt = fail;
        Check(run() != ClockResult::Ok && removeCalls <= 1, "guarded copied-byte failure cannot invent native completion or cause a replay");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        Check(run() != ClockResult::Ok && removeCalls <= 1, "every current callback boundary can revoke without replay");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = run(); });
    worker.join();
    Check(foreign != ClockResult::Ok && !removeCalls, "real foreign thread cannot borrow native GameThread removal authority");
    std::puts("Production native streaming removal transaction fixture completed");
    std::printf("Passed %u native streaming-removal checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
