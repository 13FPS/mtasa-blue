/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "Bridge.h"
#include "MtaControllerSnapshot.h"
#include "TransportClient.h"
#include "CoreStartupReference.h"
#include "../game_de/NativePadReader.h"
#include "../core/CGameAdapterModule.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <new>
#include <windows.h>

namespace
{
    // Explicit ownership keeps Winsock setup/shutdown out of DLL static constructors
    // and destructors, which execute under the Windows loader lock. The host must
    // disconnect before unloading the DLL, and call the ABI from one owning thread.
    deonline::TransportClient*             client = nullptr;
    bool                                   runtimeBound = false;
    int                                    moduleAnchor = 0;
    std::atomic<DWORD>                     controllerThread{0};
    deonline::ControllerSnapshot           controllerSnapshot;
    mta::game_de::NativePadReader          nativePadReader;
    std::array<CControllerState, 2>        nativeCurrent;
    std::array<CControllerState, 2>        nativePrevious;
    mta::game_de::NativeDeviceState        nativeDevice;
    bool                                   nativeSnapshot = false;
    CGameAdapterModule*                    movementAdapter = nullptr;
    int                                    movementOwnerIdentity = 0;
    mta::game_de::NativeMovementTrialState movementState;
    bool                                   movementSnapshot = false;
    deonline::CoreStartupReference         coreStartup;

    void ClearNativePad()
    {
        nativePadReader.Reset();
        nativeCurrent = {};
        nativePrevious = {};
        nativeDevice = {};
        nativeSnapshot = false;
    }

    bool ControllerAccessAllowed()
    {
        // Reject foreign callers before touching snapshot storage or the legacy
        // runtime flag. Transport retains its existing single-thread contract.
        return controllerThread.load(std::memory_order_acquire) == GetCurrentThreadId() && runtimeBound;
    }

    float FloatFromBits(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

}

std::uint32_t DE_ApiVersion()
{
    return 2;
}

int DE_BindRuntime()
{
    if (runtimeBound)
        return 1;
    if (!DE_CheckGameBuild())
        return 0;
    const auto runtime = GetModuleHandleW(L"cleo_redux64.asi");
    if (!runtime)
        return 0;
    // Resolve the documented public SDK without depending on private runtime
    // implementation code. No script context or game pointer is retained.
    using GetHostId = int (*)();
    using GetSDKVersion = long (*)();
    using OnRuntimeInit = void (*)(void (*)());
    const auto host = reinterpret_cast<GetHostId>(GetProcAddress(runtime, "GetHostId"));
    const auto sdk = reinterpret_cast<GetSDKVersion>(GetProcAddress(runtime, "GetSDKVersion"));
    const auto onInit = reinterpret_cast<OnRuntimeInit>(GetProcAddress(runtime, "OnRuntimeInit"));
    if (!host || !sdk || !onInit || host() != 8 || sdk() < 4)
        return 0;
    HMODULE self = nullptr;
    // CLEO has no callback-unregister API. Pinning prevents a stale callback if
    // a script is reloaded/unloaded; Windows reclaims this module at process exit.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&moduleAnchor), &self))
        return 0;
    onInit(DE_Disconnect);
    runtimeBound = true;
    controllerThread.store(GetCurrentThreadId(), std::memory_order_release);
    return 1;
}

int DE_ConnectLocal(std::uint32_t nonce)
{
    return runtimeBound ? DE_Connect("127.0.0.1", 22040, nonce) : 0;
}
int DE_Connect(const char* host, std::uint16_t port, std::uint64_t nonce)
{
    DE_Disconnect();
    try
    {
        client = new deonline::TransportClient;
        if (client->Open(host, port, nonce))
            return 1;
    }
    catch (...)
    {
        // C ABI callers cannot safely receive a C++ exception across module boundaries.
    }
    DE_Disconnect();
    return 0;
}
void DE_Disconnect()
{
    // A runtime restart can request cancellation, but only the worker may
    // release its Core handle; never wait for the loader on a CLEO callback.
    coreStartup.Cancel();
    // Connection/runtime cleanup must discard input coverage and any unfinished
    // transaction so observations cannot survive an adapter session teardown.
    controllerSnapshot.Reset();
    DE_MtaMovementEnd();
    ClearNativePad();
    delete client;
    client = nullptr;
}

int DE_MtaMovementBegin()
{
    if (!ControllerAccessAllowed() || movementAdapter || coreStartup.IsPending())
        return 0;
    try
    {
        movementState = {};
        movementSnapshot = false;
        // Like the existing Core consumer host fixture, this diagnostic owner
        // is an opaque identity. The actual module implements the trial; the
        // compatibility bridge supplies no substitute CGame or hook lifecycle.
        movementAdapter = new (std::nothrow) CGameAdapterModule;
        if (!movementAdapter || !movementAdapter->StartAdjacent(reinterpret_cast<CCoreInterface*>(&movementOwnerIdentity)) ||
            !movementAdapter->SubmitControlActions({}) || !movementAdapter->BeginNativeMovementTrial())
        {
            DE_MtaMovementEnd();
            return 0;
        }
        if (DE_MtaMovementCapture() != 1)
        {
            DE_MtaMovementEnd();
            return 0;
        }
        return 1;
    }
    catch (...)
    {
        DE_MtaMovementEnd();
        return 0;
    }
}

int DE_MtaMovementSubmit(std::uint32_t phase)
{
    if (!ControllerAccessAllowed() || !movementAdapter || !movementAdapter->IsStarted() || phase > 3)
        return 0;
    try
    {
        mta::game_de::ControlActionFrame frame;
        if (phase != 0)
            frame.heldActions = MtaInput::ActionBit(FORWARDS);
        if (phase == 1)
            frame.heldActions |= MtaInput::ActionBit(WALK);
        else if (phase == 3)
            frame.heldActions |= MtaInput::ActionBit(SPRINT);
        return movementAdapter->SubmitControlActions(frame) ? 1 : 0;
    }
    catch (...)
    {
        DE_MtaMovementEnd();
        return 0;
    }
}

int DE_MtaMovementCapture()
{
    if (!ControllerAccessAllowed() || !movementAdapter || !movementAdapter->IsStarted())
        return 0;
    try
    {
        mta::game_de::NativeMovementTrialState next;
        if (!movementAdapter->GetNativeMovementTrialState(next))
            return 0;
        movementState = next;
        movementSnapshot = true;
        return 1;
    }
    catch (...)
    {
        return 0;
    }
}

std::uint32_t DE_MtaMovementRead(std::uint32_t component)
{
    if (!ControllerAccessAllowed() || !movementSnapshot)
        return 0;
    const std::array<std::uint64_t, 12> values = {movementState.bound,
                                                  movementState.active,
                                                  movementState.callbackCount,
                                                  movementState.scopedSteps,
                                                  movementState.routedQueries,
                                                  movementState.neutralQueries,
                                                  movementState.restoredSteps,
                                                  movementState.restoreFailures,
                                                  movementState.foreignThreadCallbacks,
                                                  movementState.nestedCallbacks,
                                                  static_cast<std::uint32_t>(movementState.failureReason),
                                                  movementState.lastMoveState};
    if (component >= values.size() || values[component] > (std::numeric_limits<std::uint32_t>::max)())
        return 0;
    return static_cast<std::uint32_t>(values[component]);
}

int DE_MtaMovementEnd()
{
    if (!movementAdapter)
        return 1;
    try
    {
        // A refused shutdown must retain the real adapter/module; unloading
        // code with live native hooks would leave an invalid game callback.
        if (movementAdapter->IsStarted())
        {
            if (!movementAdapter->EndNativeMovementTrial())
                return 0;
            mta::game_de::NativeMovementTrialState finalState;
            if (!movementAdapter->GetNativeMovementTrialState(finalState))
                return 0;
            movementState = finalState;
            movementSnapshot = true;
        }
        movementAdapter->Stop();
        if (movementAdapter->IsStarted())
            return 0;
        delete movementAdapter;
        movementAdapter = nullptr;
        return 1;
    }
    catch (...)
    {
        return 0;
    }
}
int DE_CoreStartupBegin()
{
    // The job owns the real Core/backend until terminal cleanup. Keep the
    // separate scoped writer and transport session out of that lifetime.
    return ControllerAccessAllowed() && !movementAdapter && !client && coreStartup.Begin(&moduleAnchor) ? 1 : 0;
}

int DE_CoreStartupPoll()
{
    return ControllerAccessAllowed() ? coreStartup.Poll() : -1;
}

int DE_CoreStartupCancel()
{
    return ControllerAccessAllowed() && coreStartup.Cancel() ? 1 : 0;
}

std::uint32_t DE_CoreStartupRead(std::uint32_t component)
{
    return ControllerAccessAllowed() ? coreStartup.Read(component) : 0;
}

int DE_CoreInputBegin()
{
    // Share the startup worker's ownership and cancellation instead of loading
    // a second Core or retaining a Core export across its terminal unload.
    return ControllerAccessAllowed() && !movementAdapter && !client && coreStartup.BeginInputReference(&moduleAnchor) ? 1 : 0;
}

int DE_CoreInputCapture()
{
    return ControllerAccessAllowed() && coreStartup.CaptureInput() ? 1 : 0;
}

std::uint32_t DE_CoreInputRead(std::uint32_t component)
{
    return ControllerAccessAllowed() ? coreStartup.ReadInput(component) : 0;
}

int DE_CoreMovementBegin()
{
    return ControllerAccessAllowed() && !movementAdapter && !client && coreStartup.BeginMovementReference(&moduleAnchor) ? 1 : 0;
}

int DE_CoreMovementCapture()
{
    return ControllerAccessAllowed() && coreStartup.CaptureMovement() ? 1 : 0;
}

std::uint32_t DE_CoreMovementRead(std::uint32_t component)
{
    return ControllerAccessAllowed() ? coreStartup.ReadMovement(component) : 0;
}

int DE_CoreRenderBegin()
{
    return ControllerAccessAllowed() && !movementAdapter && !client && coreStartup.BeginRenderReference(&moduleAnchor) ? 1 : 0;
}

int DE_CoreRenderCapture()
{
    return ControllerAccessAllowed() && coreStartup.CaptureRender() ? 1 : 0;
}

std::uint32_t DE_CoreRenderRead(std::uint32_t component)
{
    return ControllerAccessAllowed() ? coreStartup.ReadRender(component) : 0;
}

std::uint32_t DE_CoreRenderApiVersionV2()
{
    return DefinitiveRenderReference::ApiVersionV2;
}

int DE_CoreRenderBeginV2(std::uint32_t route)
{
    return ControllerAccessAllowed() && !movementAdapter && !client && coreStartup.BeginRenderReferenceV2(&moduleAnchor, route) ? 1 : 0;
}

int DE_CoreRenderCaptureV2()
{
    return ControllerAccessAllowed() && coreStartup.CaptureRenderV2() ? 1 : 0;
}

std::uint32_t DE_CoreRenderReadV2(std::uint32_t component)
{
    return ControllerAccessAllowed() ? coreStartup.ReadRenderV2(component) : 0;
}

int DE_Pump()
{
    try
    {
        return client && client->Pump() ? 1 : 0;
    }
    catch (...)
    {
        return 0;
    }
}
int DE_SubmitTransform(float x, float y, float z, float heading)
{
    try
    {
        return client && client->Submit({CVector(x, y, z), heading}) ? 1 : 0;
    }
    catch (...)
    {
        return 0;
    }
}

int DE_SubmitTransformBits(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t heading)
{
    return DE_SubmitTransform(FloatFromBits(x), FloatFromBits(y), FloatFromBits(z), FloatFromBits(heading));
}
std::uint32_t DE_LocalPlayerId()
{
    return client ? client->PlayerId() : 0;
}

std::uint32_t DE_ReadPlayers(DEPlayerState* output, std::uint32_t capacity)
{
    if (!client || (!output && capacity))
        return 0;
    const auto count = static_cast<std::uint32_t>(client->Players().size());
    for (std::uint32_t index = 0; index < std::min(count, capacity); ++index)
    {
        const auto& player = client->Players()[index];
        output[index] = {player.id, player.transform.position.fX, player.transform.position.fY, player.transform.position.fZ, player.transform.heading};
    }
    return count;
}

std::uint32_t DE_PlayerCount()
{
    return client ? static_cast<std::uint32_t>(client->Players().size()) : 0;
}

std::uint32_t DE_ReadPlayerComponent(std::uint32_t index, std::uint32_t component)
{
    if (!client || index >= client->Players().size() || component > 4)
        return 0;
    const auto& player = client->Players()[index];
    if (!component)
        return player.id;
    const float   values[] = {player.transform.position.fX, player.transform.position.fY, player.transform.position.fZ, player.transform.heading};
    std::uint32_t bits;
    std::memcpy(&bits, &values[component - 1], sizeof(bits));
    return bits;
}

int DE_ControllerBegin()
{
    return ControllerAccessAllowed() && controllerSnapshot.Begin() ? 1 : 0;
}

int DE_ControllerSet(std::uint32_t index, std::int32_t value)
{
    return ControllerAccessAllowed() && controllerSnapshot.Set(index, value) ? 1 : 0;
}

int DE_ControllerCommit()
{
    return ControllerAccessAllowed() && controllerSnapshot.Commit() ? 1 : 0;
}

std::uint32_t DE_ControllerRead(std::uint32_t which, std::uint32_t index)
{
    return ControllerAccessAllowed() ? controllerSnapshot.Read(which, index) : 0;
}

std::uint32_t DE_ControllerCurrentMask()
{
    return ControllerAccessAllowed() ? controllerSnapshot.CurrentMask() : 0;
}

std::uint32_t DE_ControllerPreviousMask()
{
    return ControllerAccessAllowed() ? controllerSnapshot.PreviousMask() : 0;
}

void DE_ControllerReset()
{
    if (ControllerAccessAllowed())
        controllerSnapshot.Reset();
}

int DE_NativePadBind()
{
    if (!ControllerAccessAllowed())
        return 0;
    ClearNativePad();
    return nativePadReader.Bind() ? 1 : 0;
}

int DE_NativePadCapture()
{
    if (!ControllerAccessAllowed())
        return 0;
    std::array<CControllerState, 2> current;
    std::array<CControllerState, 2> previous;
    mta::game_de::NativeDeviceState device;
    for (std::uint32_t pad = 0; pad < 2; ++pad)
    {
        if (!nativePadReader.Read(pad, current[pad], previous[pad]))
        {
            ClearNativePad();
            return 0;
        }
    }
    if (!nativePadReader.ReadDeviceState(device))
    {
        ClearNativePad();
        return 0;
    }
    // Publish all diagnostic caches together. A failed device read must not
    // leave fresh candidate bytes beside device values from an earlier capture.
    nativeCurrent = current;
    nativePrevious = previous;
    nativeDevice = device;
    nativeSnapshot = true;
    return 1;
}

std::uint32_t DE_NativePadRead(std::uint32_t pad, std::uint32_t which, std::uint32_t index)
{
    static constexpr std::array<signed short CControllerState::*, 24> fields = {&CControllerState::LeftStickX,
                                                                                &CControllerState::LeftStickY,
                                                                                &CControllerState::RightStickX,
                                                                                &CControllerState::RightStickY,
                                                                                &CControllerState::LeftShoulder1,
                                                                                &CControllerState::LeftShoulder2,
                                                                                &CControllerState::RightShoulder1,
                                                                                &CControllerState::RightShoulder2,
                                                                                &CControllerState::DPadUp,
                                                                                &CControllerState::DPadDown,
                                                                                &CControllerState::DPadLeft,
                                                                                &CControllerState::DPadRight,
                                                                                &CControllerState::Start,
                                                                                &CControllerState::Select,
                                                                                &CControllerState::ButtonSquare,
                                                                                &CControllerState::ButtonTriangle,
                                                                                &CControllerState::ButtonCross,
                                                                                &CControllerState::ButtonCircle,
                                                                                &CControllerState::ShockButtonL,
                                                                                &CControllerState::ShockButtonR,
                                                                                &CControllerState::m_bChatIndicated,
                                                                                &CControllerState::m_bPedWalk,
                                                                                &CControllerState::m_bVehicleMouseLook,
                                                                                &CControllerState::m_bRadioTrackSkip};
    if (!ControllerAccessAllowed() || !nativeSnapshot || pad >= 2 || which >= 2 || index >= fields.size())
        return 0;
    const auto& state = which == 0 ? nativeCurrent[pad] : nativePrevious[pad];
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(state.*fields[index]));
}

std::uint32_t DE_NativeDeviceRead(std::uint32_t index)
{
    if (!ControllerAccessAllowed() || !nativeSnapshot || index >= mta::game_de::NativeDeviceStateCount)
        return 0;
    // The script FFI transports uint32 bits; preserve every raw signed value
    // without assigning physical key or MTA action semantics to this table.
    return static_cast<std::uint32_t>(nativeDevice.sources[index]);
}

void DE_NativePadReset()
{
    if (ControllerAccessAllowed())
        ClearNativePad();
}
