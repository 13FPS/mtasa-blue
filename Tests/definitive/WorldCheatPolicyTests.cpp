/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's exact cheat-name protocol and read/set permissions
 *****************************************************************************/
#include <game/CWorldCheatPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

namespace
{
    using Policy = MtaGame::CWorldCheatPolicy;
    using Id = Policy::Id;
    unsigned checks = 0;

    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, message);
            std::exit(EXIT_FAILURE);
        }
    }

    // This is the pre-extraction MTA API contract, independent of native cheat
    // command aliases or either executable's physical flag-array ordinals.
    constexpr std::array<const char*, 19> LegacyNames = {"hovercars",        "aircars",         "extrabunny",       "extrajump",       "tankmode",
                                                         "noreload",         "perfecthandling", "allcarshavenitro", "airboats",        "infiniteoxygen",
                                                         "walkunderwater",   "fasterclock",     "fastergameplay",   "slowergameplay",  "alwaysmidnight",
                                                         "fullweaponaiming", "infinitehealth",  "neverwanted",      "healtharmormoney"};
    constexpr std::array<Id, 19>          Identities = {Id::HoverCars,        Id::AirCars,         Id::ExtraBunny,       Id::ExtraJump,       Id::TankMode,
                                                        Id::NoReload,         Id::PerfectHandling, Id::AllCarsHaveNitro, Id::AirBoats,        Id::InfiniteOxygen,
                                                        Id::WalkUnderwater,   Id::FasterClock,     Id::FasterGameplay,   Id::SlowerGameplay,  Id::AlwaysMidnight,
                                                        Id::FullWeaponAiming, Id::InfiniteHealth,  Id::NeverWanted,      Id::HealthArmorMoney};

    void Refuses(const char* name)
    {
        for (Id sentinel : {Id::NeverWanted, static_cast<Id>(255)})
        {
            Id observed = sentinel;
            Check(!Policy::Find(name, observed) && observed == sentinel, "unknown/malformed name preserves the caller's prior logical identity");
        }
    }
}

int main()
{
    static_assert(static_cast<unsigned>(Id::Count) == 19 && sizeof(Id) == 1);
    static_assert(std::is_same_v<decltype(Policy::Get(Id::HoverCars)), const Policy::Member*>);
    static_assert(static_cast<unsigned>(Id::HoverCars) == 0 && static_cast<unsigned>(Id::AirCars) == 1 && static_cast<unsigned>(Id::ExtraBunny) == 2 &&
                  static_cast<unsigned>(Id::ExtraJump) == 3);

    unsigned readable = 0, settable = 0;
    for (std::size_t index = 0; index != LegacyNames.size(); ++index)
    {
        const auto* value = Policy::Get(Identities[index]);
        Check(value && value == &Policy::Members[index] && value->id == Identities[index] && static_cast<unsigned>(value->id) == index,
              "stable logical IDs preserve actual classic registration order");
        Check(std::strcmp(value->name, LegacyNames[index]) == 0, "each shared name exactly preserves the existing MTA SDK spelling");
        Check(value->canRead && value->canBeSet == (index < 4), "all nineteen names remain readable and only the actual four properties are settable");
        readable += value->canRead ? 1u : 0u;
        settable += value->canBeSet ? 1u : 0u;
        Id result = static_cast<Id>(255);
        Check(Policy::Find(LegacyNames[index], result) && result == Identities[index], "exact SDK names resolve their logical rows");
        const std::string copiedName(value->name);
        result = Id::NeverWanted;
        Check(Policy::Find(copiedName.c_str(), result) && result == value->id, "lookup compares bytes rather than string-literal pointer identity");
        for (std::size_t other = index + 1; other != LegacyNames.size(); ++other)
            Check(std::strcmp(value->name, Policy::Members[other].name) != 0 && value->id != Policy::Members[other].id,
                  "registry contains no duplicate names or conflicting logical identities");

        Refuses((copiedName + " ").c_str());
        Refuses((" " + copiedName).c_str());
        Refuses((copiedName + "x").c_str());
        Refuses(copiedName.substr(0, copiedName.size() - 1).c_str());
        for (std::size_t character = 0; character != copiedName.size(); ++character)
        {
            std::string changed = copiedName;
            changed[character] = static_cast<char>(changed[character] - 'a' + 'A');
            Refuses(changed.c_str());
        }
        std::string terminated = copiedName;
        terminated.push_back('\0');
        terminated += "native command alias suffix";
        result = static_cast<Id>(255);
        Check(Policy::Find(terminated.c_str(), result) && result == value->id, "C-string termination retains the existing map lookup convention");
    }
    Check(readable == 19 && settable == 4, "logical read and set surfaces retain their exact existing sizes");

    for (unsigned raw = 0; raw != 256; ++raw)
    {
        const auto* value = Policy::Get(static_cast<Id>(raw));
        Check((value != nullptr) == (raw < 19), "every malformed byte-sized ID is rejected before table indexing");
        if (value)
            Check(static_cast<unsigned>(value->id) == raw, "every admitted ID resolves its own stable logical member");
    }
    Refuses(nullptr);
    for (const char* invalid : {"",
                                "Hovercars",
                                "hover_cars",
                                "hovercars\n",
                                "hovercars\t",
                                "RIPAZHA",
                                "CJPHONEHOME",
                                "KANGAROO",
                                "HESOYAM",
                                "ROCKETMAN",
                                "DRIVEONWATER",
                                "CHITTYCHITTYBANGBANG",
                                "JHJOECW",
                                "R4C32S34",
                                "UD131RL1L",
                                "TSCCSCC12234",
                                "UUTTUULRS44",
                                "allcarsnitro",
                                "healtharmourmoney",
                                "infinitehealth ",
                                "\xff",
                                "hovercars\xc2\xa0"})
        Refuses(invalid);
    for (const auto& member : Policy::Members)
    {
        Id result = static_cast<Id>(255);
        Check(Policy::Find(member.name, result) && Policy::Get(result) == &member,
              "failed lookups never mutate or replace subsequent shared member resolution");
    }
    std::printf("Passed %u shared MTA cheat policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
