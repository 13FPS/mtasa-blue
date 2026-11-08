/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's model-ID LOD defaults and complete reset ordering
 ****************************************************************************/
#pragma once
#include <algorithm>

namespace MtaGame
{
    struct CModelLodDistancePolicy
    {
        static float Limit(float distance, bool overrideMaxDistance) noexcept
        {
            // Keep the original std::min argument order: negative values,
            // signed zero and NaN retain the classic setter's float behavior.
            return overrideMaxDistance ? distance : std::min(distance, 325.f);
        }
        template <class Find>
        static float GetOriginal(Find&& find)
        {
            float original = 0.0f;
            // This is an SDK model-ID default-cache query. An untouched model
            // returns positive zero even when its current native LOD differs.
            return find(original) ? original : 0.0f;
        }
        template <class Value, class Read, class Remember, class Store>
        static bool Set(const Value& requested, Read&& read, Remember&& remember, Store&& store)
        {
            Value current{};
            if (!read(current))
                return false;
            // An initial same-value setter still captures its first default.
            // Callers retain SDK model-ID keys even when metadata rows alias.
            if (!remember(current))
                return false;
            return store(requested);
        }
        template <class Map, class Restore>
        static bool ResetAll(Map& originals, Restore&& restore)
        {
            for (auto& entry : originals)
                if (!restore(entry.first, entry.second))
                    return false;
            // Ordered SDK model-ID maps preserve alias reset-write order.
            // Keep every default until all callbacks complete; a guarded
            // backend can retain an unfinished traversal for safe retirement.
            originals.clear();
            return true;
        }
    };
}
