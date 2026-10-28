/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's ped metadata publication before special streaming
 ****************************************************************************/
#pragma once

namespace MtaGame
{
    class CModelPedCreationPolicy final
    {
    public:
        template <class CreateAndPublish, class RequestSpecial>
        static void MakePedModel(CreateAndPublish&& createAndPublish, RequestSpecial&& requestSpecial)
        {
            // Publication is an observable prefix, not a transaction. Resolve
            // streaming only afterward and retain that prefix on refusal.
            createAndPublish();
            requestSpecial();
        }
    };
}
