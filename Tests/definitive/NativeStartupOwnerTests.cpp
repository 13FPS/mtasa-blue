/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guarded native typed-outer ownership over owned byte buffers
 *****************************************************************************/
#include "../../Client/game_de/NativeStartupOwnerAccess.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace
{
    using Access = mta::game_de::NativeStartupOwnerAccess;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u startup-owner checks: %s\n", checks, reason);
            std::exit(EXIT_FAILURE);
        }
    }
    template <typename T, std::size_t N>
    void Put(std::array<unsigned char, N>& bytes, std::size_t offset, T value)
    {
        if (offset > N || sizeof(value) > N - offset)
            std::abort();
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
    constexpr std::uintptr_t image = 0x10000000, root = 0x20000000, nodes = 0x30000000;
    constexpr std::uintptr_t expectedClass = 0x40000000, derivedClass = 0x40001000, wrongClass = 0x40002000;
    constexpr std::uintptr_t derivedChain = 0x50000000, wrongChain = 0x50001000;
    constexpr std::uintptr_t derivedVtable = 0x60000000, secondDerivedVtable = 0x60001000;
    constexpr std::uintptr_t nativeTarget = image + 0x30027f0, cacheAddress = image + 0x5727430;
    constexpr std::uintptr_t NodeAddress(std::size_t index)
    {
        return nodes + index * 0x1000;
    }
    struct Fixture;
    Fixture* current = nullptr;
    struct Fixture
    {
        enum class Mutation
        {
            None,
            Cache,
            RootOuter,
            NodeOuter,
            NodeClass,
            TypeHeader,
            Token,
            Backreference,
            Vtable,
            VirtualTarget,
            BetweenPaths
        };
        std::array<unsigned char, 8>                     cache{};
        std::array<unsigned char, 0x300>                 rootObject{};
        std::array<std::array<unsigned char, 0x300>, 65> objects{};
        std::array<std::array<unsigned char, 0x40>, 3>   classes{};
        std::array<unsigned char, 3 * 8>                 derivedSlots{}, wrongSlots{};
        std::array<unsigned char, 0x160>                 vtable{}, otherVtable{};
        unsigned       reads = 0, denied = 0, failAt = 0, cacheReads = 0, rootOuterReads = 0, nodeOuterReads = 0, nodeClassReads = 0;
        unsigned       typeReads = 0, tokenReads = 0, backreferenceReads = 0, vtableReads = 0, virtualReads = 0, rootClassReads = 0;
        std::uintptr_t denyAddress = 0;
        Mutation       mutation = Mutation::None;
        std::size_t    length = 3;
        explicit Fixture(std::size_t count = 3) : length(count)
        {
            current = this;
            Put(cache, 0, expectedClass);
            Put(rootObject, 0x10, derivedClass);  // Deliberate root-type decoy.
            Put(rootObject, 0x20, count ? NodeAddress(0) : std::uintptr_t{0});
            Put(classes[0], 0x30, std::uintptr_t{0});
            Put(classes[0], 0x38, std::int32_t{1});
            Put(classes[1], 0x30, derivedChain);
            Put(classes[1], 0x38, std::int32_t{2});
            Put(classes[2], 0x30, wrongChain);
            Put(classes[2], 0x38, std::int32_t{2});
            Put(derivedSlots, 8, expectedClass + 0x30);
            Put(wrongSlots, 8, wrongClass + 0x30);
            Put(vtable, 0x158, nativeTarget);
            Put(otherVtable, 0x158, nativeTarget);
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                Put(objects[index], 0, derivedVtable);
                Put(objects[index], 0x10, index + 1 == count ? derivedClass : wrongClass);
                Put(objects[index], 0x20, index + 1 < count ? NodeAddress(index + 1) : std::uintptr_t{0});
                Put(objects[index], 0x2c0, root);
            }
        }
        template <std::size_t N>
        static const unsigned char* Region(std::uintptr_t at, std::size_t bytes, std::uintptr_t address, const std::array<unsigned char, N>& data)
        {
            return bytes <= N && at >= address && at - address <= N - bytes ? data.data() + (at - address) : nullptr;
        }
        bool Read(std::uintptr_t at, void* destination, std::size_t bytes) noexcept
        {
            ++reads;
            const unsigned char* source = Region(at, bytes, cacheAddress, cache);
            if (!source)
                source = Region(at, bytes, root, rootObject);
            for (std::size_t index = 0; !source && index < objects.size(); ++index)
                source = Region(at, bytes, NodeAddress(index), objects[index]);
            for (std::size_t index = 0; !source && index < classes.size(); ++index)
                source = Region(at, bytes, expectedClass + index * 0x1000, classes[index]);
            if (!source)
                source = Region(at, bytes, derivedChain, derivedSlots);
            if (!source)
                source = Region(at, bytes, wrongChain, wrongSlots);
            if (!source)
                source = Region(at, bytes, derivedVtable, vtable);
            if (!source)
                source = Region(at, bytes, secondDerivedVtable, otherVtable);
            if (!source || at == denyAddress || reads == failAt)
            {
                ++denied;
                std::memset(destination, 0xa7, bytes);
                return false;
            }
            std::memcpy(destination, source, bytes);
            const auto replace = [&](std::uintptr_t value)
            {
                if (bytes == 8)
                    std::memcpy(destination, &value, 8);
            };
            if (at == cacheAddress && ++cacheReads == 2 && mutation == Mutation::Cache)
                replace(0);
            if (at == root + 0x20 && ++rootOuterReads == 2 && mutation == Mutation::RootOuter)
                replace(NodeAddress(1));
            if (at == NodeAddress(0) + 0x20 && ++nodeOuterReads == 2 && mutation == Mutation::NodeOuter)
                replace(NodeAddress(2));
            if (at == NodeAddress(0) + 0x10 && ++nodeClassReads == 2 && mutation == Mutation::NodeClass)
                replace(derivedClass);
            if (at == root + 0x10)
                ++rootClassReads;
            if (at == derivedClass + 0x30 && bytes == 12)
            {
                ++typeReads;
                if ((mutation == Mutation::TypeHeader && typeReads == 2) || (mutation == Mutation::BetweenPaths && typeReads >= 8))
                {
                    const std::int32_t changed = 3;
                    std::memcpy(static_cast<unsigned char*>(destination) + 8, &changed, 4);
                }
            }
            if (at == derivedChain + 8 && ++tokenReads == 2 && mutation == Mutation::Token)
                replace(derivedClass + 0x30);
            if (length && at == NodeAddress(length - 1) + 0x2c0 && ++backreferenceReads == 2 && mutation == Mutation::Backreference)
                replace(0);
            if (length && at == NodeAddress(length - 1) && ++vtableReads == 2 && mutation == Mutation::Vtable)
                replace(secondDerivedVtable);
            if (at == derivedVtable + 0x158 && ++virtualReads == 2 && mutation == Mutation::VirtualTarget)
                replace(nativeTarget + 8);
            return true;
        }
        static bool GuardedRead(std::uintptr_t at, void* destination, std::size_t bytes) noexcept { return current && current->Read(at, destination, bytes); }
        bool        Observe(Access::Value& output, std::uintptr_t imageAddress = image, std::uintptr_t rootAddress = root) const
        {
            return Access(imageAddress, GuardedRead).Read(rootAddress, output);
        }
    };
    constexpr std::uintptr_t sentinel = 0x77770000;
    void                     Refused(Fixture& fixture, const char* reason)
    {
        Access::Value value{sentinel};
        Check(!fixture.Observe(value) && value.gameModeAddress == sentinel, reason);
    }
}

int main()
{
    static_assert(sizeof(std::uintptr_t) == 8);
    unsigned normalReads = 0;
    {
        Fixture       fixture;
        const auto    initialObjects = fixture.objects;
        Access::Value value{sentinel};
        Check(fixture.Observe(value) && value.gameModeAddress == NodeAddress(2),
              "first declared-derived enclosing GameMode matches with its own GetWorld target");
        Check(!fixture.rootClassReads && fixture.objects == initialObjects, "root's matching metadata is ignored and observations do not write object buffers");
        normalReads = fixture.reads;
    }
    {
        Fixture first;
        Put(first.objects[0], 0x10, derivedClass);
        Access::Value value{sentinel};
        Check(first.Observe(value) && value.gameModeAddress == NodeAddress(0), "first typed outer wins even when a later valid owner exists");
        Put(first.objects[0], 0x2c0, std::uintptr_t{0});
        Refused(first, "a first-match backreference failure cannot select a later matching owner");
    }
    {
        Fixture changedVptr;
        Put(changedVptr.objects[2], 0, secondDerivedVtable);
        Access::Value value{sentinel};
        Check(changedVptr.Observe(value) && value.gameModeAddress == NodeAddress(2),
              "declared subclasses with another vtable are admitted when GetWorld semantics match");
    }
    for (std::size_t length : {std::size_t{64}, std::size_t{65}})
    {
        Fixture       fixture(length);
        Access::Value value{sentinel};
        const bool    observed = fixture.Observe(value);
        Check(observed == (length == 64) && value.gameModeAddress == (length == 64 ? NodeAddress(63) : sentinel),
              "64-node policy admits its last node and refuses a 65-node path");
    }
    {
        Fixture none(0);
        Refused(none, "missing enclosing owner refuses without publishing null as readiness");
        Fixture cycle;
        Put(cycle.objects[2], 0x10, wrongClass);
        Put(cycle.objects[2], 0x20, NodeAddress(0));
        Refused(cycle, "cyclic outer path refuses boundedly");
        Check(cycle.reads < normalReads, "cycle refusal does not loop until an external timeout");
        Fixture self;
        Put(self.rootObject, 0x20, root);
        Refused(self, "a malformed self-outer cannot admit root as its own owner");
    }
    for (const auto address : {cacheAddress, root + 0x20, NodeAddress(0) + 0x20, NodeAddress(0) + 0x10, wrongClass + 0x30, expectedClass + 0x30, wrongChain + 8,
                               NodeAddress(2) + 0x2c0, NodeAddress(2), derivedVtable + 0x158})
    {
        Fixture denied;
        denied.denyAddress = address;
        Refused(denied, "read policy failure at a distinct owner/type/vtable prerequisite preserves output");
        Check(denied.denied == 1, "guarded refusal stops after the first partial failed read");
    }
    for (unsigned read = 1; read <= normalReads; ++read)
    {
        Fixture denied;
        denied.failAt = read;
        Refused(denied, "each guarded-copy boundary independently refuses without overwriting output");
    }
    for (const auto mutation : {Fixture::Mutation::Cache, Fixture::Mutation::RootOuter, Fixture::Mutation::NodeOuter, Fixture::Mutation::NodeClass,
                                Fixture::Mutation::TypeHeader, Fixture::Mutation::Token, Fixture::Mutation::Backreference, Fixture::Mutation::Vtable,
                                Fixture::Mutation::VirtualTarget, Fixture::Mutation::BetweenPaths})
    {
        Fixture unstable;
        unstable.mutation = mutation;
        Refused(unstable, "moving cache, nodes, class headers/tokens or owner dispatch cannot become a stable path");
    }
    {
        Fixture absentCache;
        Put(absentCache.cache, 0, std::uintptr_t{0});
        Refused(absentCache, "absent GameMode cache never invokes StaticClass to create metadata");
        Fixture badBackreference;
        Put(badBackreference.objects[2], 0x2c0, root + 0x1000);
        Refused(badBackreference, "separate owner/root correlation must match exactly");
        Fixture overridden;
        Put(overridden.vtable, 0x158, nativeTarget + 8);
        Refused(overridden, "an overridden GetWorld target refuses even with valid derived metadata");
        Fixture wrong;
        Put(wrong.objects[2], 0x10, wrongClass);
        Refused(wrong, "an exact known method target does not substitute for the declared GameMode class relation");
        Fixture badClass;
        Put(badClass.objects[2], 0x10, std::uintptr_t{0});
        Refused(badClass, "null candidate class refuses metadata traversal");
    }
    {
        Fixture       overflow;
        Access::Value value{sentinel};
        const auto    high = (std::numeric_limits<std::uintptr_t>::max)();
        Check(!overflow.Observe(value, high - 16) && value.gameModeAddress == sentinel && !overflow.reads,
              "image-relative cache addition overflow refuses before reading");
        Check(!overflow.Observe(value, image, high - 0x1f) && value.gameModeAddress == sentinel, "root outer field overflow refuses");
        Check(!Access(image, nullptr).Read(root, value) && value.gameModeAddress == sentinel, "missing guard refuses");
        Check(!overflow.Observe(value, image, 0) && value.gameModeAddress == sentinel, "null root refuses");
        Check(!overflow.Observe(value, 0) && value.gameModeAddress == sentinel, "null image base refuses");
        Put(overflow.rootObject, 0x20, high - 0x10);
        Refused(overflow, "outer-node field arithmetic cannot wrap into unrelated memory");
        Fixture badVtable;
        Put(badVtable.objects[2], 0, high - 0x100);
        Refused(badVtable, "virtual target slot arithmetic cannot wrap");
    }
    std::printf("Passed %u native startup-owner checks\n", checks);
    std::puts("Typed outer first-match, metadata subclasses, independent backreference, exact GetWorld target and full-path stability checked");
    return EXIT_SUCCESS;
}
