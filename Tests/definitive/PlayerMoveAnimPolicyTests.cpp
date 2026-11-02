/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real classic move-animation choices, independent of native effects
 ****************************************************************************/
#include <windows.h>
#include <game/CPlayerMoveAnimPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
    using Policy = MtaGame::CPlayerMoveAnimPolicy;
    unsigned checks = 0;
    void     Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }
}

#include "PlayerMoveAnimItineraryTests.inl"

int main()
{
    static_assert(Policy::ChooseProcessAnimGroup(MOVE_CSAW_M, MOVE_PLAYER_F) == MOVE_CSAW_F);
    static_assert(Policy::ChooseDefaultMotionGroup(MOVE_JETPACK, MOVE_ARMED_M) == MOVE_PLAYER_M);
    struct Boundary
    {
        std::uint32_t group;
        Policy::Kind  kind;
    };
    const Boundary boundaries[] = {{0, Policy::Kind::Default},   {1, Policy::Kind::Invalid},          {53, Policy::Kind::Invalid},
                                   {54, Policy::Kind::Custom},   {70, Policy::Kind::Custom},          {71, Policy::Kind::Invalid},
                                   {117, Policy::Kind::Invalid}, {118, Policy::Kind::Custom},         {138, Policy::Kind::Custom},
                                   {139, Policy::Kind::Invalid}, {0x7fffffff, Policy::Kind::Invalid}, {0xffffffff, Policy::Kind::Invalid}};
    for (const auto& boundary : boundaries)
    {
        const auto selection = Policy::SelectRequest(boundary.group);
        Check(selection.kind == boundary.kind && (selection.blockName != nullptr) == (boundary.kind == Policy::Kind::Custom),
              "setter boundary rejects gaps/raw negatives, while default has no native block intent");
    }
    struct Block
    {
        eMoveAnim   group;
        const char* name;
    };
    const Block blocks[] = {{MOVE_PLAYER_F, "fat"},     {MOVE_ROCKET_F, "fat"},      {MOVE_ARMED_F, "fat"},       {MOVE_BBBAT_F, "fat"},
                            {MOVE_CSAW_F, "fat"},       {MOVE_PLAYER_M, "muscular"}, {MOVE_ROCKET_M, "muscular"}, {MOVE_ARMED_M, "muscular"},
                            {MOVE_BBBAT_M, "muscular"}, {MOVE_CSAW_M, "muscular"},   {MOVE_PLAYER, "ped"},        {MOVE_ROCKET, "ped"},
                            {MOVE_ARMED, "ped"},        {MOVE_BBBAT, "ped"},         {MOVE_CSAW, "ped"},          {MOVE_SNEAK, "ped"},
                            {MOVE_JETPACK, "ped"},      {MOVE_MAN, "ped"},           {MOVE_SKATE, "skate"}};
    for (const auto& block : blocks)
    {
        const auto selection = Policy::SelectRequest(block.group);
        Check(selection.kind == Policy::Kind::Custom && selection.blockName && std::strcmp(selection.blockName, block.name) == 0,
              "all physique/pose blocks and skate retain their actual streaming block name");
    }
    const int requests[] = {MOVE_DEFAULT, MOVE_PLAYER_F, MOVE_ARMED_M, MOVE_SNEAK, MOVE_JETPACK, MOVE_SKATE, -17, 999};
    for (const int requested : requests)
    {
        Check(Policy::ChooseProcessAnimGroup(requested, 0) == requested, "no override preserves the engine process choice, including opaque values");
        Check(Policy::ChooseDefaultMotionGroup(requested, 0) == requested, "no override preserves the clothes producer choice");
    }
    const int jetpackOverrides[] = {MOVE_PLAYER_F, MOVE_ARMED_M, MOVE_SNEAK, MOVE_SKATE, -9, 999};
    for (const int custom : jetpackOverrides)
        Check(Policy::ChooseProcessAnimGroup(MOVE_JETPACK, custom) == MOVE_JETPACK, "a real jetpack process request outranks every nonzero custom override");

    // These requested poses include different native physiques. A normal body
    // override changes only physique, preserving the engine's current pose.
    struct Pose
    {
        int                requested;
        std::array<int, 3> result;
    };
    const Pose poses[] = {{MOVE_PLAYER_F, {MOVE_PLAYER, MOVE_PLAYER_F, MOVE_PLAYER_M}},
                          {MOVE_ROCKET_F, {MOVE_ROCKET, MOVE_ROCKET_F, MOVE_ROCKET_M}},
                          {MOVE_ARMED_M, {MOVE_ARMED, MOVE_ARMED_F, MOVE_ARMED_M}},
                          {MOVE_BBBAT, {MOVE_BBBAT, MOVE_BBBAT_F, MOVE_BBBAT_M}},
                          {MOVE_CSAW_M, {MOVE_CSAW, MOVE_CSAW_F, MOVE_CSAW_M}}};
    const int  physiques[] = {MOVE_PLAYER, MOVE_PLAYER_F, MOVE_PLAYER_M};
    for (const auto& pose : poses)
        for (unsigned physique = 0; physique != 3; ++physique)
            Check(Policy::ChooseProcessAnimGroup(pose.requested, physiques[physique]) == pose.result[physique],
                  "ordinary overrides preserve each requested pose and choose the configured physique");
    const int special[] = {MOVE_ROCKET_F, MOVE_ARMED_M, MOVE_BBBAT, MOVE_CSAW_M};
    for (const int custom : special)
        Check(Policy::ChooseProcessAnimGroup(MOVE_PLAYER_M, custom) == custom, "explicit weapon poses remain authoritative over an ordinary requested pose");
    const int opaque[] = {MOVE_SNEAK, MOVE_JETPACK, MOVE_SKATE, -1, 1, 71, 117, 139, std::numeric_limits<int>::max()};
    for (const int custom : opaque)
    {
        Check(Policy::ChooseProcessAnimGroup(MOVE_ARMED_M, custom) == custom,
              "process hook preserves non-physique/raw custom values without applying setter validation");
        Check(Policy::ChooseDefaultMotionGroup(MOVE_ARMED_M, custom) == MOVE_ARMED_M,
              "clothes hook does not turn non-physique/raw overrides into body variants");
    }
    const std::array<int, 2> clothes[] = {{MOVE_PLAYER, MOVE_PLAYER},     {MOVE_PLAYER_F, MOVE_PLAYER_F}, {MOVE_PLAYER_M, MOVE_PLAYER_M},
                                          {MOVE_ROCKET_M, MOVE_PLAYER_M}, {MOVE_ARMED_F, MOVE_PLAYER_F},  {MOVE_CSAW, MOVE_PLAYER}};
    for (const auto& choice : clothes)
        Check(Policy::ChooseDefaultMotionGroup(MOVE_BBBAT_F, choice[0]) == choice[1],
              "clothes extracts physique from both ordinary and explicit special poses");
    Check(Policy::ChooseDefaultMotionGroup(MOVE_JETPACK, MOVE_PLAYER_F) == MOVE_PLAYER_F, "clothes intentionally has no process-hook jetpack exception");
    Check(Policy::ChooseDefaultMotionGroup(-77, MOVE_ARMED_M) == MOVE_PLAYER_M, "qualified custom physique wins even when the clothes request is opaque");
    Check(Policy::ChooseProcessAnimGroup(-77, MOVE_PLAYER_F) == MOVE_PLAYER_F, "an opaque process request cannot supply a pose for physique blending");
    TestPlayerMoveAnimItinerary();
    std::printf("Passed %u shared MTA player move-animation policy checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
    return EXIT_SUCCESS;
}
