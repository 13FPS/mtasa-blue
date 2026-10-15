/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only native-root refusal diagnostics outside the render ABI
 *****************************************************************************/
#pragma once
#include "CGameRenderObservation.h"

namespace mta::game_de
{
    inline constexpr std::uint32_t RenderRootsDiagnosticVersionV1 = 1;
    enum class RenderRootsDiagnosticPhase : std::uint32_t
    {
        NotAttempted = 0,
        NativeRoots,
        BeforeCloseReadback,
        AfterCloseReadback
    };
    enum class RenderRootsDiagnosticCheckpoint : std::uint32_t
    {
        NotAttempted = 0,
        ViewportTable,
        Owner,
        Device,
        Manager,
        ManagerTable,
        Queue,
        Swapchain,
        CustomPresent,
        GpuCount,
        AfrCount,
        ContextRoot,
        Context,
        ContextDevice,
        Alternate,
        NodeMask,
        ContextManager,
        CommandData,
        CommandManager,
        BufferIndex,
        SelectorA,
        SelectorB,
        Texture,
        NativeTexture,
        View,
        Resource,
        ResourceTable,
        RawResource,
        TrackedState,
        ReadComplete,
        CustomClassification,
        AssociationValidation
    };
    enum class RenderRootsDiagnosticRefusalReason : std::uint32_t
    {
        None = 0,
        GuardedRead,
        RequiredNull,
        TableMismatch,
        TextureBias,
        StructuredException
    };
    enum class RenderRootsDiagnosticScalarMask : std::uint32_t
    {
        ContextNodeMask = 1,
        NativeBufferIndex = 2
    };
    struct RenderRootsDiagnosticV1
    {
        // A single E_POINTER in the render wire cannot identify a rejected
        // read, layout guard or SEH. This separate scalar cache explains that
        // refusal without changing its HRESULT, admission or rendering facts.
        // Version0 means no accepted callback has published a record yet.
        std::uint32_t version = 0, generationLow = 0, generationHigh = 0;
        std::uint32_t acceptedSnapshotsLow = 0, acceptedSnapshotsHigh = 0, dispatchEntriesLow = 0, dispatchEntriesHigh = 0;
        std::uint32_t queryPhase = 0, checkpoint = 0, refusalReason = 0, exceptionCode = 0;
        std::uint32_t scalarValidMask = 0, contextNodeMask = 0, nativeBufferIndex = 0;
        // Correlation belongs to the accepted after-original publication. A
        // publication miss or later fatal callback may retain older evidence.
        // None is not a proof of a coherent frame; checkpoint31 can accompany
        // unsupported association. Only StructuredException carries its code,
        // and COM Release faults are never reported as root-read exceptions.
    };
    static_assert(sizeof(RenderRootsDiagnosticV1) == 14 * sizeof(std::uint32_t));
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport) Status ReadGameRenderRootsDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* retainedLease,
                                                                            RenderRootsDiagnosticV1* output) noexcept;
#else
    extern "C" Status ReadGameRenderRootsDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* retainedLease, RenderRootsDiagnosticV1* output) noexcept;
#endif
}
