/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native identities for MTA's four settable world cheat properties
 *****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "../sdk/game/CWorldCheatPolicy.h"

namespace mta::game_de::NativeCheatProfile
{
    enum class Flag : std::uint8_t
    {
        HoverCars,
        AirCars,
        ExtraBunny,
        ExtraJump
    };
    inline constexpr std::uintptr_t ImageSize = 0x5ca4400, FlagsRva = 0x51bd0b0, DispatchTableRva = 0x4306470;
    inline constexpr std::size_t    NativeFlagCount = 117;
    struct Member
    {
        const char*   sdkName;
        std::uint32_t nativeIndex;
    };
    // These are the actual settable CGameSA registry names. Native command
    // aliases establish their identity independently from classic x86 offsets.
    inline constexpr std::array<Member, 4> Members = {{{"hovercars", 45}, {"aircars", 60}, {"extrabunny", 61}, {"extrajump", 72}}};
    struct Pin
    {
        std::uintptr_t                 rva;
        std::size_t                    size;
        std::array<unsigned char, 160> bytes;
    };
    // Exact byte pins are filled from independently decoded disk evidence in
    // the current batch's cheats-proof/four-cheat-flags-qualification.json.
    // The initializer is verified as provenance only and is never called: its
    // broader reset would also change native time scale and forced weather.
    extern const std::array<Pin, 7> Code;
    struct Registration
    {
        std::uintptr_t instructionRva, nameRva;
        const char*    name;
    };
    extern const std::array<Registration, 10> Registrations;
    struct Binding
    {
        std::uintptr_t image = 0, flags = 0;
    };

    // Logical SDK identity is independent from native registration ordinal.
    // These read-only descriptors do not enlarge the four writable properties.
    struct ReadMember
    {
        MtaGame::CWorldCheatPolicy::Id id;
        std::uint32_t                  nativeIndex;
        std::uintptr_t                 actionRva;
        Pin                            consumer;
    };
    struct ReadRegistration
    {
        MtaGame::CWorldCheatPolicy::Id id;
        Registration                   native;
    };
    extern const std::array<ReadMember, 14>       ReadMembers;
    extern const std::array<ReadRegistration, 30> ReadRegistrations;
    const ReadMember*                             FindRead(MtaGame::CWorldCheatPolicy::Id id) noexcept;
    // A checked borrowed profile is not engine thread/generation authority.
    // Reads preserve classic raw-nonzero semantics; writes stay strict binary.
    bool RevalidateRead(const Binding& binding, MtaGame::CWorldCheatPolicy::Id id) noexcept;

    bool Find(const char* sdkName, Flag& output) noexcept;
    bool Qualify(Binding& output) noexcept;
    bool Revalidate(const Binding& binding) noexcept;
}
