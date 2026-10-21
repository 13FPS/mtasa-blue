/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share actual MTA world-cheat names and logical SDK permissions
 *****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MtaGame
{
    class CWorldCheatPolicy final
    {
    public:
        // Keep the existing classic constructor order. These are logical MTA
        // identities, not native cheat-array indices or callable command IDs.
        enum class Id : std::uint8_t
        {
            HoverCars,
            AirCars,
            ExtraBunny,
            ExtraJump,
            TankMode,
            NoReload,
            PerfectHandling,
            AllCarsHaveNitro,
            AirBoats,
            InfiniteOxygen,
            WalkUnderwater,
            FasterClock,
            FasterGameplay,
            SlowerGameplay,
            AlwaysMidnight,
            FullWeaponAiming,
            InfiniteHealth,
            NeverWanted,
            HealthArmorMoney,
            Count
        };

        struct Member
        {
            Id          id;
            const char* name;
            bool        canRead, canBeSet;
        };

        // Permission preserves the existing SDK registry contract. A port must
        // independently qualify the corresponding native storage/owner before
        // using it; a readable name does not manufacture an available binding.
        inline static constexpr std::array<Member, static_cast<std::size_t>(Id::Count)> Members = {{
            {Id::HoverCars, "hovercars", true, true},
            {Id::AirCars, "aircars", true, true},
            {Id::ExtraBunny, "extrabunny", true, true},
            {Id::ExtraJump, "extrajump", true, true},
            {Id::TankMode, "tankmode", true, false},
            {Id::NoReload, "noreload", true, false},
            {Id::PerfectHandling, "perfecthandling", true, false},
            {Id::AllCarsHaveNitro, "allcarshavenitro", true, false},
            {Id::AirBoats, "airboats", true, false},
            {Id::InfiniteOxygen, "infiniteoxygen", true, false},
            {Id::WalkUnderwater, "walkunderwater", true, false},
            {Id::FasterClock, "fasterclock", true, false},
            {Id::FasterGameplay, "fastergameplay", true, false},
            {Id::SlowerGameplay, "slowergameplay", true, false},
            {Id::AlwaysMidnight, "alwaysmidnight", true, false},
            {Id::FullWeaponAiming, "fullweaponaiming", true, false},
            {Id::InfiniteHealth, "infinitehealth", true, false},
            {Id::NeverWanted, "neverwanted", true, false},
            {Id::HealthArmorMoney, "healtharmormoney", true, false},
        }};

        static constexpr const Member* Get(Id id) noexcept
        {
            const auto index = static_cast<std::size_t>(id);
            return index < Members.size() ? &Members[index] : nullptr;
        }

        static bool Find(const char* name, Id& output) noexcept
        {
            if (!name)
                return false;
            for (const auto& member : Members)
                if (std::strcmp(name, member.name) == 0)
                {
                    output = member.id;
                    return true;
                }
            // Preserve a caller's prior identity on failure. Case folding or
            // native command aliases would expand the actual MTA API surface.
            return false;
        }
    };
}
