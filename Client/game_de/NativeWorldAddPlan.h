/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preflight native sector/LOD allocation order before any World Add
 ****************************************************************************/
#pragma once
#include "NativeWorldIplPlan.h"
#include "NativeWorldAddProfile.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mta::game_de::NativeWorldAddPlan
{
    using Rect = NativeWorldIplPlan::Rect;
    enum class Kind : unsigned char
    {
        Building,
        Dummy
    };
    enum class PoolKind : unsigned char
    {
        Single,
        Double
    };
    struct Step
    {
        std::uintptr_t headRva = 0;
        std::uint32_t  slot = 0;
        unsigned char  before = 0, after = 0;
    };
    struct Value
    {
        PoolKind                     pool = PoolKind::Single;
        std::array<unsigned char, 4> ranges{};
        std::vector<Step>            steps;
        std::vector<unsigned char>   flagsAfter;
        std::int32_t                 cursorAfter = -1;
    };
    inline PoolKind Select(Kind kind, bool lod) noexcept
    {
        return !lod && kind == Kind::Dummy ? PoolKind::Double : PoolKind::Single;
    }
    inline ClockResult Require(Kind kind, bool lod, const Rect& bounds, std::int32_t cursor, const std::vector<unsigned char>& flags, Value& output)
    {
        namespace P = NativeWorldAddProfile;
        if ((kind != Kind::Building && kind != Kind::Dummy) || !NativeWorldIplPlan::Valid(bounds))
            return ClockResult::InvalidArgument;
        const auto pool = Select(kind, lod);
        const auto capacity = pool == PoolKind::Single ? P::SingleCapacity : P::DoubleCapacity;
        if (flags.size() != capacity || cursor < -1 || cursor >= static_cast<std::int32_t>(capacity))
            return ClockResult::BindingChanged;
        const auto index = [lod](float coordinate)
        {
            // For finite ordered bounds the native clamp's asymmetric2999 /
            // 3000 edge cases select the same cells as this bounded coordinate.
            // Keep the two scalar rounding steps used by native mulss/addss.
            coordinate = (std::max)(-3000.0f, (std::min)(2999.0f, coordinate));
            const float scaled = coordinate * (lod ? 0.005f : 0.02f);
            const float shifted = scaled + (lod ? 15.0f : 60.0f);
            return lod ? static_cast<std::int32_t>(shifted) : static_cast<std::int32_t>(std::floor(shifted));
        };
        const auto left = index(bounds[0]), right = index(bounds[2]), bottom = index(bounds[3]), top = index(bounds[1]);
        const auto side = lod ? P::LodSide : P::SectorSide;
        if (left < 0 || bottom < 0 || right < left || top < bottom || right >= static_cast<std::int32_t>(side) || top >= static_cast<std::int32_t>(side))
            return ClockResult::BindingChanged;
        const auto needed = static_cast<std::size_t>(right - left + 1) * static_cast<std::size_t>(top - bottom + 1);
        if (static_cast<std::size_t>(std::count_if(flags.begin(), flags.end(), [](unsigned char flag) { return (flag & 0x80) != 0; })) < needed)
            return ClockResult::NotInitialized;
        Value value;
        value.pool = pool;
        value.ranges = {static_cast<unsigned char>(left), static_cast<unsigned char>(right), static_cast<unsigned char>(bottom),
                        static_cast<unsigned char>(top)};
        value.flagsAfter = flags;
        value.cursorAfter = cursor;
        value.steps.reserve(needed);
        for (auto y = bottom; y <= top; ++y)
            for (auto x = left; x <= right; ++x)
            {
                std::size_t checked = 0;
                do
                {
                    if (++value.cursorAfter == static_cast<std::int32_t>(capacity))
                        value.cursorAfter = 0;
                    if (++checked > capacity)
                        return ClockResult::BindingChanged;
                } while (!(value.flagsAfter[static_cast<std::size_t>(value.cursorAfter)] & 0x80));
                const auto slot = static_cast<std::uint32_t>(value.cursorAfter);
                const auto before = value.flagsAfter[slot];
                const auto after = static_cast<unsigned char>(((before & 0x7f) + 1) & 0x7f);
                value.flagsAfter[slot] = after;
                const auto cell = static_cast<std::size_t>(y) * side + static_cast<std::size_t>(x);
                const auto head = lod ? P::LodRva + cell * 8 : P::SectorsRva + cell * 16 + (pool == PoolKind::Double ? 8 : 0);
                value.steps.push_back({head, slot, before, after});
            }
        output = std::move(value);
        return ClockResult::Ok;
    }
}
