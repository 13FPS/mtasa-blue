/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep original pool capacity independent of mutable live pool size
 ****************************************************************************/
#include "CGameBuildingPoolDefaultDE.h"
#include <limits>
namespace mta::game_de
{
    int CGameBuildingPoolDefaultDE::GetResetBuildingPoolCapacity() const
    {
        std::uint32_t capacity = 0;
        RequireNativeWrite(NativeWrites().ReadDefaultBuildingPoolCapacity(capacity), "reset building-pool default capacity");
        // Reset compares this qualified original with a separately read live
        // size. A cached request, live size or a literal fallback would silently
        // suppress restoration after resize or conceal unavailable authority.
        if (!capacity || capacity > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()))
            RequireNativeWrite(ClockResult::InvalidArgument, "reset building-pool default capacity");
        return static_cast<int>(capacity);
    }
}
