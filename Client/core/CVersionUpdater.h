/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CVersionUpdater.h
 *  PURPOSE:     Header file for version update check and message dialog
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <cstdint>

class CVersionUpdaterInterface
{
public:
    virtual ~CVersionUpdaterInterface() {}
    virtual bool           SaveConfigToXML() = 0;
    virtual void           EnableChecking(bool bOn) = 0;
    virtual void           DoPulse() = 0;
    virtual void           InitiateUpdate(const SString& strType, const SString& strData, const SString& strHost) = 0;
    virtual bool           IsOptionalUpdateInfoRequired(const SString& strHost) = 0;
    virtual void           InitiateDataFilesFix() = 0;
    virtual void           InitiateManualCheck() = 0;
    virtual void           GetAseServerList(std::vector<SString>& outResult) = 0;
    virtual void           InitiateSidegradeLaunch(const SString& strVersion, const SString& strIp, ushort usPort, const SString& strName,
                                                   const SString& strPassword) = 0;
    virtual void           GetBrowseVersionMaps(std::map<SString, int>& outBlockedVersionMap, std::map<SString, int>& outAllowedVersionMap) = 0;
    virtual void           GetNewsSettings(SString& strOutOldestPost, uint& uiOutMaxHistoryLength) = 0;
    virtual const SString& GetDebugFilterString() = 0;
};

CVersionUpdaterInterface* GetVersionUpdater();

namespace FrontendBackgroundServices
{
    enum class Status : std::uint32_t
    {
        Ok,
        Busy,
        Conflict,
        WrongThread,
        InvalidArgument,
        Failed
    };

    struct UpdaterSnapshot
    {
        std::uint32_t version = 1, ownerThread = 0, instanceCreated = 0, workerStarted = 0, configLoaded = 0;
        std::uint64_t backgroundRequestsRejected = 0;
    };

    // Reserve before constructing CLocalGUI. Ordinary GetVersionUpdater calls
    // then lazily create the real updater in config/metadata-only mode, without
    // its process-lifetime thread. Existing classic callers keep their default.
    // Release after every borrowed frontend pointer is retired, before XML/Core
    // unload. Busy/conflict/failure retains the reservation and owned instance.
    Status AcquireUpdater(const void* owner) noexcept;
    Status ReadUpdater(const void* owner, UpdaterSnapshot& output) noexcept;
    Status ReleaseUpdater(const void* owner) noexcept;
}
