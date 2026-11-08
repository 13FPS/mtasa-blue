/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's slot backup/restore itinerary without a native layout
 ****************************************************************************/
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace MtaGame
{
    struct CWorldPoolBackupPolicy
    {
        template <class Contains, class Remove, class Matrix, class Release, class Capture, class Absent>
        static bool BackupSlots(std::size_t count, Contains&& contains, Remove&& remove, Matrix&& matrix, Release&& release, Capture&& capture, Absent&& absent)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                if (!contains(index))
                {
                    if (!absent(index))
                        return false;
                    continue;
                }
                // Classic captures bytes AFTER native world/resource cleanup,
                // optional matrix detach and pool Release. A DE owner journals
                // each accepted prefix; false never manufactures a rollback.
                if (!remove(index) || !matrix(index) || !release(index) || !capture(index))
                    return false;
            }
            return true;
        }
        template <class Present, class AllocateCopy, class Matrix, class AddWorld, class Reapply>
        static bool RestoreSlots(std::size_t backupCount, std::size_t poolCount, Present&& present, AllocateCopy&& allocateCopy, Matrix&& matrix,
                                 AddWorld&& addWorld, Reapply&& reapply)
        {
            const auto count = (std::min)(backupCount, poolCount);
            for (std::size_t index = 0; index < count; ++index)
                if (present(index) && (!allocateCopy(index) || !matrix(index) || !addWorld(index) || !reapply(index)))
                    return false;
            // The caller disposes snapshots only after this complete traversal.
            // Native truncation/disposal additionally requires all saved owned
            // resource releases to have acknowledged, not merely a small size.
            return true;
        }
        static bool InMovedRange(std::uintptr_t pointer, std::uintptr_t begin, std::uintptr_t end) noexcept { return pointer >= begin && pointer < end; }
        template <class Ipl, class Objects, class Backup, class Dummies>
        static void RebaseMovedBuildings(bool hadOldPool, Ipl&& ipl, Objects&& objects, Backup&& backup, Dummies&& dummies)
        {
            if (!hadOldPool)
                return;
            // Only pointers into the moved buildings allocation are rebased.
            // A dummy/object LOD in another pool retains its original address.
            ipl();
            objects();
            backup();
            dummies();
        }
        template <class VehicleLinks, class PedLinks, class ObjectLinks>
        static void SweepLinks(bool alreadySwept, VehicleLinks&& vehicles, PedLinks&& peds, ObjectLinks&& objects)
        {
            if (alreadySwept)
                return;
            vehicles();
            peds();
            objects();
        }
    };
}
