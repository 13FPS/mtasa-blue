/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only setup refusal diagnostics, separate from observation ABI
 *****************************************************************************/
#pragma once
#include "CGameRenderObservation.h"

namespace mta::game_de
{
    inline constexpr std::uint32_t RenderSetupDiagnosticVersionV1 = 1;
    enum class RenderSetupStage : std::uint32_t
    {
        NotAttempted = 0,
        PrepareFrameBoundary,
        NativeThreadModule,
        NativeThreadExport,
        NativeThreadImage,
        NativeThreadStubRead,
        NativeThreadStubMatch,
        CollectReserve,
        CollectSnapshot,
        CollectFirst,
        CollectOpen,
        CollectNext,
        ThreadLimit,
        HoldReserve,
        HoldSuspend,
        StableEnumerate,
        StableThreadId,
        StableDuplicate,
        StableSuspend,
        StableContext,
        StableInstruction,
        StableLimit,
        TransactionBegin,
        TransactionCurrent,
        TransactionPeers,
        FinalContext,
        FinalInstruction,
        InFlight,
        ProtectionBaseline,
        WrapperChange,
        LifecycleOwnership,
        LifecycleChange,
        CallsiteChange,
        TransactionCommit,
        HooksCommitted,
        RollbackCallsite,
        TransactionAbort,
        SafeToResume,
        RelayRelease,
        Resume,
        Complete
    };
    enum class RenderSetupErrorDomain : std::uint32_t
    {
        None = 0,
        Win32,
        NtStatus,
        Detours
    };
    struct RenderSetupDiagnosticV1
    {
        // Only the owning worker writes this cache. The first install refusal
        // survives cleanup, so a successful detach cannot hide its cause. This
        // report grants no callback, endpoint, rendering or GPU admission.
        std::uint32_t version = 0, generationLow = 0, generationHigh = 0, attempted = 0;
        std::uint32_t stage = 0, failureStage = 0, errorDomain = 0, errorCode = 0;
        // Counts describe the install attempt: collected peers, peak own holds,
        // and stable-discovery passes attempted. Commit flags retain historical
        // facts: the vendor commit returned success, then postcommit ownership
        // checks passed. A zero hooksCommitted does not prove that no patch was
        // committed. installSucceeded also requires safe protection restoration
        // and every owned resume. Cleanup has its own fields below.
        std::uint32_t initialThreads = 0, heldThreads = 0, stablePasses = 0;
        std::uint32_t transactionCommitted = 0, hooksCommitted = 0;
        std::uint32_t cleanupAttempted = 0, cleanupComplete = 0, retainedSuspensions = 0, safetyHeldSuspensions = 0, installSucceeded = 0;
    };
    static_assert(sizeof(RenderSetupDiagnosticV1) == 18 * sizeof(std::uint32_t));
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport) Status ReadGameRenderSetupDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* retainedLease,
                                                                            RenderSetupDiagnosticV1* output) noexcept;
#else
    extern "C" Status ReadGameRenderSetupDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* retainedLease, RenderSetupDiagnosticV1* output) noexcept;
#endif
}
