/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual definition count/name SDK methods use distinct owner channels
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
            std::fprintf(stderr, "Animation definition SDK check %u failed\n", checks);
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
        const std::uint64_t  generation = (std::numeric_limits<std::uint64_t>::max)() - 31;
        const std::uintptr_t address = (std::numeric_limits<std::uintptr_t>::max)() - 0x7000;
        std::int32_t         count = 0;
        std::uintptr_t       returnedName = address;
        ClockResult          countStatus = ClockResult::Ok, nameStatus = ClockResult::Ok;
        std::uint64_t        countGeneration = 0, nameGeneration = 0;
        std::uint32_t        requestedId = 0;
        unsigned             counts = 0, names = 0, oldCounts = 0;
        AssocGroupId*        mutateCaller = nullptr;
        ClockResult          ReadAnimationAssocDefinitionCount(std::uint64_t gen, std::int32_t& output) noexcept override
        {
            ++counts;
            countGeneration = gen;
            output = count;
            return countStatus;
        }
        ClockResult ReadAnimationGroupName(std::uint64_t gen, std::uint32_t id, std::uintptr_t& output) noexcept override
        {
            ++names;
            nameGeneration = gen;
            requestedId = id;
            if (mutateCaller)
                *mutateCaller = 0;
            output = returnedName;
            return nameStatus;
        }
        ClockResult ReadAnimationManagerCounts(std::uint64_t, NativeAnimationManagerCounts&) noexcept override
        {
            ++oldCounts;
            return ClockResult::NotInitialized;
        }
    };
}
int main()
{
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner.generation);
    for (const auto raw :
         {(std::numeric_limits<std::int32_t>::min)(), std::int32_t{-1}, std::int32_t{0}, std::int32_t{1}, (std::numeric_limits<std::int32_t>::max)()})
    {
        owner.count = raw;
        Check(manager.GetNumAnimAssocDefinitions() == raw && owner.countGeneration == owner.generation);
    }
    Check(owner.names == 0 && owner.oldCounts == 0);
    for (const auto id : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{139}, std::uint32_t{200}, std::uint32_t{0x80000000u}, std::uint32_t{0xffffffffu}})
    {
        Check(manager.GetAnimGroupName(static_cast<AssocGroupId>(id)) == reinterpret_cast<const char*>(owner.address));
        Check(owner.requestedId == id && owner.nameGeneration == owner.generation);
    }
    AssocGroupId caller = 0xfedcba98ul;
    owner.mutateCaller = &caller;
    Check(manager.GetAnimGroupName(caller) == reinterpret_cast<const char*>(owner.address));
    Check(caller == 0 && owner.requestedId == 0xfedcba98u);
    owner.mutateCaller = nullptr;
    owner.returnedName = 0;
    Refuses([&] { manager.GetAnimGroupName(0); }, ClockResult::BindingChanged, "GetAnimGroupName");
    owner.returnedName = owner.address;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                               ClockResult::OwnerConflict, ClockResult::Busy})
    {
        owner.countStatus = failure;
        owner.nameStatus = failure;
        Refuses([&] { manager.GetNumAnimAssocDefinitions(); }, failure, "GetNumAnimAssocDefinitions");
        Refuses([&] { manager.GetAnimGroupName(0xfffffffful); }, failure, "GetAnimGroupName");
    }
    NativeAnimationManagerAccess           absent;
    PrivateAnimationFactoryManagerReceiver unconfigured(absent, owner.generation);
    Refuses([&] { unconfigured.GetNumAnimAssocDefinitions(); }, ClockResult::NotInitialized, "GetNumAnimAssocDefinitions");
    Refuses([&] { unconfigured.GetAnimGroupName(0); }, ClockResult::NotInitialized, "GetAnimGroupName");
    Check(owner.oldCounts == 0);
    std::puts("Actual animation definition SDK fixture completed");
    std::printf("Passed %u animation definition SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
