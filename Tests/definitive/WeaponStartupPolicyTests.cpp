/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual classic weapon startup row/metadata sequence
 *****************************************************************************/
#include <game/CWeaponStartupPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
    using Policy = MtaGame::CWeaponStartupPolicy;
    using Entry = Policy::Entry;
    unsigned checks = 0;
    void     Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, description);
            std::exit(EXIT_FAILURE);
        }
    }
    bool Same(const Entry& first, const Entry& second)
    {
        return first.row == second.row && first.infoType == second.infoType && first.statType == second.statType && first.skill == second.skill;
    }
    // Frozen tuples from the actual pre-extraction CGameSA constructor. Keep
    // literal row/info/stat/skill values so changing a shared loop or enum order
    // cannot silently change both implementation and expected startup behavior.
    constexpr std::array<std::array<int, 4>, 80> Golden{
        {{0, 22, 0, 1},   {1, 23, 1, 1},   {2, 24, 2, 1},   {3, 25, 3, 1},   {4, 26, 4, 1},   {5, 27, 5, 1},   {6, 28, 6, 1},   {7, 29, 7, 1},
         {8, 30, 8, 1},   {9, 31, 9, 1},   {10, 32, 10, 1}, {11, 33, 11, 1}, {12, 34, 12, 1}, {13, 35, 13, 1}, {14, 36, 14, 1}, {15, 37, 15, 1},
         {16, 38, 16, 1}, {17, 39, 17, 1}, {18, 40, 18, 1}, {19, 41, 19, 1}, {20, 42, 20, 1}, {21, 43, 21, 1}, {22, 44, 22, 1}, {23, 45, 23, 1},
         {24, 46, 24, 1}, {25, 47, 25, 1}, {26, 48, 26, 1}, {27, 49, 27, 1}, {28, 50, 28, 1}, {29, 51, 29, 1}, {30, 52, 30, 1}, {31, 53, 31, 1},
         {32, 54, 32, 1}, {33, 55, 33, 1}, {34, 56, 34, 1}, {35, 57, 35, 1}, {36, 58, 36, 1}, {37, 59, 37, 1}, {38, 60, 38, 1}, {39, 61, 39, 1},
         {40, 62, 40, 1}, {41, 63, 41, 1}, {42, 64, 42, 1}, {43, 65, 43, 1}, {44, 66, 44, 1}, {45, 67, 45, 1}, {46, 68, 46, 1}, {47, 22, 22, 0},
         {48, 23, 23, 0}, {49, 24, 24, 0}, {50, 25, 25, 0}, {51, 26, 26, 0}, {52, 27, 27, 0}, {53, 28, 28, 0}, {54, 29, 29, 0}, {55, 30, 30, 0},
         {56, 31, 31, 0}, {57, 32, 32, 0}, {58, 22, 22, 2}, {59, 23, 23, 2}, {60, 24, 24, 2}, {61, 25, 25, 2}, {62, 26, 26, 2}, {63, 27, 27, 2},
         {64, 28, 28, 2}, {65, 29, 29, 2}, {66, 30, 30, 2}, {67, 31, 31, 2}, {68, 32, 32, 2}, {69, 22, 22, 3}, {70, 23, 23, 3}, {71, 24, 24, 3},
         {72, 25, 25, 3}, {73, 26, 26, 3}, {74, 27, 27, 3}, {75, 28, 28, 3}, {76, 29, 29, 3}, {77, 30, 30, 3}, {78, 31, 31, 3}, {79, 32, 32, 3}}};
    bool IsGolden(const Entry& entry, std::size_t row)
    {
        const auto& value = Golden[row];
        return entry.row == static_cast<std::size_t>(value[0]) && static_cast<int>(entry.infoType) == value[1] &&
               static_cast<int>(entry.statType) == value[2] && static_cast<int>(entry.skill) == value[3];
    }
    struct Recorder
    {
        std::vector<Entry> values;
        Recorder() { values.reserve(80); }
        Recorder(const Recorder&) = delete;
        Recorder& operator=(const Recorder&) = delete;
        void      operator()(const Entry& value) { values.push_back(value); }
    };
    struct Failure
    {
        std::size_t row;
        const void* identity;
    };
}

int main()
{
    static_assert(MtaGame::CWeaponInfoIndexPolicy::StandardCount == 47 && MtaGame::CWeaponInfoIndexPolicy::SkillCount == 11 &&
                  MtaGame::CWeaponInfoIndexPolicy::SlotCount == Golden.size());
    Recorder first;
    Policy::Initialize(first);
    Check(first.values.size() == Golden.size(), "shared schedule invokes the existing noncopyable consumer once for each of eighty startup rows");
    for (std::size_t row = 0; row < first.values.size(); ++row)
    {
        Check(IsGolden(first.values[row], row), "every literal legacy row/info/stat/skill tuple preserves exact current MTA startup behavior");
        std::size_t selected = 999;
        Check(MtaGame::CWeaponInfoIndexPolicy::TryGetIndex(first.values[row].statType, first.values[row].skill, selected) && selected == row,
              "each startup stat key selects its exact pre-existing standard/poor/pro/special bank row");
    }
    Check(first.values[0].infoType != first.values[0].statType && first.values[46].infoType != first.values[46].statType,
          "standard info metadata intentionally differs from standard live-stat metadata at both boundaries");

    for (std::size_t failing = 0; failing < Golden.size(); ++failing)
    {
        std::vector<Entry> prefix;
        prefix.reserve(80);
        bool caught = false;
        try
        {
            Policy::Initialize(
                [&](const Entry& entry)
                {
                    prefix.push_back(entry);
                    if (entry.row == failing)
                        throw Failure{entry.row, &prefix};
                });
        }
        catch (const Failure& failure)
        {
            caught = failure.row == failing && failure.identity == &prefix;
        }
        Check(caught && prefix.size() == failing + 1, "consumer exception propagates unchanged without invoking later rows or retrying the failing row");
        for (std::size_t row = 0; row < prefix.size(); ++row)
            Check(Same(prefix[row], first.values[row]), "every accepted callback prefix remains in its exact original order after failure");
    }
    Recorder independent;
    Policy::Initialize(independent);
    Check(independent.values.size() == first.values.size(), "pure schedule retains no hidden process-global progress or cached consumer");
    for (std::size_t row = 0; row < independent.values.size(); ++row)
        Check(Same(independent.values[row], first.values[row]), "a separately owned consumer sees the same complete actual MTA schedule");
    std::printf("Passed %u shared weapon startup schedule checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
