/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete actual CStats SDK object using its existing native owner
 *****************************************************************************/
#pragma once
#include "CStatsReadDE.h"
#include "NativeStatsWriteAccess.h"

namespace mta::game_de
{
    class CStatsDE final : public CStatsReadDE
    {
    public:
        CStatsDE(const NativeStatsReader& reader, NativeStatsWriteAccess& writes) noexcept : CStatsReadDE(reader), native(writes) {}
        void SetStatValue(unsigned short index, float value) override;
        void ModifyStat(unsigned short index, float value) override;

    private:
        static void             Require(ClockResult result, const char* operation);
        NativeStatsWriteAccess& native;
    };
    static_assert(!std::is_abstract_v<CStatsDE> && std::is_base_of_v<CStats, CStatsDE>);
}
