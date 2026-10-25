/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep pending cleanup objects and callback DLLs through teardown
 ****************************************************************************/
#include "../../Client/core/StartupModule.h"
#if defined(MTA_TEST_CLEANUP_QUARANTINE_PROVIDER)
extern "C" __declspec(dllexport) unsigned CleanupQuarantineMarker()
{
    return static_cast<unsigned>(sizeof(void*) * 8);
}
#else
    #include "../../Client/core/ClassicCustomAnimationTeardown.h"
    #include <cstdio>
    #include <cstdlib>
    #include <cwchar>
    #include <cstring>
    #include <filesystem>
    #include <initializer_list>
    #include <stdexcept>
    #include <type_traits>

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    struct PolicyManager
    {
        bool             pending = false;
        mutable unsigned queries = 0;
        bool             HasPendingCustomAnimationCleanup() const noexcept
        {
            ++queries;
            return pending;
        }
    };
    struct PolicyGame
    {
        PolicyManager* manager = nullptr;
        bool           refuseManager = false, refuseTerminate = false;
        unsigned       managerQueries = 0, terminations = 0;
        PolicyManager* GetAnimManager()
        {
            ++managerQueries;
            if (refuseManager)
                throw std::runtime_error("injected owner query refusal");
            return manager;
        }
        void Terminate()
        {
            ++terminations;
            if (refuseTerminate)
                throw std::runtime_error("injected termination refusal");
        }
    };
    void RunPolicyChecks()
    {
        namespace Policy = MtaGame::ClassicCustomAnimationTeardown;
        PolicyGame* empty = nullptr;
        Check(!Policy::NeedsRetention(empty), "null game does not invent cleanup debt");
        unsigned unloads = 0, retentions = 0;
        Check(!Policy::Finish(
                  empty, false, [&] { ++unloads; }, [&] { ++retentions; }) &&
                  unloads == 1 && !retentions,
              "null clear game selects ordinary loader release");
        for (const bool pending : {false, true})
        {
            PolicyManager manager;
            manager.pending = pending;
            PolicyGame object;
            object.manager = &manager;
            PolicyGame* game = &object;
            unloads = retentions = 0;
            const auto retained = Policy::Finish(
                game, false,
                [&]
                {
                    Check(!game && object.terminations == 1, "clear borrow before actual unload callback");
                    ++unloads;
                },
                [&]
                {
                    Check(!game && object.terminations == 1, "clear borrow before actual retain callback");
                    ++retentions;
                });
            Check(retained == pending && !game && object.managerQueries == 1 && manager.queries == 1 && object.terminations == 1,
                  "actual policy queries current manager before self-deleting game boundary");
            Check(unloads == (pending ? 0u : 1u) && retentions == (pending ? 1u : 0u), "pending and clear select exactly one loader action");
        }
        PolicyManager priorManager;
        priorManager.pending = true;
        PolicyGame prior;
        prior.manager = &priorManager;
        const bool earlier = Policy::NeedsRetention(&prior);
        Check(earlier && priorManager.queries == 1, "real preflight records pending while client and early owners still exist");
        priorManager.pending = false;
        PolicyGame* priorBorrow = &prior;
        unloads = retentions = 0;
        Check(Policy::Finish(
                  priorBorrow, earlier, [&] { ++unloads; }, [&] { ++retentions; }),
              "earlier pending evidence survives a later clear query opportunity");
        Check(!priorBorrow && prior.terminations == 1 && priorManager.queries == 1 && !unloads && retentions == 1,
              "early retain decision is monotonic and does not dereference manager after termination");
        for (const bool refused : {false, true})
        {
            PolicyGame unavailable;
            unavailable.refuseManager = refused;
            Check(Policy::NeedsRetention(&unavailable), "missing or refusing manager cannot certify callback lifetime");
            PolicyGame* game = &unavailable;
            unloads = retentions = 0;
            Check(Policy::Finish(
                      game, false, [&] { ++unloads; }, [&] { ++retentions; }) &&
                      !game && !unloads && retentions == 1,
                  "unknown owner preserves module ownership conservatively");
        }
        PolicyManager manager;
        PolicyGame    refused;
        refused.manager = &manager;
        refused.refuseTerminate = true;
        PolicyGame* game = &refused;
        unloads = retentions = 0;
        bool caught = false;
        try
        {
            static_cast<void>(Policy::Finish(game, false, [&] { ++unloads; }, [&] { ++retentions; }));
        }
        catch (const std::runtime_error&)
        {
            caught = true;
        }
        Check(caught && game == &refused && !unloads && !retentions, "failed Terminate grants neither module release nor false borrow retirement");
    }
    unsigned Marker(HMODULE module)
    {
        using Read = unsigned (*)();
        Read       read = nullptr;
        const auto entry = GetProcAddress(module, "CleanupQuarantineMarker");
        static_assert(sizeof(read) == sizeof(entry));
        std::memcpy(&read, &entry, sizeof(read));
        Check(read != nullptr, "owned provider export remains executable");
        return read();
    }
    void Quarantine(const std::filesystem::path& path)
    {
        Check(path.is_absolute() && !GetModuleHandleW(path.c_str()), "isolated owned DLL begins unloaded");
        HMODULE retained = nullptr;
        {
            MtaStartup::Module module;
            Check(module.Load(path.wstring()), "real owner loads exact task DLL");
            retained = module.Get();
            Check(Marker(retained) == sizeof(void*) * 8, "matching host executes its own provider");
            module.RetainForProcessTeardown();
            Check(!module.Get() && GetModuleHandleW(path.c_str()) == retained, "quarantine relinquishes RAII handle while preserving loader reference");
            Check(!module.Load(path.wstring()) && module.Error() == ERROR_SHUTDOWN_IN_PROGRESS, "quarantined owner refuses future loads permanently");
            static_cast<void>(module.Unload());
            Check(GetModuleHandleW(path.c_str()) == retained, "explicit Unload cannot release the quarantined reference");
            module.RetainForProcessTeardown();
            Check(GetModuleHandleW(path.c_str()) == retained, "repeat quarantine cannot accidentally free retained code");
        }
        Check(GetModuleHandleW(path.c_str()) == retained && Marker(retained) == sizeof(void*) * 8,
              "actual owner destructor leaves callback code mapped until isolated process exit");
        {
            MtaStartup::Module empty;
            empty.RetainForProcessTeardown();
            Check(!empty.Load(path.wstring()) && empty.Error() == ERROR_SHUTDOWN_IN_PROGRESS, "empty quarantine also permanently closes that startup owner");
        }
        std::puts("Actual startup module cleanup quarantine fixture completed");
    }
}

    // The generated body is extracted unchanged from actual CGameSA.cpp. Only the
    // unrelated game class shell is reduced, so this proves its close/query/delete
    // order without creating a positive full CGame implementation.
    #include "ClassicGameTerminate.generated.h"

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc == 3 && std::wcscmp(argv[1], L"--quarantine") == 0)
        {
            Quarantine(std::filesystem::path(argv[2]));
            std::printf("Passed %u actual cleanup quarantine checks (%zu-bit)\n", checks, sizeof(void*) * 8);
            return 0;
        }
        Check(argc == 1, "policy scope has no external services");
        RunPolicyChecks();
        RunActualTerminateChecks();
        std::puts("Actual classic cleanup teardown policy fixture completed");
        std::puts("Actual GameSA cleanup creator fence fixture completed");
        std::printf("Passed %u actual classic cleanup teardown checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Classic cleanup teardown failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
#endif
