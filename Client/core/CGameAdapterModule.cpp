/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep game adapter creation, callbacks and teardown owned by MTA Core
 *****************************************************************************/

#include "CGameAdapterModule.h"

#include <algorithm>
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
        // API6 may retain peers stopped inside their allocator. Its error
        // capacity is reserved before Begin, so diagnostics must never grow it
        // while that lease can still own suspended threads.
        if (m_renderObservationV6 && std::strlen(error) > m_error.capacity())
        {
            m_error.clear();
            return;
        }
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
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    m_hasEndpointDiagnostic = false;
    m_renderEndpointReaderV2 = nullptr;
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
    m_renderRoute = mta::game_de::RenderObservationRoute::Unknown;
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
        if (m_renderObservationV6)
            SetError("Game adapter callback requires its owning Core thread");
        else
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
    if (m_renderObservationV6)
    {
        // Stack formatting avoids even a temporary CRT allocation after a
        // provider failure has retained suspended peers for cleanup retry.
        std::array<char, 256> error{};
        std::size_t           offset = 0;
        const auto            append = [&](const char* part)
        {
            const auto bytes = (std::min)(std::strlen(part), error.size() - 1 - offset);
            std::memcpy(error.data() + offset, part, bytes);
            offset += bytes;
        };
        append(operation);
        append(": ");
        append(StatusName(status));
        SetError(error.data());
    }
    else
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
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    m_hasEndpointDiagnostic = false;
    m_renderEndpointReaderV2 = nullptr;
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
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
    if (m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6)
    {
        SetError("A render observation lease is already retained; end it before another begin");
        return false;
    }
    // A new allowed lease cannot expose a previous API6 setup generation.
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
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
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    return RecordStatus(m_renderObservation->Begin(), "BeginNativeRenderObservation");
}

bool CGameAdapterModule::BeginNativeRenderObservation(mta::game_de::RenderObservationRoute route)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("BeginNativeRenderObservationV2"))
        return false;
    if (route != RenderObservationRoute::D3D11 && route != RenderObservationRoute::D3D12)
    {
        SetError("An explicit supported render observation route is required");
        return false;
    }
    if (m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6)
    {
        SetError("A render observation lease is already retained; end it before selecting another version or route");
        return false;
    }
    // A new allowed lease cannot expose a previous API6 setup generation.
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
    const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "CreateGameRenderObservationV2");
    if (!address)
    {
        SetError("The selected adapter does not export version2 render diagnostics; no legacy route was substituted");
        return false;
    }
    using Factory = decltype(&CreateGameRenderObservationV2);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address));
    std::memcpy(&factory, &address, sizeof(factory));
    m_renderObservationV2 = factory(RenderObservationApiVersionV2, m_ownerIdentity, route);
    if (!m_renderObservationV2)
    {
        SetError("The version2 render factory rejected its version/owner/route or could not allocate a lease");
        return false;
    }
    m_renderRoute = route;
    m_hasRenderObservation = false;
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    return RecordStatus(m_renderObservationV2->Begin(), "BeginNativeRenderObservationV2");
}

bool CGameAdapterModule::BeginNativeRenderOwnershipObservation(mta::game_de::RenderObservationRoute route)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("BeginNativeRenderObservationV3"))
        return false;
    if (route != RenderObservationRoute::D3D12)
    {
        SetError("An explicit supported render observation route is required");
        return false;
    }
    if (m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6)
    {
        SetError("A render observation lease is already retained; end it before selecting another version or route");
        return false;
    }
    // A new allowed lease cannot expose a previous API6 setup generation.
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
    const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "CreateGameRenderObservationV3");
    if (!address)
    {
        SetError("The selected adapter does not export version3 render diagnostics; no legacy route was substituted");
        return false;
    }
    using Factory = decltype(&CreateGameRenderObservationV3);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address));
    std::memcpy(&factory, &address, sizeof(factory));
    m_renderObservationV3 = factory(RenderObservationApiVersionV3, m_ownerIdentity, route);
    if (!m_renderObservationV3)
    {
        SetError("The version3 render factory rejected its version/owner/route or could not allocate a lease");
        return false;
    }
    m_renderRoute = route;
    m_hasRenderObservation = false;
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    return RecordStatus(m_renderObservationV3->Begin(), "BeginNativeRenderObservationV3");
}

bool CGameAdapterModule::BeginNativeRenderClassificationObservation(mta::game_de::RenderObservationRoute route)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("BeginNativeRenderObservationV4"))
        return false;
    if (route != RenderObservationRoute::D3D12)
    {
        SetError("An explicit supported render observation route is required");
        return false;
    }
    if (m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6)
    {
        SetError("A render observation lease is already retained; end it before selecting another version or route");
        return false;
    }
    // A new allowed lease cannot expose a previous API6 setup generation.
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
    const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "CreateGameRenderObservationV4");
    if (!address)
    {
        SetError("The selected adapter does not export version4 render diagnostics; no legacy route was substituted");
        return false;
    }
    using Factory = decltype(&CreateGameRenderObservationV4);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address));
    std::memcpy(&factory, &address, sizeof(factory));
    m_renderObservationV4 = factory(RenderObservationApiVersionV4, m_ownerIdentity, route);
    if (!m_renderObservationV4)
    {
        SetError("The version4 render factory rejected its version/owner/route or could not allocate a lease");
        return false;
    }
    // Classification needs its own optional factory and state layout. Never
    // substitute an older observer that cannot report the custom presenter.
    m_renderRoute = route;
    m_hasRenderObservation = false;
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    return RecordStatus(m_renderObservationV4->Begin(), "BeginNativeRenderObservationV4");
}

bool CGameAdapterModule::BeginNativeRenderFrameBoundaryObservation(mta::game_de::RenderObservationRoute route)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("BeginNativeRenderObservationV5"))
        return false;
    if (route != RenderObservationRoute::D3D12)
    {
        SetError("An explicit supported render observation route is required");
        return false;
    }
    if (m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6)
    {
        SetError("A render observation lease is already retained; end it before selecting another version or route");
        return false;
    }
    // A newly allowed attempt must not relabel a previous generation's cached
    // evidence if the optional factory is missing or cannot allocate a lease.
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    m_hasEndpointDiagnostic = false;
    // A new allowed lease cannot expose a previous API6 setup generation.
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
    const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "CreateGameRenderObservationV5");
    if (!address)
    {
        SetError("The selected adapter does not export version5 render diagnostics; no legacy route was substituted");
        return false;
    }
    using Factory = decltype(&CreateGameRenderObservationV5);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address));
    std::memcpy(&factory, &address, sizeof(factory));
    m_renderObservationV5 = factory(RenderObservationApiVersionV5, m_ownerIdentity, route);
    if (!m_renderObservationV5)
    {
        SetError("The version5 render factory rejected its version/owner/route or could not allocate a lease");
        return false;
    }
    // Actual Present entry needs a distinct lease and state contract. A native
    // wrapper observation cannot silently stand in for the later COM boundary.
    m_renderRoute = route;
    m_hasRenderObservation = false;
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    m_hasEndpointDiagnostic = false;
    return RecordStatus(m_renderObservationV5->Begin(), "BeginNativeRenderObservationV5");
}

bool CGameAdapterModule::BeginNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute route)
{
    return StartNativeRenderDispatchObservation(route, false);
}

bool CGameAdapterModule::BeginNativeRenderDeviceObservation()
{
    return StartNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute::D3D12, true);
}

bool CGameAdapterModule::StartNativeRenderDispatchObservation(mta::game_de::RenderObservationRoute route, bool captureDevice)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("BeginNativeRenderObservationV6"))
        return false;
    if (route != RenderObservationRoute::D3D12)
    {
        SetError("An explicit supported render observation route is required");
        return false;
    }
    if (m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6)
    {
        SetError("A render observation lease is already retained; end it before selecting another version or route");
        return false;
    }
    // A newly allowed attempt must not relabel a previous generation's cached
    // evidence if the optional factory is missing or cannot allocate a lease.
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    m_hasEndpointDiagnostic = false;
    m_renderEndpointReaderV2 = nullptr;
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_renderDeviceTakeV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
    try
    {
        m_error.reserve(512);
    }
    catch (...)
    {
        SetError("Cannot reserve version6 cleanup diagnostic storage before Begin");
        return false;
    }
    const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "CreateGameRenderObservationV6");
    if (!address)
    {
        SetError("The selected adapter does not export version6 render diagnostics; no legacy route was substituted");
        return false;
    }
    using Factory = decltype(&CreateGameRenderObservationV6);
    Factory factory = nullptr;
    static_assert(sizeof(factory) == sizeof(address));
    std::memcpy(&factory, &address, sizeof(factory));
    m_renderObservationV6 = factory(RenderObservationApiVersionV6, m_ownerIdentity, route);
    if (!m_renderObservationV6)
    {
        SetError("The version6 render factory rejected its version/owner/route or could not allocate a lease");
        return false;
    }
    // Begin can retain suspended peers after a failed patch or resume. Resolve
    // this optional export while peers still run, so later cleanup diagnostics
    // never wait for a loader lock owned by one of those suspended threads.
    const auto diagnosticAddress = GetProcAddress(static_cast<HMODULE>(m_module), "ReadGameRenderEndpointDiagnosticV2");
    static_assert(sizeof(m_renderEndpointReaderV2) == sizeof(diagnosticAddress));
    std::memcpy(&m_renderEndpointReaderV2, &diagnosticAddress, sizeof(m_renderEndpointReaderV2));
    const auto setupAddress = GetProcAddress(static_cast<HMODULE>(m_module), "ReadGameRenderSetupDiagnosticV1");
    static_assert(sizeof(m_renderSetupReaderV1) == sizeof(setupAddress));
    std::memcpy(&m_renderSetupReaderV1, &setupAddress, sizeof(m_renderSetupReaderV1));
    const auto rootsAddress = GetProcAddress(static_cast<HMODULE>(m_module), "ReadGameRenderRootsDiagnosticV1");
    static_assert(sizeof(m_renderRootsReaderV1) == sizeof(rootsAddress));
    std::memcpy(&m_renderRootsReaderV1, &rootsAddress, sizeof(m_renderRootsReaderV1));
    if (captureDevice)
    {
        // Both optional exports are resolved and the fresh lease is requested
        // before Begin can retain a peer holding Windows' loader lock. No
        // historical observer is silently upgraded to persistent COM ownership.
        const auto                 requestAddress = GetProcAddress(static_cast<HMODULE>(m_module), "RequestGameRenderDeviceLeaseV1");
        const auto                 takeAddress = GetProcAddress(static_cast<HMODULE>(m_module), "TakeGameRenderDeviceLeaseV1");
        RequestRenderDeviceLeaseV1 request = nullptr;
        static_assert(sizeof(request) == sizeof(requestAddress));
        static_assert(sizeof(m_renderDeviceTakeV1) == sizeof(takeAddress));
        std::memcpy(&request, &requestAddress, sizeof(request));
        std::memcpy(&m_renderDeviceTakeV1, &takeAddress, sizeof(m_renderDeviceTakeV1));
        if (!request || !m_renderDeviceTakeV1)
        {
            m_renderDeviceTakeV1 = nullptr;
            // Begin has not run, so this fresh allocation owns no hooks, held
            // peers or device. Dispose it without creating a fake ended cache.
            m_renderObservationV6->Release();
            m_renderObservationV6 = nullptr;
            SetError("The selected adapter lacks the explicit device lease exports; no read-only fallback was started");
            return false;
        }
        if (!RecordStatus(request(RenderDeviceLeaseVersionV1, m_renderObservationV6), "RequestNativeRenderDeviceLease"))
        {
            m_renderDeviceTakeV1 = nullptr;
            m_renderObservationV6->Release();
            m_renderObservationV6 = nullptr;
            return false;
        }
    }
    // Preserve the old DXGI-entry result alongside the separately labeled
    // native dispatch record; an existing Present hook remains untouched.
    m_renderRoute = route;
    m_hasRenderObservation = false;
    m_hasRenderObservationV2 = false;
    m_hasRenderObservationV3 = false;
    m_hasRenderObservationV4 = false;
    m_hasRenderObservationV5 = false;
    m_hasRenderObservationV6 = false;
    m_hasEndpointDiagnostic = false;
    return RecordStatus(m_renderObservationV6->Begin(), "BeginNativeRenderObservationV6");
}

bool CGameAdapterModule::TakeNativeRenderDeviceLease(mta::game_de::RenderDeviceLeaseV1& output)
{
    if (!RequireOwnerThread("TakeNativeRenderDeviceLease"))
        return false;
    // Output owns its reference once transferred. Refuse a nonempty output
    // rather than overwriting ownership, and never replay a cached device.
    if (output.device || !m_renderObservationV6 || !m_renderDeviceTakeV1)
    {
        SetError("Taking a device requires an empty output and the active explicitly requested version6 lease");
        return false;
    }
    mta::game_de::RenderDeviceLeaseV1 next;
    if (!RecordStatus(m_renderDeviceTakeV1(mta::game_de::RenderDeviceLeaseVersionV1, m_renderObservationV6, &next), "TakeNativeRenderDeviceLease"))
        return false;
    output = next;
    // The receiver must finish End safely before calling cgui or any GPU API;
    // a transferred strong reference survives the observer's later removal.
    return true;
}

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationState& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationState"))
        return false;
    if (m_renderObservationV2 || m_hasRenderObservationV2 || m_renderObservationV3 || m_hasRenderObservationV3 || m_renderObservationV4 ||
        m_hasRenderObservationV4 || m_renderObservationV5 || m_hasRenderObservationV5 || m_renderObservationV6 || m_hasRenderObservationV6)
    {
        SetError("Version2 render observations require the version2 state contract");
        return false;
    }
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

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationStateV2& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationStateV2"))
        return false;
    if (m_renderObservationV2)
    {
        mta::game_de::RenderObservationStateV2 next;
        if (!RecordStatus(m_renderObservationV2->ReadState(next), "GetNativeRenderObservationStateV2"))
            return false;
        if (next.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) || (next.observedRoute && next.observedRoute != next.requestedRoute))
        {
            SetError("Render observation provider returned a different route; caller state preserved");
            return false;
        }
        m_lastRenderObservationV2 = next;
        m_hasRenderObservationV2 = true;
    }
    if (!m_hasRenderObservationV2)
    {
        SetError("No version2 native render observation state is available");
        return false;
    }
    state = m_lastRenderObservationV2;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationStateV3& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationStateV3"))
        return false;
    if (m_renderObservationV3)
    {
        mta::game_de::RenderObservationStateV3 next;
        if (!RecordStatus(m_renderObservationV3->ReadState(next), "GetNativeRenderObservationStateV3"))
            return false;
        if (next.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (next.base.observedRoute && next.base.observedRoute != next.base.requestedRoute))
        {
            SetError("Render observation provider returned a different route; caller state preserved");
            return false;
        }
        m_lastRenderObservationV3 = next;
        m_hasRenderObservationV3 = true;
    }
    if (!m_hasRenderObservationV3)
    {
        SetError("No version3 native render observation state is available");
        return false;
    }
    state = m_lastRenderObservationV3;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationStateV4& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationStateV4"))
        return false;
    if (m_renderObservationV4)
    {
        mta::game_de::RenderObservationStateV4 next;
        if (!RecordStatus(m_renderObservationV4->ReadState(next), "GetNativeRenderObservationStateV4"))
            return false;
        if (next.base.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (next.base.base.observedRoute && next.base.base.observedRoute != next.base.base.requestedRoute))
        {
            SetError("Render observation provider returned a different route; caller state preserved");
            return false;
        }
        m_lastRenderObservationV4 = next;
        m_hasRenderObservationV4 = true;
    }
    if (!m_hasRenderObservationV4 || m_renderObservationV5 || m_hasRenderObservationV5 || m_renderObservationV6 || m_hasRenderObservationV6)
    {
        SetError("No version4 native render observation state is available");
        return false;
    }
    state = m_lastRenderObservationV4;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationStateV5& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationStateV5"))
        return false;
    if (m_renderObservationV5)
    {
        mta::game_de::RenderObservationStateV5 next;
        if (!RecordStatus(m_renderObservationV5->ReadState(next), "GetNativeRenderObservationStateV5"))
            return false;
        if (next.base.base.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (next.base.base.base.observedRoute && next.base.base.base.observedRoute != next.base.base.base.requestedRoute))
        {
            SetError("Render observation provider returned a different route; caller state preserved");
            return false;
        }
        m_lastRenderObservationV5 = next;
        m_hasRenderObservationV5 = true;
    }
    if (!m_hasRenderObservationV5 || m_renderObservationV6 || m_hasRenderObservationV6)
    {
        SetError("No version5 native render observation state is available");
        return false;
    }
    state = m_lastRenderObservationV5;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderObservationState(mta::game_de::RenderObservationStateV6& state)
{
    if (!RequireOwnerThread("GetNativeRenderObservationStateV6"))
        return false;
    if (m_renderObservationV6)
    {
        mta::game_de::RenderObservationStateV6 next;
        if (!RecordStatus(m_renderObservationV6->ReadState(next), "GetNativeRenderObservationStateV6"))
            return false;
        if (next.base.base.base.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (next.base.base.base.base.observedRoute && next.base.base.base.base.observedRoute != next.base.base.base.base.requestedRoute))
        {
            SetError("Render observation provider returned a different route; caller state preserved");
            return false;
        }
        m_lastRenderObservationV6 = next;
        m_hasRenderObservationV6 = true;
    }
    if (!m_hasRenderObservationV6)
    {
        SetError("No version6 native render observation state is available");
        return false;
    }
    state = m_lastRenderObservationV6;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderEndpointDiagnostic(mta::game_de::RenderEndpointDiagnosticV1& state)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("GetNativeRenderEndpointDiagnostic"))
        return false;
    if (m_renderObservationV6)
    {
        // This optional export diagnoses why the existing API5 gate refused an
        // endpoint. Missing support never substitutes a different render lease.
        if (!m_renderEndpointReaderV2)
        {
            SetError("The retained API6 provider has no separate endpoint diagnostic export");
            return false;
        }
        RenderEndpointDiagnosticV1 next;
        if (!RecordStatus(m_renderEndpointReaderV2(RenderEndpointDiagnosticVersionV1, m_renderObservationV6, &next), "ReadGameRenderEndpointDiagnosticV2"))
            return false;
        if (next.version != RenderEndpointDiagnosticVersionV1)
        {
            SetError("Endpoint diagnostic provider returned a different version; caller output preserved");
            return false;
        }
        m_lastEndpointDiagnostic = next;
        m_hasEndpointDiagnostic = true;
    }
    else if (m_renderObservationV5)
    {
        // This optional export diagnoses why the existing API5 gate refused an
        // endpoint. Missing support never substitutes a different render lease.
        const auto address = GetProcAddress(static_cast<HMODULE>(m_module), "ReadGameRenderEndpointDiagnosticV1");
        if (!address)
        {
            SetError("The retained API5 provider has no separate endpoint diagnostic export");
            return false;
        }
        using Reader = decltype(&ReadGameRenderEndpointDiagnosticV1);
        Reader reader = nullptr;
        static_assert(sizeof(reader) == sizeof(address));
        std::memcpy(&reader, &address, sizeof(reader));
        RenderEndpointDiagnosticV1 next;
        if (!RecordStatus(reader(RenderEndpointDiagnosticVersionV1, m_renderObservationV5, &next), "ReadGameRenderEndpointDiagnosticV1"))
            return false;
        if (next.version != RenderEndpointDiagnosticVersionV1)
        {
            SetError("Endpoint diagnostic provider returned a different version; caller output preserved");
            return false;
        }
        m_lastEndpointDiagnostic = next;
        m_hasEndpointDiagnostic = true;
    }
    if (!m_hasEndpointDiagnostic || (!m_renderObservationV5 && !m_hasRenderObservationV5 && !m_renderObservationV6 && !m_hasRenderObservationV6))
    {
        SetError("No retained API5 or API6 endpoint diagnostic state is available");
        return false;
    }
    state = m_lastEndpointDiagnostic;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderSetupDiagnostic(mta::game_de::RenderSetupDiagnosticV1& state)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("GetNativeRenderSetupDiagnostic"))
        return false;
    if (m_renderObservationV6)
    {
        // Begin may fail after holding peers. Use only the export resolved
        // before Begin, and never turn an unavailable read into stale evidence.
        if (!m_renderSetupReaderV1)
        {
            SetError("The retained API6 provider has no separate setup diagnostic export");
            return false;
        }
        RenderSetupDiagnosticV1 next;
        if (!RecordStatus(m_renderSetupReaderV1(RenderSetupDiagnosticVersionV1, m_renderObservationV6, &next), "ReadGameRenderSetupDiagnosticV1"))
            return false;
        if (next.version != RenderSetupDiagnosticVersionV1)
        {
            SetError("Setup diagnostic provider returned a different version; caller output preserved");
            return false;
        }
        m_lastSetupDiagnostic = next;
        m_hasSetupDiagnostic = true;
    }
    if (!m_hasSetupDiagnostic || (!m_renderObservationV6 && !m_hasRenderObservationV6))
    {
        SetError("No retained API6 setup diagnostic state is available");
        return false;
    }
    state = m_lastSetupDiagnostic;
    m_error.clear();
    return true;
}

bool CGameAdapterModule::GetNativeRenderRootsDiagnostic(mta::game_de::RenderRootsDiagnosticV1& state)
{
    using namespace mta::game_de;
    if (!RequireOwnerThread("GetNativeRenderRootsDiagnostic"))
        return false;
    if (m_renderObservationV6)
    {
        // Resolve before Begin and read only the provider's published scalars.
        // A busy/held provider must not turn an earlier report into a new one.
        if (!m_renderRootsReaderV1)
        {
            SetError("The retained API6 provider has no separate roots diagnostic export");
            return false;
        }
        RenderRootsDiagnosticV1 next;
        if (!RecordStatus(m_renderRootsReaderV1(RenderRootsDiagnosticVersionV1, m_renderObservationV6, &next), "ReadGameRenderRootsDiagnosticV1"))
            return false;
        if (next.version != RenderRootsDiagnosticVersionV1)
        {
            SetError("Roots diagnostic provider returned a different version; caller output preserved");
            return false;
        }
        m_lastRootsDiagnostic = next;
        m_hasRootsDiagnostic = true;
    }
    if (!m_hasRootsDiagnostic || (!m_renderObservationV6 && !m_hasRenderObservationV6))
    {
        SetError("No retained API6 roots diagnostic state is available");
        return false;
    }
    state = m_lastRootsDiagnostic;
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
    if (m_renderObservationV6)
    {
        // Preserve the completed boundary report before releasing the lease.
        // Pending native hooks retain Core, the adapter and their DLL together.
        if (m_renderObservationV6->End() != mta::game_de::Status::Ok)
        {
            SetError("Version6 render observation End failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        mta::game_de::RenderObservationStateV6 final;
        if (m_renderObservationV6->ReadState(final) != mta::game_de::Status::Ok)
        {
            SetError("Final version6 render observation read failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        if (final.base.base.base.base.base.active || final.base.base.base.base.base.pendingCleanup ||
            final.base.base.base.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (final.base.base.base.base.observedRoute && final.base.base.base.base.observedRoute != final.base.base.base.base.requestedRoute))
        {
            SetError("Ended version6 render observation reports unsafe cleanup or a different route; lease, adapter and module retained");
            return false;
        }
        m_lastRenderObservationV6 = final;
        m_hasRenderObservationV6 = true;
        // A busy optional registry can miss the final read. Do not relabel an
        // earlier pre-End setup report as completed cleanup after Release.
        m_hasSetupDiagnostic = false;
        m_hasRootsDiagnostic = false;
        // Copy optional scalar evidence while its exact lease still exists.
        // A diagnostic miss cannot block an otherwise safe hook teardown.
        try
        {
            mta::game_de::RenderEndpointDiagnosticV1 diagnostic;
            GetNativeRenderEndpointDiagnostic(diagnostic);
            mta::game_de::RenderSetupDiagnosticV1 setup;
            GetNativeRenderSetupDiagnostic(setup);
            mta::game_de::RenderRootsDiagnosticV1 roots;
            GetNativeRenderRootsDiagnostic(roots);
        }
        catch (...)
        {
        }
        m_renderObservationV6->Release();
        m_renderObservationV6 = nullptr;
        m_renderDeviceTakeV1 = nullptr;
        m_error.clear();
        return true;
    }
    if (m_renderObservationV5)
    {
        // Preserve the completed boundary report before releasing the lease.
        // Pending native hooks retain Core, the adapter and their DLL together.
        if (m_renderObservationV5->End() != mta::game_de::Status::Ok)
        {
            SetError("Version5 render observation End failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        mta::game_de::RenderObservationStateV5 final;
        if (m_renderObservationV5->ReadState(final) != mta::game_de::Status::Ok)
        {
            SetError("Final version5 render observation read failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        if (final.base.base.base.base.active || final.base.base.base.base.pendingCleanup ||
            final.base.base.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (final.base.base.base.observedRoute && final.base.base.base.observedRoute != final.base.base.base.requestedRoute))
        {
            SetError("Ended version5 render observation reports unsafe cleanup or a different route; lease, adapter and module retained");
            return false;
        }
        m_lastRenderObservationV5 = final;
        m_hasRenderObservationV5 = true;
        // Copy optional scalar evidence while its exact lease still exists.
        // A diagnostic miss cannot block an otherwise safe hook teardown.
        try
        {
            mta::game_de::RenderEndpointDiagnosticV1 diagnostic;
            GetNativeRenderEndpointDiagnostic(diagnostic);
        }
        catch (...)
        {
        }
        m_renderObservationV5->Release();
        m_renderObservationV5 = nullptr;
        m_error.clear();
        return true;
    }
    if (m_renderObservationV4)
    {
        // The optional version owns its own state layout. Cache only its actual
        // ended values; never reinterpret them through the legacy API1 lease.
        if (m_renderObservationV4->End() != mta::game_de::Status::Ok)
        {
            SetError("Version4 render observation End failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        mta::game_de::RenderObservationStateV4 final;
        if (m_renderObservationV4->ReadState(final) != mta::game_de::Status::Ok)
        {
            SetError("Final version4 render observation read failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        if (final.base.base.base.active || final.base.base.base.pendingCleanup || final.base.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (final.base.base.observedRoute && final.base.base.observedRoute != final.base.base.requestedRoute))
        {
            SetError("Ended version4 render observation reports unsafe cleanup or a different route; lease, adapter and module retained");
            return false;
        }
        m_lastRenderObservationV4 = final;
        m_hasRenderObservationV4 = true;
        m_renderObservationV4->Release();
        m_renderObservationV4 = nullptr;
        m_error.clear();
        return true;
    }
    if (m_renderObservationV3)
    {
        // The optional version owns its own state layout. Cache only its actual
        // ended values; never reinterpret them through the legacy API1 lease.
        if (m_renderObservationV3->End() != mta::game_de::Status::Ok)
        {
            SetError("Version3 render observation End failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        mta::game_de::RenderObservationStateV3 final;
        if (m_renderObservationV3->ReadState(final) != mta::game_de::Status::Ok)
        {
            SetError("Final version3 render observation read failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        if (final.base.base.active || final.base.base.pendingCleanup || final.base.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (final.base.observedRoute && final.base.observedRoute != final.base.requestedRoute))
        {
            SetError("Ended version3 render observation reports unsafe cleanup or a different route; lease, adapter and module retained");
            return false;
        }
        m_lastRenderObservationV3 = final;
        m_hasRenderObservationV3 = true;
        m_renderObservationV3->Release();
        m_renderObservationV3 = nullptr;
        m_error.clear();
        return true;
    }
    if (m_renderObservationV2)
    {
        // The optional version owns its own state layout. Cache only its actual
        // ended values; never reinterpret them through the legacy API1 lease.
        if (m_renderObservationV2->End() != mta::game_de::Status::Ok)
        {
            SetError("Version2 render observation End failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        mta::game_de::RenderObservationStateV2 final;
        if (m_renderObservationV2->ReadState(final) != mta::game_de::Status::Ok)
        {
            SetError("Final version2 render observation read failed; lease, adapter and module retained for cleanup retry");
            return false;
        }
        if (final.base.active || final.base.pendingCleanup || final.requestedRoute != static_cast<std::uint32_t>(m_renderRoute) ||
            (final.observedRoute && final.observedRoute != final.requestedRoute))
        {
            SetError("Ended version2 render observation reports unsafe cleanup or a different route; lease, adapter and module retained");
            return false;
        }
        m_lastRenderObservationV2 = final;
        m_hasRenderObservationV2 = true;
        m_renderObservationV2->Release();
        m_renderObservationV2 = nullptr;
        m_error.clear();
        return true;
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
    if ((m_renderObservation || m_renderObservationV2 || m_renderObservationV3 || m_renderObservationV4 || m_renderObservationV5 || m_renderObservationV6) &&
        !EndNativeRenderObservation())
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
    m_renderEndpointReaderV2 = nullptr;
    m_renderSetupReaderV1 = nullptr;
    m_renderRootsReaderV1 = nullptr;
    m_hasSetupDiagnostic = false;
    m_hasRootsDiagnostic = false;
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
