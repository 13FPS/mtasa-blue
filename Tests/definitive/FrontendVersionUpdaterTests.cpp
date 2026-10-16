/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual updater's frontend ownership in a test-only Core
 *****************************************************************************/
// This object is linked only into the disposable fixture DLL with the normal
// Core objects. No diagnostic exports are added to the installed client.
#include "../../Client/core/StdInc.h"
#include "../../Client/core/CVersionUpdater.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace
{
    std::uint32_t         checks = 0;
    std::array<char, 512> failure{};

    void Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }

    void Run()
    {
        using namespace FrontendBackgroundServices;
        int firstOwner = 0, conflictingOwner = 0;
        Check(AcquireUpdater(nullptr) == Status::InvalidArgument, "null owner cannot reserve updater");
        Check(AcquireUpdater(&firstOwner) == Status::Ok, "reserve frontend mode before ordinary getter");
        struct Cleanup
        {
            const void* owner;
            ~Cleanup() { ReleaseUpdater(owner); }
        } cleanup{&firstOwner};
        UpdaterSnapshot state;
        Check(ReadUpdater(&firstOwner, state) == Status::Ok && state.version == 1 && state.ownerThread == GetCurrentThreadId() && !state.instanceCreated &&
                  !state.workerStarted && !state.configLoaded,
              "reservation is lazy and creates no updater worker");
        Check(AcquireUpdater(&firstOwner) == Status::Ok, "same owner can reserve idempotently");
        Check(AcquireUpdater(&conflictingOwner) == Status::Conflict, "another owner cannot adopt active reservation");
        auto* updater = GetVersionUpdater();
        Check(updater != nullptr && updater == GetVersionUpdater(), "ordinary getter returns one real owned updater");
        Check(ReadUpdater(&firstOwner, state) == Status::Ok && state.instanceCreated && !state.workerStarted && !state.configLoaded,
              "actual frontend updater constructor creates no CThreadHandle or config access");

        // Every call would otherwise touch the real singleton Core, question
        // dialog or HTTP manager, which this fixture intentionally never creates.
        updater->EnableChecking(true);
        updater->EnableChecking(false);
        updater->DoPulse();
        updater->InitiateDataFilesFix();
        updater->InitiateManualCheck();
        updater->InitiateSidegradeLaunch("fixture", "127.0.0.1", 22003, "test", "");
        updater->InitiateUpdate("Mandatory", "fixture", "fixture");
        updater->InitiateUpdate("Optional", "fixture", "fixture");
        Check(ReadUpdater(&firstOwner, state) == Status::Ok && state.backgroundRequestsRejected == 6 && !state.workerStarted && !state.configLoaded,
              "all async entry points reject before accessing Core, GUI, registry or HTTP");
        Check(updater->IsOptionalUpdateInfoRequired("fixture"), "refused recommendation does not consume metadata intent");
        Check(!updater->SaveConfigToXML(), "unused metadata does not create a config dependency during save");

        UpdaterSnapshot preserved;
        preserved.version = 77;
        preserved.instanceCreated = 99;
        Check(ReadUpdater(&conflictingOwner, preserved) == Status::Conflict && preserved.version == 77 && preserved.instanceCreated == 99,
              "wrong owner preserves caller output");
        Check(ReleaseUpdater(&conflictingOwner) == Status::Conflict, "wrong owner cannot release the actual singleton");
        Check(ReadUpdater(nullptr, preserved) == Status::InvalidArgument && preserved.version == 77, "invalid owner preserves caller output");

        Status      foreignAcquire = Status::Ok, foreignRead = Status::Ok, foreignRelease = Status::Ok;
        bool        getterRejected = false, interfaceRejected = false;
        std::thread foreign(
            [&]
            {
                foreignAcquire = AcquireUpdater(&firstOwner);
                foreignRead = ReadUpdater(&firstOwner, preserved);
                foreignRelease = ReleaseUpdater(&firstOwner);
                try
                {
                    GetVersionUpdater();
                }
                catch (const std::logic_error&)
                {
                    getterRejected = true;
                }
                try
                {
                    updater->DoPulse();
                }
                catch (const std::logic_error&)
                {
                    interfaceRejected = true;
                }
            });
        foreign.join();
        Check(foreignAcquire == Status::WrongThread && foreignRead == Status::WrongThread && foreignRelease == Status::WrongThread,
              "ownership operations remain on the frontend creation thread");
        Check(preserved.version == 77 && preserved.instanceCreated == 99 && getterRejected && interfaceRejected,
              "foreign getter/interface calls cannot access owned metadata or mutate caller state");
        Check(ReadUpdater(&firstOwner, state) == Status::Ok && state.backgroundRequestsRejected == 6 && state.instanceCreated && !state.workerStarted,
              "foreign rejection retains owner and actual updater");

        Check(ReleaseUpdater(&firstOwner) == Status::Ok, "release real metadata-only updater before Core unload");
        Check(ReleaseUpdater(&firstOwner) == Status::Ok, "completed release is idempotent without creating classic singleton");
        Check(AcquireUpdater(&firstOwner) == Status::Ok, "frontend owner can acquire a fresh lifecycle");
        Check(ReadUpdater(&firstOwner, state) == Status::Ok && !state.instanceCreated && !state.backgroundRequestsRejected,
              "reacquisition has fresh lazy state");
        Check(ReleaseUpdater(&firstOwner) == Status::Ok, "uncreated frontend reservation releases without destructor work");
        Check(AcquireUpdater(&conflictingOwner) == Status::Ok, "completed owner does not prevent a distinct frontend lifecycle");
        cleanup.owner = &conflictingOwner;
        Check(GetVersionUpdater() != nullptr && ReadUpdater(&conflictingOwner, state) == Status::Ok && !state.workerStarted,
              "new owner uses the actual no-worker constructor again");
        Check(ReleaseUpdater(&conflictingOwner) == Status::Ok, "new owner releases before fixture DLL teardown");
    }
}

extern "C" __declspec(dllexport) int MTA_RunFrontendVersionUpdaterTests(std::uint32_t* bodyChecks)
{
    if (!bodyChecks)
        return 1;
    checks = 0;
    failure.fill(0);
    try
    {
        Run();
        *bodyChecks = checks;
        return 0;
    }
    catch (const std::exception& error)
    {
        ::_snprintf_s(failure.data(), failure.size(), _TRUNCATE, "%s", error.what());
        *bodyChecks = checks;
        return 1;
    }
    catch (...)
    {
        ::_snprintf_s(failure.data(), failure.size(), _TRUNCATE, "%s", "unknown actual updater fixture exception");
        *bodyChecks = checks;
        return 1;
    }
}

extern "C" __declspec(dllexport) const char* MTA_FrontendVersionUpdaterTestFailure()
{
    return failure.data();
}
