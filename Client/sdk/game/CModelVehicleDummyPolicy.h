/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's dummy default, model hold and restoration boundaries
 *****************************************************************************/
#pragma once
#include <utility>
namespace MtaGame
{
    struct CModelVehicleDummyPolicy
    {
        template <class PrepareModel, class Increment, class Remember, class Store>
        static bool Mutate(PrepareModel&& prepare, Increment&& increment, Remember&& remember, Store&& store)
        {
            bool first = false;
            if (!prepare(first))
                return false;
            // MTA inserts its per-model cache before incrementing the model's
            // hold. Even a same-value setter captures a default and owns a hold.
            if (first && !increment())
                return false;
            if (!remember())
                return false;
            return store();
        }
        template <class Entries, class Restore, class Decrement, class Erase>
        static bool RestoreOne(Entries& entries, Restore&& restore, Decrement&& decrement, bool remove, Erase&& erase)
        {
            for (auto& entry : entries)
                if (!restore(entry.first, entry.second))
                    return false;
            // Empty per-model maps still own their first increment. Keep the
            // reference until every position has been restored or null-skipped.
            if (!decrement())
                return false;
            if (remove)
                erase();
            return true;
        }
        template <class Models, class Restore, class BeforeClear>
        static bool RestoreAll(Models& models, Restore&& restore, BeforeClear&& beforeClear)
        {
            for (auto& entry : models)
                if (!restore(entry.first, entry.second))
                    return false;
            // A DE owner acknowledges its private reference journal before
            // consuming the same whole-map clear boundary as classic MTA.
            if (!beforeClear())
                return false;
            models.clear();
            return true;
        }
        template <class Models, class Restore>
        static bool RestoreAll(Models& models, Restore&& restore)
        {
            return RestoreAll(models, std::forward<Restore>(restore), [] { return true; });
        }
    };
}
