/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#include "NativeFrontendHost.h"
#include <array>
#include <atomic>
#include <filesystem>

namespace
{
    DWORD WINAPI Bootstrap(void* context)
    {
        // UAL can be configured to call plugins during DLL attachment. A separate
        // thread keeps filesystem work, Bridge hashing/loading and Core startup
        // outside that loader-lock context even when its defaults are overridden.
        try
        {
            const auto                 self = static_cast<HMODULE>(context);
            std::array<wchar_t, 32768> path{};
            const auto                 count = GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
            if (!count || count >= path.size())
                return 0;
            const auto root = std::filesystem::path(std::wstring(path.data(), count)).parent_path();
            const auto ini = root / L"MtaDefinitive.ini";
            const auto enabled = GetPrivateProfileIntW(L"MTA", L"Enabled", 0, ini.c_str());
            const auto persistence = GetPrivateProfileIntW(L"MTA", L"BindingPersistence", 0, ini.c_str());
            const auto input = GetPrivateProfileIntW(L"MTA", L"LocalInput", 0, ini.c_str());
            if (enabled != 1 || persistence > 1 || input > 1)
                return 0;
            const auto bridgePath = root / L"CLEO" / L"de-online" / L"de_client_bridge.dll";
            const auto bridge = LoadLibraryExW(bridgePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!bridge)
                return 0;
            const auto start = reinterpret_cast<deonline::NativeFrontendHostProtocol::Start>(GetProcAddress(bridge, "DE_NativeFrontendStartV1"));
            deonline::NativeFrontendHostProtocol::RequestV1 request;
            request.flags =
                (persistence ? deonline::NativeFrontendHostProtocol::BindingPersistence : 0u) | (input ? deonline::NativeFrontendHostProtocol::LocalInput : 0u);
            if (!start || start(&request) != 1)
                FreeLibrary(bridge);
            // A successful start pins Bridge and this ASI for the controller and
            // any retained worker/native cleanup. There is no unload-at-exit hook.
        }
        catch (...)
        {
            OutputDebugStringW(L"MTA Definitive native frontend initialization failed.\n");
        }
        return 0;
    }
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::atomic<bool> initialized{false};
    bool                     expected = false;
    if (!initialized.compare_exchange_strong(expected, true))
        return;
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&InitializeASI), &self))
        return;
    const auto thread = CreateThread(nullptr, 0, Bootstrap, self, 0, nullptr);
    if (thread)
        CloseHandle(thread);
    // Neither InitializeASI nor DllMain waits for the bootstrap/controller.
}
