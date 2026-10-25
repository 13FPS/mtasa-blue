/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact-path ownership of architecture-compatible startup modules
 *****************************************************************************/
#pragma once

#include <windows.h>
#include <string>

namespace MtaStartup
{
    // Core and deathmatch must execute modules of their own architecture. An
    // x64 server net.dll is not a substitute for the separate client netc.dll.
    // Keep this check next to the real load so callers cannot admit a DLL from
    // an ambient search directory or overwrite an already-owned loader ref.
    class Module final
    {
    public:
        Module() = default;
        ~Module() { Unload(); }
        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;

#if defined(_M_ARM64) || defined(__aarch64__)
        static constexpr WORD HostMachine = IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_X64) || defined(__x86_64__)
        static constexpr WORD HostMachine = IMAGE_FILE_MACHINE_AMD64;
#else
        static constexpr WORD HostMachine = IMAGE_FILE_MACHINE_I386;
#endif

        bool Load(const std::wstring& path)
        {
            if (m_retainedForProcessTeardown)
            {
                m_error = ERROR_SHUTDOWN_IN_PROGRESS;
                return false;
            }
            if (m_module)
            {
                m_error = ERROR_ALREADY_EXISTS;
                return false;
            }
            m_machine = 0;
            // Both production callers already construct absolute paths. Reject
            // relative paths here rather than turn a missing startup dependency
            // into a current-directory or PATH-dependent selection.
            const bool drive = path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' &&
                               (path[2] == L'\\' || path[2] == L'/');
            const bool unc = path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\';
            if ((!drive && !unc) || path.find(L'\0') != std::wstring::npos)
            {
                m_error = ERROR_INVALID_NAME;
                return false;
            }
            const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                m_error = GetLastError();
                return false;
            }
            IMAGE_DOS_HEADER  dos{};
            IMAGE_FILE_HEADER header{};
            DWORD             signature = 0;
            WORD              magic = 0;
            LARGE_INTEGER     length{};
            const bool        valid =
                GetFileSizeEx(file, &length) && ReadAt(file, 0, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
                dos.e_lfanew >= static_cast<LONG>(sizeof(dos)) &&
                static_cast<LONGLONG>(dos.e_lfanew) + static_cast<LONGLONG>(sizeof(signature) + sizeof(header) + sizeof(magic)) <= length.QuadPart &&
                ReadAt(file, dos.e_lfanew, &signature, sizeof(signature)) && signature == IMAGE_NT_SIGNATURE &&
                ReadAt(file, static_cast<LONGLONG>(dos.e_lfanew) + static_cast<LONGLONG>(sizeof(signature)), &header, sizeof(header)) &&
                header.SizeOfOptionalHeader >= sizeof(magic) && (header.Characteristics & IMAGE_FILE_DLL) &&
                ReadAt(file, static_cast<LONGLONG>(dos.e_lfanew) + static_cast<LONGLONG>(sizeof(signature) + sizeof(header)), &magic, sizeof(magic));
            if (valid)
                m_machine = header.Machine;
            if (!valid || header.Machine != HostMachine || magic != (sizeof(void*) == 8 ? IMAGE_NT_OPTIONAL_HDR64_MAGIC : IMAGE_NT_OPTIONAL_HDR32_MAGIC))
            {
                CloseHandle(file);
                m_error = ERROR_BAD_EXE_FORMAT;
                return false;
            }
            // Preserve the classic module-directory dependency behavior. Core
            // still selects its explicit MTA dependency directory; no global
            // DLL directory is changed by this owner. Keep the file open without
            // write/delete sharing until LoadLibrary has selected these bytes.
            m_module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            m_error = m_module ? ERROR_SUCCESS : GetLastError();
            CloseHandle(file);
            return m_module != nullptr;
        }

        bool Unload() noexcept
        {
            if (m_module && !FreeLibrary(m_module))
            {
                // A refused unload cannot certify retirement or allow a new
                // module to replace callback code that this owner still holds.
                m_error = GetLastError();
                return false;
            }
            m_module = nullptr;
            m_error = ERROR_SUCCESS;
            m_machine = 0;
            return true;
        }
        void RetainForProcessTeardown() noexcept
        {
            // Pending native work and CPU ticket vtables still reference this
            // DLL. Transfer its loader reference to process lifetime without
            // calling FreeLibrary from this owner or its member destructor.
            // This is shutdown quarantine, never a live-game reload protocol.
            m_retainedForProcessTeardown = true;
            m_module = nullptr;
            m_error = ERROR_SUCCESS;
            m_machine = 0;
        }
        HMODULE Get() const noexcept { return m_module; }
        DWORD   Error() const noexcept { return m_error; }
        WORD    Machine() const noexcept { return m_machine; }

    private:
        static bool ReadAt(HANDLE file, LONGLONG offset, void* output, DWORD bytes) noexcept
        {
            LARGE_INTEGER position{};
            position.QuadPart = offset;
            DWORD read = 0;
            return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) && ReadFile(file, output, bytes, &read, nullptr) && read == bytes;
        }
        HMODULE m_module = nullptr;
        DWORD   m_error = ERROR_SUCCESS;
        WORD    m_machine = 0;
        bool    m_retainedForProcessTeardown = false;
    };
}
