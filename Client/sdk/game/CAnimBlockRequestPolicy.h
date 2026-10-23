/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share MTA's actual animation-block request effect order
 *****************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>
#include "CAnimBlock.h"

namespace MtaGame
{
    class CAnimBlockRequestPolicy final
    {
    public:
        template <class IsLoaded, class GetIndex, class GetIfpBase, class LookupModel, class RequestModel, class AddRef, class LoadAll, class HasModelLoaded>
        static void Request(std::uint32_t type, bool allowBlockingFail, IsLoaded&& isLoaded, GetIndex&& getIndex, GetIfpBase&& getIfpBase,
                            LookupModel&& lookupModel, RequestModel&& requestModel, AddRef&& addRef, LoadAll&& loadAll, HasModelLoaded&& hasModelLoaded)
        {
            // Keep the callable well-formed in release without evaluating it.
            static_cast<void>(hasModelLoaded);
            if (isLoaded())
                return;
            // Preserve the original unsigned DWORD arithmetic, including a
            // signed index's conversion and wrap. Neither reader is cached.
            const std::uint32_t model = static_cast<std::uint32_t>(getIndex()) + static_cast<std::uint32_t>(getIfpBase());
            if (type == static_cast<std::uint32_t>(BLOCKING))
            {
                // Classic deliberately performs this otherwise unused lookup
                // first. Its nullable result is not an availability gate.
                static_cast<void>(lookupModel(model));
                requestModel(model, std::uint32_t{0x14});
                addRef();
                loadAll(true);
                if (!allowBlockingFail)
                {
                    // These are the original standard asserts. NDEBUG must
                    // remove the observations too, not merely their failure.
                    assert(isLoaded());
                    assert(hasModelLoaded(model));
                }
            }
            else
            {
                // Preserve the original else branch for every raw nonblocking
                // intent; do not invent enum clamping, retries or CPU refs.
                requestModel(model, std::uint32_t{0x04});
                addRef();
            }
            // Exceptions leave their accepted request/ref/load prefix intact.
            // This policy supplies no native reference effect or rollback.
        }
    };
}
