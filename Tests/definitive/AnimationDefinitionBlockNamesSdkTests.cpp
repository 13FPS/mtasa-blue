/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual definition block-name borrow and fixed-prefix lookup SDK routes
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <new>
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
            std::fprintf(stderr, "Definition block-name SDK check %u failed\n", checks);
            std::exit(1);
        }
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
        Check(refused);
    }
    struct Owner final : NativeAnimationManagerAccess
    {
        const std::uint64_t  generation = (std::numeric_limits<std::uint64_t>::max)() - 37;
        const std::uintptr_t address = (std::numeric_limits<std::uintptr_t>::max)() - 0x8000;
        std::uintptr_t       returnedName = address;
        std::uint64_t        nameGeneration = 0, findGeneration = 0;
        std::uint32_t        requestedId = 0, result = 0;
        ClockResult          nameStatus = ClockResult::Ok, findStatus = ClockResult::Ok;
        std::string          received;
        unsigned             names = 0, finds = 0, forbidden = 0;
        char*                mutateOriginal = nullptr;
        AssocGroupId*        mutateId = nullptr;
        ClockResult          ReadAnimationGroupBlockName(std::uint64_t gen, std::uint32_t id, std::uintptr_t& output) noexcept override
        {
            ++names;
            nameGeneration = gen;
            requestedId = id;
            if (mutateId)
                *mutateId = 0;
            output = returnedName;
            return nameStatus;
        }
        ClockResult FindFirstAnimationGroupByBlockName(std::uint64_t gen, const std::string& query, std::uint32_t& output) noexcept override
        {
            ++finds;
            findGeneration = gen;
            if (mutateOriginal)
                *mutateOriginal = '?';
            try
            {
                received = query;
            }
            catch (const std::bad_alloc&)
            {
                return ClockResult::ReadFailure;
            }
            output = result;
            return findStatus;
        }
        ClockResult ReadAnimationGroupName(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationAssocDefinitionCount(std::uint64_t, std::int32_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
}
int main()
{
    static_assert(NativeAnimationAssociationGroupPrefixCount == 118);
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner.generation);
    for (const auto id : {std::uint32_t{0}, std::uint32_t{118}, std::uint32_t{139}, std::uint32_t{200}, std::uint32_t{0x80000000u}, std::uint32_t{0xffffffffu}})
    {
        Check(manager.GetAnimBlockName(static_cast<AssocGroupId>(id)) == reinterpret_cast<const char*>(owner.address));
        Check(owner.requestedId == id && owner.nameGeneration == owner.generation);
    }
    AssocGroupId originalId = 0xfedcba98ul;
    owner.mutateId = &originalId;
    Check(manager.GetAnimBlockName(originalId) == reinterpret_cast<const char*>(owner.address));
    Check(!originalId && owner.requestedId == 0xfedcba98u);
    owner.mutateId = nullptr;
    const char* queries[] = {"", "miX", "miX\xe9\xff"};
    for (const auto* query : queries)
    {
        owner.result = 3;
        Check(manager.GetFirstAssocGroup(query) == 3);
        Check(owner.received == query && owner.findGeneration == owner.generation);
    }
    std::string longName(5001, 'q');
    longName.back() = static_cast<char>(0xe9);
    owner.result = 117;
    Check(manager.GetFirstAssocGroup(longName.c_str()) == 117 && owner.received == longName);
    char terminated[] = {'a', 0, 't', 'a', 'i', 'l', 0};
    Check(manager.GetFirstAssocGroup(terminated) == 117 && owner.received == "a");
    char copied[] = "preCallback";
    owner.mutateOriginal = copied;
    Check(manager.GetFirstAssocGroup(copied) == 117 && copied[0] == '?' && owner.received == "preCallback");
    owner.mutateOriginal = nullptr;
    owner.result = 0;
    Check(manager.GetFirstAssocGroup("first") == 0);
    owner.result = 118;
    Check(manager.GetFirstAssocGroup("missing") == 118);
    const auto beforeNull = owner.finds;
    bool       invalid = false;
    try
    {
        manager.GetFirstAssocGroup(nullptr);
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    Check(invalid && owner.finds == beforeNull);
    for (const auto bad : {std::uint32_t{119}, std::uint32_t{0xffffffffu}})
    {
        owner.result = bad;
        Refuses([&] { manager.GetFirstAssocGroup("bad"); }, ClockResult::BindingChanged, "GetFirstAssocGroup");
    }
    owner.result = 118;
    owner.returnedName = 0;
    Refuses([&] { manager.GetAnimBlockName(0); }, ClockResult::BindingChanged, "GetAnimBlockName");
    owner.returnedName = owner.address;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                               ClockResult::OwnerConflict, ClockResult::Busy})
    {
        owner.findStatus = failure;
        owner.nameStatus = failure;
        Refuses([&] { manager.GetFirstAssocGroup("query"); }, failure, "GetFirstAssocGroup");
        Refuses([&] { manager.GetAnimBlockName(0xfffffffful); }, failure, "GetAnimBlockName");
    }
    NativeAnimationManagerAccess           absent;
    PrivateAnimationFactoryManagerReceiver unconfigured(absent, owner.generation);
    Refuses([&] { unconfigured.GetFirstAssocGroup("query"); }, ClockResult::NotInitialized, "GetFirstAssocGroup");
    Refuses([&] { unconfigured.GetAnimBlockName(0); }, ClockResult::NotInitialized, "GetAnimBlockName");
    Check(owner.forbidden == 0);
    std::puts("Actual definition block-name SDK fixture completed");
    std::printf("Passed %u definition block-name SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
