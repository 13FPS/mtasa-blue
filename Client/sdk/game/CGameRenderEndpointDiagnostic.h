/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Separate value-only endpoint admission diagnostics for a V5 lease
 *****************************************************************************/
#pragma once
#include "CGameRenderObservation.h"

namespace mta::game_de
{
    inline constexpr std::uint32_t RenderEndpointDiagnosticVersionV1 = 1;
    enum class RenderEndpointPrepareGate : std::uint32_t
    {
        NotAttempted = 0,
        ModuleLookup,
        ModulePath,
        SystemPath,
        FileOpen,
        FileSize,
        HashProvider,
        HashCreate,
        FileRead,
        HashUpdate,
        FileDigest,
        Unwind,
        LoadedCodeRead,
        LoadedCodeHash,
        ModulePin,
        Complete
    };
    enum class RenderEndpointPrepareReason : std::uint32_t
    {
        NotAttempted = 0,
        Admitted,
        OperationFailed,
        Mismatch
    };
    enum class RenderEndpointErrorDomain : std::uint32_t
    {
        None = 0,
        Win32,
        NtStatus
    };
    enum class RenderEndpointLoadedMatch : std::uint32_t
    {
        NotChecked = 0,
        Match,
        Mismatch,
        ReadFault
    };
    enum class RenderEndpointSlotStatus : std::uint32_t
    {
        NotObserved = 0,
        Read,
        ReadFault
    };
    // Names identify worker-cached loaded module ownership, not trusted code or
    // endpoint admission. Other/unknown modules always remain unsupported.
    enum class RenderEndpointOwnerClass : std::uint32_t
    {
        Unknown = 0,
        Dxgi,
        GameImage,
        SteamOverlay,
        AmdHook,
        SocialClubRenderer,
        SocialClub,
        OtherImage,
        PrivateExecutable,
        OtherMemory
    };
    enum class RenderEndpointBranchKind : std::uint32_t
    {
        NotChecked = 0,
        NoSupportedBranch,
        RelativeE9,
        IndirectFF25,
        ReadFault
    };
    struct RenderEndpointDiagnosticV1
    {
        // Preparation and loadedEntryMatch describe the worker's pre-hook
        // admission attempt. Slot fields describe the completed native sample;
        // its branch may be our own installed detour when slotEqualsPinned=1.
        // Module classes/RVAs classify location only, never callable trust.
        std::uint32_t version = 0, generationLow = 0, generationHigh = 0, acceptedSnapshotsLow = 0, acceptedSnapshotsHigh = 0;
        std::uint32_t preparationGate = 0, preparationReason = 0, errorDomain = 0, errorCode = 0, loadedEntryMatch = 0;
        std::uint32_t slotStatus = 0, slotOwnerClass = 0, slotRva = 0, slotEqualsPinned = 0;
        std::uint32_t slotBranchKind = 0, slotBranchOwnerClass = 0, slotBranchRva = 0;
        std::uint32_t systemBranchKind = 0, systemBranchOwnerClass = 0, systemBranchRva = 0;
    };
    static_assert(sizeof(RenderEndpointDiagnosticV1) == 20 * sizeof(std::uint32_t));
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport) Status ReadGameRenderEndpointDiagnosticV1(std::uint32_t version, IGameRenderObservationV5* retainedLease,
                                                                               RenderEndpointDiagnosticV1* output) noexcept;
#else
    extern "C" Status ReadGameRenderEndpointDiagnosticV1(std::uint32_t version, IGameRenderObservationV5* retainedLease,
                                                         RenderEndpointDiagnosticV1* output) noexcept;
#endif

    // V2 selects an exact API6 lease, while the value-only diagnostic payload
    // and version argument remain V1. The original export stays V5-only.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport) Status ReadGameRenderEndpointDiagnosticV2(std::uint32_t version, IGameRenderObservationV6* retainedLease,
                                                                               RenderEndpointDiagnosticV1* output) noexcept;
#else
    extern "C" Status ReadGameRenderEndpointDiagnosticV2(std::uint32_t version, IGameRenderObservationV6* retainedLease,
                                                         RenderEndpointDiagnosticV1* output) noexcept;
#endif
}
