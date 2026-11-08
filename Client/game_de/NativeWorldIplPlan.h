/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preflight exact native region-tree allocation demand before writes
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <limits>

namespace mta::game_de::NativeWorldIplPlan
{
    using Rect = std::array<float, 4>;
    struct Node
    {
        Rect                        bounds{};
        std::uint32_t               level = 0;
        std::array<std::int32_t, 4> child{{-1, -1, -1, -1}};
    };
    struct Budget
    {
        std::uint32_t quadNodes = 0, links = 0;
    };
    inline bool Valid(const Rect& r) noexcept
    {
        for (const auto value : r)
            if (!std::isfinite(value))
                return false;
        return r[0] <= r[2] && r[3] <= r[1] && std::isfinite(r[0] + r[2]) && std::isfinite(r[1] + r[3]);
    }
    inline Rect Child(const Rect& parent, std::uint32_t quadrant) noexcept
    {
        // Exact addss/mulss itinerary in the pinned native Add/Overlap pair.
        // A DE row remains four native floats; no world/unit conversion occurs.
        const float midX = (parent[0] + parent[2]) * 0.5f;
        const float midY = (parent[1] + parent[3]) * 0.5f;
        Rect        result = parent;
        if (quadrant & 1)
            result[0] = midX;
        else
            result[2] = midX;
        if (quadrant & 2)
            result[1] = midY;
        else
            result[3] = midY;
        return result;
    }
    inline bool Overlaps(const Rect& a, const Rect& b) noexcept
    {
        return !(a[0] > b[2] || a[2] < b[0] || a[3] > b[1] || a[1] < b[3]);
    }
    inline ClockResult Require(const std::vector<Node>& current, const std::vector<Rect>& items, std::uint32_t freeQuadNodes, std::uint32_t freeLinks,
                               Budget& output)
    {
        if (current.empty())
            return ClockResult::InvalidArgument;
        std::vector<unsigned> parents(current.size());
        for (std::size_t index = 0; index != current.size(); ++index)
        {
            const auto& node = current[index];
            if (!Valid(node.bounds) || node.level > 12)
                return ClockResult::BindingChanged;
            for (std::uint32_t quadrant = 0; quadrant != 4; ++quadrant)
            {
                const auto child = node.child[quadrant];
                if (child < 0)
                    continue;
                if (!node.level || static_cast<std::size_t>(child) >= current.size() || current[static_cast<std::size_t>(child)].level != node.level - 1 ||
                    current[static_cast<std::size_t>(child)].bounds != Child(node.bounds, quadrant) || ++parents[static_cast<std::size_t>(child)] != 1)
                    return ClockResult::BindingChanged;
            }
        }
        if (parents[0])
            return ClockResult::BindingChanged;
        for (std::size_t index = 1; index != parents.size(); ++index)
            if (parents[index] != 1)
                return ClockResult::BindingChanged;
        std::vector<Node> nodes = current;
        Budget            budget;
        for (const auto& rect : items)
        {
            if (!Valid(rect))
                return ClockResult::BindingChanged;
            std::vector<std::int32_t> pending{0};
            while (!pending.empty())
            {
                const auto index = pending.back();
                pending.pop_back();
                if (index < 0 || static_cast<std::size_t>(index) >= nodes.size())
                    return ClockResult::BindingChanged;
                const auto node = nodes[static_cast<std::size_t>(index)];
                if (!node.level)
                {
                    if (budget.links == freeLinks)
                        return ClockResult::NotInitialized;
                    ++budget.links;
                    continue;
                }
                for (std::uint32_t quadrant = 0; quadrant != 4; ++quadrant)
                {
                    const auto bounds = Child(node.bounds, quadrant);
                    if (!Overlaps(bounds, rect))
                        continue;
                    auto child = node.child[quadrant];
                    if (child < 0)
                    {
                        if (budget.quadNodes == freeQuadNodes || nodes.size() >= static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)()))
                            return ClockResult::NotInitialized;
                        child = static_cast<std::int32_t>(nodes.size());
                        nodes.push_back(Node{bounds, node.level - 1, {{-1, -1, -1, -1}}});
                        nodes[static_cast<std::size_t>(index)].child[quadrant] = child;
                        ++budget.quadNodes;
                    }
                    pending.push_back(child);
                }
            }
        }
        output = budget;
        return ClockResult::Ok;
    }
}
