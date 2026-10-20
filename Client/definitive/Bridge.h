/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#pragma once

#include <cstdint>
#include "NativeFrontendHost.h"

#ifdef DE_BRIDGE_EXPORTS
    #define DE_BRIDGE_API extern "C" __declspec(dllexport)
#else
    #define DE_BRIDGE_API extern "C" __declspec(dllimport)
#endif

// This ABI carries values rather than classic GTA pointers. A future DE adapter
// can supply transforms only after its game-thread/entity access is verified.
struct DEPlayerState
{
    std::uint32_t id;
    float         x;
    float         y;
    float         z;
    float         heading;
};

DE_BRIDGE_API std::uint32_t DE_ApiVersion();
// Runtime commands only request owner-thread transitions; capture fixes all416
// words coherently and can transiently refuse without waiting on native peers.
DE_BRIDGE_API std::uint32_t DE_FrontendRuntimeApiVersionV1();
DE_BRIDGE_API std::uint32_t DE_FrontendRuntimeStartV1();
DE_BRIDGE_API int           DE_FrontendRuntimeCommandV1(std::uint32_t generation, std::uint32_t sequence, std::uint32_t command);
DE_BRIDGE_API int           DE_FrontendRuntimeHeartbeatV1(std::uint32_t generation);
DE_BRIDGE_API int           DE_FrontendRuntimePollV1();
DE_BRIDGE_API int           DE_FrontendRuntimeCaptureV1();
DE_BRIDGE_API std::uint32_t DE_FrontendRuntimeReadV1(std::uint32_t field);
// The existing menu runtime may opt into actual configured local input after
// Hide. Its separate32-word record never changes the frontend416 ABI.
DE_BRIDGE_API std::uint32_t DE_LocalInputRuntimeApiVersionV1();
DE_BRIDGE_API std::uint32_t DE_LocalInputRuntimeStartV1();
// Persistent Apply is independently selected; historical starts keep their
// session-only configuration contract and require no persistence Core export.
DE_BRIDGE_API std::uint32_t DE_LocalInputRuntimeStartPersistentV1();
DE_BRIDGE_API int           DE_LocalInputRuntimeCaptureV1();
DE_BRIDGE_API std::uint32_t DE_LocalInputRuntimeReadV1(std::uint32_t field);
// ASI startup owns the existing worker from its dedicated native controller.
// Reads copy one coherent Native32; Stop requests retirement without joining.
DE_BRIDGE_API int DE_NativeFrontendStartV1(const deonline::NativeFrontendHostProtocol::RequestV1* request);
DE_BRIDGE_API int DE_NativeFrontendStopV1();
DE_BRIDGE_API int DE_NativeFrontendReadV1(std::uint32_t* output, std::uint32_t count);
// These guards keep the experimental CLEO adapter tied to the executable tested
// here. Matching its hash is a prerequisite, not proof that game natives work.
DE_BRIDGE_API int  DE_CheckExecutableBuild(const wchar_t* path);
DE_BRIDGE_API int  DE_CheckGameBuild();
DE_BRIDGE_API int  DE_BindRuntime();
DE_BRIDGE_API int  DE_ConnectLocal(std::uint32_t nonce);
DE_BRIDGE_API int  DE_Connect(const char* host, std::uint16_t port, std::uint64_t nonce);
DE_BRIDGE_API void DE_Disconnect();
DE_BRIDGE_API int  DE_Pump();
DE_BRIDGE_API int  DE_SubmitTransform(float x, float y, float z, float heading);
// Integer bit patterns avoid ambiguous float-register marshaling in script FFI.
DE_BRIDGE_API int DE_SubmitTransformBits(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t heading);
DE_BRIDGE_API std::uint32_t DE_LocalPlayerId();
// Returns total available states, while writing at most capacity records.
// Callers iterate only min(returned count, capacity); nullptr/zero queries size.
DE_BRIDGE_API std::uint32_t DE_ReadPlayers(DEPlayerState* output, std::uint32_t capacity);
// Component 0 is the ID; 1..4 are float bit patterns. This allocation-free view
// prevents script reloads from abandoning foreign output/string buffers.
DE_BRIDGE_API std::uint32_t DE_PlayerCount();
DE_BRIDGE_API std::uint32_t DE_ReadPlayerComponent(std::uint32_t index, std::uint32_t component);

// Input observations reuse MTA's controller type without changing game input.
// Only fields 0..19 are covered (mask 0xfffff); the four MTA logical extras stay
// unverified. A complete transaction advances previous/current exactly once.
// These calls require the successful runtime bind's owning thread. Read returns
// sign-extended int32 bits; masks distinguish unknown storage from observed zero.
DE_BRIDGE_API int DE_ControllerBegin();
DE_BRIDGE_API int DE_ControllerSet(std::uint32_t index, std::int32_t value);
DE_BRIDGE_API int DE_ControllerCommit();
DE_BRIDGE_API std::uint32_t DE_ControllerRead(std::uint32_t which, std::uint32_t index);
DE_BRIDGE_API std::uint32_t DE_ControllerCurrentMask();
DE_BRIDGE_API std::uint32_t DE_ControllerPreviousMask();
DE_BRIDGE_API void          DE_ControllerReset();

// The compatibility harness exercises the exact reader compiled into Game DE.
// Capture commits both candidate state pairs and the raw 127-source device
// table only after every read succeeds. Component reads use that cache rather
// than sampling memory separately for each script FFI call. These values do
// not establish physical key mappings or the candidate controller layout.
DE_BRIDGE_API int DE_NativePadBind();
DE_BRIDGE_API int DE_NativePadCapture();
DE_BRIDGE_API std::uint32_t DE_NativePadRead(std::uint32_t pad, std::uint32_t which, std::uint32_t index);
// Returns raw int32 bits; unavailable, unbound and out-of-range reads return0.
// Callers must require a successful capture before interpreting observed zero.
DE_BRIDGE_API std::uint32_t DE_NativeDeviceRead(std::uint32_t index);
DE_BRIDGE_API void          DE_NativePadReset();

// Exercise the actual MTA-owned backend and Core consumer in the compatibility
// harness. This is a bounded local movement trial, not a remote-player facade.
// Phase0=neutral,1=MTA walk forwards,2=forwards,3=forwards+sprint. No OS key
// mapping or animation commands are used; the module synthesizes MTA actions.
DE_BRIDGE_API int DE_MtaMovementBegin();
DE_BRIDGE_API int DE_MtaMovementSubmit(std::uint32_t phase);
DE_BRIDGE_API int DE_MtaMovementCapture();
// A successful capture fixes the metrics cache; End also publishes final metrics.
// Components0/1=bound/active,2..9=counters,10=failureReason,11=raw move state.
DE_BRIDGE_API std::uint32_t DE_MtaMovementRead(std::uint32_t component);
DE_BRIDGE_API int           DE_MtaMovementEnd();

// ProbeAPI2 schedules actual Core startup on one owned worker. Poll returns
// 0=pending,1=terminal,-1=unavailable. Cancel requests cooperative cleanup.
// Read exposes19 values only after terminal cleanup; field18=Win32 load error.
DE_BRIDGE_API int DE_CoreStartupBegin();
DE_BRIDGE_API int DE_CoreStartupPoll();
DE_BRIDGE_API int DE_CoreStartupCancel();
DE_BRIDGE_API std::uint32_t DE_CoreStartupRead(std::uint32_t component);

// This opt-in job keeps the actual Core's configured-input owner alive for a
// bounded read-only session. Capture freezes one worker-published sample before
// individual FFI reads, so a script cannot combine fields from different pulses.
DE_BRIDGE_API int DE_CoreInputBegin();
DE_BRIDGE_API int DE_CoreInputCapture();
DE_BRIDGE_API std::uint32_t DE_CoreInputRead(std::uint32_t component);

// Clock API1 owns a separate atomic 96-field read-only reference snapshot.
// Startup Poll/Cancel retain the existing single worker cleanup lifetime.
DE_BRIDGE_API std::uint32_t DE_CoreClockApiVersion();
DE_BRIDGE_API int           DE_CoreClockBegin();
DE_BRIDGE_API int           DE_CoreClockCapture();
DE_BRIDGE_API std::uint32_t DE_CoreClockRead(std::uint32_t component);

// The Set/restore reference uses its own API1 128-field report. Historical
// read-only clock entry points never select or expose a time-writing job.
DE_BRIDGE_API std::uint32_t DE_CoreClockSetApiVersion();
DE_BRIDGE_API int           DE_CoreClockSetBegin();
DE_BRIDGE_API int           DE_CoreClockSetCapture();
DE_BRIDGE_API std::uint32_t DE_CoreClockSetRead(std::uint32_t component);

// Application-frame observation owns a separate API1 report; the historical
// clock protocol retains its original read-only workload and field count.
DE_BRIDGE_API std::uint32_t DE_CoreApplicationFrameApiVersion();
DE_BRIDGE_API int           DE_CoreApplicationFrameBegin();
DE_BRIDGE_API int           DE_CoreApplicationFrameCapture();
DE_BRIDGE_API std::uint32_t DE_CoreApplicationFrameRead(std::uint32_t component);

// The composite startup observations have their own 192-word API1 report.
// Frame API1 remains128 words and grants no startup readiness claim.
DE_BRIDGE_API std::uint32_t DE_CoreStartupObservationApiVersion();
DE_BRIDGE_API int           DE_CoreStartupObservationBegin();
DE_BRIDGE_API int           DE_CoreStartupObservationCapture();
DE_BRIDGE_API std::uint32_t DE_CoreStartupObservationRead(std::uint32_t component);

// Configured movement keeps the same actual Core owner through native teardown.
// The producer's 96 values include eligibility, native restoration and real pin
// ownership; a pinned backend is distinct from an active adapter or live hooks.
DE_BRIDGE_API int DE_CoreMovementBegin();
DE_BRIDGE_API int DE_CoreMovementCapture();
DE_BRIDGE_API std::uint32_t DE_CoreMovementRead(std::uint32_t component);

// Renderer observations share the retained async Core owner; these exports
// expose only coherent numeric snapshots and never native rendering pointers.
DE_BRIDGE_API int DE_CoreRenderBegin();
DE_BRIDGE_API int DE_CoreRenderCapture();
DE_BRIDGE_API std::uint32_t DE_CoreRenderRead(std::uint32_t component);

// API2 selects an explicit native route and carries four additional scalar
// provenance fields. API1's forty-field cache and exports remain independent.
DE_BRIDGE_API std::uint32_t DE_CoreRenderApiVersionV2();
DE_BRIDGE_API int           DE_CoreRenderBeginV2(std::uint32_t route);
DE_BRIDGE_API int           DE_CoreRenderCaptureV2();
DE_BRIDGE_API std::uint32_t DE_CoreRenderReadV2(std::uint32_t component);
DE_BRIDGE_API std::uint32_t DE_CoreRenderApiVersionV3();
DE_BRIDGE_API int           DE_CoreRenderBeginV3(std::uint32_t route);
DE_BRIDGE_API int           DE_CoreRenderCaptureV3();
DE_BRIDGE_API std::uint32_t DE_CoreRenderReadV3(std::uint32_t component);
DE_BRIDGE_API std::uint32_t DE_CoreRenderApiVersionV4();
DE_BRIDGE_API int           DE_CoreRenderBeginV4(std::uint32_t route);
DE_BRIDGE_API int           DE_CoreRenderCaptureV4();
DE_BRIDGE_API std::uint32_t DE_CoreRenderReadV4(std::uint32_t component);
DE_BRIDGE_API std::uint32_t DE_CoreRenderApiVersionV5();
DE_BRIDGE_API int           DE_CoreRenderBeginV5(std::uint32_t route);
DE_BRIDGE_API int           DE_CoreRenderCaptureV5();
DE_BRIDGE_API std::uint32_t DE_CoreRenderReadV5(std::uint32_t component);
DE_BRIDGE_API std::uint32_t DE_CoreRenderApiVersionV6();
DE_BRIDGE_API int           DE_CoreRenderBeginV6(std::uint32_t route);
DE_BRIDGE_API int           DE_CoreRenderCaptureV6();
DE_BRIDGE_API std::uint32_t DE_CoreRenderReadV6(std::uint32_t component);

// Explicit MTA-owned offscreen startup uses its own scalar API1/64 cache.
// It never repurposes an API6 render field or implies native frame ownership.
DE_BRIDGE_API std::uint32_t DE_CoreGuiStartupApiVersion();
DE_BRIDGE_API int           DE_CoreGuiStartupBegin();
DE_BRIDGE_API int           DE_CoreGuiStartupCapture();
DE_BRIDGE_API std::uint32_t DE_CoreGuiStartupRead(std::uint32_t component);

// A separate API1/96 describes only MTA's own child HWND/composition surface.
// API6 and GUI startup's API1/64 retain their existing scalar meanings.
DE_BRIDGE_API std::uint32_t DE_CoreGuiSurfaceApiVersion();
DE_BRIDGE_API int           DE_CoreGuiSurfaceBegin();
DE_BRIDGE_API int           DE_CoreGuiSurfaceCapture();
DE_BRIDGE_API std::uint32_t DE_CoreGuiSurfaceRead(std::uint32_t component);

// Real MTA menu startup has an independent API1/160 cache. The script alone
// changes native player control and acknowledges its generation through this
// scalar mailbox; no native engine command executes on the Core worker.
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendApiVersion();
// Core14 keeps configured neutral native input on its owning worker. These
// copied256 values do not borrow the older script player-control mailbox.
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendInputApiVersion();
DE_BRIDGE_API int           DE_CoreGuiFrontendInputBegin();
DE_BRIDGE_API int           DE_CoreGuiFrontendInputCapture();
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendInputRead(std::uint32_t component);
// Core15 is a separate completed-Core lifecycle, never InitializeCore success.
// Its96 copied values retain Begin/after-view/stopped owner snapshots; the
// existing input256 cache remains available without changing old protocols.
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendStartupApiVersion();
DE_BRIDGE_API int           DE_CoreGuiFrontendStartupBegin();
DE_BRIDGE_API int           DE_CoreGuiFrontendStartupCapture();
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendStartupRead(std::uint32_t component);
// V2 composes Read96 on this completed Core before the existing bounded GUI.
// The owner32 cache proves that retirement retained the same backend and Core.
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendStartupApiVersionV2();
DE_BRIDGE_API int           DE_CoreGuiFrontendStartupBeginV2();
DE_BRIDGE_API int           DE_CoreGuiFrontendStartupCaptureClockV2();
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendStartupReadClockV2(std::uint32_t component);
DE_BRIDGE_API int           DE_CoreGuiFrontendStartupCaptureClockOwnerV2();
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendStartupReadClockOwnerV2(std::uint32_t component);
// A renewable host lifetime is independently selected. Neither these288
// values nor the generation token can be read through historical trial APIs.
DE_BRIDGE_API std::uint32_t DE_CorePersistentFrontendApiVersion();
DE_BRIDGE_API std::uint32_t DE_CorePersistentFrontendBegin();
DE_BRIDGE_API int           DE_CorePersistentFrontendHeartbeat(std::uint32_t generation);
DE_BRIDGE_API int           DE_CorePersistentFrontendCapture();
DE_BRIDGE_API std::uint32_t DE_CorePersistentFrontendRead(std::uint32_t component);
DE_BRIDGE_API int           DE_CorePersistentFrontendCaptureLifecycle();
DE_BRIDGE_API std::uint32_t DE_CorePersistentFrontendReadLifecycle(std::uint32_t component);
DE_BRIDGE_API int           DE_CoreGuiFrontendBegin();
DE_BRIDGE_API int           DE_CoreGuiFrontendCapture();
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendRead(std::uint32_t component);
DE_BRIDGE_API int           DE_CoreGuiFrontendControlCapture();
DE_BRIDGE_API std::uint32_t DE_CoreGuiFrontendControlRead(std::uint32_t component);
DE_BRIDGE_API int           DE_CoreGuiFrontendControlAck(std::uint32_t generation);
DE_BRIDGE_API int           DE_CoreGuiFrontendControlHeartbeat(std::uint32_t generation);
DE_BRIDGE_API int           DE_CoreGuiFrontendControlRevoke(std::uint32_t generation);
DE_BRIDGE_API int           DE_CoreGuiFrontendControlRestored(std::uint32_t generation);
DE_BRIDGE_API int           DE_CoreGuiFrontendControlFocused();
