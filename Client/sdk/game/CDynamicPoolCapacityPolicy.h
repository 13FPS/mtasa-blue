/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA part-capacity growth/shrink and partial failures
 ****************************************************************************/
#pragma once
#include <cstddef>
#include <new>

namespace MtaGame
{
    struct CDynamicPoolCapacityPolicy
    {
        template <class Current, class Grow, class LastUsed, class LastCapacity, class RemoveLast>
        static bool Set(std::size_t requested, Current&& readCurrent, Grow&& grow, LastUsed&& lastUsed, LastCapacity&& lastCapacity, RemoveLast&& removeLast)
        {
            if (!requested)
                return false;
            auto current = readCurrent();
            if (current == requested)
                return false;
            if (current < requested)
            {
                while (current < requested)
                {
                    try
                    {
                        current += grow();
                    }
                    catch (const std::bad_alloc&)
                    {
                        return false;
                    }
                }
            }
            else
            {
                for (;;)
                {
                    if (lastUsed())
                        return false;
                    current -= lastCapacity();
                    if (current < requested)
                        return false;
                    removeLast();
                    if (current == requested)
                        return true;
                }
            }
            // The accepted part prefix survives a later allocation/shrink
            // refusal, exactly as in MTA. Existing live parts never move.
            return true;
        }
    };
}
