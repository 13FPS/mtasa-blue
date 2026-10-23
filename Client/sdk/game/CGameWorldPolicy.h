/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual world backup/restore and pool resize itinerary
 *****************************************************************************/
#pragma once
#include <array>
#include <cstddef>
#include <limits>
namespace MtaGame
{
    class CGameWorldPolicy final
    {
    public:
        using IplName = std::array<unsigned char, 8>;
        using IplFilter = bool (*)(const IplName&) noexcept;
        static bool ShouldRestoreIpl(const IplName& name) noexcept
        {
            constexpr IplName barriers{'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'};
            // Classic compares precisely these eight bytes, including case.
            // A copied name keeps the backend's raw IPL layout out of this SDK.
            for (std::size_t index = 0; index < name.size(); ++index)
                if (name[index] != barriers[index])
                    return true;
            return false;
        }
        static bool IsBuildingPoolSizeRepresentable(std::size_t size) noexcept { return size <= static_cast<std::size_t>(std::numeric_limits<int>::max()); }
        template <class DisableIpl, class RemoveCovers, class RemovePlants, class RemoveShadows, class BackupDummies, class BackupBuildings, class DropCaches,
                  class MarkRemoved>
        static void Remove(DisableIpl&& disableIpl, RemoveCovers&& removeCovers, RemovePlants&& removePlants, RemoveShadows&& removeShadows,
                           BackupDummies&& backupDummies, BackupBuildings&& backupBuildings, DropCaches&& dropCaches, MarkRemoved&& markRemoved)
        {
            disableIpl();
            removeCovers();
            removePlants();
            removeShadows();
            backupDummies();
            backupBuildings();
            dropCaches();
            markRemoved(true);
        }
        template <class RestoreBuildings, class RestoreDummies, class EnableIpl, class MarkRemoved>
        static void Restore(RestoreBuildings&& restoreBuildings, RestoreDummies&& restoreDummies, EnableIpl&& enableIpl, MarkRemoved&& markRemoved)
        {
            restoreBuildings();
            restoreDummies();
            enableIpl(&ShouldRestoreIpl);
            markRemoved(false);
        }
        template <class GetSize, class RemoveWorld, class DropCaches, class Resize, class RestoreWorld>
        static bool SetBuildingPoolSize(std::size_t size, bool worldRemoved, GetSize&& getSize, RemoveWorld&& removeWorld, DropCaches&& dropCaches,
                                        Resize&& resize, RestoreWorld&& restoreWorld)
        {
            if (!IsBuildingPoolSizeRepresentable(size))
                return false;
            // Capture before GetSize: its virtual dispatch may re-enter the
            // game, but the original call keeps its already-selected itinerary.
            const bool shouldRemoveWorld = !worldRemoved;
            const int  currentSize = getSize();
            if (currentSize >= 0 && static_cast<std::size_t>(currentSize) == size && !shouldRemoveWorld)
            {
                dropCaches();
                return true;
            }
            if (shouldRemoveWorld)
                removeWorld();
            else
                dropCaches();
            const bool status = resize(static_cast<int>(size));
            if (shouldRemoveWorld)
                restoreWorld();
            // A false resize still restores; an exception preserves its prefix
            // and does not manufacture a restore, rollback or automatic replay.
            return status;
        }
    };
}
