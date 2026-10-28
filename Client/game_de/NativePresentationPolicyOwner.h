/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core-bound persistent presentation provider and client retirement
 *****************************************************************************/
#pragma once
#include <game/CGamePresentationPolicy.h>
#include <cstdint>

namespace mta::game_de
{
    class NativePresentationPolicyOwner final
    {
    public:
        NativePresentationPolicyOwner() noexcept = default;
        NativePresentationPolicyOwner(const NativePresentationPolicyOwner&) = delete;
        NativePresentationPolicyOwner& operator=(const NativePresentationPolicyOwner&) = delete;

        // Only GameAdapter::Initialize binds this tuple, after its actual Core
        // identity/thread admission. It is separate from the Present producer.
        ClockResult BindOwner(CCoreInterface* owner, std::uint32_t producerThread, const void* adapterIdentity) noexcept;
        ClockResult Begin(PresentationPolicyStateV1& output) noexcept;
        ClockResult BindClient(std::uint64_t providerGeneration, const void* clientIdentity, PresentationPolicyClientV1& output) noexcept;
        ClockResult DisableVSync(const PresentationPolicyClientV1& client) noexcept;
        ClockResult RetireClient(const PresentationPolicyClientV1& client) noexcept;
        ClockResult RetireAll() noexcept;
        ClockResult End(std::uint64_t providerGeneration = 0) noexcept;
        ClockResult ReadState(PresentationPolicyStateV1& output) const noexcept;
        bool        Pending() const noexcept;

    private:
        ClockResult     Access() const noexcept;
        CCoreInterface* core = nullptr;
        const void*     adapter = nullptr;
        std::uint32_t   producer = 0;
        std::uint64_t   generation = 0;
    };
}
