/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share existing MTA cursor ownership across bounded and leased menus
 *****************************************************************************/
#pragma once

#include <memory>
#include <utility>

namespace FrontendCursorLease
{
    // Resource remains CSetCursorPosHook in production. Keeping its ownership
    // decisions together prevents a new menu lifetime from omitting pointer
    // suppression, and lets deterministic fixtures exercise the same ordering
    // without replacing the actual game's cursor hook or its expiry policy.
    template <class Resource>
    class Owner
    {
    public:
        Owner() noexcept = default;
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;

        template <class Window, class Admit>
        bool AdmitAfterNeutral(bool neutralVerified, Window window, Admit&& admit) noexcept
        {
            if (!neutralVerified)
                return false;
            if (attempted || resource || Resource::GetSingletonPtr())
            {
                fault = true;
                return false;
            }
            attempted = true;
            ended = false;
            try
            {
                resource = std::make_unique<Resource>();
            }
            catch (...)
            {
                ended = true;
                fault = true;
                return false;
            }
            if (!resource->BeginFrontendLease(window))
            {
                // Begin can fail after attaching a hook. Its same object must
                // survive until native input restoration and successful End.
                fault = true;
                return false;
            }
            return std::forward<Admit>(admit)();
        }

        template <class Draw>
        bool RenewBeforeDraw(Draw&& draw) noexcept
        {
            if (!resource || !resource->RenewFrontendLease())
            {
                fault = true;
                return false;
            }
            std::forward<Draw>(draw)();
            return true;
        }

        template <class Detach>
        bool DisarmBeforeDetach(Detach&& detach) noexcept
        {
            if (resource)
                resource->DisarmFrontendLease();
            return std::forward<Detach>(detach)();
        }

        bool EndAfterNative(bool nativeEnded) noexcept
        {
            if (!nativeEnded)
                return false;
            if (resource && !ended)
                ended = resource->EndFrontendLease();
            return ended;
        }

        Resource* Get() const noexcept { return resource.get(); }
        bool      Attempted() const noexcept { return attempted; }
        bool      Ended() const noexcept { return ended; }
        bool      Fault() const noexcept { return fault; }

    private:
        std::unique_ptr<Resource> resource;
        bool                      attempted = false, ended = true, fault = false;
    };
}
