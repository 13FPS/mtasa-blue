/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise immediate native UObject identity over owned mock memory
 *****************************************************************************/
#include "../../Client/game_de/NativeUObjectMembershipAccess.h"
#include "../../Client/game_de/NativeUObjectMembershipProfile.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Access = mta::game_de::NativeUObjectMembershipAccess;
    constexpr std::uintptr_t image = 0x10000000, object = 0x20000000, chunks = 0x21000000;
    constexpr std::uintptr_t chunk0 = 0x22000000, chunk1 = 0x23000000, vtable = 0x30000000, objectClass = 0x31000000;
    constexpr std::uintptr_t registry = image + 0x5087380, objectCount = image + 0x5087394;
    constexpr std::uint32_t  serial = 0x1234;
    unsigned                 checks = 0;
    void                     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u UObject-membership checks: %s\n", checks, reason);
            std::exit(EXIT_FAILURE);
        }
    }
    struct Region
    {
        std::uintptr_t             address;
        std::vector<unsigned char> bytes;
    };
    struct Fixture;
    Fixture* current = nullptr;
    struct Fixture
    {
        std::vector<Region> regions;
        std::uintptr_t      item = 0, deny = 0, changeAt = 0;
        unsigned            reads = 0, failAt = 0, watched = 0, changeRead = 2;
        std::uint64_t       changeValue = 0;
        std::size_t         changeBytes = 0;
        void                Add(std::uintptr_t address, std::size_t bytes) { regions.push_back({address, std::vector<unsigned char>(bytes)}); }
        void                PutBytes(std::uintptr_t address, const void* value, std::size_t size)
        {
            for (auto& region : regions)
                if (address >= region.address && address - region.address <= region.bytes.size() && size <= region.bytes.size() - (address - region.address))
                {
                    std::memcpy(region.bytes.data() + address - region.address, value, size);
                    return;
                }
            std::abort();
        }
        template <typename T>
        void Put(std::uintptr_t address, T value)
        {
            PutBytes(address, &value, sizeof(value));
        }
        explicit Fixture(std::int32_t index = 0)
        {
            current = this;
            item = (index >= 65536 ? chunk1 : chunk0) + (static_cast<std::uint32_t>(index) & 0xffffu) * 24u;
            Add(object, 0x28);
            Add(chunks, 16);
            Add(item, 24);
            Add(registry, 8);
            Add(objectCount, 4);
            Put(object, vtable);
            Put(object + 8, std::uint32_t{0});
            Put(object + 0xc, index);
            Put(object + 0x10, objectClass);
            Put(object + 0x20, std::uintptr_t{0});
            Put(registry, chunks);
            Put(objectCount, index + 1);
            Put(chunks, chunk0);
            Put(chunks + 8, chunk1);
            Put(item, object);
            Put(item + 8, std::uint32_t{0});
            Put(item + 0x10, serial);
        }
        static bool Read(std::uintptr_t address, void* output, std::size_t size) noexcept
        {
            auto& fixture = *current;
            if (++fixture.reads == fixture.failAt || (fixture.deny && address <= fixture.deny && fixture.deny - address < size))
                return false;
            if (address == fixture.changeAt && ++fixture.watched == fixture.changeRead)
                fixture.PutBytes(address, &fixture.changeValue, fixture.changeBytes);
            for (const auto& region : fixture.regions)
                if (address >= region.address && address - region.address <= region.bytes.size() && size <= region.bytes.size() - (address - region.address))
                {
                    std::memcpy(output, region.bytes.data() + address - region.address, size);
                    return true;
                }
            return false;
        }
    };
    Access::Snapshot Sentinel()
    {
        Access::Snapshot value{};
        value.objectAddress = 0x12345678;
        value.vtableAddress = 0x55555555;
        value.index = 123;
        value.serial = 456;
        return value;
    }
    bool Untouched(const Access::Snapshot& value)
    {
        return value.objectAddress == 0x12345678 && value.vtableAddress == 0x55555555 && value.index == 123 && value.serial == 456;
    }
    void Refuses(Fixture& fixture, const char* reason)
    {
        (void)fixture;
        auto value = Sentinel();
        Check(!Access(image, Fixture::Read).Capture(object, value), reason);
        Check(Untouched(value), "refusal preserves previous output");
    }
}

int main()
{
    static_assert(mta::game_de::NativeUObjectMembershipProfile::RawToWeakConstructor.size() == 225);
    static_assert(mta::game_de::NativeUObjectMembershipProfile::StrictWeakResolver.size() == 90);
    unsigned successfulReads = 0;
    for (const std::int32_t index : {0, 1, 65535, 65536, 65537})
    {
        Fixture          fixture(index);
        Access           access(image, Fixture::Read);
        Access::Snapshot value{};
        Check(access.Capture(object, value), "valid native chunk boundary membership");
        successfulReads = fixture.reads;
        Check(value.objectAddress == object && value.vtableAddress == vtable && value.classAddress == objectClass && value.outerAddress == 0 &&
                  value.index == index && value.serial == serial && value.itemAddress == fixture.item,
              "actual captured identity");
        Check(access.Recheck(value), "same scope identity rechecks");
        Check(Access::MatchesWeak(value, index, serial), "existing native positive weak token matches");
        Check(!Access::MatchesWeak(value, index, 0) && !Access::MatchesWeak(value, -1, serial) && !Access::MatchesWeak(value, index + 1, serial) &&
                  !Access::MatchesWeak(value, index, serial + 1),
              "weak mismatch refuses");
    }
    {
        Fixture          fixture;
        Access           access(image, Fixture::Read);
        Access::Snapshot value{};
        fixture.Put(fixture.item + 0x10, std::uint32_t{0});
        Check(access.Capture(object, value) && value.serial == 0 && access.Recheck(value), "unassigned raw serial is observed without mutation");
        Check(!Access::MatchesWeak(value, 0, 0), "raw zero serial does not manufacture a native weak handle");
        fixture.Put(fixture.item + 0x10, serial);
        Check(!access.Recheck(value), "native lazy serial assignment invalidates old snapshot");
    }
    {
        Fixture          fixture;
        Access::Snapshot value{};
        fixture.Put(object + 8, std::uint32_t{0x10});
        Check(Access(image, Fixture::Read).Capture(object, value), "class metadata is not rejected by a GameMode-only RF_ClassDefaultObject rule");
    }
    for (unsigned fail = 1; fail <= successfulReads; ++fail)
    {
        Fixture fixture;
        fixture.failAt = fail;
        Refuses(fixture, "every consumed read refusal remains refusal");
    }
    const std::uintptr_t watchedAddresses[] = {object,   object + 8, object + 0xc, object + 0x10, object + 0x20, objectCount,
                                               registry, chunks,     chunk0,       chunk0 + 8,    chunk0 + 0x10};
    const std::size_t    sizes[] = {8, 4, 4, 8, 8, 4, 8, 8, 8, 4, 4};
    const std::uint64_t  changes[] = {vtable + 8, 1, 1, objectClass + 8, objectClass, 2, chunks + 8, chunk1, object + 8, 0x30000000, serial + 1};
    for (std::size_t at = 0; at < sizeof(watchedAddresses) / sizeof(watchedAddresses[0]); ++at)
    {
        Fixture fixture;
        fixture.changeAt = watchedAddresses[at];
        fixture.changeValue = changes[at];
        fixture.changeBytes = sizes[at];
        Refuses(fixture, "change during capture is rejected by dependency fence");
    }
    for (std::size_t at = 0; at < sizeof(watchedAddresses) / sizeof(watchedAddresses[0]); ++at)
    {
        Fixture          fixture;
        Access           access(image, Fixture::Read);
        Access::Snapshot value{};
        Check(access.Capture(object, value), "prepare immediate identity recheck");
        fixture.PutBytes(watchedAddresses[at], &changes[at], sizes[at]);
        Check(!access.Recheck(value), "every consumed dependency change invalidates prior identity");
    }
    for (const std::uint32_t flags : {0x10000000u, 0x20000000u, 0x30000000u})
    {
        Fixture fixture;
        fixture.Put(fixture.item + 8, flags);
        Refuses(fixture, "native weak unreachable/pending-kill mask rejected");
    }
    {
        Fixture fixture;
        fixture.Put(fixture.item + 8, std::uint32_t{0x08000000});
        Access::Snapshot value{};
        Check(Access(image, Fixture::Read).Capture(object, value), "unrelated native item flag is preserved");
    }
    for (const std::int32_t index : {-1, (std::numeric_limits<std::int32_t>::max)()})
    {
        Fixture fixture;
        fixture.Put(object + 0xc, index);
        Refuses(fixture, "bad signed native object index refused");
    }
    {
        Fixture fixture;
        fixture.Put(objectCount, std::int32_t{0});
        Refuses(fixture, "empty native registry refuses");
    }
    {
        Fixture fixture;
        fixture.Put(registry, (std::numeric_limits<std::uintptr_t>::max)() - 3);
        Refuses(fixture, "corrupt root and arithmetic overflow refuse");
    }
    {
        Fixture fixture;
        auto    value = Sentinel();
        Check(!Access(image, nullptr).Capture(object, value) && Untouched(value), "missing checked reader preserves output");
        Check(!Access(image, Fixture::Read).Capture(0, value) && Untouched(value), "null object preserves output");
        Check(!Access(0, Fixture::Read).Capture(object, value) && Untouched(value), "null image preserves output");
        Check(!Access(image, Fixture::Read).Recheck(value) && !Access::MatchesWeak(value, 123, 456), "uncaptured user fields confer no membership");
    }
    std::printf("Passed %u native UObject-membership checks\n", checks);
    std::puts("Immediate raw identity, strict weak serials, chunk boundaries and complete dependency bookends checked");
    return EXIT_SUCCESS;
}
