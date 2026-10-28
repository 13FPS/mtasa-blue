#pragma once
#include <game/CGamePresentationPolicy.h>
#include <array>

namespace PresentationHandoffFixture
{
    enum Control : std::uint32_t
    {
        Normal,
        FailBegin,
        FailRetire,
        FailEnd,
        PoisonRead,
        BindReturnsStampThenFails,
        UnpinnedRead,
        MalformedPinRead
    };
    struct Snapshot
    {
        std::uint32_t                            begins = 0, binds = 0, disables = 0, retires = 0, ends = 0, resets = 0, shutdowns = 0, releases = 0;
        std::uint32_t                            order = 0, retireOrder = 0, shutdownOrder = 0;
        mta::game_de::PresentationPolicyStateV1  policy;
        mta::game_de::PresentationPolicyClientV1 client;
    };
    using SetControl = void (*)(Control) noexcept;
    using Read = void (*)(Snapshot*) noexcept;
    using CreateStorage = bool (*)(mta::game_de::PresentationPolicyBorrowLifetime*) noexcept;
}
