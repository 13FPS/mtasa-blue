/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise native startup provider reads over owned mock memory
 *****************************************************************************/
#include "../../Client/game_de/NativeStartupProvidersAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Access = mta::game_de::NativeStartupProvidersAccess;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u startup-provider checks: %s\n", checks, reason);
            std::exit(EXIT_FAILURE);
        }
    }
    constexpr std::uintptr_t image = 0x10000000, world = 0x20000000, weak = 0x21000000, chunks = 0x22000000;
    constexpr std::uintptr_t chunk0 = 0x23000000, chunk1 = 0x24000000, controller = 0x25000000, engine = 0x26000000;
    constexpr std::uintptr_t viewport = 0x27000000, manager = 0x28000000;
    constexpr std::uintptr_t pcBase = 0x30000000, pcDerived = 0x30001000, vpBase = 0x31000000, vpDerived = 0x31001000;
    constexpr std::uintptr_t pcChain = 0x32000000, vpChain = 0x32001000;
    constexpr std::uintptr_t pcCache = image + 0x5727ca8, vpCache = image + 0x5728948, engineRoot = image + 0x57d7f70;
    constexpr std::uintptr_t registryRoot = image + 0x5087380, registryCount = image + 0x5087394;
    constexpr std::uint32_t  serial = 0x1234;
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
        unsigned            reads = 0, failAt = 0, weakReads = 0;
        std::uintptr_t      deny = 0, mutateAt = 0;
        unsigned            mutationRead = 2, watchedReads = 0;
        std::uint64_t       mutationValue = 0;
        std::size_t         mutationBytes = 8;
        std::uintptr_t      item = 0;
        void                Add(std::uintptr_t address, std::size_t bytes) { regions.push_back({address, std::vector<unsigned char>(bytes)}); }
        template <typename T>
        void Put(std::uintptr_t address, T value)
        {
            for (auto& region : regions)
                if (address >= region.address && address - region.address <= region.bytes.size() &&
                    sizeof(value) <= region.bytes.size() - (address - region.address))
                {
                    std::memcpy(region.bytes.data() + address - region.address, &value, sizeof(value));
                    return;
                }
            std::abort();
        }
        explicit Fixture(std::int32_t index = 0)
        {
            current = this;
            item = (index >= 65536 ? chunk1 : chunk0) + (static_cast<std::uint32_t>(index) & 65535u) * 24u;
            for (const auto address : {pcCache, vpCache, engineRoot, registryRoot})
                Add(address, 8);
            Add(registryCount, 4);
            Add(world, 0x1d0);
            Add(weak, 8);
            Add(chunks, 16);
            Add(item, 24);
            Add(controller, 0x40);
            Add(engine, 0x788);
            Add(viewport, 0x368);
            for (const auto address : {pcBase, pcDerived, vpBase, vpDerived})
                Add(address, 0x40);
            Add(pcChain, 24);
            Add(vpChain, 24);
            Put(pcCache, pcBase);
            Put(vpCache, vpBase);
            Put(engineRoot, engine);
            Put(registryRoot, chunks);
            Put(registryCount, index + 1);
            Put(world + 0x1c0, weak);
            Put(world + 0x1c8, std::int32_t{1});
            Put(weak, index);
            Put(weak + 4, serial);
            Put(chunks, chunk0);
            Put(chunks + 8, chunk1);
            Put(item, controller);
            Put(item + 0x10, serial);
            // Vptrs are deliberate unrelated decoys. Only native UClass-chain
            // relations may qualify these declared provider subclasses.
            Put(controller, std::uintptr_t{0x76543210});
            Put(controller + 0x10, pcDerived);
            Put(engine + 0x780, viewport);
            Put(viewport, std::uintptr_t{0x76543218});
            Put(viewport + 0x10, vpDerived);
            Put(viewport + 0x360, manager);
            Put(pcBase + 0x38, std::int32_t{1});
            Put(pcDerived + 0x38, std::int32_t{2});
            Put(pcDerived + 0x30, pcChain);
            Put(pcChain + 8, pcBase + 0x30);
            Put(vpBase + 0x38, std::int32_t{1});
            Put(vpDerived + 0x38, std::int32_t{2});
            Put(vpDerived + 0x30, vpChain);
            Put(vpChain + 8, vpBase + 0x30);
        }
        bool Read(std::uintptr_t address, void* output, std::size_t bytes) noexcept
        {
            ++reads;
            if (address == weak)
                ++weakReads;
            for (const auto& region : regions)
                if (address >= region.address && address - region.address <= region.bytes.size() && bytes <= region.bytes.size() - (address - region.address))
                {
                    if (reads == failAt || address == deny)
                        break;
                    std::memcpy(output, region.bytes.data() + address - region.address, bytes);
                    if (address == mutateAt && ++watchedReads == mutationRead)
                    {
                        // A field can be consumed in a 12-byte header. Mutate
                        // its pointer without reading beyond this 8-byte value.
                        std::memcpy(output, &mutationValue, bytes < mutationBytes ? bytes : mutationBytes);
                    }
                    return true;
                }
            std::memset(output, 0xa7, bytes);
            return false;
        }
    };
    bool Read(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        return current && current->Read(address, output, bytes);
    }
    Access::Value Sentinel()
    {
        return {0xa5, 0x5a, 0xa55a};
    }
    void Refused(Fixture& fixture, const char* reason, std::uintptr_t owner = world)
    {
        const auto before = Sentinel();
        auto       value = before;
        Check(!Access(image, &Read).Read(owner, value) && value.controllerAddress == before.controllerAddress &&
                  value.viewportAddress == before.viewportAddress && value.managerAddress == before.managerAddress,
              reason);
        Check(fixture.reads < 1000, "corrupt provider data cannot consume unbounded native callback work");
    }
}
int main()
{
    unsigned successfulReads = 0;
    for (const auto index : {0, 65535, 65536, 65537})
    {
        Fixture fixture(index);
        auto    value = Sentinel();
        Check(Access(image, &Read).Read(world, value) && value.controllerAddress == controller && value.viewportAddress == viewport &&
                  value.managerAddress == manager,
              "resolve the first weak controller across real chunk boundaries and retain the already cached viewport manager");
        successfulReads = fixture.reads;
    }
    for (const auto index : {-1, (std::numeric_limits<std::int32_t>::max)()})
    {
        Fixture fixture;
        fixture.Put(weak, index);
        Refused(fixture, "negative or registry-out-of-range weak indices cannot become raw object pointers");
    }
    for (const auto flags : {0x10000000u, 0x20000000u, 0x30000000u})
    {
        Fixture fixture;
        fixture.Put(fixture.item + 8, flags);
        Refused(fixture, "both native weak-resolution reject bits independently invalidate a registry item");
    }
    for (const auto mask : {0u, 0x00800000u})
    {
        Fixture fixture;
        fixture.Put(fixture.item + 8, mask);
        auto value = Sentinel();
        Check(Access(image, &Read).Read(world, value), "unrelated item flags cannot be invented as extra native rejection masks");
    }
    for (const auto count : {0, -1})
    {
        Fixture fixture;
        fixture.Put(world + 0x1c8, count);
        Refused(fixture, "nonpositive controller counts cannot read an absent first element");
        Check(!fixture.weakReads, "nonpositive counts do not dereference the weak record");
    }
    {
        Fixture fixture;
        fixture.Put(world + 0x1c8, (std::numeric_limits<std::int32_t>::max)());
        auto value = Sentinel();
        Check(Access(image, &Read).Read(world, value), "the first-record reader cannot invent an unproved native array capacity");
    }
    for (const auto address : {pcCache, vpCache, engineRoot, registryRoot, chunks, world + 0x1c0, engine + 0x780, viewport + 0x360, std::uintptr_t{0}})
    {
        Fixture fixture;
        if (address)
            fixture.Put(address, std::uintptr_t{0});
        else
            fixture.Put(fixture.item, std::uintptr_t{0});
        Refused(fixture, "missing caches, roots or providers preserve the entire output without invoking any lazy initializer");
    }
    for (const auto address : {weak + 4, std::uintptr_t{0}})
    {
        Fixture fixture;
        fixture.Put(address ? address : fixture.item + 0x10, std::uint32_t{0});
        Refused(fixture, "null or mismatched weak serials cannot qualify a recycled registry slot");
    }
    {
        Fixture fixture;
        fixture.Put(fixture.item + 0x10, serial + 1);
        Refused(fixture, "a different nonzero serial still identifies a recycled weak object slot");
    }
    for (const auto address : {pcChain + 8, vpChain + 8})
    {
        Fixture fixture;
        fixture.Put(address, address == pcChain + 8 ? pcBase : vpBase);
        Refused(fixture, "class pointers and vptr decoys cannot replace the native embedded class-chain token");
    }
    for (const auto address : {pcCache, vpCache, engineRoot, registryRoot, world + 0x1c0, engine + 0x780, viewport + 0x360, controller + 0x10, viewport + 0x10,
                               pcChain + 8, vpChain + 8})
    {
        Fixture fixture;
        fixture.mutateAt = address;
        Refused(fixture, "a changing consumed provider dependency cannot publish a mixed pair of graph observations");
    }
    for (unsigned failure = 1; failure <= successfulReads; ++failure)
    {
        Fixture fixture(65537);
        fixture.failAt = failure;
        Refused(fixture, "every guarded read failure, including partial output writes, preserves all caller fields");
    }
    {
        Fixture fixture;
        fixture.Put(world + 0x1c0, (std::numeric_limits<std::uintptr_t>::max)() - 3);
        Refused(fixture, "weak record address overflow is refused before attempting a wrapping read");
    }
    {
        Fixture fixture;
        Refused(fixture, "a world field address overflow cannot borrow unrelated low memory", (std::numeric_limits<std::uintptr_t>::max)() - 1);
        auto value = Sentinel();
        Check(!Access(0, &Read).Read(world, value) && !Access(image, nullptr).Read(world, value), "an absent image or guarded reader cannot inspect providers");
    }
    std::printf("Passed %u native startup-provider checks\n", checks);
    std::puts("Weak registry serials, declared subclasses, cached viewport manager and complete path coherence checked");
}
