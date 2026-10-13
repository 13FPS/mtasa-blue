/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep game adapter creation, callbacks and teardown owned by MTA Core
 *****************************************************************************/

#include "CGameAdapterModule.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <utility>
#include <windows.h>

namespace
{
    int moduleAnchor = 0;

    const char* StatusName(mta::game_de::Status status) noexcept
    {
        using mta::game_de::Status;
        switch (status)
        {
            case Status::Ok:
                return "Ok";
            case Status::InvalidArgument:
                return "InvalidArgument";
            case Status::WrongThread:
                return "WrongThread";
            case Status::NotInitialized:
                return "NotInitialized";
            case Status::Unsupported:
                return "Unsupported";
        }
        return "UnknownStatus";
    }

    constexpr std::uint64_t Capability(mta::game_de::AdapterCapability capability) noexcept
    {
        return static_cast<std::uint64_t>(capability);
    }
}

CGameAdapterModule::~CGameAdapterModule()
{
    Stop();
}

void CGameAdapterModule::SetError(const char* error) noexcept
{
    try
    {
        m_error = error;
    }
    catch (...)
    {
    }
}

bool CGameAdapterModule::Start(CCoreInterface* owner, const std::wstring& absoluteModulePath)
{
#if !defined(_WIN64)
    (void)owner;
    (void)absoluteModulePath;
    SetError("The Definitive Edition game adapter requires Windows x64; no module was loaded");
    return false;
#else
    if (IsStarted() || m_module)
    {
        SetError("A game adapter is already owned by this Core consumer");
        return false;
    }
    if (!owner)
    {
        SetError("A game adapter requires its owning MTA CCoreInterface identity");
        return false;
    }
    if (absoluteModulePath.empty() || absoluteModulePath.find(L'\0') != std::wstring::npos || !std::filesystem::path(absoluteModulePath).is_absolute())
    {
        SetError("Game adapter module path must be explicit, absolute and contain no embedded NUL");
        return false;
    }
    // An explicit backend path avoids an accidental game_sa or search-directory
    // fallback. Only dependencies use the module directory and system directory.
    const auto module = LoadLibraryExW(absoluteModulePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
    {
        m_error = "Loading the explicit Game DE adapter failed (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    const auto address = GetProcAddress(module, "CreateGameAdapter");
    if (!address)
    {
        FreeLibrary(module);
        SetError("Game DE module is missing the CreateGameAdapter ABI factory");
        return false;
    }
    using Factory = decltype(&mta::game_de::CreateGameAdapter);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address), "Windows module function pointers retain their representation");
    std::memcpy(&factory, &address, sizeof(factory));
    auto* adapter = factory(mta::game_de::AdapterAbiVersion);
    if (!adapter)
    {
        FreeLibrary(module);
        SetError("Game DE factory rejected the requested adapter ABI or could not allocate an instance");
        return false;
    }
    const auto status = adapter->Initialize(owner);
    if (status != mta::game_de::Status::Ok)
    {
        adapter->Shutdown();
        adapter->Release();
        FreeLibrary(module);
        return RecordStatus(status, "Initialize");
    }
    m_module = module;
    m_adapter = adapter;
    m_ownerThread = GetCurrentThreadId();
    m_ownerIdentity = owner;
    m_hasRenderObservation = false;
    m_error.clear();
    return true;
#endif
}

std::wstring CGameAdapterModule::AdjacentModulePath(std::string& error)
{
    HMODULE module = nullptr;
    // The address is inside Core in production, and inside the consumer test in
    // isolation. Resolve beside that module rather than the process cwd or GTA.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&moduleAnchor),
                            &module))
    {
        error = "Could not resolve the MTA consumer's owning module";
        return {};
    }
    std::array<wchar_t, 32768> path{};
    const auto                 length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
    {
        error = "MTA consumer module path is unavailable or truncated";
        return {};
    }
    error.clear();
    return (std::filesystem::path(std::wstring(path.data(), length)).parent_path() / L"game_de.dll").wstring();
}

bool CGameAdapterModule::StartAdjacent(CCoreInterface* owner)
{
    const auto path = AdjacentModulePath(m_error);
    return !path.empty() && Start(owner, path);
}

bool CGameAdapterModule::RequireOwnerThread(const char* operation)
{
    if (!m_adapter)
    {
        m_error = std::string(operation) + ": game adapter is not initialized";
        return false;
    }
    if (m_ownerThread != GetCurrentThreadId())
    {
        m_error = std::string(operation) + ": game adapter callback requires its owning Core thread";
        return false;
    }
    return true;
}

bool CGameAdapterModule::RecordStatus(mta::game_de::Status status, const char* operation)
{
    if (status == mta::game_de::Status::Ok)
    {
        m_error.clear();
        return true;
    }
    m_error = std::string(operation) + ": " + StatusName(status);
    return false;
}

bool CGameAdapterModule::Pulse()
{
    return RequireOwnerThread("Pulse") && RecordStatus(m_adapter->Pulse(), "Pulse");
}

bool CGameAdapterModule::Reset()
{
    if (!RequireOwnerThread("Reset") || !EndNativeRenderObservation() || !RecordStatus(m_adapter->Reset(), "Reset"))
        return false;
    m_hasRenderObservation = false;
    return true;
}

bool CGameAdapterModule::PublishLocalObservation(const mta::game_de::LocalObservation& observation)
{
    return RequireOwnerThread("PublishLocalObservation") && RecordStatus(m_adapter->PublishLocalObservation(observation), "PublishLocalObservation");
}

bool CGameAdapterModule::GetLocalObservation(mta::game_de::LocalObservation& observation)
{
    return RequireOwnerThread("GetLocalObservation") && RecordStatus(m_adapter->GetLocalObservation(observation), "GetLocalObservation");
}

bool CGameAdapterModule::SubmitControlActions(const mta::game_de::ControlActionFrame& frame)
{
    return RequireOwnerThread("SubmitControlActions") && RecordStatus(m_adapter->SubmitControlActions(frame), "SubmitControlActions");
}

bool CGameAdapterModule::GetMtaInputState(mta::game_de::MtaInputState& state)
{
    return RequireOwnerThread("GetMtaInputState") && RecordStatus(m_adapter->GetMtaInputState(state), "GetMtaInputState");
}

bool CGameAdapterModule::BindNativeInput()
{
    return RequireOwnerThread("BindNativeInput") && RecordStatus(m_adapter->BindNativeInput(), "BindNativeInput");
}

bool CGameAdapterModule::ReadNativeControllerState(std::uint32_t padIndex, mta::game_de::NativeControllerState& state)
{
    return RequireOwnerThread("ReadNativeControllerState") && RecordStatus(m_adapter->ReadNativeControllerState(padIndex, state), "ReadNativeControllerState");
}

bool CGameAdapterModule::ReadNativeDeviceState(mta::game_de::NativeDeviceState& state)
{
    return RequireOwnerThread("ReadNativeDeviceState") && RecordStatus(m_adapter->ReadNativeDeviceState(state), "ReadNativeDeviceState");
}

bool CGameAdapterModule::BeginNativeMovementTrial()
{
    return RequireOwnerThread("BeginNativeMovementTrial") && RecordStatus(m_adapter->BeginNativeMovementTrial(), "BeginNativeMovementTrial");
}

bool CGameAdapterModule::GetNativeMovementTrialState(mta::game_de::NativeMovementTrialState& state)
{
    return RequireOwnerThread("GetNativeMovementTrialState") && RecordStatus(m_adapter->GetNativeMovementTrialState(state), "GetNativeMovementTrialState");
}

bool CGameAdapterModule::EndNativeMovementTrial()
{
    return RequireOwnerThread("EndNativeMovementTrial") && RecordStatus(m_adapter->EndNativeMovementTrial(), "EndNativeMovementTrial");
}

bool CGameAdapterModule::GetNativeMovementEligibility(mta::game_de::NativeMovementEligibility& state)
{
    return RequireOwnerThread("GetNativeMovementEligibility") && RecordStatus(m_adapter->GetNativeMovementEligibility(state), "GetNativeMovementEligibility");
}

bool CGameAdapterModule::BorrowGameInterface(CGame*& game)
{
    if (!RequireOwnerThread("BorrowGameInterface"))
        return false;
    if (!(m_adapter->GetCapabilities() & Capability(mta::game_de::AdapterCapability::GameInterface)))
    {
        SetError("The selected adapter does not provide the CGame interface capability");
        return false;
    }
    auto* provided = m_adapter->GetGameInterface();
    if (!provided)
    {
        SetError("The selected adapter declared CGame support but returned no game interface");
        return false;
    }
    // The adapter owns this facade and its module lifetime. Core may borrow it
    // without taking the classic factory's Terminate/delete responsibility.
    game = provided;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::BeginNativeRenderObservation()
{
    if (!RequireOwnerThread("BeginNativeRenderObservation"))
        return false;
    if (m_renderObservation)
    {
        SetError("A render observation lease is already retained; end it before another begin");
        return false;
    }
    const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "CreateGameRenderObservation");
    if (!address)
    {
        SetError("The selected adapter does not export optional render diagnostics");
        return false;
    }
    using Factory = decltype(&mta::game_de::CreateGameRenderObservation);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address));
    std::memcpy(&factory, &address, sizeof(factory));
    m_renderObservation = factory(mta::game_de::RenderObservationApiVersion, m_ownerIdentity);
    if (!m_renderObservation)
    {
        SetError("The optional render factory rejected its version/owner or could not allocate a lease");
        return false;
    }
    // Failed Begin can still own pinned callback state. Keep the lease until
    // explicit End proves cleanup rather than releasing on a false start.
    m_hasRenderObservation = false;
    return RecordStatus(m_renderObservation->Begin(), "BeginNativeRenderObservation");
}

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationState& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationState"))
        return false;
    if (m_renderObservation)
    {
        mta::game_de::RenderObservationState next;
        if (!RecordStatus(m_renderObservation->ReadState(next), "GetNativeRenderObservationState"))
            return false;
        m_lastRenderObservation = next;
        m_hasRenderObservation = true;
    }
    if (!m_hasRenderObservation)
    {
        SetError("No native render observation state is available");
        return false;
    }
    state = m_lastRenderObservation;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::EndNativeRenderObservation() noexcept
{
    // Stop calls this during cleanup, so errors must not allocate or throw.
    if (!m_adapter || m_ownerThread != GetCurrentThreadId())
    {
        SetError("Render observation cleanup requires an initialized adapter's owning Core thread");
        return false;
    }
    if (!m_renderObservation)
        return true;
    if (m_renderObservation->End() != mta::game_de::Status::Ok)
    {
        SetError("Render observation End failed; lease, adapter and module retained for cleanup retry");
        return false;
    }
    mta::game_de::RenderObservationState final;
    if (m_renderObservation->ReadState(final) != mta::game_de::Status::Ok)
    {
        SetError("Final render observation read failed; lease, adapter and module retained for cleanup retry");
        return false;
    }
    if (final.active || final.pendingCleanup)
    {
        SetError("Ended render observation still reports active cleanup; lease, adapter and module retained");
        return false;
    }
    // Capture actual ended counters before Release. Core's final callback can
    // read this immutable value after the native observation object is gone.
    m_lastRenderObservation = final;
    m_hasRenderObservation = true;
    m_renderObservation->Release();
    m_renderObservation = nullptr;
    m_error.clear();
    return true;
}

void CGameAdapterModule::Stop() noexcept
{
    if (m_adapter && m_ownerThread != GetCurrentThreadId())
    {
        // Do not unload code while a different thread may own adapter callbacks.
        // Core must destroy this nonmovable consumer on its initialization thread.
        SetError("Game adapter teardown requires its owning Core thread; module retained");
        return;
    }
    if (m_renderObservation && !EndNativeRenderObservation())
        return;
    if (m_adapter)
    {
        if (m_adapter->Shutdown() != mta::game_de::Status::Ok)
        {
            // Native hooks can retain module code and state until a later owner
            // retry succeeds. Releasing or unloading after failure is unsafe.
            SetError("Game adapter Shutdown failed; adapter and module retained for cleanup retry");
            return;
        }
        m_adapter->Release();
        m_adapter = nullptr;
    }
    if (m_module)
    {
        FreeLibrary(static_cast<HMODULE>(m_module));
        m_module = nullptr;
    }
    m_ownerThread = 0;
    m_ownerIdentity = nullptr;
}

std::uint64_t CGameAdapterModule::GetCapabilities() const noexcept
{
    return m_adapter && m_ownerThread == GetCurrentThreadId() ? m_adapter->GetCapabilities() : 0;
}

bool CGameAdapterModule::IsGameplayReady() const noexcept
{
    const auto required = Capability(mta::game_de::AdapterCapability::GameInterface) | Capability(mta::game_de::AdapterCapability::EngineInputWrite) |
                          Capability(mta::game_de::AdapterCapability::RemotePlayerContext);
    return (GetCapabilities() & required) == required && m_adapter->GetGameInterface() != nullptr;
}

std::string CGameAdapterModule::GameplayUnavailableReason() const
{
    if (!m_adapter)
        return "Game DE adapter is not initialized";
    if (m_ownerThread != GetCurrentThreadId())
        return "Game DE gameplay capability checks require the owning Core thread";
    const auto  capabilities = GetCapabilities();
    std::string reason;
    for (const auto& entry : {std::pair{mta::game_de::AdapterCapability::GameInterface, "GameInterface"},
                              std::pair{mta::game_de::AdapterCapability::EngineInputWrite, "EngineInputWrite"},
                              std::pair{mta::game_de::AdapterCapability::RemotePlayerContext, "RemotePlayerContext"}})
        if (!(capabilities & Capability(entry.first)))
            reason += (reason.empty() ? "Game DE adapter lacks required gameplay capabilities: " : ", ") + std::string(entry.second);
    if (reason.empty() && !m_adapter->GetGameInterface())
        reason = "Game DE adapter advertises gameplay capabilities but returned no CGame interface";
    return reason;
}
