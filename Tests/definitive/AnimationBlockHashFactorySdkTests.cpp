/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual unsigned/block animation factory preserves hash semantics
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlockDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Block hash SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("GetAnimation(hash, block)") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    template <class Action>
    void Invalid(Action action)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::invalid_argument&)
        {
            refused = true;
        }
        Check(refused);
    }
    bool Equal(const NativeAnimationBlockIdentity& a, const NativeAnimationBlockIdentity& b)
    {
        return a.index == b.index && a.ownerGeneration == b.ownerGeneration && a.name == b.name;
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationBlockAccess, NativeAnimationHierarchyAccess
    {
        const std::uint64_t                    generation = (std::numeric_limits<std::uint64_t>::max)() - 19;
        const std::uintptr_t                   row = (std::numeric_limits<std::uintptr_t>::max)() - 0x5000;
        NativeAnimationBlockIdentity           identity{127, {'b', 'a', 'n', 'k', 0, 'x'}, generation}, received{};
        NativeAnimationHierarchyReferenceToken resolved{generation, row}, readToken{};
        std::uint32_t                          receivedHash = 0;
        std::uint64_t                          resolvedGeneration = 0;
        std::uintptr_t                         resolvedAddress = 0, found = row;
        ClockResult                            findStatus = ClockResult::Ok, resolveStatus = ClockResult::Ok;
        unsigned                               finds = 0, resolves = 0, forbidden = 0;
        bool                                   defaultFind = false;
        std::unique_ptr<CAnimBlock>*           mutateBlock = nullptr;
        ClockResult FindAnimationBlockHierarchy(const NativeAnimationBlockIdentity& expected, std::uint32_t hash, std::uintptr_t& output) noexcept override
        {
            ++finds;
            received = expected;
            receivedHash = hash;
            if (mutateBlock)
                mutateBlock->reset();
            if (defaultFind)
                return NativeAnimationBlockAccess::FindAnimationBlockHierarchy(expected, hash, output);
            if (findStatus == ClockResult::Ok)
                output = found;
            return findStatus;
        }
        ClockResult ResolveBankAnimationHierarchyReference(std::uint64_t gen, std::uintptr_t address,
                                                           NativeAnimationHierarchyReferenceToken& output) noexcept override
        {
            ++resolves;
            resolvedGeneration = gen;
            resolvedAddress = address;
            if (resolveStatus == ClockResult::Ok)
                output = resolved;
            return resolveStatus;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token, NativeAnimationHierarchyValues& output) noexcept override
        {
            readToken = token;
            output.interfaceAddress = row;
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationBlockHierarchy(const NativeAnimationBlockIdentity&, std::size_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity&, NativeAnimationBlockValues&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult DestroyPrivateAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
    class Block final : public CAnimBlockDE
    {
    public:
        Block(Owner& service, const NativeAnimationBlockIdentity& identity) : CAnimBlockDE(service, identity), owner(service) {}
        void                   Request(EModelRequestType, bool) override { std::abort(); }
        CAnimBlockSAInterface* GetInterface() override
        {
            ++owner.forbidden;
            return nullptr;
        }

    private:
        Owner& owner;
    };
}
int main()
{
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner, owner, owner.generation);
    std::unique_ptr<CAnimBlock>            block = std::make_unique<Block>(owner, owner.identity);
    for (const unsigned hash : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu})
    {
        auto result = manager.GetAnimation(hash, block);
        Check(result && owner.receivedHash == hash && Equal(owner.received, owner.identity));
        Check(owner.resolvedGeneration == owner.generation && owner.resolvedAddress == owner.row);
        Check(result->GetInterface() == reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.row));
        Check(owner.readToken.ownerGeneration == owner.generation && owner.readToken.locator == owner.row && owner.forbidden == 0);
    }
    const auto beforeMiss = owner.resolves;
    owner.found = 0;
    Check(!manager.GetAnimation(0xffffffffu, block));
    Check(owner.resolves == beforeMiss && owner.receivedHash == 0xffffffffu && owner.forbidden == 0);
    owner.found = owner.row;
    owner.mutateBlock = &block;
    auto copied = manager.GetAnimation(0x87654321u, block);
    Check(!block && copied && Equal(owner.received, owner.identity) && owner.receivedHash == 0x87654321u);
    owner.mutateBlock = nullptr;
    copied.reset();
    Check(owner.forbidden == 0);
    const auto beforeInvalid = owner.finds;
    Invalid([&] { manager.GetAnimation(7u, block); });
    Owner other;
    block = std::make_unique<Block>(other, owner.identity);
    Invalid([&] { manager.GetAnimation(7u, block); });
    auto stale = owner.identity;
    --stale.ownerGeneration;
    block = std::make_unique<Block>(owner, stale);
    Refuses([&] { manager.GetAnimation(7u, block); }, ClockResult::BindingChanged);
    Check(owner.finds == beforeInvalid);
    block = std::make_unique<Block>(owner, owner.identity);
    owner.defaultFind = true;
    Refuses([&] { manager.GetAnimation(7u, block); }, ClockResult::NotInitialized);
    owner.defaultFind = false;
    for (const auto failure :
         {ClockResult::NotInitialized, ClockResult::ReadFailure, ClockResult::BindingChanged, ClockResult::WrongThread, ClockResult::OwnerConflict})
    {
        owner.findStatus = failure;
        const auto before = owner.resolves;
        Refuses([&] { manager.GetAnimation(0xfedcba98u, block); }, failure);
        Check(owner.resolves == before && owner.receivedHash == 0xfedcba98u);
    }
    owner.findStatus = ClockResult::Ok;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::BindingChanged, ClockResult::ReadFailure})
    {
        owner.resolveStatus = failure;
        Refuses([&] { manager.GetAnimation(7u, block); }, failure);
    }
    owner.resolveStatus = ClockResult::Ok;
    const auto valid = owner.resolved;
    for (unsigned invalid = 0; invalid != 2; ++invalid)
    {
        owner.resolved = valid;
        if (invalid == 0)
            --owner.resolved.ownerGeneration;
        else
            --owner.resolved.locator;
        Refuses([&] { manager.GetAnimation(7u, block); }, ClockResult::BindingChanged);
    }
    owner.resolved = valid;
    PrivateAnimationFactoryManagerReceiver observer(owner, owner.generation);
    const auto                             beforeObserver = owner.finds;
    Refuses([&] { observer.GetAnimation(7u, block); }, ClockResult::NotInitialized);
    Check(owner.finds == beforeObserver && owner.forbidden == 0);
    Invalid(
        [&]
        {
            PrivateAnimationFactoryManagerReceiver wrong(owner, other, owner, owner.generation);
            (void)wrong;
        });
    Invalid(
        [&]
        {
            PrivateAnimationFactoryManagerReceiver wrong(owner, owner, other, owner.generation);
            (void)wrong;
        });
    std::puts("Actual animation block hash factory SDK fixture completed");
    std::printf("Passed %u block hash SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
