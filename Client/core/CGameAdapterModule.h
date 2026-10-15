/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core ownership of an explicitly selected game adapter module
 *****************************************************************************/

#pragma once

#include <game/CGameAdapter.h>
#include <game/CGameRenderObservation.h>
#include <game/CGameRenderEndpointDiagnostic.h>
#include <game/CGameRenderSetupDiagnostic.h>
#include <game/CGameRenderRootsDiagnostic.h>
#include <game/CGameRenderDeviceLease.h>
#include <game/CGameRenderDeviceWindowLease.h>
#include <cstdint>
#include <string>

class CGameAdapterModule final
{
public:
    CGameAdapterModule() = default;
    ~CGameAdapterModule();
    CGameAdapterModule(const CGameAdapterModule&) = delete;
    CGameAdapterModule& operator=(const CGameAdapterModule&) = delete;

    bool                Start(CCoreInterface* owner, const std::wstring& absoluteModulePath);
    bool                StartAdjacent(CCoreInterface* owner);
    static std::wstring AdjacentModulePath(std::string& error);
    bool                Pulse();
    bool                Reset();
    bool                PublishLocalObservation(const mta::game_de::LocalObservation& observation);
    bool                GetLocalObservation(mta::game_de::LocalObservation& observation);
    bool                SubmitControlActions(const mta::game_de::ControlActionFrame& frame);
    bool                GetMtaInputState(mta::game_de::MtaInputState& state);
    bool                BindNativeInput();
    bool                ReadNativeControllerState(std::uint32_t padIndex, mta::game_de::NativeControllerState& state);
    bool                ReadNativeDeviceState(mta::game_de::NativeDeviceState& state);
    bool                BeginNativeMovementTrial();
    bool                GetNativeMovementTrialState(mta::game_de::NativeMovementTrialState& state);
    bool                EndNativeMovementTrial();
    bool                GetNativeMovementEligibility(mta::game_de::NativeMovementEligibility& state);
    bool                BorrowGameInterface(CGame*& game);
    bool                BeginNativeRenderObservation();
    bool                BeginNativeRenderObservation(mta::game_de::RenderObservationRoute route);
    bool                GetNativeRenderObservationState(mta::game_de::RenderObservationState& state);
    bool                GetNativeRenderObservationState(mta::game_de::RenderObservationStateV2& state);
    bool                BeginNativeRenderOwnershipObservation(mta::game_de::RenderObservationRoute route);
    bool                GetNativeRenderObservationState(mta::game_de::RenderObservationStateV3& state);
    bool                BeginNativeRenderClassificationObservation(mta::game_de::RenderObservationRoute route);
    bool                GetNativeRenderObservationState(mta::game_de::RenderObservationStateV4& state);
    bool                BeginNativeRenderFrameBoundaryObservation(mta::game_de::RenderObservationRoute route);
    bool                GetNativeRenderObservationState(mta::game_de::RenderObservationStateV5& state);
    bool                BeginNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute route);
    // The device handoff is a separate request. Historical API6 callers keep
    // their callback-local references and never acquire an owned device here.
    bool BeginNativeRenderDeviceObservation();
    bool TakeNativeRenderDeviceLease(mta::game_de::RenderDeviceLeaseV1& output);
    bool BeginNativeRenderDeviceWindowObservation();
    bool TakeNativeRenderDeviceWindowLease(mta::game_de::RenderDeviceWindowLeaseV2& output);
    bool GetNativeRenderObservationState(mta::game_de::RenderObservationStateV6& state);
    bool GetNativeRenderEndpointDiagnostic(mta::game_de::RenderEndpointDiagnosticV1& state);
    bool GetNativeRenderSetupDiagnostic(mta::game_de::RenderSetupDiagnosticV1& state);
    bool GetNativeRenderRootsDiagnostic(mta::game_de::RenderRootsDiagnosticV1& state);
    bool EndNativeRenderObservation() noexcept;
    void Stop() noexcept;

    bool               IsStarted() const noexcept { return m_adapter != nullptr; }
    std::uint64_t      GetCapabilities() const noexcept;
    bool               IsGameplayReady() const noexcept;
    std::string        GameplayUnavailableReason() const;
    const std::string& GetError() const noexcept { return m_error; }

private:
    bool RequireOwnerThread(const char* operation);
    bool StartNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute route, bool captureDevice, bool captureWindow = false);
    bool RecordStatus(mta::game_de::Status status, const char* operation);
    void SetError(const char* error) noexcept;

    // Instance callbacks and teardown belong to the same Core thread. Keeping
    // the DLL alive until Release avoids retaining a vtable into unloaded code.
    void*                                                       m_module = nullptr;
    mta::game_de::IGameAdapter*                                 m_adapter = nullptr;
    mta::game_de::IGameRenderObservation*                       m_renderObservation = nullptr;
    mta::game_de::IGameRenderObservationV2*                     m_renderObservationV2 = nullptr;
    mta::game_de::RenderObservationState                        m_lastRenderObservation;
    mta::game_de::RenderObservationStateV2                      m_lastRenderObservationV2;
    bool                                                        m_hasRenderObservation = false;
    bool                                                        m_hasRenderObservationV2 = false;
    mta::game_de::IGameRenderObservationV3*                     m_renderObservationV3 = nullptr;
    mta::game_de::RenderObservationStateV3                      m_lastRenderObservationV3;
    bool                                                        m_hasRenderObservationV3 = false;
    mta::game_de::IGameRenderObservationV4*                     m_renderObservationV4 = nullptr;
    mta::game_de::RenderObservationStateV4                      m_lastRenderObservationV4;
    bool                                                        m_hasRenderObservationV4 = false;
    mta::game_de::IGameRenderObservationV5*                     m_renderObservationV5 = nullptr;
    mta::game_de::RenderObservationStateV5                      m_lastRenderObservationV5;
    bool                                                        m_hasRenderObservationV5 = false;
    mta::game_de::IGameRenderObservationV6*                     m_renderObservationV6 = nullptr;
    mta::game_de::TakeRenderDeviceLeaseV1                       m_renderDeviceTakeV1 = nullptr;
    mta::game_de::TakeRenderDeviceWindowLeaseV2                 m_renderDeviceWindowTakeV2 = nullptr;
    decltype(&mta::game_de::ReadGameRenderEndpointDiagnosticV2) m_renderEndpointReaderV2 = nullptr;
    mta::game_de::RenderObservationStateV6                      m_lastRenderObservationV6;
    bool                                                        m_hasRenderObservationV6 = false;
    mta::game_de::RenderEndpointDiagnosticV1                    m_lastEndpointDiagnostic;
    bool                                                        m_hasEndpointDiagnostic = false;
    decltype(&mta::game_de::ReadGameRenderSetupDiagnosticV1)    m_renderSetupReaderV1 = nullptr;
    mta::game_de::RenderSetupDiagnosticV1                       m_lastSetupDiagnostic;
    bool                                                        m_hasSetupDiagnostic = false;
    decltype(&mta::game_de::ReadGameRenderRootsDiagnosticV1)    m_renderRootsReaderV1 = nullptr;
    mta::game_de::RenderRootsDiagnosticV1                       m_lastRootsDiagnostic;
    bool                                                        m_hasRootsDiagnostic = false;
    mta::game_de::RenderObservationRoute                        m_renderRoute = mta::game_de::RenderObservationRoute::Unknown;
    CCoreInterface*                                             m_ownerIdentity = nullptr;
    std::uint32_t                                               m_ownerThread = 0;
    std::string                                                 m_error;
};
