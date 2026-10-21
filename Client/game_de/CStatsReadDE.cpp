/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual CStats SDK read and classifier semantics
 *****************************************************************************/
#include "CStatsReadDE.h"
#include "NativeStatsProfile.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    static_assert(static_cast<std::uint32_t>(MAX_FLOAT_STATS) == NativeStatsProfile::FloatCount &&
                  static_cast<std::uint32_t>(STATS_OFFSET) == NativeStatsProfile::IntegerFirst &&
                  static_cast<std::uint32_t>(MAX_INT_FLOAT_STATS) == NativeStatsProfile::IntegerSdkEnd);
    void CStatsReadDE::Require(Status status, const char* operation)
    {
        if (status != Status::Ok)
            throw std::runtime_error(std::string("DE CStats ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(status)) +
                                     ")");
    }
    float CStatsReadDE::GetStatValue(unsigned short index)
    {
        float value = 0;
        Require(native.Read(index, value), "GetStatValue");
        return value;
    }
    unsigned short CStatsReadDE::GetSkillStatIndex(eWeaponType type)
    {
        unsigned short value = 0;
        Require(native.ReadSkillIndex(type, value), "GetSkillStatIndex");
        return value;
    }
}
