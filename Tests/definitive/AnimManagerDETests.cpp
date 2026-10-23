/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual partial animation-manager SDK dispatch and missing-name facts
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlendSequence.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include "AnimationManagerFixture.generated.h"
using namespace mta::game_de;
namespace
{
    unsigned managerChecks = 0;
    void     CheckManager(bool ok, const char* why)
    {
        ++managerChecks;
        if (!ok)
        {
            std::fprintf(stderr, "FAIL %u: %s\n", managerChecks, why);
            std::exit(1);
        }
    }
    struct ManagerOwner final : NativeAnimationManagerAccess
    {
        unsigned                     countCalls = 0, lookupCalls = 0;
        std::uint64_t                receivedGeneration = 0;
        NativeAnimationBlockName     receivedName{};
        NativeAnimationManagerCounts counts{17, 73};
        std::int32_t                 selected = 2;
        ClockResult                  result = ClockResult::Ok;
        char*                        callerName = nullptr;
        ClockResult                  ReadAnimationManagerCounts(std::uint64_t generation, NativeAnimationManagerCounts& output) noexcept override
        {
            ++countCalls;
            receivedGeneration = generation;
            output = counts;
            return result;
        }
        ClockResult FindAnimationBlockIndex(std::uint64_t generation, const NativeAnimationBlockName& name, std::int32_t& output) noexcept override
        {
            ++lookupCalls;
            receivedGeneration = generation;
            receivedName = name;
            if (callerName)
                callerName[0] = 'X';
            output = selected;
            return result;
        }
    };
    template <class Operation>
    void RefusedManager(ManagerOwner& owner, Operation operation, const char* name)
    {
        const auto before = owner.countCalls + owner.lookupCalls;
        bool       threw = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string(error.what()).find(name) != std::string::npos;
        }
        CheckManager(threw && owner.countCalls + owner.lookupCalls == before + 1,
                     "actual SDK refusal names its operation and dispatches once without retry or fallback");
    }
}
int main()
{
    constexpr std::uint64_t         generation = 0xfedcba9876543210ull;
    ManagerOwner                    owner;
    AnimationManagerFixtureReceiver receiver(owner, generation);
    CAnimManager&                   sdk = receiver;
    CheckManager(owner.countCalls == 0 && owner.lookupCalls == 0,
                 "constructing partial manager copies borrowed owner/generation without native initialization or refs");
    for (const auto blocks : {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, std::int32_t{181}, (std::numeric_limits<std::int32_t>::min)(),
                              (std::numeric_limits<std::int32_t>::max)()})
        for (const auto animations :
             {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
        {
            owner.counts = {blocks, animations};
            auto before = owner.countCalls;
            CheckManager(sdk.GetNumAnimBlocks() == blocks && owner.countCalls == before + 1 && owner.receivedGeneration == generation,
                         "actual SDK block count preserves fresh signed32 bits and immutable generation through exactly one dispatch");
            before = owner.countCalls;
            CheckManager(sdk.GetNumAnimations() == animations && owner.countCalls == before + 1 && owner.receivedGeneration == generation,
                         "actual SDK animation extent preserves signed32 counter independently from loaded block flags or bank range");
        }
    for (const auto selected : {std::int32_t{-1}, std::int32_t{0}, std::int32_t{179}, (std::numeric_limits<std::int32_t>::max)()})
    {
        owner.selected = selected;
        const auto before = owner.lookupCalls;
        CheckManager(sdk.GetAnimationBlockIndex("PeD") == selected && owner.lookupCalls == before + 1 && owner.receivedGeneration == generation &&
                         owner.receivedName[0] == 'P' && owner.receivedName[1] == 'e' && owner.receivedName[2] == 'D' && !owner.receivedName[3],
                     "actual SDK copied-name lookup preserves spelling and genuine absent -1 without cache, normalization or fabricated block wrapper");
    }
    owner.selected = 1;
    char caller[] = "ped";
    owner.callerName = caller;
    CheckManager(sdk.GetAnimationBlockIndex(caller) == 1 && caller[0] == 'X' && owner.receivedName[0] == 'p',
                 "caller string is copied before owner dispatch, so later caller storage changes cannot alter that selected query");
    owner.callerName = nullptr;
    char longest[16];
    std::memset(longest, 'x', sizeof(longest));
    longest[15] = 0;
    CheckManager(sdk.GetAnimationBlockIndex(longest) == 1 && owner.receivedName[14] == 'x' && !owner.receivedName[15],
                 "complete15-byte native name is admitted with explicit copied terminator");
    CheckManager(sdk.GetAnimationBlockIndex("") == 1 && !owner.receivedName[0], "empty terminated query keeps genuine native lookup intent");
    char binary[]{static_cast<char>(0x80), static_cast<char>(0xff), 0};
    CheckManager(sdk.GetAnimationBlockIndex(binary) == 1 && owner.receivedName[0] == 0x80 && owner.receivedName[1] == 0xff,
                 "copied query retains native high bytes without locale or ASCII-only rejection");
    for (const auto status :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::InvalidArgument})
    {
        owner.result = status;
        RefusedManager(owner, [&] { sdk.GetNumAnimBlocks(); }, "GetNumAnimBlocks");
        RefusedManager(owner, [&] { sdk.GetNumAnimations(); }, "GetNumAnimations");
        RefusedManager(owner, [&] { sdk.GetAnimationBlockIndex("ped"); }, "GetAnimationBlockIndex");
    }
    owner.result = ClockResult::Ok;
    for (const auto selected : {std::int32_t{-2}, (std::numeric_limits<std::int32_t>::min)()})
    {
        owner.selected = selected;
        RefusedManager(owner, [&] { sdk.GetAnimationBlockIndex("ped"); }, "sentinel");
    }
    char unterminated[16];
    std::memset(unterminated, 'x', sizeof(unterminated));
    for (const auto text : {static_cast<const char*>(nullptr), static_cast<const char*>(unterminated)})
    {
        const auto before = owner.lookupCalls;
        bool       rejected = false;
        try
        {
            sdk.GetAnimationBlockIndex(text);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        CheckManager(rejected && owner.lookupCalls == before, "null or unterminated input refuses before native owner dispatch, not as valid missing-name -1");
    }
    NativeAnimationManagerAccess    unavailable;
    AnimationManagerFixtureReceiver missing(unavailable, generation);
    bool                            refused = false;
    try
    {
        missing.GetAnimationBlockIndex("ped");
    }
    catch (const std::runtime_error&)
    {
        refused = true;
    }
    CheckManager(refused, "default owner cannot fabricate an available manager or missing-name result");
    std::puts("Actual animation manager observation SDK fixture completed");
    std::printf("Passed %u animation manager SDK checks (%zu-bit; no native engine code executed)\n", managerChecks, sizeof(void*) * 8);
    return 0;
}
