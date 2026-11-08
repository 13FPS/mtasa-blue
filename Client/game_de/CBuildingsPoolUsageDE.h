/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the real nested building pool through the existing pools owner
 ****************************************************************************/
#pragma once
#include "CPoolsUsageDE.h"
#include <game/CBuildingsPool.h>

namespace mta::game_de
{
    // CGame's Reset/World policy reads this nested SDK receiver separately
    // from CPools. Borrow the same pools registry so these methods cannot
    // silently acquire a different native owner or cache a startup snapshot.
    class CBuildingsPoolUsageDE : public CBuildingsPool
    {
    public:
        int         GetSize() const override;
        bool        HasFreeBuildingSlot() override;
        ClockResult ReadUsage(NativePoolUsageAccess::Value&) const noexcept;
        CBuildingsPoolUsageDE(const CBuildingsPoolUsageDE&) = delete;
        CBuildingsPoolUsageDE& operator=(const CBuildingsPoolUsageDE&) = delete;

    protected:
        // The enclosing complete receiver owns lifetime/serialization. Its
        // CPoolsUsageDE must outlive this reader and every borrowed SDK call.
        explicit CBuildingsPoolUsageDE(CPoolsUsageDE& pools) noexcept : pools(pools) {}
        ~CBuildingsPoolUsageDE() = default;

    private:
        CPoolsUsageDE& pools;
        static void    RequireUsage(ClockResult, const char*);
    };
    // Allocation, resize, removal and backup remain mandatory effects. Reading
    // a current native bank supplies none of their missing lifetime authority.
    static_assert(std::is_abstract_v<CBuildingsPoolUsageDE> && std::is_base_of_v<CBuildingsPool, CBuildingsPoolUsageDE>);
}
