/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Expose native SingleLink usage through its actual nested MTA SDK
 ****************************************************************************/
#pragma once
#include <cstddef>
#include "CPoolsUsageDE.h"
#include <game/CPtrNodeSingleLinkPool.h>

namespace mta::game_de
{
    // The shared SDK uses size_t while native allocation counts are bounded
    // signed32 fields. The existing pools reader qualifies that native width;
    // this receiver preserves the SDK return width without inventing storage.
    class CPtrNodeSingleLinkPoolUsageDE : public CPtrNodeSingleLinkPool
    {
    public:
        std::size_t GetCapacity() const override;
        std::size_t GetUsedSize() const override;
        ClockResult ReadUsage(NativePoolUsageAccess::Value&) const noexcept;
        CPtrNodeSingleLinkPoolUsageDE(const CPtrNodeSingleLinkPoolUsageDE&) = delete;
        CPtrNodeSingleLinkPoolUsageDE& operator=(const CPtrNodeSingleLinkPoolUsageDE&) = delete;

    protected:
        explicit CPtrNodeSingleLinkPoolUsageDE(CPoolsUsageDE& pools) noexcept : pools(pools) {}
        ~CPtrNodeSingleLinkPoolUsageDE() = default;

    private:
        // This same actual registry must outlive the nested borrowed reader.
        // Every read freshly re-enters its established native callback owner.
        CPoolsUsageDE& pools;
        static void    RequireUsage(ClockResult, const char*);
    };
    // Native SingleLink storage is still fixed. No classic dynamic pool or
    // successful Resize/ResetCapacity substitute is manufactured here.
    static_assert(std::is_abstract_v<CPtrNodeSingleLinkPoolUsageDE> && std::is_base_of_v<CPtrNodeSingleLinkPool, CPtrNodeSingleLinkPoolUsageDE>);
}
