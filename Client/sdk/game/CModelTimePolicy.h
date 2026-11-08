/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's first timed-model default and complete reset ordering
 ****************************************************************************/
#pragma once
namespace MtaGame
{
    struct CModelTimePolicy
    {
        struct Hours
        {
            char on = 0, off = 0;
        };
        template <class Tuple, class Read, class Remember, class Store>
        static bool Set(const Tuple& requested, Read&& read, Remember&& remember, Store&& store)
        {
            Tuple current{};
            if (!read(current))
                return false;
            // Actual SetTime remembers even an initial same-hours call. A
            // model alias must not replace that first native time-info default
            // when another script subsequently changes the same storage.
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
            // Keep all first defaults until the entire traversal completes.
            // A guarded backend can mark acknowledged entries in each value
            // and retain an unfinished prefix without replaying native writes.
            originals.clear();
            return true;
        }
    };
}
