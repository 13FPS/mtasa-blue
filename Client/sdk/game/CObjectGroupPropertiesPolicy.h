/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's modified flag separately from its retained first defaults
 ****************************************************************************/
#pragma once
#include <cmath>

namespace MtaGame
{
    struct CObjectGroupPropertiesPolicy
    {
        static bool UnchangedFloat(float current, float desired) noexcept
        {
            // This is MTA's actual epsilon test. Equal NaNs/infinities do not
            // become a new bitwise no-op rule and no value clamp is introduced.
            return std::fabs(current - desired) < 0.0001f;
        }
        template <class Value>
        static bool UnchangedValue(const Value& current, const Value& desired) noexcept
        {
            // Preserve the caller's typed comparison: full SDK enums before
            // byte conversion, logical booleans and CVector's own epsilon.
            return current == desired;
        }
        template <class Capture>
        static void ChangeSafeguard(bool& modified, Capture&& capture)
        {
            if (modified)
                return;
            modified = true;
            capture();
        }
        template <class Restore>
        static bool RestoreDefault(bool& modified, Restore&& restore)
        {
            if (!modified)
                return true;
            if (!restore())
                return false;
            // Single restore deliberately retains the first original in the
            // registry. A later edit must not capture that edit's predecessor.
            modified = false;
            return true;
        }
        template <class Map, class Restore>
        static bool RestoreAll(Map& originals, Restore&& restore)
        {
            for (auto& entry : originals)
                if (!restore(entry.first, entry.second))
                    return false;
            // Native owners may retain an acknowledged field prefix until the
            // complete explicit reset succeeds. End is never a restore call.
            originals.clear();
            return true;
        }
    };
}
