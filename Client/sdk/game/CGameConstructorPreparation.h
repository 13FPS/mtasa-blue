/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit constructor-service preparation through the retained adapter
 *****************************************************************************/
#pragma once
#include "CGameClockService.h"

namespace mta::game_de
{
    inline constexpr std::uint32_t ConstructorPreparationVersionV1 = 1;
    enum ConstructorPreparationFlags : std::uint32_t
    {
        ConstructorPreparationComplete = 1,
        ConstructorPreparationFailed = 2,
        ConstructorPreparationReadyNotified = 4
    };

    // Completed rows are acknowledged MTA constructor defaults, while a failed
    // current row may already own native effects. Values expose no SDK pointer
    // and cannot certify that the remaining CGame services or factories exist.
    struct ConstructorPreparationStateV1
    {
        std::uint32_t size = sizeof(ConstructorPreparationStateV1), version = ConstructorPreparationVersionV1;
        std::uint64_t generation = 0;
        std::uint32_t completedRows = 0, registeredRows = 0, totalRows = 0, flags = 0;
        std::uint32_t reserved[8]{};
    };
    static_assert(std::is_standard_layout_v<ConstructorPreparationStateV1> && std::is_trivially_copyable_v<ConstructorPreparationStateV1> &&
                  sizeof(ConstructorPreparationStateV1) == 64);

    // This explicit write-capable mode reuses the existing Renew/Read/End clock
    // owner. It does not widen the read-only startup observer or Adapter ABI6.
    // Keep the adapter/module retained until End succeeds, including failed
    // setup with cleanup debt. Read refuses before an admitted row and after
    // retirement; no implicit preparation, lease renewal or replay occurs.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_CONSTRUCTOR_PREPARATION_EXPORT __declspec(dllexport)
#else
    #define MTA_CONSTRUCTOR_PREPARATION_EXPORT
#endif
    extern "C" MTA_CONSTRUCTOR_PREPARATION_EXPORT Status BeginGameConstructorPreparationV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                           const ClockServiceRequestV1* request,
                                                                                           ClockServiceStateV1*         output) noexcept;
    extern "C" MTA_CONSTRUCTOR_PREPARATION_EXPORT Status ReadGameConstructorPreparationV1(std::uint32_t version, IGameAdapter* adapter,
                                                                                          std::uint64_t                  generation,
                                                                                          ConstructorPreparationStateV1* output) noexcept;
#undef MTA_CONSTRUCTOR_PREPARATION_EXPORT
}
