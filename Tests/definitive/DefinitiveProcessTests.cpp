/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check host classification without starting or accessing a game
 *****************************************************************************/

#include "../../Client/core/DefinitiveProcess.h"
#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
    int        checks = 0;
    const auto check = [&](bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    };
    try
    {
        using DefinitiveProcess::IsCandidate;
        check(IsCandidate(L"C:\\Steam\\Gameface\\Binaries\\Win64\\SanAndreas.exe", true), "DE Windows path recognized");
        check(IsCandidate(L"C:/Steam/Gameface/Binaries/Win64/SanAndreas.exe", true), "forward slash path recognized");
        check(IsCandidate(L"SanAndreas.exe", true), "basename recognized");
        check(IsCandidate(L"C:\\Games\\sAnAnDrEaS.ExE", true), "Windows basename comparison ignores ASCII case");
        check(!IsCandidate(L"C:\\Games\\SanAndreas.exe", false), "x86 cannot select DE startup");
        check(!IsCandidate(L"C:\\Games\\gta_sa.exe", true), "classic executable keeps its existing entry path");
        check(!IsCandidate(L"C:\\SanAndreas.exe\\other.exe", true), "only final path component selects the candidate");
        check(!IsCandidate(L"C:\\Games\\SanAndreas.exe\\", true), "directory cannot be an executable candidate");
        check(!IsCandidate(L"C:\\Games\\SanAndreas.exe.bak", true), "backup cannot select DE startup");
        check(!IsCandidate(L"C:\\Games\\MySanAndreas.exe", true), "suffix match cannot select DE startup");
        check(!IsCandidate(L"SanAndreas.exe ", true), "trailing characters are rejected");
        check(!IsCandidate(L"", true), "empty executable is rejected");
        const std::wstring embeddedNull{L"SanAndreas.exe\0other", 20};
        check(!IsCandidate(embeddedNull, true), "embedded NUL cannot hide a different executable");
        check(!DefinitiveProcess::IsCurrentCandidate(), "actual fixture host cannot select DE startup");
        std::cout << "Passed " << checks << " DE process entry checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
