/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual group-owned child facades and their CPU-only retirement
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssocGroupDE.h"
#include "../../Client/game_de/NativeAnimationBlockAccess.h"
#include "../../Client/game_de/NativeAnimationStaticAssociationAccess.h"
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
namespace group_child_products_tests
{
    unsigned                checks = 0, gameCalls = 0;
    std::array<void*, 8>    watched{};
    std::array<unsigned, 8> deleted{};
    void                    Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    void Watch(void* address, std::size_t slot)
    {
        Check(address && slot < watched.size() && !watched[slot], "child allocation watch must have a live unique target");
        watched[slot] = address;
        deleted[slot] = 0;
    }
    void ObserveDelete(void* address) noexcept
    {
        for (std::size_t i = 0; i < watched.size(); ++i)
            if (address && watched[i] == address)
            {
                ++deleted[i];
                // Stop observing this address before a later allocation can
                // reuse it; CPU allocator reuse is not a child double-delete.
                watched[i] = nullptr;
            }
    }
    [[noreturn]] void UnqualifiedGame()
    {
        ++gameCalls;
        std::fputs("Unexpected game operation in group child-product fixture\n", stderr);
        std::abort();
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected, const char* operation)
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
            refused = text.find(operation) != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused, "child factory must preserve operation-specific owner refusal");
    }
    bool Same(const NativeAnimationAssociationGroupReferenceToken& a, const NativeAnimationAssociationGroupReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.index == b.index && a.locator == b.locator;
    }
    bool Same(const NativeAnimationBlockIdentity& a, const NativeAnimationBlockIdentity& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.index == b.index && a.name == b.name;
    }
    bool Same(const NativeAnimationStaticAssociationReferenceToken& a, const NativeAnimationStaticAssociationReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.groupIndex == b.groupIndex && a.index == b.index && a.locator == b.locator;
    }
}
// Observe only the CPU wrapper allocations returned by the actual group.
// Native rows below are caller-owned arrays and are never eligible for free.
void* operator new(std::size_t bytes)
{
    if (auto* value = std::malloc(bytes ? bytes : 1))
        return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept
{
    group_child_products_tests::ObserveDelete(value);
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept
{
    ::operator delete(value);
}
void* operator new[](std::size_t bytes)
{
    return ::operator new(bytes);
}
void operator delete[](void* value) noexcept
{
    ::operator delete(value);
}
void operator delete[](void* value, std::size_t) noexcept
{
    ::operator delete(value);
}
#include "AnimationAssociationGroupFixture.generated.h"
#include "AnimationAssociationGroupChildGameFixture.generated.h"
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::GetAnimBlock), decltype(&CAnimBlendAssocGroupDE::GetAnimBlock)>);
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::GetAnimation), decltype(&CAnimBlendAssocGroupDE::GetAnimation)>);
static_assert(!std::is_abstract_v<CAnimBlendAssocGroupDE> && !std::is_abstract_v<AnimationAssociationGroupReceiver>);
namespace group_child_products_tests
{
    constexpr std::uint64_t Generation = 0xfedcba9876543210ull;
    struct Owner final : NativeAnimationAssociationGroupAccess, NativeAnimationBlockAccess, NativeAnimationStaticAssociationAccess
    {
        alignas(std::uintptr_t) std::array<unsigned char, 32> groupRow{}, blockA{}, blockB{}, staticA{}, staticB{};
        NativeAnimationAssociationGroupReferenceToken                 group{Generation, 5, reinterpret_cast<std::uintptr_t>(groupRow.data())}, receivedGroup{};
        NativeAnimationAssociationGroupBlock                          block{};
        std::array<NativeAnimationStaticAssociationReferenceToken, 2> rows{
            NativeAnimationStaticAssociationReferenceToken{Generation, 5, 0, reinterpret_cast<std::uintptr_t>(staticA.data())},
            NativeAnimationStaticAssociationReferenceToken{Generation, 5, 1, reinterpret_cast<std::uintptr_t>(staticB.data())}};
        NativeAnimationStaticAssociationReferenceToken receivedStatic{};
        NativeAnimationBlockIdentity                   receivedBlock{};
        ClockResult                                    blockStatus = ClockResult::Ok, staticStatus = ClockResult::Ok, staticReadStatus = ClockResult::Ok;
        std::uint32_t                                  offset = 100, receivedId = 0;
        unsigned                                       blockLookups = 0, staticLookups = 0, blockReads = 0, staticReads = 0, adds = 0, scalarReads = 0;
        bool                                           badStaticGeneration = false, badStaticGroup = false, nullStaticLocator = false;
        Owner() { SelectBlock(false); }
        void SelectBlock(bool second)
        {
            block.interfaceAddress = reinterpret_cast<std::uintptr_t>((second ? blockB : blockA).data());
            block.identity = {};
            block.identity.index = second ? 11u : 7u;
            block.identity.ownerGeneration = Generation;
            block.identity.name = second ? NativeAnimationBlockName{'b', 'i', 'k', 'e', 0} : NativeAnimationBlockName{'p', 'e', 'd', 0};
            std::memcpy(reinterpret_cast<void*>(block.interfaceAddress), block.identity.name.data(), block.identity.name.size());
        }
        ClockResult ReadAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&,
                                                  NativeAnimationAssociationGroupValues&) noexcept override
        {
            ++scalarReads;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationAssociationGroupBlock(const NativeAnimationAssociationGroupReferenceToken& token,
                                                       NativeAnimationAssociationGroupBlock&                output) noexcept override
        {
            ++blockLookups;
            receivedGroup = token;
            output = block;
            return Same(token, group) ? blockStatus : ClockResult::OwnerConflict;
        }
        ClockResult ResolveAnimationAssociationGroupStaticById(const NativeAnimationAssociationGroupReferenceToken& token, std::uint32_t id,
                                                               NativeAnimationStaticAssociationReferenceToken& output) noexcept override
        {
            ++staticLookups;
            receivedGroup = token;
            receivedId = id;
            if (!Same(token, group))
                return ClockResult::OwnerConflict;
            const auto index = static_cast<std::uint32_t>(id - offset);
            if (index >= rows.size())
                return ClockResult::InvalidArgument;
            output = rows[index];
            if (badStaticGeneration)
                ++output.ownerGeneration;
            if (badStaticGroup)
                ++output.groupIndex;
            if (nullStaticLocator)
                output.locator = 0;
            return staticStatus;
        }
        ClockResult SetAnimationAssociationGroupIdOffset(const NativeAnimationAssociationGroupReferenceToken& token, std::int32_t value) noexcept override
        {
            if (!Same(token, group))
                return ClockResult::OwnerConflict;
            offset = static_cast<std::uint32_t>(value);
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity& identity, NativeAnimationBlockValues& output) noexcept override
        {
            ++blockReads;
            receivedBlock = identity;
            if (!Same(identity, block.identity))
                return ClockResult::BindingChanged;
            output = {identity.index, -7, 12, 1};
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationBlockName(const NativeAnimationBlockIdentity& identity, char*& output) noexcept override
        {
            ++blockReads;
            receivedBlock = identity;
            if (!Same(identity, block.identity))
                return ClockResult::BindingChanged;
            output = reinterpret_cast<char*>(block.interfaceAddress);
            return ClockResult::Ok;
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity&) noexcept override
        {
            ++adds;
            return ClockResult::WriteFailure;
        }
        ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken& token,
                                                   NativeAnimationStaticAssociationValues&               output) noexcept override
        {
            ++staticReads;
            receivedStatic = token;
            if (token.index >= rows.size() || !Same(token, rows[token.index]))
                return ClockResult::BindingChanged;
            output = {token.locator, 3, static_cast<std::int16_t>(700 + token.index), -7, -1, static_cast<std::uintptr_t>(0x10203040u)};
            return staticReadStatus;
        }
    };
    struct GroupOnly final : NativeAnimationAssociationGroupAccess
    {
        NativeAnimationAssociationGroupBlock           block{};
        NativeAnimationStaticAssociationReferenceToken row{};
        ClockResult                                    ReadAnimationAssociationGroupBlock(const NativeAnimationAssociationGroupReferenceToken&,
                                                                                          NativeAnimationAssociationGroupBlock& output) noexcept override
        {
            output = block;
            return ClockResult::Ok;
        }
        ClockResult ResolveAnimationAssociationGroupStaticById(const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t,
                                                               NativeAnimationStaticAssociationReferenceToken& output) noexcept override
        {
            output = row;
            return ClockResult::Ok;
        }
    };
    void ProductChecks()
    {
        Owner       owner;
        GameFixture game;
        auto        caller = owner.group;
        {
            AnimationAssociationGroupReceiver receiver(owner, caller, game);
            CAnimBlendAssocGroup&             sdk = receiver;
            caller = {};
            auto* first = sdk.GetAnimBlock();
            Check(first && first->GetIndex() == 7 && first->GetIDOffset() == -7 && first->IsLoaded(), "real block product must dispatch through its owner");
            Check(reinterpret_cast<std::uintptr_t>(first->GetInterface()) == owner.block.interfaceAddress && Same(owner.receivedBlock, owner.block.identity),
                  "block interface is an opaque native tag from the real block facade");
            Check(first == sdk.GetAnimBlock() && Same(owner.receivedGroup, owner.group),
                  "same current block pointer and immutable identity preserve CPU cache");
            Watch(first, 0);
            const auto originalIdentity = owner.block.identity;
            owner.block.identity.name[0] = 'X';
            Refuses([&] { (void)sdk.GetAnimBlock(); }, ClockResult::BindingChanged, "GetAnimBlock");
            Check(!deleted[0], "same-pointer identity drift cannot silently delete or renew the old borrowed facade");
            owner.block.identity = originalIdentity;
            ++owner.block.identity.ownerGeneration;
            Refuses([&] { (void)sdk.GetAnimBlock(); }, ClockResult::BindingChanged, "GetAnimBlock");
            owner.block.identity = originalIdentity;
            ++owner.block.identity.index;
            Refuses([&] { (void)sdk.GetAnimBlock(); }, ClockResult::BindingChanged, "GetAnimBlock");
            owner.block.identity = originalIdentity;
            owner.block.identity.name.fill('X');
            Refuses([&] { (void)sdk.GetAnimBlock(); }, ClockResult::BindingChanged, "GetAnimBlock");
            owner.block.identity = originalIdentity;
            Check(sdk.GetAnimBlock() == first, "restored exact identity still names the original cached CPU facade");
            owner.SelectBlock(true);
            auto* second = sdk.GetAnimBlock();
            Check(second && second->GetIndex() == 11 && reinterpret_cast<std::uintptr_t>(second->GetInterface()) == owner.block.interfaceAddress &&
                      deleted[0] == 1,
                  "a changed native block pointer retires only the previous CPU facade");
            Watch(second, 1);
            owner.block.interfaceAddress = 0;
            Check(!sdk.GetAnimBlock() && deleted[1] == 1, "source null block clears the cached CPU facade");
            owner.SelectBlock(false);
            first = sdk.GetAnimBlock();
            Watch(first, 4);
            auto* staticFirst = sdk.GetAnimation(100);
            auto* staticSecond = sdk.GetAnimation(101);
            Check(staticFirst && staticSecond && staticFirst != staticSecond && staticFirst == sdk.GetAnimation(100),
                  "physical row cache returns stable separate CPU facades");
            Check(reinterpret_cast<std::uintptr_t>(staticFirst) != owner.rows[0].locator &&
                      reinterpret_cast<std::uintptr_t>(staticSecond) != owner.rows[1].locator,
                  "native engine rows are never cast into SDK facade objects");
            Check(staticFirst->GetAnimID() == 700 && staticSecond->GetAnimID() == 701 && staticFirst->GetFlags() == -1,
                  "actual static SDK methods dispatch through the selected physical owner token");
            Watch(staticFirst, 2);
            Watch(staticSecond, 3);
            sdk.SetIDOffset(200);
            Check(sdk.GetAnimation(200) == staticFirst && staticFirst->GetAnimID() == 700 && sdk.GetAnimation(201) == staticSecond,
                  "offset changes affect new ID selection without invalidating existing physical CPU row facades");
            sdk.SetIDOffset(-1);
            Check(sdk.GetAnimation(0xffffffffu) == staticFirst && owner.receivedId == 0xffffffffu && !owner.scalarReads,
                  "GetAnimation passes full uint32 ID without a created-count, enum or manager precheck");
            for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                                       ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
            {
                owner.blockStatus = failure;
                Refuses([&] { (void)sdk.GetAnimBlock(); }, failure, "GetAnimBlock");
                owner.staticStatus = failure;
                Refuses([&] { (void)sdk.GetAnimation(0xffffffffu); }, failure, "GetAnimation");
            }
            owner.blockStatus = ClockResult::Ok;
            owner.staticStatus = ClockResult::Ok;
            owner.staticReadStatus = ClockResult::ReadFailure;
            Refuses([&] { (void)sdk.GetAnimation(0xffffffffu); }, ClockResult::ReadFailure, "GetAnimation");
            owner.staticReadStatus = ClockResult::Ok;
            owner.badStaticGeneration = true;
            Refuses([&] { (void)sdk.GetAnimation(0xffffffffu); }, ClockResult::BindingChanged, "GetAnimation");
            owner.badStaticGeneration = false;
            owner.badStaticGroup = true;
            Refuses([&] { (void)sdk.GetAnimation(0xffffffffu); }, ClockResult::BindingChanged, "GetAnimation");
            owner.badStaticGroup = false;
            owner.nullStaticLocator = true;
            Refuses([&] { (void)sdk.GetAnimation(0xffffffffu); }, ClockResult::BindingChanged, "GetAnimation");
            owner.nullStaticLocator = false;
            Check(sdk.GetAnimation(0xffffffffu) == staticFirst && !deleted[2] && !deleted[3] && !owner.adds && !gameCalls,
                  "factory refusals preserve existing cached products without game calls or native ref mutations");
        }
        Check(deleted[2] == 1 && deleted[3] == 1 && deleted[4] == 1 && !owner.adds && !gameCalls,
              "actual group destruction releases all cached CPU children without touching native row or block ownership");
        std::puts("Actual animation association group child CPU lifetime fixture completed");
        AnimationAssociationGroupReceiver withoutGame(owner, owner.group);
        Refuses([&] { (void)withoutGame.GetAnimBlock(); }, ClockResult::NotInitialized, "GetAnimBlock");
        owner.block.interfaceAddress = 0;
        Check(!withoutGame.GetAnimBlock(), "null source block does not require an unavailable game provider");
        GroupOnly                         missing;
        AnimationAssociationGroupReceiver missingServices(missing, owner.group, game);
        Check(!missingServices.GetAnimBlock(), "null source block also requires no optional block service");
        owner.SelectBlock(false);
        missing.block = owner.block;
        missing.row = owner.rows[0];
        Refuses([&] { (void)missingServices.GetAnimBlock(); }, ClockResult::NotInitialized, "GetAnimBlock");
        Refuses([&] { (void)missingServices.GetAnimation(0xffffffffu); }, ClockResult::NotInitialized, "GetAnimation");
        NativeAnimationAssociationGroupAccess absent;
        AnimationAssociationGroupReceiver     unconfigured(absent, owner.group, game);
        Refuses([&] { (void)unconfigured.GetAnimBlock(); }, ClockResult::NotInitialized, "GetAnimBlock");
        Refuses([&] { (void)unconfigured.GetAnimation(0); }, ClockResult::NotInitialized, "GetAnimation");
        Check(!gameCalls && !owner.adds && !owner.scalarReads, "child product construction/lifetime needs no successful game/native mutation body");
        std::puts("Actual animation association group child products SDK fixture completed");
    }
}
int main()
{
    try
    {
        group_child_products_tests::ProductChecks();
        std::printf("Passed %u association group child products SDK checks (%zu-bit; dispatch only, no native engine code)\n",
                    group_child_products_tests::checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u group child-product SDK checks: %s\n", group_child_products_tests::checks, error.what());
        return 1;
    }
}
