/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA-owned jetpack weapon permissions across game backends
 *****************************************************************************/
#pragma once

#include <cstddef>

namespace MtaGame
{
    // These permissions are client policy consumed by an engine's qualified
    // jetpack hook. Updating the table itself neither patches native code nor
    // establishes that a backend has installed such a hook.
    template <std::size_t Size, std::size_t FirstWeapon>
    class CJetpackWeaponPolicy
    {
        static_assert(FirstWeapon < Size);

    public:
        bool Get(int weaponType) const noexcept { return IsValid(weaponType) && enabled[static_cast<std::size_t>(weaponType)]; }

        void Set(int weaponType, bool value) noexcept
        {
            if (IsValid(weaponType))
                enabled[static_cast<std::size_t>(weaponType)] = value;
        }

    private:
        static bool IsValid(int weaponType) noexcept
        {
            return weaponType >= 0 && static_cast<std::size_t>(weaponType) >= FirstWeapon && static_cast<std::size_t>(weaponType) < Size;
        }

        // Classic's normal ZERO_ON_NEW factory already clears its former
        // SFixedArray. Preserve those permissions explicitly when reused by DE
        // or constructed outside that allocator. Existing map/reset traffic
        // remains responsible for choosing the session's allowed weapons.
        bool enabled[Size]{};
    };
}
