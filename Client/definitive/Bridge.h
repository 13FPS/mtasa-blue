/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#pragma once

#include <cstdint>

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
