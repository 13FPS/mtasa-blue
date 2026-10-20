/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        launch/Main.cpp
 *  PURPOSE:     Launcher
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "DefinitivePreflight/Preflight.h"

// C++ STL
#include <array>        // std::array for safe command buffer
#include <bit>          // std::bit_cast for safe function pointer conversion
#include <expected>     // std::expected/std::unexpected for error handling
#include <format>       // std::format for string formatting
#include <ranges>       // std::ranges::copy/copy_n for safe copying
#include <span>         // std::span for memory view with bounds checking
#include <string_view>  // std::string_view for string parameters and constants
#include <type_traits>  // std::is_trivially_copyable_v for compile-time checks
#include <utility>      // std::forward for perfect forwarding

// C STL
#include <cstring>  // std::strlen for C string length

// Platform-specific headers
#include <psapi.h>  // GetModuleInformation, MODULEINFO

/*
    IMPORTANT

    If this project changes, a new release build should be copied into
    the Shared\data\launchers directory.

    The .exe in Shared\data\launchers will be used by the installer and updater.

    (set flag.new_client_exe on the build server to generate new exe)
*/

namespace mta::launcher
{
    enum class LoadResult : std::uint32_t
    {
        Success = 0,
        PathError = 1,
        LoadError = 2,
        FunctionError = 3
    };

    enum class DllLoadError : std::uint32_t
    {
        FileNotFound = 1,
        InvalidPath = 2,
        LoadFailed = 3
    };

    // concepts for type safety
    template <std::size_t N>
    concept ValidBufferSize = N > 0 && N <= 65536;

#ifdef MTA_DEBUG
    constexpr std::string_view LOADER_DLL = "loader_d.dll";
#else
    constexpr std::string_view LOADER_DLL = "loader.dll";
#endif

    // RAII wrapper for preserving GetLastError
    class [[nodiscard]] LastErrorPreserver
    {
        DWORD saved_error_;

    public:
        LastErrorPreserver() noexcept : saved_error_(GetLastError()) {}

        ~LastErrorPreserver() noexcept { SetLastError(saved_error_); }

        LastErrorPreserver(const LastErrorPreserver&) = delete;
        LastErrorPreserver& operator=(const LastErrorPreserver&) = delete;
        LastErrorPreserver(LastErrorPreserver&&) = delete;
        LastErrorPreserver& operator=(LastErrorPreserver&&) = delete;
    };

    // RAII scope_exit with perfect forwarding
    template <typename F>
    struct [[nodiscard]] scope_exit
    {
        F f;

        explicit scope_exit(F&& func) noexcept : f(std::forward<F>(func)) {}

        ~scope_exit() noexcept { f(); }

        scope_exit(const scope_exit&) = delete;
        scope_exit& operator=(const scope_exit&) = delete;
        scope_exit(scope_exit&&) = delete;
        scope_exit& operator=(scope_exit&&) = delete;
    };

    template <typename F>
    constexpr auto make_scope_exit(F&& f) noexcept
    {
        return scope_exit<F>{std::forward<F>(f)};
    }

    // DLL directory guard with error preservation and optimized allocation
    class [[nodiscard]] DllDirectoryGuard
    {
        std::wstring original_dir;
        bool         changed = false;

    public:
        DllDirectoryGuard()
        {
            LastErrorPreserver error_guard;
            DWORD              len = GetDllDirectoryW(0, nullptr);
            if (len > 0)
            {
                try
                {
                    original_dir.resize_and_overwrite(len,
                                                      [](wchar_t* buf, std::size_t n) -> std::size_t
                                                      {
                                                          DWORD copied = GetDllDirectoryW(static_cast<DWORD>(n), buf);
                                                          return copied < n ? copied : 0;
                                                      });
                }
                catch (...)
                {
                    original_dir.resize(len);
                    if (GetDllDirectoryW(len, original_dir.data()) == 0)
                        original_dir.clear();
                }
            }
        }

        [[nodiscard]] bool SetDirectory(const wchar_t* path) noexcept
        {
            if (SetDllDirectoryW(path))
            {
                changed = true;
                return true;
            }
            return false;
        }

        ~DllDirectoryGuard() noexcept
        {
            if (changed)
                SetDllDirectoryW(original_dir.empty() ? nullptr : original_dir.c_str());
        }

        DllDirectoryGuard(const DllDirectoryGuard&) = delete;
        DllDirectoryGuard& operator=(const DllDirectoryGuard&) = delete;
        DllDirectoryGuard(DllDirectoryGuard&&) = delete;
        DllDirectoryGuard& operator=(DllDirectoryGuard&&) = delete;
    };

    // COM initialization with error handling
    [[nodiscard]] std::expected<void, HRESULT> InitializeCOM() noexcept
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr))
            return std::unexpected(hr);
        return {};
    }

    // Library loading with optimized string conversion
    [[nodiscard]] std::expected<HMODULE, DllLoadError> LoadLibrarySafe(const SString& dll_path, std::string_view dll_name, const SString& mta_path,
                                                                       bool preflight = false)
    {
        WString dll_path_w = FromUTF8(dll_path);
        if (dll_path_w.empty()) [[unlikely]]
            return std::unexpected(DllLoadError::InvalidPath);

        if (preflight)
        {
            // Headless validation loads only its adjacent MTA-owned validator;
            // neither the working directory nor the game bundle supplies DLLs.
            if (HMODULE module = LoadLibraryExW(dll_path_w.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32))
                return module;
            return std::unexpected(DllLoadError::LoadFailed);
        }

        if (HMODULE module = LoadLibraryW(dll_path_w.c_str()))
            return module;

        DllDirectoryGuard dll_guard;
        WString           mta_path_w = FromUTF8(mta_path);
        std::wstring      dll_name_w;
        try
        {
            dll_name_w.resize_and_overwrite(dll_name.size() * 2,
                                            [&dll_name](wchar_t* buf, std::size_t n) -> std::size_t
                                            {
                                                SString temp{dll_name.data(), dll_name.size()};
                                                WString wide = FromUTF8(temp);
                                                if (wide.empty())
                                                    return 0;
                                                std::size_t len = std::min(n, wide.length());
                                                std::ranges::copy_n(wide.c_str(), len, buf);
                                                return len;
                                            });
        }
        catch (...)
        {
            SString temp{dll_name.data(), dll_name.size()};
            WString wide = FromUTF8(temp);
            dll_name_w.assign(wide.begin(), wide.end());
        }

        if (mta_path_w.empty() || dll_name_w.empty()) [[unlikely]]
            return std::unexpected(DllLoadError::InvalidPath);

        if (!dll_guard.SetDirectory(mta_path_w.c_str()))
            return std::unexpected(DllLoadError::LoadFailed);

        std::wstring full_path_w = std::wstring(mta_path_w.c_str()) + L"\\" + dll_name_w;
        if (HMODULE module = LoadLibraryW(full_path_w.c_str()))
            return module;

        return std::unexpected(DllLoadError::LoadFailed);
    }

    // Function execution with bounds checking and secure conversion
    [[nodiscard]] std::expected<int, LoadResult> ExecuteDoWinMain(HMODULE module, HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show, bool preflight = false)
    {
        using DoWinMainFunc = int (*)(HINSTANCE, HINSTANCE, LPSTR, int);

        if (!module) [[unlikely]]
            return std::unexpected(LoadResult::FunctionError);

        FARPROC proc = GetProcAddress(module, "DoWinMain");
        if (!proc) [[unlikely]]
        {
            if (!preflight)
                AddReportLog(5713, "Launcher Main: DoWinMain function not found");
            return std::unexpected(LoadResult::FunctionError);
        }

        // Validate function pointer within module bounds
        MODULEINFO info{};
        if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
            return std::unexpected(LoadResult::FunctionError);

        std::span<const std::byte> module_span{static_cast<const std::byte*>(info.lpBaseOfDll), info.SizeOfImage};
        auto                       func_ptr = reinterpret_cast<const std::byte*>(proc);

        if (func_ptr < module_span.data() || func_ptr >= (module_span.data() + module_span.size()))
        {
            if (!preflight)
                AddReportLog(5713, "Launcher Main: Function pointer outside module bounds");
            return std::unexpected(LoadResult::FunctionError);
        }

        // Command line handling
        constexpr std::size_t MAX_CMD = 8192;
        static_assert(ValidBufferSize<MAX_CMD>);

        std::array<char, MAX_CMD> safe_cmd{};
        if (cmd)
        {
            auto src_view = std::string_view{cmd, std::min(std::strlen(cmd), MAX_CMD - 1)};
            std::ranges::copy(src_view, safe_cmd.begin());
        }

        // Function pointer conversion with safety checks
        static_assert(sizeof(FARPROC) == sizeof(DoWinMainFunc), "Function pointer size mismatch");
        static_assert(alignof(FARPROC) == alignof(DoWinMainFunc), "Function pointer alignment mismatch");
        static_assert(std::is_trivially_copyable_v<FARPROC>, "FARPROC must be trivially copyable for bit_cast");
        static_assert(std::is_trivially_copyable_v<DoWinMainFunc>, "DoWinMainFunc must be trivially copyable for bit_cast");

        auto do_win_main = std::bit_cast<DoWinMainFunc>(proc);
        if (!do_win_main) [[unlikely]]
            return std::unexpected(LoadResult::FunctionError);

        try
        {
            return do_win_main(inst, prev, safe_cmd.data(), show);
        }
        catch (...)
        {
            if (!preflight)
                AddReportLog(5722, "Launcher Main: Exception in DoWinMain");
            return std::unexpected(LoadResult::FunctionError);
        }
    }

#if defined(MTA_DE_LAUNCHER_PREFLIGHT) && defined(_WIN64)
    int ExecuteDefinitiveHostLaunch(HMODULE module, bool installed)
    {
        const auto entry = GetProcAddress(module, installed ? "DE_PrepareInstalledHostLaunchV1" : "DE_PrepareHostLaunchV1");
        MODULEINFO info{};
        if (!entry || !GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
            return DefinitivePreflight::Status("host_launch_export_invalid", 3);
        const auto address = reinterpret_cast<std::uintptr_t>(entry);
        const auto base = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
        if (address < base || address - base >= info.SizeOfImage)
            return DefinitivePreflight::Status("host_launch_export_invalid", 3);
        const auto                              prepare = std::bit_cast<DefinitivePreflight::PrepareHostLaunchV1>(entry);
        DefinitivePreflight::HostLaunchResultV1 result;
        const int                               code = prepare(&result);
        if (result.size != sizeof(result) || result.version != 1 || result.status[sizeof(result.status) - 1] != '\0')
            return DefinitivePreflight::Status("host_launch_result_invalid", 3);
        if (code != 0)
            return DefinitivePreflight::Status(result.status, 4, result.win32Error);
        if ((result.verifiedFiles != 60 && result.verifiedFiles != 64) || std::strcmp(result.status, "host_launch_prepared") != 0)
            return DefinitivePreflight::Status("host_launch_result_invalid", 3);

        const auto com = InitializeCOM();
        if (!com)
            return DefinitivePreflight::Status("host_launch_com_failed", 5, static_cast<DWORD>(com.error()), result.verifiedFiles, true);
        const auto cleanup = make_scope_exit([] { CoUninitialize(); });
        // Reuse MTA's shell dispatch and Steam's authenticated launch route.
        // The URI is fixed: validated input paths never become shell commands,
        // and request acceptance does not prove GTA or MTA startup succeeded.
        if (!ShellExecuteNonBlocking("open", DefinitivePreflight::SteamLaunchUri))
            return DefinitivePreflight::Status("host_launch_request_failed", 5, 0, result.verifiedFiles, true);
        return DefinitivePreflight::Status("host_launch_requested", 0, 0, result.verifiedFiles, true, true);
    }
#endif

    // Path validation
    [[nodiscard]] constexpr bool ValidatePathSafety(std::string_view path) noexcept
    {
        return path.find("..") == std::string_view::npos && path.find("//") == std::string_view::npos;
    }

    // Path discovery with validation
    [[nodiscard]] std::expected<SString, LoadResult> FindMtaPath(const SString& launch_path)
    {
        if (!ValidatePathSafety(launch_path)) [[unlikely]]
        {
            AddReportLog(5731, std::format("Launcher Main: Invalid launch path: '{}'", std::string_view{launch_path}));
            return std::unexpected(LoadResult::PathError);
        }

        SString mta_path = PathJoin(launch_path, "mta");
        if (!ValidatePathSafety(mta_path)) [[unlikely]]
            return std::unexpected(LoadResult::PathError);

        if (DirectoryExists(mta_path))
            return mta_path;

        SString parent = PathJoin(launch_path, "..");
        if (!ValidatePathSafety(parent)) [[unlikely]]
            return std::unexpected(LoadResult::PathError);

        SString alt = PathJoin(parent, "mta");

        if (ValidatePathSafety(alt) && DirectoryExists(alt))
            return alt;

        AddReportLog(5730, std::format("Launcher Main: MTA directory not found from: '{}'", std::string_view{launch_path}));
        return std::unexpected(LoadResult::PathError);
    }
}  // namespace mta::launcher

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    using namespace mta::launcher;

    if (DefinitivePreflight::Requested())
    {
#if defined(MTA_DE_LAUNCHER_PREFLIGHT) && defined(_WIN64)
        // Explicit DE selection reuses the actual launcher dispatch while
        // keeping classic installation, patching and loader paths separate.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        std::array<wchar_t, 32768> executable{};
        const DWORD                length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!length || length >= executable.size())
            return DefinitivePreflight::Status("launcher_path_invalid", 1, GetLastError());
        const SString directory = PathJoin(ExtractPath(ToUTF8(executable.data())), "mta");
        const SString path = PathJoin(directory, "de_preflight.dll");
        const auto    loaded = LoadLibrarySafe(path, "de_preflight.dll", directory, true);
        if (!loaded)
            return DefinitivePreflight::Status("preflight_loader_load_failed", 2, GetLastError());
        const auto unload = make_scope_exit([module = loaded.value()] { FreeLibrary(module); });
        if (DefinitivePreflight::LaunchRequested() || DefinitivePreflight::InstalledLaunchRequested())
            return ExecuteDefinitiveHostLaunch(loaded.value(), DefinitivePreflight::InstalledLaunchRequested());
        const auto result = ExecuteDoWinMain(loaded.value(), hInstance, hPrevInstance, lpCmdLine, nCmdShow, true);
        return result ? result.value() : DefinitivePreflight::Status("preflight_loader_export_failed", 3);
#else
        return DefinitivePreflight::Status("preflight_requires_optin_x64_launcher", 1);
#endif
    }
#if defined(MTA_DE_LAUNCHER_PREFLIGHT)
    // The x64 target requires an explicit engineering selection; it must not
    // start Steam merely because the experimental launcher was double-clicked.
    return DefinitivePreflight::Status("explicit_de_preflight_required", 1);
#endif

    // Configure silent error handling
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    // COM initialization
    auto com_result = InitializeCOM();
    if (!com_result) [[unlikely]]
    {
        AddReportLog(5737, std::format("Launcher Main: COM initialization failed (hr=0x{:08X})", static_cast<DWORD>(com_result.error())));
    }

    auto com_cleanup = make_scope_exit([]() noexcept { CoUninitialize(); });

    // Set taskbar grouping
    [[maybe_unused]] HRESULT hr = SetCurrentProcessExplicitAppUserModelID(L"Multi Theft Auto " MTA_STR(MTASA_VERSION_MAJOR) L"." MTA_STR(MTASA_VERSION_MINOR));

    // Path discovery
    SString launch_path = GetLaunchPath();
    auto    mta_path_result = FindMtaPath(launch_path);

    if (!mta_path_result) [[unlikely]]
    {
        return std::to_underlying(mta_path_result.error());
    }

    SString mta_path = mta_path_result.value();
    SString dll_path = PathJoin(mta_path, SString{LOADER_DLL.data(), LOADER_DLL.size()});

    AddReportLog(5725, SString("Launcher Main: Launch: '%s', MTA: '%s'", launch_path.c_str(), mta_path.c_str()));

    // DLL loading
    auto module_result = LoadLibrarySafe(dll_path, LOADER_DLL, mta_path);

    if (!module_result) [[unlikely]]
    {
        DWORD   error = GetLastError();
        SString msg = std::format("Launcher Main: Failed to load: '{}'\n\n{}", dll_path.data(), GetSystemErrorMessage(error).data());
        AddReportLog(5711, msg);

        // Pattern matching
        switch (module_result.error())
        {
            case DllLoadError::FileNotFound:
                AddReportLog(5734, "Launcher Main: Build the 'Loader' project to create loader_d.dll");
                BrowseToSolution("loader-dll-missing", ASK_GO_ONLINE, msg);
                break;
            case DllLoadError::InvalidPath:
            case DllLoadError::LoadFailed:
                BrowseToSolution("loader-dll-not-loadable", ASK_GO_ONLINE, msg);
                break;
        }

        return std::to_underlying(LoadResult::LoadError);
    }

    HMODULE module = module_result.value();

    // RAII module cleanup
    auto module_cleanup = make_scope_exit(
        [module]() noexcept
        {
            if (module)
                FreeLibrary(module);
        });

    // Execution with concepts
    auto exec_result = ExecuteDoWinMain(module, hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    return exec_result.value_or(std::to_underlying(LoadResult::FunctionError));
}
