/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual Core optional preparation ownership without GTA
 *****************************************************************************/
#include "../../Client/core/CGameAdapterModule.h"
// The shared weapon policy consumes MTA's existing Windows SDK type names.
// Admit those real declarations before the policy in this standalone host.
#include <windows.h>
#include <game/CWeaponInfoIndexPolicy.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

using namespace mta::game_de;
struct ConstructorFixtureStats
{
    std::uint32_t  beforeConfigure = 0, factories = 0, initializes = 0;
    std::uint32_t  begins = 0, reads = 0, renewals = 0, clockReads = 0, ends = 0, resets = 0, shutdowns = 0, releases = 0;
    std::uint32_t  badBegin = 0, badRead = 0, completed = 3, registered = 4, flags = 0;
    std::uint32_t  lastVersion = 0, maximumDuration = 0;
    std::uintptr_t initializedAdapter = 0, lastAdapter = 0;
    Status         beginStatus = Status::Ok, readStatus = Status::Ok;
    bool           allowCleanup = true;
};

#if defined(MTA_CONSTRUCTOR_PROVIDER)
// This controlled DLL describes only the optional ABI. Its methods never
// access a game, construct a CGame, initialize defaults or claim capabilities.
namespace
{
    ConstructorFixtureStats* stats = nullptr;
    IGameAdapter*            adapterIdentity = nullptr;
    ClockServiceStateV1      clockState;
    DWORD                    producer = 0;
    bool                     pending = false;
    std::uint32_t            factories = 0, initializes = 0, beforeConfigure = 0;

    class FixtureAdapter final : public IGameAdapter
    {
    public:
        Status Initialize(CCoreInterface* owner) noexcept override
        {
            if (!owner)
                return Status::InvalidArgument;
            producer = GetCurrentThreadId();
            ++initializes;
            return Status::Ok;
        }
        Status Pulse() noexcept override { return Status::Ok; }
        Status Reset() noexcept override
        {
            if (stats)
                ++stats->resets;
            if (pending && (!stats || !stats->allowCleanup))
                return Status::Unsupported;
            pending = false;
            return Status::Ok;
        }
        Status Shutdown() noexcept override
        {
            if (stats)
                ++stats->shutdowns;
            return pending ? Status::Unsupported : Status::Ok;
        }
        void Release() noexcept override
        {
            if (stats)
                ++stats->releases;
            adapterIdentity = nullptr;
            delete this;
        }
        std::uint64_t GetCapabilities() const noexcept override { return 0; }
        CGame*        GetGameInterface() const noexcept override { return nullptr; }
        Status        PublishLocalObservation(const LocalObservation&) noexcept override { return Status::Unsupported; }
        Status        GetLocalObservation(LocalObservation&) noexcept override { return Status::Unsupported; }
        Status        SubmitControlActions(const ControlActionFrame&) noexcept override { return Status::Unsupported; }
        Status        GetMtaInputState(MtaInputState&) noexcept override { return Status::Unsupported; }
        Status        BindNativeInput() noexcept override { return Status::Unsupported; }
        Status        ReadNativeControllerState(std::uint32_t, NativeControllerState&) noexcept override { return Status::Unsupported; }
        Status        ReadNativeDeviceState(NativeDeviceState&) noexcept override { return Status::Unsupported; }
        Status        BeginNativeMovementTrial() noexcept override { return Status::Unsupported; }
        Status        GetNativeMovementTrialState(NativeMovementTrialState&) noexcept override { return Status::Unsupported; }
        Status        EndNativeMovementTrial() noexcept override { return Status::Unsupported; }
        Status        GetNativeMovementEligibility(NativeMovementEligibility&) noexcept override { return Status::Unsupported; }
    };
    bool Owner(IGameAdapter* adapter) noexcept
    {
        return adapter && adapter == adapterIdentity && stats && producer == GetCurrentThreadId();
    }
}

extern "C" __declspec(dllexport) void ConfigureConstructorFixture(ConstructorFixtureStats* output) noexcept
{
    stats = output;
    if (stats)
    {
        stats->beforeConfigure = beforeConfigure;
        stats->factories = factories;
        stats->initializes = initializes;
        stats->initializedAdapter = reinterpret_cast<std::uintptr_t>(adapterIdentity);
    }
}
namespace mta::game_de
{
    extern "C" IGameAdapter* CreateGameAdapter(std::uint32_t version) noexcept
    {
        if (version != AdapterAbiVersion || adapterIdentity)
            return nullptr;
        ++factories;
        try
        {
            return adapterIdentity = new FixtureAdapter;
        }
        catch (...)
        {
            return nullptr;
        }
    }
    extern "C" Status BeginGameConstructorPreparationV1(std::uint32_t version, IGameAdapter* adapter, const ClockServiceRequestV1* request,
                                                        ClockServiceStateV1* output) noexcept
    {
        if (!stats)
        {
            ++beforeConfigure;
            return Status::NotInitialized;
        }
        if (!Owner(adapter) || version != ConstructorPreparationVersionV1 || !request || !output)
            return Status::InvalidArgument;
        ++stats->begins;
        stats->lastVersion = version;
        stats->lastAdapter = reinterpret_cast<std::uintptr_t>(adapter);
        stats->maximumDuration = request->maximumDurationMs;
        clockState = {};
        clockState.generation = 16 + stats->begins;
        clockState.producerThread = producer;
        clockState.bound = clockState.hooked = clockState.active = 1;
        pending = true;
        auto next = clockState;
        switch (stats->badBegin)
        {
            case 1:
                --next.size;
                break;
            case 2:
                ++next.version;
                break;
            case 3:
                ++next.producerThread;
                break;
            case 4:
                next.generation = 0;
                break;
            case 5:
                next.reserved[1] = 1;
                break;
            default:
                break;
        }
        *output = next;
        return stats->beginStatus;
    }
    extern "C" Status ReadGameConstructorPreparationV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation,
                                                       ConstructorPreparationStateV1* output) noexcept
    {
        if (!Owner(adapter) || version != ConstructorPreparationVersionV1 || !generation || !output)
            return Status::InvalidArgument;
        ++stats->reads;
        if (!pending || generation != clockState.generation)
            return Status::NotInitialized;
        ConstructorPreparationStateV1 next;
        next.generation = generation;
        next.totalRows = static_cast<std::uint32_t>(MtaGame::CWeaponInfoIndexPolicy::SlotCount);
        next.completedRows = stats->completed;
        next.registeredRows = stats->registered;
        next.flags = stats->flags;
        switch (stats->badRead)
        {
            case 1:
                --next.size;
                break;
            case 2:
                ++next.version;
                break;
            case 3:
                ++next.generation;
                break;
            case 4:
                --next.totalRows;
                break;
            case 5:
                next.completedRows = next.totalRows + 1;
                break;
            case 6:
                next.registeredRows = next.totalRows + 1;
                break;
            case 7:
                next.registeredRows = next.completedRows - 1;
                break;
            case 8:
                next.registeredRows = next.completedRows + 2;
                break;
            case 9:
                next.flags = 8;
                break;
            case 10:
                next.flags = ConstructorPreparationComplete;
                break;
            case 11:
                next.completedRows = next.registeredRows = next.totalRows;
                next.flags = ConstructorPreparationComplete | ConstructorPreparationFailed;
                break;
            case 12:
                next.flags = ConstructorPreparationReadyNotified;
                break;
            default:
                if (stats->badRead >= 13 && stats->badRead <= 20)
                    next.reserved[stats->badRead - 13] = 1;
                break;
        }
        *output = next;  // Also poison output on refusal: the real Core must preserve its caller.
        return stats->readStatus;
    }
    extern "C" Status RenewGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (!Owner(adapter) || version != ClockServiceVersionV1 || !pending || generation != clockState.generation)
            return Status::InvalidArgument;
        ++stats->renewals;
        return Status::Ok;
    }
    extern "C" Status ReadGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, ClockServiceStateV1* output) noexcept
    {
        if (!Owner(adapter) || version != ClockServiceVersionV1 || !output)
            return Status::InvalidArgument;
        ++stats->clockReads;
        *output = clockState;
        return Status::Ok;
    }
    extern "C" Status EndGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (!Owner(adapter) || version != ClockServiceVersionV1 || generation != clockState.generation)
            return Status::InvalidArgument;
        ++stats->ends;
        if (!stats->allowCleanup)
            return Status::Unsupported;
        pending = false;
        clockState.bound = clockState.hooked = clockState.active = 0;
        clockState.restored = 1;
        return Status::Ok;
    }
}
#else
namespace
{
    std::uint32_t checks = 0;
    void          Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    template <class T>
    bool Same(const T& left, const T& right)
    {
        return std::memcmp(&left, &right, sizeof(T)) == 0;
    }
    struct Host
    {
        ConstructorFixtureStats stats;
        CGameAdapterModule      module;
        std::wstring            path;
        // Initialize borrows only an identity. No fixture dereferences this as
        // a constructed Core or claims the full Core host ran in this process.
        alignas(void*) std::array<unsigned char, sizeof(void*)> opaqueOwner{};
        explicit Host(const wchar_t* input) : path(input)
        {
            Check(module.Start(reinterpret_cast<CCoreInterface*>(opaqueOwner.data()), path), "actual Core loads explicit controlled ABI6 provider");
            const auto loaded = GetModuleHandleW(path.c_str());
            Check(loaded != nullptr, "Core owns the loaded optional provider module");
            const auto address = GetProcAddress(loaded, "ConfigureConstructorFixture");
            Check(address != nullptr, "fixture config export available only to this host test");
            using Configure = void (*)(ConstructorFixtureStats*) noexcept;
            Configure configure = nullptr;
            static_assert(sizeof(configure) == sizeof(address));
            std::memcpy(&configure, &address, sizeof(configure));
            configure(&stats);
            Check(stats.factories == 1 && stats.initializes == 1 && !stats.beforeConfigure && !stats.begins,
                  "loading and Initialize never auto-begin constructor preparation");
        }
        void Close()
        {
            module.Stop();
            Check(!module.IsStarted() && stats.releases == 1 && GetModuleHandleW(path.c_str()) == nullptr,
                  "successful retirement releases the exact instance before unloading its provider");
        }
    };
    ConstructorPreparationStateV1 Sentinel()
    {
        ConstructorPreparationStateV1 state;
        state.generation = 900;
        state.completedRows = 91;
        state.registeredRows = 92;
        state.totalRows = 93;
        state.flags = 94;
        return state;
    }
    void CompleteProvider(const wchar_t* path)
    {
        Host       host(path);
        auto       progress = Sentinel();
        const auto unchanged = progress;
        Check(!host.module.ReadNativeConstructorPreparation(17, progress) && Same(progress, unchanged) && !host.stats.reads,
              "Core does not synthesize progress before explicit admission");
        ClockServiceStateV1 ownership;
        ownership.hour = 99;
        const auto oldOwnership = ownership;
        for (unsigned mode = 0; mode < 7; ++mode)
        {
            ClockServiceRequestV1 invalid;
            switch (mode)
            {
                case 0:
                    --invalid.size;
                    break;
                case 1:
                    ++invalid.version;
                    break;
                case 2:
                    invalid.maximumDurationMs = 0;
                    break;
                case 3:
                    ++invalid.maximumDurationMs;
                    break;
                default:
                    invalid.reserved[mode - 4] = 1;
                    break;
            }
            Check(!host.module.BeginNativeConstructorPreparation(invalid, ownership) && Same(ownership, oldOwnership) && !host.stats.begins,
                  "malformed request refuses before native ownership or provider invocation");
        }
        ClockServiceRequestV1 request;
        request.maximumDurationMs = 1234;
        Check(host.module.BeginNativeConstructorPreparation(request, ownership) && ownership.generation == 17 &&
                  ownership.producerThread == GetCurrentThreadId() && host.stats.begins == 1 && host.stats.lastVersion == 1 &&
                  host.stats.lastAdapter == host.stats.initializedAdapter && host.stats.maximumDuration == 1234,
              "explicit preparation forwards one exact request to its retained adapter and producer token");
        Check(host.module.ReadNativeConstructorPreparation(17, progress) && progress.generation == 17 && progress.completedRows == 3 &&
                  progress.registeredRows == 4 && progress.totalRows == MtaGame::CWeaponInfoIndexPolicy::SlotCount && !progress.flags,
              "actual Core forwards one acknowledged prefix snapshot without inventing initialized services");
        auto calls = host.stats.reads;
        progress = Sentinel();
        Check(!host.module.ReadNativeConstructorPreparation(18, progress) && Same(progress, unchanged) && host.stats.reads == calls,
              "another generation cannot read or adopt this preparation attempt");
        bool        foreignBegin = true, foreignRead = true;
        std::thread other(
            [&]
            {
                ClockServiceStateV1 otherOwnership;
                auto                otherProgress = Sentinel();
                foreignBegin = host.module.BeginNativeConstructorPreparation({}, otherOwnership);
                foreignRead = host.module.ReadNativeConstructorPreparation(17, otherProgress) || !Same(otherProgress, unchanged);
            });
        other.join();
        Check(!foreignBegin && !foreignRead && host.stats.begins == 1 && host.stats.reads == calls,
              "foreign Core threads refuse before either optional provider callback");
        Check(host.module.RenewNativeClockService(17) && host.stats.renewals == 1 && host.module.ReadNativeClockService(ownership) &&
                  host.stats.clockReads == 1 && ownership.generation == 17,
              "preparation uses the existing exact module clock Renew and Read owner");
        Check(!host.module.BeginNativeConstructorPreparation({}, ownership) && !host.module.BeginNativeClockService({}, ownership) &&
                  !host.module.BeginNativeStartupObservation({}, ownership) && host.stats.begins == 1,
              "another preparation or clock workload cannot replace pending ownership");
        for (const auto status : {Status::InvalidArgument, Status::WrongThread, Status::NotInitialized, Status::Unsupported})
        {
            host.stats.readStatus = status;
            progress = Sentinel();
            Check(!host.module.ReadNativeConstructorPreparation(17, progress) && Same(progress, unchanged),
                  "provider refusal cannot overwrite caller progress with an unacknowledged value");
        }
        host.stats.readStatus = Status::Ok;
        for (unsigned mode = 1; mode <= 20; ++mode)
        {
            host.stats.badRead = mode;
            progress = Sentinel();
            Check(!host.module.ReadNativeConstructorPreparation(17, progress) && Same(progress, unchanged),
                  "malformed prefix, version, flags, reserved words or token preserves caller output");
        }
        host.stats.badRead = 0;
        host.stats.flags = ConstructorPreparationFailed;
        Check(host.module.ReadNativeConstructorPreparation(17, progress) && progress.flags == ConstructorPreparationFailed && progress.completedRows == 3 &&
                  progress.registeredRows == 4,
              "a failed current row retains its accepted prefix without successful replay");
        host.stats.completed = host.stats.registered = static_cast<std::uint32_t>(MtaGame::CWeaponInfoIndexPolicy::SlotCount);
        host.stats.flags = ConstructorPreparationComplete | ConstructorPreparationReadyNotified;
        Check(host.module.ReadNativeConstructorPreparation(17, progress) && progress.flags == 5,
              "the exact complete schedule and ready notification remain value-only evidence");
        Check(!host.module.IsGameplayReady() && !host.module.GetCapabilities(), "preparation never adds ABI6 gameplay capabilities");
        Check(!host.module.EndNativeClockService(18) && !host.stats.ends, "only the preparation's exact token can reach clock End");
        Check(host.module.EndNativeClockService(17) && host.stats.ends == 1, "the existing clock End retires this preparation owner once");
        calls = host.stats.reads;
        progress = Sentinel();
        Check(!host.module.ReadNativeConstructorPreparation(17, progress) && Same(progress, unchanged) && host.stats.reads == calls,
              "retired progress never survives as a live preparation read");
        Check(host.module.BeginNativeConstructorPreparation({}, ownership) && ownership.generation == 18 && host.module.Reset(),
              "actual successful Reset retires a second explicit attempt");
        Check(!host.module.ReadNativeConstructorPreparation(18, progress), "Reset clears the preparation token before reuse");
        host.stats.beginStatus = Status::Unsupported;
        host.stats.allowCleanup = false;
        Check(!host.module.BeginNativeConstructorPreparation({}, ownership) && ownership.generation == 19 && host.stats.begins == 3,
              "failed admission preserves its real cleanup generation while reporting failure");
        host.module.Stop();
        Check(host.module.IsStarted() && !host.stats.releases && GetModuleHandleW(path) != nullptr,
              "Shutdown refusal retains this exact adapter and provider code for cleanup retry");
        Check(!host.module.EndNativeClockService(19) && host.stats.ends == 2 && !host.module.BeginNativeConstructorPreparation({}, ownership) &&
                  host.stats.begins == 3,
              "failed End keeps cleanup debt without replaying native preparation");
        host.stats.allowCleanup = true;
        Check(host.module.EndNativeClockService(19) && host.stats.ends == 3, "later explicit End can discharge the same retained debt");
        host.Close();
    }
    void MalformedAdmission(const wchar_t* path, unsigned mode)
    {
        Host host(path);
        host.stats.badBegin = mode;
        ClockServiceStateV1 ownership;
        ownership.hour = 99;
        const auto before = ownership;
        Check(!host.module.BeginNativeConstructorPreparation({}, ownership) && Same(ownership, before) && host.stats.begins == 1,
              "malformed returned owner token cannot be published as a usable preparation token");
        auto       progress = Sentinel();
        const auto unchanged = progress;
        Check(!host.module.ReadNativeConstructorPreparation(17, progress) && Same(progress, unchanged) && !host.stats.reads &&
                  !host.module.BeginNativeConstructorPreparation({}, ownership) && host.stats.begins == 1,
              "malformed admission retains cleanup debt without progress or implicit retry");
        Check(host.module.ReadNativeClockService(ownership) && ownership.generation == 17 && host.module.EndNativeClockService(17),
              "the original exact provider clock can recover and retire malformed-admission debt");
        host.Close();
    }
    void MissingGroup(const wchar_t* path)
    {
        Host                host(path);
        ClockServiceStateV1 ownership;
        ownership.hour = 99;
        const auto before = ownership;
        auto       progress = Sentinel();
        const auto unchanged = progress;
        Check(!host.module.BeginNativeConstructorPreparation({}, ownership) && Same(ownership, before) && !host.stats.begins &&
                  !host.module.ReadNativeConstructorPreparation(17, progress) && Same(progress, unchanged) && !host.stats.reads,
              "a missing optional entry or clock retirement dependency refuses the entire mode before effects");
        Check(host.module.Pulse(), "older and partial providers remain usable through their unchanged ABI6 contract");
        host.Close();
    }
}
int wmain(int argc, wchar_t** argv)
{
    static_assert(sizeof(ConstructorPreparationStateV1) == 64 && ConstructorPreparationVersionV1 == 1 && AdapterAbiVersion == 6);
    static_assert(std::is_nothrow_invocable_r_v<Status, decltype(ReadGameConstructorPreparationV1), std::uint32_t, IGameAdapter*, std::uint64_t,
                                                ConstructorPreparationStateV1*>);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try
    {
        Check(sizeof(void*) == 8 && argc == 8, "focused loader host requires seven explicit x64 fixture DLLs");
        CompleteProvider(argv[1]);
        for (unsigned mode = 1; mode <= 5; ++mode)
            MalformedAdmission(argv[1], mode);
        for (int index = 2; index < argc; ++index)
            MissingGroup(argv[index]);
        std::cout << "Production Core constructor preparation optional owner fixture completed\n";
        std::cout << "Passed " << checks << " Core constructor preparation owner checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
