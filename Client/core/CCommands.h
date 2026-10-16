/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CCommands.h
 *  PURPOSE:     Header file for dynamic command manager
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <core/CCommandsInterface.h>
#include <string>
#include <cstdint>
#include "CSingleton.h"

class CCommands : public CCommandsInterface, public CSingleton<CCommands>
{
public:
    enum class Policy
    {
        Classic,
        FrontendLocal
    };
    explicit CCommands(Policy policy = Policy::Classic);
    ~CCommands();

    void Add(const char* szCommand, const char* szDescription, PFNCOMMANDHANDLER pfnHandler, bool bModCommand = false, bool bAllowScriptedBind = false);

    unsigned int Count();
    bool         Exists(const char* szCommand);

    bool Execute(const char* szCommandLine);
    bool Execute(const char* szCommand, const char* szParameters, bool bHandleRemotely = false, bool bIsScriptedBind = false);

    void Delete(const char* szCommand);
    void DeleteAll();

    void                     SetExecuteHandler(pfnExecuteCommandHandler pfnHandler) { m_pfnExecuteHandler = pfnHandler; }
    pfnExecuteCommandHandler GetExecuteHandler() { return m_pfnExecuteHandler; }

    tagCOMMANDENTRY* Get(const char* szCommand, bool bCheckIfMod = false, bool bModCommand = false);

    std::list<COMMANDENTRY*>::iterator IterBegin() { return m_CommandList.begin(); }
    std::list<COMMANDENTRY*>::iterator IterEnd() { return m_CommandList.end(); }

    bool          IsFrontendLocal() const noexcept { return m_Policy == Policy::FrontendLocal; }
    std::uint32_t GetFrontendExecuted() const noexcept { return m_FrontendExecuted; }
    std::uint32_t GetFrontendRejected() const noexcept { return m_FrontendRejected; }
    std::uint32_t GetFrontendLastCommand() const noexcept { return m_FrontendLastCommand; }

private:
    void ExecuteHandler(PFNCOMMAND pfnHandler, const char* szParameters);
    bool ExecuteFrontend(const char* command, const char* parameters, bool remotely, bool scriptedBind);

    std::list<COMMANDENTRY*> m_CommandList;
    pfnExecuteCommandHandler m_pfnExecuteHandler;
    Policy                   m_Policy;
    std::uint32_t            m_FrontendExecuted = 0, m_FrontendRejected = 0, m_FrontendLastCommand = 0;
};
