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
#include <game/CGameFrontendInputLease.h>
#include <game/CGameLocalInputLease.h>
#include <game/CGameClockService.h>
#include <game/CGameApplicationFrameObservation.h>
#include <game/CGameStartupObservation.h>
#include <game/CGameTimeService.h>
#include <game/CGameFrontendAdmission.h>
#include <cstdint>
#include <string>

class CGameAdapterModule final
{
public:
    CGameAdapterModule() = default;
    ~CGameAdapterModule();
    CGameAdapterModule(const CGameAdapterModule&) = delete;
    CGameAdapterModule& operator=(const CGameAdapterModule&) = delete;

    bool                 Start(CCoreInterface* owner, const std::wstring& absoluteModulePath);
    bool                 StartAdjacent(CCoreInterface* owner);
    static std::wstring  AdjacentModulePath(std::string& error);
    bool                 Pulse();
    bool                 Reset();
    bool                 PublishLocalObservation(const mta::game_de::LocalObservation& observation);
    bool                 GetLocalObservation(mta::game_de::LocalObservation& observation);
    bool                 SubmitControlActions(const mta::game_de::ControlActionFrame& frame);
    bool                 GetMtaInputState(mta::game_de::MtaInputState& state);
    bool                 BindNativeInput();
    bool                 ReadNativeControllerState(std::uint32_t padIndex, mta::game_de::NativeControllerState& state);
    bool                 ReadNativeDeviceState(mta::game_de::NativeDeviceState& state);
    bool                 BeginNativeMovementTrial();
    bool                 GetNativeMovementTrialState(mta::game_de::NativeMovementTrialState& state);
    bool                 EndNativeMovementTrial();
    bool                 BeginFrontendInputLease(void* window, mta::game_de::FrontendInputLeaseStateV1& state);
    bool                 RenewFrontendInputLease(std::uint64_t generation);
    bool                 ReadFrontendInputLease(mta::game_de::FrontendInputLeaseStateV1& state);
    bool                 EndFrontendInputLease(std::uint64_t generation);
    bool                 BeginLocalInputLease(void* window, mta::game_de::LocalInputLeaseStateV1& state);
    bool                 RenewLocalInputLease(std::uint64_t generation);
    bool                 ReadLocalInputLease(mta::game_de::LocalInputLeaseStateV1& state);
    bool                 EndLocalInputLease(std::uint64_t generation);
    bool                 BeginNativeClockService(const mta::game_de::ClockServiceRequestV1& request, mta::game_de::ClockServiceStateV1& state);
    bool                 SubmitNativeClockCommand(const mta::game_de::ClockCommandV1& command);
    bool                 RenewNativeClockService(std::uint64_t generation);
    bool                 ReadNativeClockService(mta::game_de::ClockServiceStateV1& state);
    bool                 EndNativeClockService(std::uint64_t generation);
    bool                 BeginNativeApplicationFrameObservation(const mta::game_de::ClockServiceRequestV1& request, mta::game_de::ClockServiceStateV1& state);
    bool                 ReadNativeApplicationFrameObservation(std::uint64_t generation, mta::game_de::ApplicationFrameObservationStateV1& state);
    bool                 BeginNativeStartupObservation(const mta::game_de::ClockServiceRequestV1& request, mta::game_de::ClockServiceStateV1& state);
    bool                 ReadNativeStartupObservation(std::uint64_t generation, mta::game_de::StartupObservationStateV1& state);
    bool                 HasGameTimeService() const noexcept;
    mta::game_de::Status ReadGameTimeState(mta::game_de::GameTimeStateV1& state) noexcept;
    mta::game_de::Status ReadFrontendAdmission(mta::game_de::FrontendAdmissionStateV1& state) noexcept;
    bool                 GetNativeMovementEligibility(mta::game_de::NativeMovementEligibility& state);
    bool                 BorrowGameInterface(CGame*& game);
    bool                 BeginNativeRenderObservation();
    bool                 BeginNativeRenderObservation(mta::game_de::RenderObservationRoute route);
    bool                 GetNativeRenderObservationState(mta::game_de::RenderObservationState& state);
    bool                 GetNativeRenderObservationState(mta::game_de::RenderObservationStateV2& state);
    bool                 BeginNativeRenderOwnershipObservation(mta::game_de::RenderObservationRoute route);
    bool                 GetNativeRenderObservationState(mta::game_de::RenderObservationStateV3& state);
    bool                 BeginNativeRenderClassificationObservation(mta::game_de::RenderObservationRoute route);
    bool                 GetNativeRenderObservationState(mta::game_de::RenderObservationStateV4& state);
    bool                 BeginNativeRenderFrameBoundaryObservation(mta::game_de::RenderObservationRoute route);
    bool                 GetNativeRenderObservationState(mta::game_de::RenderObservationStateV5& state);
    bool                 BeginNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute route);
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
    bool RequireClockRetired();
    bool RequireLocalInputRetired();
    bool StartNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute route, bool captureDevice, bool captureWindow = false);
    bool RecordStatus(mta::game_de::Status status, const char* operation);
    void SetError(const char* error) noexcept;

    // Instance callbacks and teardown belong to the same Core thread. Keeping
    // the DLL alive until Release avoids retaining a vtable into unloaded code.
    void*                       m_module = nullptr;
    mta::game_de::IGameAdapter* m_adapter = nullptr;
    // Resolve before native hook admission. Cleanup must never need loader
    // lookup while a retained peer might be suspended inside loader code.
    decltype(&mta::game_de::BeginGameFrontendInputLeaseV1)          m_frontendLeaseBegin = nullptr;
    decltype(&mta::game_de::RenewGameFrontendInputLeaseV1)          m_frontendLeaseRenew = nullptr;
    decltype(&mta::game_de::ReadGameFrontendInputLeaseV1)           m_frontendLeaseRead = nullptr;
    decltype(&mta::game_de::EndGameFrontendInputLeaseV1)            m_frontendLeaseEnd = nullptr;
    decltype(&mta::game_de::BeginGameLocalInputLeaseV1)             m_localLeaseBegin = nullptr;
    decltype(&mta::game_de::RenewGameLocalInputLeaseV1)             m_localLeaseRenew = nullptr;
    decltype(&mta::game_de::ReadGameLocalInputLeaseV1)              m_localLeaseRead = nullptr;
    decltype(&mta::game_de::EndGameLocalInputLeaseV1)               m_localLeaseEnd = nullptr;
    decltype(&mta::game_de::BeginGameClockServiceV1)                m_clockBegin = nullptr;
    decltype(&mta::game_de::SubmitGameClockCommandV1)               m_clockSubmit = nullptr;
    decltype(&mta::game_de::RenewGameClockServiceV1)                m_clockRenew = nullptr;
    decltype(&mta::game_de::ReadGameClockServiceV1)                 m_clockRead = nullptr;
    decltype(&mta::game_de::EndGameClockServiceV1)                  m_clockEnd = nullptr;
    decltype(&mta::game_de::BeginGameApplicationFrameObservationV1) m_applicationFrameBegin = nullptr;
    decltype(&mta::game_de::ReadGameApplicationFrameObservationV1)  m_applicationFrameRead = nullptr;
    decltype(&mta::game_de::BeginGameStartupObservationV1)          m_startupObservationBegin = nullptr;
    decltype(&mta::game_de::ReadGameStartupObservationV1)           m_startupObservationRead = nullptr;
    decltype(&mta::game_de::ReadGameTimeStateV1)                    m_gameTimeRead = nullptr;
    decltype(&mta::game_de::ReadGameFrontendAdmissionV1)            m_frontendAdmissionRead = nullptr;
    mta::game_de::IGameRenderObservation*                           m_renderObservation = nullptr;
    mta::game_de::IGameRenderObservationV2*                         m_renderObservationV2 = nullptr;
    mta::game_de::RenderObservationState                            m_lastRenderObservation;
    mta::game_de::RenderObservationStateV2                          m_lastRenderObservationV2;
    bool                                                            m_hasRenderObservation = false;
    bool                                                            m_hasRenderObservationV2 = false;
    mta::game_de::IGameRenderObservationV3*                         m_renderObservationV3 = nullptr;
    mta::game_de::RenderObservationStateV3                          m_lastRenderObservationV3;
    bool                                                            m_hasRenderObservationV3 = false;
    mta::game_de::IGameRenderObservationV4*                         m_renderObservationV4 = nullptr;
    mta::game_de::RenderObservationStateV4                          m_lastRenderObservationV4;
    bool                                                            m_hasRenderObservationV4 = false;
    mta::game_de::IGameRenderObservationV5*                         m_renderObservationV5 = nullptr;
    mta::game_de::RenderObservationStateV5                          m_lastRenderObservationV5;
    bool                                                            m_hasRenderObservationV5 = false;
    mta::game_de::IGameRenderObservationV6*                         m_renderObservationV6 = nullptr;
    mta::game_de::TakeRenderDeviceLeaseV1                           m_renderDeviceTakeV1 = nullptr;
    mta::game_de::TakeRenderDeviceWindowLeaseV2                     m_renderDeviceWindowTakeV2 = nullptr;
    decltype(&mta::game_de::ReadGameRenderEndpointDiagnosticV2)     m_renderEndpointReaderV2 = nullptr;
    mta::game_de::RenderObservationStateV6                          m_lastRenderObservationV6;
    bool                                                            m_hasRenderObservationV6 = false;
    mta::game_de::RenderEndpointDiagnosticV1                        m_lastEndpointDiagnostic;
    bool                                                            m_hasEndpointDiagnostic = false;
    decltype(&mta::game_de::ReadGameRenderSetupDiagnosticV1)        m_renderSetupReaderV1 = nullptr;
    mta::game_de::RenderSetupDiagnosticV1                           m_lastSetupDiagnostic;
    bool                                                            m_hasSetupDiagnostic = false;
    decltype(&mta::game_de::ReadGameRenderRootsDiagnosticV1)        m_renderRootsReaderV1 = nullptr;
    mta::game_de::RenderRootsDiagnosticV1                           m_lastRootsDiagnostic;
    bool                                                            m_hasRootsDiagnostic = false;
    mta::game_de::RenderObservationRoute                            m_renderRoute = mta::game_de::RenderObservationRoute::Unknown;
    CCoreInterface*                                                 m_ownerIdentity = nullptr;
    std::uint32_t                                                   m_ownerThread = 0;
    // A failed Begin may retain a peer holding the allocator or loader lock.
    // Keep diagnostics bounded until unload, and serialize other services
    // until explicit End/Reset acknowledges clock cleanup.
    bool m_clockDiagnostics = false;
    bool m_clockAttemptPending = false;
    // Even failed native admission may owe End. Keep this separate from the
    // optional export group and never infer retirement from inactive input.
    bool          m_localLeaseAttemptPending = false;
    bool          m_localLeaseDiagnostics = false;
    std::uint64_t m_localLeaseGeneration = 0;
    std::string   m_error;
};
