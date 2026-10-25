/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CModuleLoader.h
 *  PURPOSE:     Header file for module loader class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <windows.h>
#include <string>
#include <SString.h>
#include "StartupModule.h"

class CModuleLoader
{
public:
    CModuleLoader(const std::string& ModuleName);
    CModuleLoader();
    ~CModuleLoader();

    bool LoadModule(const std::string& ModuleName);
    void UnloadModule();
    void RetainForProcessTeardown() noexcept;

    bool           IsOk() const { return m_bStatus; }
    const SString& GetLastErrorMessage() const;
    DWORD          GetLastErrorCode() const noexcept { return m_module.Error(); }
    WORD           GetImageMachine() const noexcept { return m_module.Machine(); }

    PVOID GetFunctionPointer(const std::string& FunctionName);

private:
    MtaStartup::Module m_module;
    bool               m_bStatus;
    SString            m_strLastError;
};
