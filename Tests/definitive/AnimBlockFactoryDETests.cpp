/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual block products, opaque roundtrips and polymorphic CPU lifetime
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlockRequestDE.h"
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace mta::game_de;
namespace animation_block_factory_tests
{
    unsigned checks = 0, gameCalls = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    [[noreturn]] inline void UnqualifiedGame()
    {
        ++gameCalls;
        std::fputs("Unexpected game provider call in CPU block factory fixture\n", stderr);
        std::abort();
    }
    bool Same(const NativeAnimationBlockIdentity& a, const NativeAnimationBlockIdentity& b) noexcept
    {
        return a.index == b.index && a.name == b.name && a.ownerGeneration == b.ownerGeneration;
    }
}
#include "AnimationBlockFactoryGameFixture.generated.h"
#include "AnimationBlockFactoryFixture.generated.h"

namespace animation_block_factory_tests
{
    constexpr std::uint64_t      Generation = 0xfedcba9876543210ull;
    NativeAnimationBlockIdentity Identity()
    {
        NativeAnimationBlockIdentity id;
        id.index = 17;
        id.ownerGeneration = Generation;
        id.name = {'p', 'E', 'd', 0, 0x81, 0x82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfe};
        return id;
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationBlockAccess
    {
        NativeAnimationBlockIdentity identity = Identity(), received{};
        NativeAnimationBlockName     query{};
        alignas(std::uintptr_t) std::array<unsigned char, 32> row{};
        alignas(std::uintptr_t) std::array<unsigned char, 40> hierarchy{};
        unsigned       lookups = 0, indices = 0, interfaces = 0, names = 0, hierarchies = 0, metadata = 0, adds = 0, refsReads = 0;
        std::uint64_t  generation = 0;
        std::uint32_t  requestedIndex = 0;
        std::uintptr_t locator = 0;
        std::size_t    animation = 0;
        std::int32_t   foundIndex = 17;
        std::uint16_t  refs = 7;
        bool           loaded = false, queryStable = true;
        ClockResult    lookupStatus = ClockResult::Ok, indexStatus = ClockResult::Ok, interfaceStatus = ClockResult::Ok;
        ClockResult    nameStatus = ClockResult::Ok, hierarchyStatus = ClockResult::Ok;
        char*          callerName = nullptr;
        Owner()
        {
            for (std::size_t i = 0; i < identity.name.size(); ++i)
                row[i] = identity.name[i];
        }
        std::uintptr_t Address() noexcept { return reinterpret_cast<std::uintptr_t>(row.data()); }
        unsigned       Calls() const noexcept { return lookups + indices + interfaces + names + hierarchies + metadata + adds + refsReads; }
        ClockResult    FindAnimationBlockIndex(std::uint64_t value, const NativeAnimationBlockName& input, std::int32_t& output) noexcept override
        {
            ++lookups;
            generation = value;
            query = input;
            const auto before = input;
            if (callerName)
                callerName[0] = 'X';
            queryStable = queryStable && input == before;
            if (value != Generation)
                return ClockResult::BindingChanged;
            output = foundIndex;
            return lookupStatus;
        }
        ClockResult ReadAnimationBlockIdentity(std::uint64_t value, std::uint32_t index, NativeAnimationBlockIdentity& output) noexcept override
        {
            ++indices;
            generation = value;
            requestedIndex = index;
            output = identity;
            return value == Generation ? indexStatus : ClockResult::BindingChanged;
        }
        ClockResult ReadAnimationBlockInterfaceIdentity(std::uint64_t value, std::uintptr_t input, NativeAnimationBlockIdentity& output) noexcept override
        {
            ++interfaces;
            generation = value;
            locator = input;
            if (value != Generation)
                return ClockResult::BindingChanged;
            if (input != Address())
                return ClockResult::InvalidArgument;
            output = identity;
            return interfaceStatus;
        }
        ClockResult ReadAnimationBlockName(const NativeAnimationBlockIdentity& input, char*& output) noexcept override
        {
            ++names;
            received = input;
            if (!Same(input, identity))
                return ClockResult::BindingChanged;
            output = reinterpret_cast<char*>(row.data());
            return nameStatus;
        }
        ClockResult ReadAnimationBlockHierarchy(const NativeAnimationBlockIdentity& input, std::size_t index, std::uintptr_t& output) noexcept override
        {
            ++hierarchies;
            received = input;
            animation = index;
            if (!Same(input, identity))
                return ClockResult::BindingChanged;
            output = loaded ? reinterpret_cast<std::uintptr_t>(hierarchy.data()) : 0;
            return hierarchyStatus;
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity& input, NativeAnimationBlockValues& output) noexcept override
        {
            ++metadata;
            received = input;
            if (!Same(input, identity))
                return ClockResult::BindingChanged;
            output = {identity.index, 2, 8, static_cast<std::uint8_t>(loaded ? 1 : 0)};
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationBlockRefs(const NativeAnimationBlockIdentity& input, std::uint16_t& output) noexcept override
        {
            ++refsReads;
            received = input;
            output = refs;
            return Same(input, identity) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity& input) noexcept override
        {
            ++adds;
            received = input;
            if (!Same(input, identity))
                return ClockResult::BindingChanged;
            refs = static_cast<std::uint16_t>(refs + 1u);
            return ClockResult::Ok;
        }
    };
    struct Lifetime
    {
        unsigned constructed = 0, destroyed = 0;
    };
    class TrackingBlock final : public CAnimBlockRequestDE
    {
    public:
        TrackingBlock(NativeAnimationBlockAccess& access, const NativeAnimationBlockIdentity& id, CGame& game, Lifetime& count)
            : CAnimBlockRequestDE(access, id, game), count(count)
        {
            ++count.constructed;
        }
        ~TrackingBlock() override { ++count.destroyed; }

    private:
        Lifetime& count;
    };
    template <class Operation>
    bool Refused(Operation operation, const char* method, ClockResult result)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message(error.what());
            return message.find(method) != std::string::npos && message.find(": " + std::to_string(static_cast<unsigned>(result))) != std::string::npos;
        }
        return false;
    }
    void OpaqueAndHierarchyChecks()
    {
        AnimationBlockFactoryGameFixture game;
        Owner                            owner;
        auto                             caller = owner.identity;
        Lifetime                         life;
        TrackingBlock                    product(owner, caller, game, life);
        caller = {};
        CAnimBlock& sdk = product;
        auto*       tag = sdk.GetInterface();
        Check(reinterpret_cast<std::uintptr_t>(tag) == owner.Address() && owner.names == 1 && Same(owner.received, owner.identity) && !owner.metadata &&
                  !owner.adds && !owner.refsReads && !gameCalls,
              "actual GetInterface publishes exactly the current ReadName-qualified rowbase as an opaque tag after copying identity");
        const auto huge = (std::numeric_limits<std::size_t>::max)();
        Check(sdk.GetAnimationHierarchyInterface(huge) == nullptr && owner.hierarchies == 1 && owner.animation == huge && !owner.metadata,
              "unloaded/null hierarchy preserves the complete requested size_t and does not invent a wrapper-side index gate");
        owner.loaded = true;
        Check(reinterpret_cast<std::uintptr_t>(sdk.GetAnimationHierarchyInterface(3)) == reinterpret_cast<std::uintptr_t>(owner.hierarchy.data()) &&
                  owner.hierarchies == 2 && owner.animation == 3 && Same(owner.received, owner.identity) && !owner.metadata,
              "loaded hierarchy address crosses actual SDK dispatch as an opaque DE tag without classic sizeof or extra metadata reads");
        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.nameStatus = owner.hierarchyStatus = status;
            const auto beforeName = owner.names, beforeHierarchy = owner.hierarchies;
            Check(Refused([&] { (void)sdk.GetInterface(); }, "GetInterface", status) && owner.names == beforeName + 1,
                  "each interface owner refusal propagates once rather than publishing refused pointer storage");
            Check(Refused([&] { (void)sdk.GetAnimationHierarchyInterface(3); }, "GetAnimationHierarchyInterface", status) &&
                      owner.hierarchies == beforeHierarchy + 1,
                  "each hierarchy owner refusal propagates once without retry or a synthetic null result");
        }
        struct Missing : NativeAnimationBlockAccess
        {
        } missing;
        TrackingBlock unavailable(missing, Identity(), game, life);
        Check(Refused([&] { (void)unavailable.GetInterface(); }, "GetInterface", ClockResult::NotInitialized) &&
                  Refused([&] { (void)unavailable.GetAnimationHierarchyInterface(0); }, "GetAnimationHierarchyInterface", ClockResult::NotInitialized),
              "unavailable opaque/hierarchy access retains mandatory explicit refusal");
        std::puts("Actual animation block opaque and hierarchy SDK fixture completed");
    }
    void LifetimeChecks()
    {
        AnimationBlockFactoryGameFixture game;
        Owner                            owner;
        Lifetime                         life;
        const auto                       before = owner.Calls();
        {
            std::unique_ptr<CAnimBlock> owned(new TrackingBlock(owner, owner.identity, game, life));
            Check(life.constructed == 1 && !life.destroyed && owner.Calls() == before && !gameCalls,
                  "owning only a derived CPU facade adds no native or game provider activity");
        }
        Check(life.destroyed == 1 && owner.Calls() == before && owner.refs == 7 && !gameCalls,
              "default unique_ptr<Base> destruction reaches the actual derived destructor once without native ref/free or game effects");
        std::puts("Actual animation block polymorphic CPU lifetime fixture completed");
    }
    void FactoryChecks()
    {
        AnimationBlockFactoryGameFixture     game;
        Owner                                owner;
        AnimationBlockFactoryManagerReceiver manager(owner, owner, game, Generation);
        CAnimManager&                        sdk = manager;
        Check(!owner.Calls() && !gameCalls, "factory construction only borrows explicit same-owner services and the rejecting game provider");
        char spelling[]{'P', 'e', 'D', 0};
        owner.callerName = spelling;
        auto block = sdk.GetAnimationBlock(spelling);
        owner.callerName = nullptr;
        Check(block && dynamic_cast<CAnimBlockRequestDE*>(block.get()) && owner.lookups == 1 && owner.indices == 1 && !owner.interfaces &&
                  owner.generation == Generation && owner.requestedIndex == 17 && owner.query[0] == 'P' && spelling[0] == 'X' && owner.queryStable &&
                  !owner.names && !owner.hierarchies && !owner.metadata && !owner.adds && !owner.refsReads && !gameCalls,
              "actual name factory copies its caller before callbacks and constructs only the real complete CPU block from current native identity");
        auto* tag = block->GetInterface();
        Check(reinterpret_cast<std::uintptr_t>(tag) == owner.Address() && Same(owner.received, owner.identity) && owner.received.name[15] == 0xfe,
              "factory identity retains actual complete row-name bytes and spelling instead of substituting the caller query");
        auto clone = sdk.GetAnimBlock(tag);
        Check(clone && clone.get() != block.get() && dynamic_cast<CAnimBlockRequestDE*>(clone.get()) && owner.interfaces == 1 &&
                  owner.locator == owner.Address() && owner.generation == Generation && !owner.adds && !owner.refsReads && !gameCalls,
              "actual raw-tag roundtrip re-admits current same-backend identity and creates a separate CPU facade without native refs or game calls");
        Check(clone->GetInterface() == tag, "common-client clone roundtrip preserves the exact current opaque row address");
        const auto beforeIntegerLookup = owner.lookups, beforeIntegerRead = owner.indices;
        auto       integerBlock = sdk.GetAnimationBlock(17);
        Check(integerBlock && dynamic_cast<CAnimBlockRequestDE*>(integerBlock.get()) && owner.lookups == beforeIntegerLookup &&
                  owner.indices == beforeIntegerRead + 1 && owner.requestedIndex == 17 && integerBlock->GetInterface() == tag && !owner.adds && !gameCalls,
              "integer factory re-admits its exact current row without a name lookup, cached count or native ref effect");
        const auto beforeNegative = owner.Calls();
        Check(Refused([&] { (void)sdk.GetAnimationBlock(-1); }, "GetAnimationBlock", ClockResult::InvalidArgument) && owner.Calls() == beforeNegative,
              "negative integer factory index refuses before native access instead of wrapping to another row");
        owner.indexStatus = ClockResult::InvalidArgument;
        const auto largest = (std::numeric_limits<int>::max)();
        Check(Refused([&] { (void)sdk.GetAnimationBlock(largest); }, "GetAnimationBlock", ClockResult::InvalidArgument) &&
                  owner.requestedIndex == static_cast<std::uint32_t>(largest),
              "out-of-current-range refusal preserves the complete nonnegative SDK index without clamping or pointer arithmetic");
        owner.indexStatus = ClockResult::Ok;
        const auto beforeNull = owner.Calls();
        Check(!sdk.GetAnimBlock(nullptr) && owner.Calls() == beforeNull && !gameCalls, "null raw clone returns before any owner or game access");
        owner.foundIndex = -1;
        const auto beforeMissing = owner.indices;
        Check(!sdk.GetAnimationBlock("missing") && owner.indices == beforeMissing && !gameCalls,
              "qualified missing name remains nullptr and never fabricates a block identity");
        owner.foundIndex = 17;
        for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                                  ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Expired, ClockResult::Stopped})
        {
            owner.indexStatus = status;
            const auto before = owner.indices;
            Check(Refused([&] { (void)sdk.GetAnimationBlock("ped"); }, "GetAnimationBlock", status) && owner.indices == before + 1 && !gameCalls,
                  "name factory current-row refusal remains an exact operation/code instead of a missing-name fallback");
            owner.indexStatus = ClockResult::Ok;
            owner.interfaceStatus = status;
            const auto beforeInterface = owner.interfaces;
            Check(Refused([&] { (void)sdk.GetAnimBlock(tag); }, "GetAnimBlock", status) && owner.interfaces == beforeInterface + 1 && !gameCalls,
                  "raw-tag factory refusal is never turned into a success stub or null block");
            owner.interfaceStatus = ClockResult::Ok;
        }
        auto expected = owner.identity;
        owner.identity.ownerGeneration = Generation - 1;
        Check(Refused([&] { (void)sdk.GetAnimationBlock("ped"); }, "GetAnimationBlock", ClockResult::BindingChanged),
              "name factory rejects another callback generation before allocation");
        owner.identity = expected;
        owner.identity.index = 18;
        Check(Refused([&] { (void)sdk.GetAnimationBlock("ped"); }, "GetAnimationBlock", ClockResult::BindingChanged),
              "name factory rejects another selected row index");
        owner.identity = expected;
        owner.identity.name[0] = 'X';
        Check(Refused([&] { (void)sdk.GetAnimationBlock("ped"); }, "GetAnimationBlock", ClockResult::BindingChanged),
              "row replacement between lookup and identity read cannot silently select another name");
        owner.identity = expected;
        owner.identity.name.fill('x');
        Check(Refused([&] { (void)sdk.GetAnimBlock(tag); }, "GetAnimBlock", ClockResult::BindingChanged),
              "raw factory rejects unterminated current identity without adopting an unqualified interface");
        owner.identity = expected;
        Check(Refused([&] { (void)sdk.GetAnimBlock(reinterpret_cast<CAnimBlockSAInterface*>(owner.Address() + 1)); }, "GetAnimBlock",
                      ClockResult::InvalidArgument),
              "foreign or misaligned opaque locator refusal crosses the raw factory without pointer dereference");
        Owner      other;
        bool       mismatch = false;
        const auto beforeMismatch = owner.Calls();
        try
        {
            AnimationBlockFactoryManagerReceiver wrong(owner, other, game, Generation);
        }
        catch (const std::invalid_argument&)
        {
            mismatch = true;
        }
        Check(mismatch && owner.Calls() == beforeMismatch && !other.Calls() && !gameCalls,
              "different complete CPU owners cannot be mixed merely because native image or generation values might agree");
        AnimationBlockFactoryManagerReceiver observationOnly(owner, Generation);
        CAnimManager&                        withoutProvider = observationOnly;
        const auto                           beforeProvider = owner.Calls();
        Check(Refused([&] { (void)withoutProvider.GetAnimationBlock("ped"); }, "GetAnimationBlock", ClockResult::NotInitialized) &&
                  Refused([&] { (void)withoutProvider.GetAnimBlock(tag); }, "GetAnimBlock", ClockResult::NotInitialized) &&
                  Refused([&] { (void)withoutProvider.GetAnimationBlock(17); }, "GetAnimationBlock", ClockResult::NotInitialized) &&
                  !withoutProvider.GetAnimBlock(nullptr) && owner.Calls() == beforeProvider && !gameCalls,
              "observation-only manager preserves its constructor while refusing factory products without a genuine borrowed game provider");
        const auto beforeDestroy = owner.Calls();
        block.reset();
        clone.reset();
        integerBlock.reset();
        Check(owner.Calls() == beforeDestroy && owner.refs == 7 && !gameCalls, "destroying actual manager-issued unique_ptr products retires CPU facades only");
        std::puts("Actual animation block factory SDK fixture completed");
    }
}
static_assert(std::has_virtual_destructor_v<CAnimBlock>);
static_assert(std::is_abstract_v<CAnimBlockDE> && !std::is_abstract_v<CAnimBlockRequestDE> && std::is_abstract_v<CAnimManagerDE>);
int main()
{
    try
    {
        using namespace animation_block_factory_tests;
        OpaqueAndHierarchyChecks();
        LifetimeChecks();
        FactoryChecks();
        std::printf("Passed %u animation block factory SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", animation_block_factory_tests::checks, error.what());
        return 1;
    }
}
