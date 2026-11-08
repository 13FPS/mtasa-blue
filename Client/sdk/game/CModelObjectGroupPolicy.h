/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share actual MTA object-group change and restoration ordering
 *****************************************************************************/
#pragma once
#include <cstdint>

namespace MtaGame
{
    struct CModelObjectGroupPolicy
    {
        template <class Read, class Remember, class Store>
        static bool Set(std::uint16_t requested, Read&& read, Remember&& remember, Store&& store)
        {
            std::uint16_t current = 0;
            if (!read(current))
                return false;
            // Unlike wheel setters, an unchanged group does not create a
            // default-map entry. Native refusal cannot become a same-value hit.
            if (current == requested)
                return true;
            if (!remember(current))
                return false;
            return store(requested);
        }
        template <class Find, class Store, class Forget>
        static bool RestoreOne(Find&& find, Store&& store, Forget&& forget)
        {
            std::uint16_t original = 0;
            bool          found = false;
            if (!find(original, found))
                return false;
            if (!found)
                return true;
            if (!store(original))
                return false;
            // The actual group restore erases AFTER its store. A DE owner
            // retains the original when a guarded store cannot be acknowledged.
            forget();
            return true;
        }
        template <class Map, class Restore>
        static bool RestoreAll(Map& originals, Restore&& restore)
        {
            for (auto& entry : originals)
                if (!restore(entry.first, entry.second))
                    return false;
            // All defaults remain available during the real MTA callbacks.
            // DE can journal acknowledgements inside a value while retaining
            // an incomplete prefix instead of clearing or replaying uncertainty.
            originals.clear();
            return true;
        }
    };
}
