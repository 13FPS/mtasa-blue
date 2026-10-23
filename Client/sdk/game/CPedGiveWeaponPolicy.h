/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's exact ped weapon preparation and returned-slot itinerary
 ****************************************************************************/
#pragma once
#include <cstdint>

namespace MtaGame
{
    struct CPedGiveWeaponPolicy final
    {
        template <class Type, class Skill, class Blocking, class InfoLookup, class ModelLookup, class GiveDetonator, class NativeGive, class WrapperLookup>
        static auto Give(Type type, std::uint32_t ammo, Skill skill, Type unarmed, Type satchel, Blocking blockingFlags, InfoLookup&& infoLookup,
                         ModelLookup&& modelLookup, GiveDetonator&& giveDetonator, NativeGive&& nativeGive, WrapperLookup&& wrapperLookup)
        {
            if (type != unarmed)
            {
                auto* info = infoLookup(type, skill);
                if (info)
                {
                    auto* model = modelLookup(static_cast<std::uint32_t>(info->GetModel()));
                    if (model)
                    {
                        model->Request(blockingFlags, "CPedSA::GiveWeapon");
                        model->MakeCustomModel();
                    }
                    // A missing model skips loading, not the satchel's
                    // detonator. Missing weapon info skips both, as in MTA.
                    if (type == satchel)
                        giveDetonator();
                }
            }
            // Native grant owns ammo/model/FX effects and selects its slot.
            // Preserve raw ammo and return this ped's existing wrapper; do
            // not equip, initialize a standalone row, retry or roll back.
            const auto slot = nativeGive(type, ammo);
            return wrapperLookup(slot);
        }
    };
}
