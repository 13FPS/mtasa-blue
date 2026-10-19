/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native world prerequisite observations over owned guarded buffers
 *****************************************************************************/
#include "../../Client/game_de/NativeStartupWorldAccess.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

namespace
{
    using Access = mta::game_de::NativeStartupWorldAccess;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u native startup-world checks: %s\n", checks, reason);
            std::exit(EXIT_FAILURE);
        }
    }
    bool Equal(const Access::Value& a, const Access::Value& b)
    {
        return a.outerAddress == b.outerAddress && a.levelAddress == b.levelAddress && a.worldAddress == b.worldAddress;
    }

    // Literal offsets and sparse virtual addresses express the native contract
    // independently. No virtual address is cast or dereferenced by the fixture.
    constexpr std::uintptr_t image = 0x10000000, owner = 0x20000000, outer = 0x20001000, level = 0x20002000, world = 0xdeadbeef;
    constexpr std::uintptr_t chunks = 0x21000000, chunk0 = 0x22000000, chunk1 = 0x23000000;
    constexpr std::uintptr_t levelClass = 0x24000000, wrongClass = 0x24001000, derivedClass = 0x24002000, derivedChain = 0x25002000;
    constexpr std::uintptr_t item0 = chunk0 + 7 * 24;
    constexpr Access::Value  sentinel{0x11110000, 0x22220000, 0x33330000};

    struct Fixture;
    Fixture* current = nullptr;
    struct Fixture
    {
        struct Segment
        {
            std::uintptr_t             address;
            std::vector<unsigned char> bytes;
        };
        std::vector<Segment> segments;
        unsigned             reads = 0, denied = 0, mutationReads = 0;
        std::uintptr_t       deniedAddress = 0, mutationAddress = 0;
        std::size_t          mutationByte = 0;
        unsigned             mutationHit = 2;
        bool                 mutationPersists = false;

        Fixture()
        {
            current = this;
            Map(image + 0x4203b20, 8);
            Map(image + 0x57e16f0, 8);
            Map(image + 0x5087380, 0x18);
            Map(owner, 0x40);
            Map(outer, 0xe0);
            Map(level, 0xe0);
            Map(chunks, 16);
            Map(chunk0, 512);
            Map(chunk1, 512);
            Map(levelClass, 0x40);
            Map(wrongClass, 0x40);
            Map(derivedClass, 0x40);
            Map(derivedChain, 512);
            Put(image + 0x4203b20, image + 0x30027f0);
            Put(image + 0x57e16f0, levelClass);
            Put(image + 0x5087380, chunks);
            Put(image + 0x5087394, std::int32_t{10});
            Put(owner + 0x20, outer);
            Put(outer + 0xc, std::int32_t{7});
            Put(outer + 0x10, wrongClass);
            Put(outer + 0x20, level);
            Put(level + 0x10, derivedClass);
            Put(level + 0xd0, world);
            Put(chunks, chunk0);
            Put(chunks + 8, chunk1);
            Put(item0, outer);
            Put(levelClass + 0x38, std::int32_t{2});
            Put(wrongClass + 0x38, std::int32_t{1});
            Put(derivedClass + 0x30, derivedChain);
            Put(derivedClass + 0x38, std::int32_t{3});
            Put(derivedChain + 2 * 8, levelClass + 0x30);
            // Candidate.Class is a metaclass decoy. The expected cache is the
            // UClass itself, with an intentionally opaque/null stored array.
            Put(derivedClass + 0x10, wrongClass);
        }
        void           Map(std::uintptr_t address, std::size_t bytes) { segments.push_back({address, std::vector<unsigned char>(bytes)}); }
        unsigned char* Find(std::uintptr_t at, std::size_t bytes)
        {
            for (auto& segment : segments)
                if (at >= segment.address && at - segment.address <= segment.bytes.size() && bytes <= segment.bytes.size() - (at - segment.address))
                    return segment.bytes.data() + (at - segment.address);
            return nullptr;
        }
        template <typename T>
        void Put(std::uintptr_t at, T value)
        {
            auto* destination = Find(at, sizeof(value));
            Check(destination != nullptr, "independent owned fixture field fits its mapped bytes");
            std::memcpy(destination, &value, sizeof(value));
        }
        bool Read(std::uintptr_t at, void* destination, std::size_t bytes) noexcept
        {
            ++reads;
            const auto* source = Find(at, bytes);
            if (!source || at == deniedAddress)
            {
                ++denied;
                // Guarded reads can fail after a partial local copy. None of
                // these bytes may overwrite the caller's previous result.
                std::memset(destination, 0xa7, bytes);
                return false;
            }
            std::memcpy(destination, source, bytes);
            if (at == mutationAddress)
            {
                ++mutationReads;
                if ((mutationReads == mutationHit || (mutationPersists && mutationReads >= mutationHit)) && mutationByte < bytes)
                    static_cast<unsigned char*>(destination)[mutationByte] ^= 1;
            }
            return true;
        }
        static bool GuardedRead(std::uintptr_t at, void* destination, std::size_t bytes) noexcept { return current && current->Read(at, destination, bytes); }
        bool        Observe(Access::Value& value, std::uintptr_t ownerAddress = owner, std::uintptr_t imageBase = image)
        {
            return Access(imageBase, GuardedRead).Read(ownerAddress, value);
        }
        void Refuses(const char* reason, std::uintptr_t ownerAddress = owner, std::uintptr_t imageBase = image)
        {
            auto value = sentinel;
            Check(!Observe(value, ownerAddress, imageBase) && Equal(value, sentinel), reason);
        }
        void ChunkOne()
        {
            Put(outer + 0xc, std::int32_t{65539});
            Put(image + 0x5087394, std::int32_t{65540});
            Put(chunk1 + 3 * 24, outer);
        }
        std::uintptr_t OuterPath(std::size_t depth)
        {
            auto previous = outer;
            for (std::size_t i = 1; i < depth; ++i)
            {
                const auto address = std::uintptr_t{0x26000000} + i * 0x1000;
                Map(address, 0xe0);
                Put(previous + 0x20, address);
                Put(address + 0x10, wrongClass);
                previous = address;
            }
            Put(previous + 0x10, derivedClass);
            Put(previous + 0x20, std::uintptr_t{0});
            Put(previous + 0xd0, world);
            return previous;
        }
    };

    void ContractAndSuccess()
    {
        namespace Profile = mta::game_de::NativeStartupWorldProfile;
        Check(Profile::getterRva == 0x30027f0 && Profile::typedOuterRva == 0x2fa9130 && Profile::ownerGetWorldSlotRva == 0x4203b20,
              "getter and typed-outer identities follow the sealed native route");
        Check(Profile::objectChunksRva == 0x5087380 && Profile::objectCountRva == 0x5087394 && Profile::levelClassCacheRva == 0x57e16f0,
              "registry globals and Level class cache are the actual image fields");
        Check(Profile::objectItemBytes == 24 && Profile::chunkShift == 16 && Profile::chunkMask == 0xffff && Profile::objectClassOffset == 0x10 &&
                  Profile::objectOuterOffset == 0x20 && Profile::levelWorldOffset == 0xd0,
              "native entry/chunk/type/outer/world layouts are fixed independently of classic GTA");
        for (const auto& pin : Profile::pins)
            Check(pin.bytes && pin.size && pin.size <= 171 && pin.rva, "all sealed profile pins are bounded and present");

        Fixture    fixture;
        const auto before = fixture.segments;
        auto       value = sentinel;
        Check(fixture.Observe(value) && Equal(value, {outer, level, world}), "native outer-to-Level route yields the nonnull raw world prerequisite");
        Check(!fixture.denied, "world and unrelated metaclass decoys are never dereferenced");
        Check(before.size() == fixture.segments.size(), "observation creates no fixture mappings");
        for (std::size_t i = 0; i < before.size(); ++i)
            Check(before[i].bytes == fixture.segments[i].bytes, "source bytes stay read-only");

        Fixture direct;
        direct.Put(outer + 0x10, derivedClass);
        direct.Put(outer + 0x20, std::uintptr_t{0});
        direct.Put(outer + 0xd0, world);
        Check(direct.Observe(value) && Equal(value, {outer, outer, world}), "typed immediate outer is itself the matching Level");

        Fixture firstSlot;
        firstSlot.Put(outer + 0xc, std::int32_t{0});
        firstSlot.Put(image + 0x5087394, std::int32_t{1});
        firstSlot.Put(chunk0, outer);
        Check(firstSlot.Observe(value) && Equal(value, {outer, level, world}), "first registry slot uses zero-based entry addressing");

        Fixture secondChunk;
        secondChunk.ChunkOne();
        Check(secondChunk.Observe(value) && Equal(value, {outer, level, world}), "index65539 selects chunk1 and its third slot using24-byte entries");

        Fixture    maximumPath;
        const auto last = maximumPath.OuterPath(64);
        Check(maximumPath.Observe(value) && Equal(value, {outer, last, world}), "64th outer fits the explicit observation budget");
    }

    void NativePrerequisiteRefusals()
    {
        for (const auto field : {owner + 8, outer + 8, item0 + 8})
        {
            Fixture fixture;
            fixture.Put(field, field == owner + 8 ? std::uint32_t{0x10} : (field == outer + 8 ? std::uint32_t{0x8000} : std::uint32_t{0x10000000}));
            fixture.Refuses("each exact native owner/outer/item rejection mask blocks the prerequisite");
        }
        // Other bits are not assigned guessed semantics from generic UE enum
        // names. A stable bit outside each actual native mask stays admitted.
        Fixture otherFlags;
        otherFlags.Put(owner + 8, std::uint32_t{1});
        otherFlags.Put(outer + 8, std::uint32_t{1});
        otherFlags.Put(item0 + 8, std::uint32_t{1});
        Access::Value value{};
        Check(otherFlags.Observe(value), "unrelated stable flag bits do not invent additional native masks");

        for (const auto index : {std::int32_t{-1}, std::int32_t{10}, (std::numeric_limits<std::int32_t>::max)()})
        {
            Fixture fixture;
            fixture.Put(outer + 0xc, index);
            fixture.Refuses("negative and out-of-range signed registry indices refuse");
        }
        for (const auto count : {std::int32_t{0}, std::int32_t{-1}})
        {
            Fixture fixture;
            fixture.Put(image + 0x5087394, count);
            fixture.Refuses("nonpositive signed registry count refuses before indexing");
        }
        for (const auto pointerField : {image + 0x57e16f0, owner + 0x20, image + 0x5087380, chunks, item0, outer + 0x10, level + 0xd0})
        {
            Fixture fixture;
            fixture.Put(pointerField, std::uintptr_t{0});
            fixture.Refuses("null type/owner/registry/entry/class/world prerequisites preserve the previous result");
        }
        Fixture identity;
        identity.Put(item0, level);
        identity.Refuses("extra conservative registry object identity check rejects a different object");
        Fixture wrongGetter;
        wrongGetter.Put(image + 0x4203b20, image + 0x30027f1);
        wrongGetter.Refuses("normalized getter slot cannot silently drift to another native function");
        Fixture wrongToken;
        wrongToken.Put(derivedChain + 16, levelClass);
        wrongToken.Refuses("UClass pointer is not the native embedded base-chain token");
        Fixture shallow;
        shallow.Put(derivedClass + 0x38, std::int32_t{1});
        shallow.Refuses("a candidate shallower than Level remains a stable nonmatch");
        Fixture malformed;
        malformed.Put(levelClass + 0x38, std::int32_t{-1});
        malformed.Refuses("malformed Level metadata is not a false subtype result");
        Fixture noLevel;
        noLevel.Put(outer + 0x20, std::uintptr_t{0});
        noLevel.Refuses("exhausted native outer chain has no world prerequisite");
        Fixture selfCycle;
        selfCycle.Put(outer + 0x20, outer);
        selfCycle.Refuses("a self-linked outer chain refuses without stalling");
        Fixture cycle;
        cycle.Put(level + 0x10, wrongClass);
        cycle.Put(level + 0x20, outer);
        cycle.Refuses("a longer cyclic outer chain refuses before revisiting a node");
        Fixture overBudget;
        overBudget.OuterPath(65);
        overBudget.Refuses("65th outer is a software-budget refusal rather than a claimed native capacity");
    }

    void ArithmeticAndReadRefusals()
    {
        Fixture zero;
        auto    value = sentinel;
        Check(!Access(image, nullptr).Read(owner, value) && Equal(value, sentinel), "missing guarded-read policy preserves output");
        zero.Refuses("null owner refuses", 0);
        zero.Refuses("null image base refuses", owner, 0);
        zero.Refuses("image global offset overflow refuses", owner, (std::numeric_limits<std::uintptr_t>::max)() - 0x30027f0);
        zero.Refuses("owner field offset overflow refuses", (std::numeric_limits<std::uintptr_t>::max)() - 7);
        Fixture chunkPointer;
        chunkPointer.Put(image + 0x5087380, (std::numeric_limits<std::uintptr_t>::max)() - 3);
        chunkPointer.Refuses("chunk-pointer copied span overflow refuses");
        Fixture chunkOffset;
        chunkOffset.ChunkOne();
        chunkOffset.Put(image + 0x5087380, (std::numeric_limits<std::uintptr_t>::max)() - 4);
        chunkOffset.Refuses("chunk-index address addition overflow refuses");
        Fixture itemOffset;
        itemOffset.Put(chunks, (std::numeric_limits<std::uintptr_t>::max)() - 16);
        itemOffset.Refuses("entry stride address addition overflow refuses");
        Fixture classHeader;
        classHeader.Put(outer + 0x10, (std::numeric_limits<std::uintptr_t>::max)() - 0x2f);
        classHeader.Refuses("class metadata address overflow refuses");
        Fixture classSlot;
        classSlot.Put(derivedClass + 0x30, (std::numeric_limits<std::uintptr_t>::max)() - 8);
        classSlot.Refuses("selected base-chain slot address overflow refuses");
        Fixture    worldOffset;
        const auto high = (std::numeric_limits<std::uintptr_t>::max)() - 0xc0;
        worldOffset.Map(high, 0x40);
        worldOffset.Put(owner + 0x20, high);
        worldOffset.Put(high + 0xc, std::int32_t{7});
        worldOffset.Put(high + 0x10, derivedClass);
        worldOffset.Put(item0, high);
        worldOffset.Refuses("matched Level world field address overflow refuses");

        for (const auto denied : {image + 0x4203b20, image + 0x57e16f0, owner + 8, outer + 8, outer + 0xc, image + 0x5087394, image + 0x5087380, chunks, item0,
                                  outer + 0x10, outer + 0x20, derivedClass + 0x30, levelClass + 0x30, derivedChain + 16, level + 0xd0})
        {
            Fixture fixture;
            fixture.deniedAddress = denied;
            fixture.Refuses("guarded failures on each distinct path dependency never publish partial output");
            Check(fixture.denied == 1, "read refusal stops the bounded observation immediately");
        }
    }

    void CoherentPathChecks()
    {
        for (const auto changed : {image + 0x4203b20, image + 0x57e16f0, image + 0x5087380, image + 0x5087394, owner + 8, owner + 0x20, outer + 8, outer + 0xc,
                                   chunks, item0, outer + 0x10, outer + 0x20, level + 0xd0})
        {
            Fixture fixture;
            fixture.mutationAddress = changed;
            fixture.Refuses("changes between full observed paths refuse even when masks remain numerically admissible");
        }
        Fixture itemFlags;
        itemFlags.mutationAddress = item0;
        itemFlags.mutationByte = 8;
        itemFlags.Refuses("non-rejecting item flag changes still invalidate the full path snapshot");
        for (const auto changed : {wrongClass + 0x30, levelClass + 0x30, derivedChain + 16})
        {
            Fixture fixture;
            fixture.mutationAddress = changed;
            fixture.Refuses("moving class headers or selected native tokens refuse within a type observation");
        }
        Fixture changedClassBetweenPaths;
        changedClassBetweenPaths.mutationAddress = wrongClass + 0x30;
        changedClassBetweenPaths.mutationByte = 8;
        changedClassBetweenPaths.mutationHit = 7;
        changedClassBetweenPaths.mutationPersists = true;
        changedClassBetweenPaths.Refuses("type metadata that remains a nonmatch but changes between whole paths is rejected");
        Fixture changedOpaqueBase;
        changedOpaqueBase.mutationAddress = levelClass + 0x30;
        changedOpaqueBase.mutationHit = 13;
        changedOpaqueBase.mutationPersists = true;
        changedOpaqueBase.Refuses("even opaque stored base-chain pointer changes invalidate whole-path coherence");
    }
}

int main()
{
    ContractAndSuccess();
    NativePrerequisiteRefusals();
    ArithmeticAndReadRefusals();
    CoherentPathChecks();
    std::printf("Passed %u native startup-world checks\n", checks);
    std::puts("Native getter routing, chunked registry identity, typed Level outer path, checked addresses, guarded refusals and coherent samples completed");
    return EXIT_SUCCESS;
}
