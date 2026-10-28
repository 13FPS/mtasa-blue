/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual no-texture registry initialization itinerary
 ****************************************************************************/
#pragma once
namespace MtaGame
{
    class CTextureWatchInitializationPolicy final
    {
    public:
        template <class ContainsNoTexture, class CreateNoTexture, class InsertTexture>
        static void Initialize(ContainsNoTexture&& contains, CreateNoTexture&& create, InsertTexture&& insert)
        {
            // The source uses registry membership for idempotency. If creation
            // publishes and matching insertion then refuses, keep that prefix;
            // do not replace it with a completion flag, replay or rollback.
            if (!contains())
            {
                auto* texture = create();
                insert(texture);
            }
        }
    };
}
