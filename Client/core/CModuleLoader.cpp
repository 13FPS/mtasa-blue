/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CModuleLoader.cpp
 *  PURPOSE:     Dynamic module loading
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <SharedUtil.Misc.h>

using std::string;

CModuleLoader::CModuleLoader(const string& ModuleName) : m_bStatus(false)
{
    LoadModule(ModuleName);
}

CModuleLoader::CModuleLoader() : m_bStatus(false)
{
}

CModuleLoader::~CModuleLoader()
{
    UnloadModule();
}

bool CModuleLoader::LoadModule(const string& ModuleName)
{
    if (m_module.Load(SharedUtil::FromUTF8(ModuleName)))
    {
        m_bStatus = true;
        m_strLastError = "";
    }
    else
    {
        // A refused second Load must retain the prior callback code and its
        // loader reference until explicit Unload, rather than overwrite/leak it.
        m_bStatus = m_module.Get() != nullptr;
        DWORD dwError = m_module.Error();
        char  szError[2048] = {0};
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwError, LANG_NEUTRAL, szError, sizeof(szError), NULL);
        m_strLastError = szError;
        if (dwError == ERROR_BAD_EXE_FORMAT && m_module.Machine() && m_module.Machine() != MtaStartup::Module::HostMachine)
            m_strLastError = SString("Module architecture 0x%04x does not match this MTA process (0x%04x): %s", m_module.Machine(),
                                     MtaStartup::Module::HostMachine, ModuleName.c_str());
        return false;
    }

    return m_bStatus;
}

void CModuleLoader::UnloadModule()
{
    if (m_module.Unload())
    {
        m_bStatus = false;
        m_strLastError = "";
    }
    else
    {
        m_bStatus = m_module.Get() != nullptr;
        char error[2048]{};
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, m_module.Error(), LANG_NEUTRAL, error, sizeof(error), nullptr);
        m_strLastError = error;
    }
}

PVOID CModuleLoader::GetFunctionPointer(const string& FunctionName)
{
    if (m_bStatus)
    {
        FARPROC fpProcAddr = nullptr;
        if (!SharedUtil::TryGetProcAddress(m_module.Get(), FunctionName.c_str(), fpProcAddr))
            return nullptr;

        return static_cast<PVOID>(fpProcAddr);
    }
    else
        return nullptr;
}

const SString& CModuleLoader::GetLastErrorMessage() const
{
    return m_strLastError;
}
