/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core ownership of an explicitly selected game adapter module
 *****************************************************************************/

#pragma once

#include <game/CGameAdapter.h>
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
    void                Stop() noexcept;

    bool               IsStarted() const noexcept { return m_adapter != nullptr; }
    std::uint64_t      GetCapabilities() const noexcept;
    bool               IsGameplayReady() const noexcept;
    std::string        GameplayUnavailableReason() const;
    const std::string& GetError() const noexcept { return m_error; }

private:
    bool RequireOwnerThread(const char* operation);
    bool RecordStatus(mta::game_de::Status status, const char* operation);
    void SetError(const char* error) noexcept;

    // Instance callbacks and teardown belong to the same Core thread. Keeping
    // the DLL alive until Release avoids retaining a vtable into unloaded code.
    void*                       m_module = nullptr;
    mta::game_de::IGameAdapter* m_adapter = nullptr;
    std::uint32_t               m_ownerThread = 0;
    std::string                 m_error;
};
