/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real normal-frame preparation and retained constructor row failures
 *****************************************************************************/
#include <game/CWeaponStartupPolicy.h>

namespace ConstructorWeaponFixture
{
    using Bank = std::array<NativeWeaponInfoValues, NativeWeaponInfoProfile::RowCount>;
    struct Context
    {
        ClockImageFixture*              fixture = nullptr;
        unsigned                        frames = 0, readyCalls = 0, operation = 0, refusal = 0;
        ClockResult                     first = ClockResult::NotInitialized, second = ClockResult::NotInitialized;
        NativeWeaponConstructorProgress progress{0xaaaa, 0xbbbb, true, true};
        CWeaponStatManager*             manager = nullptr;
    };

    void Ready(NativeGameTimingAccess& access, CClockDE& clock, const NativeApplicationFrame& frame, void* opaque)
    {
        auto& context = *static_cast<Context*>(opaque);
        ++context.readyCalls;
        Check(&clock == &runtime.clock && frame.generation == runtime.generation.load(),
              "constructor notification lends the existing clock and current normal-frame owner");
        Check(access.RequireConstructorWeapons(&clock, context.manager) == ClockResult::Ok && context.manager == &runtime.weaponStatManager,
              "ready notification resolves the actual complete manager owned by that same clock/runtime");
        CWeaponStatManager* repeated = nullptr;
        CWeaponInfo*        info = nullptr;
        Check(access.GetWeaponStatManager(repeated) == ClockResult::Ok && repeated == context.manager &&
                  access.GetWeaponInfoSlot(22, info) == ClockResult::Ok && info == runtime.weaponBank.GetSlot(22) &&
                  info->GetWeaponRange() == MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_PISTOL, WEAPONSKILL_STD)->weapon_range,
              "same ready consumer can freshly resolve actual SDK manager/bank reads after the row writer scope ended");
        Check(access.SetWeaponFloat(22, NativeWeaponFloatField::WeaponRange, 901.0f) != ClockResult::Ok,
              "ready service consumption never widens the internal row writer scope to client mutations");
        Check(access.SetTimeScale(0.75f) != ClockResult::Ok, "weapon constructor preparation grants no general timing mutation permission");
    }

    void Frame(NativeGameTimingAccess& access, const NativeApplicationFrame& frame, void* opaque)
    {
        auto& context = *static_cast<Context*>(opaque);
        ++context.frames;
        auto&      fixture = *context.fixture;
        const auto thread = fixture.Get<DWORD>(NativeStartGameProfile::ThreadIdRva);
        if (context.refusal == 1)
            fixture.Put(NativeStartGameProfile::ThreadIdRva, DWORD(thread + 1));
        if (context.refusal == 2)
            fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (context.operation == 1)
        {
            context.first = access.PrepareConstructorWeapons(context.progress);
            context.second = access.PrepareConstructorWeapons(context.progress);
        }
        else if (context.operation == 2)
        {
            CWeaponStatManager* selected = reinterpret_cast<CWeaponStatManager*>(std::uintptr_t{1});
            context.first = access.RequireConstructorWeapons(&runtime.clock, selected);
            Check(context.first != ClockResult::Ok && selected == reinterpret_cast<CWeaponStatManager*>(std::uintptr_t{1}),
                  "incomplete constructor services preserve the caller's prior pointer");
            context.second = access.RequireConstructorWeapons(nullptr, selected);
        }
        else
            PrepareConstructorServicesFrame(access, frame, opaque);
        fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
        fixture.Put(NativeBorrowedGcProfile::markerRva, LONG{0});
    }

    // Only owned module pages and independent existing producer/consumer pins
    // are supplied. Every production memory/profile/thread guard still runs.
    void Seed(ClockImageFixture& fixture, std::array<unsigned char, 8>& model)
    {
        const auto admit = [&](std::uintptr_t rva, const void* value, std::size_t bytes, DWORD protection)
        {
            DWORD previous = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), bytes, PAGE_READWRITE, &previous) != FALSE,
                  "constructor fixture temporarily owns only its selected image span");
            std::memcpy(reinterpret_cast<void*>(fixture.image + rva), value, bytes);
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), bytes, protection, &ignored) != FALSE &&
                      ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), bytes),
                  "constructor fixture restores exact code/data image protection");
        };
        // Focus mode starts with a fresh clock/FPS image. Supply its own
        // genuine application pins instead of inheriting another suite's seed.
        fixture.AdmitApplicationPins();
        fixture.AdmitWeaponPins();
        fixture.AdmitWeaponVectorPins();
        for (const auto& pin : NativeWeaponInfoExtraProfile::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        const std::int32_t animationCount = 256;
        admit(NativeWeaponInfoWriteProfile::AnimationCountLoadRva, NativeWeaponInfoWriteProfile::AnimationCountLoad.data(),
              NativeWeaponInfoWriteProfile::AnimationCountLoad.size(), PAGE_EXECUTE_READ);
        admit(NativeWeaponInfoWriteProfile::AnimationCountRva, &animationCount, sizeof(animationCount), PAGE_READWRITE);
        std::vector<unsigned char> aim(NativeWeaponInfoExtraProfile::AimRecordBytes * NativeWeaponInfoExtraProfile::AimRecordCount);
        std::vector<unsigned char> combos(NativeWeaponInfoExtraProfile::ComboRecordBytes * 11);
        admit(NativeWeaponInfoExtraProfile::AimTableRva, aim.data(), aim.size(), PAGE_READWRITE);
        admit(NativeWeaponInfoExtraProfile::ComboTableRva, combos.data(), combos.size(), PAGE_READWRITE);
        constexpr std::uintptr_t vtableRva = 0x431c100;
        const auto               vtable = fixture.image + vtableRva;
        std::memcpy(model.data(), &vtable, sizeof(vtable));
        std::array<std::uintptr_t, 13> methods{};
        methods[NativeWeaponInfoExtraProfile::ModelVirtualMethodOffset / sizeof(std::uintptr_t)] = fixture.image + NativeWeaponInfoExtraProfile::Code[0].rva;
        admit(vtableRva, methods.data(), sizeof(methods), PAGE_READONLY);
        std::vector<std::uintptr_t> models(NativeWeaponInfoExtraProfile::ModelCount);
        MtaGame::CWeaponStartupPolicy::Initialize(
            [&](const MtaGame::CWeaponStartupPolicy::Entry& entry)
            {
                const auto* defaults = MtaGame::CWeaponStatDefaults::Select(entry.statType, entry.skill);
                if (defaults)
                    for (int id : {defaults->model, defaults->model2})
                        if (id >= 0)
                        {
                            Check(static_cast<std::size_t>(id) < models.size(), "shared startup model is inside the existing native storage contract");
                            models[static_cast<std::size_t>(id)] = reinterpret_cast<std::uintptr_t>(model.data());
                        }
            });
        admit(NativeWeaponInfoExtraProfile::ModelTableRva, models.data(), models.size() * sizeof(models[0]), PAGE_READWRITE);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const DWORD         thread = GetCurrentThreadId();
        const unsigned char initialized = 1;
        const LONG          zero = 0;
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, sizeof(initialized), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &zero, sizeof(zero), PAGE_READWRITE);
        ClockReaderTestHost::image = fixture.image;
    }

    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        alignas(std::uintptr_t) std::array<unsigned char, 8> model{};
        Seed(fixture, model);
        Context context;
        context.fixture = &fixture;
        NativeClockService              service;
        NativeWeaponConstructorProgress untouched{17, 19, true, true};
        ClockServiceStateV1             refused;
        Check(!service.BeginGameConstructorFrames({}, refused, &Ready, &context) && refused.failure == ClockResult::UnsupportedBuild,
              "real constructor entry retains the shipping wrong-executable admission before hooks or native default effects");
        Check(runtime.access.PrepareConstructorWeapons(untouched) != ClockResult::Ok && untouched.completedRows == 17 && untouched.registeredRows == 19,
              "an unbound producer cannot consume a constructor row or overwrite its receipt");
        ApplicationFrameContext observer;
        BeginApplicationOwned(service, fixture, observer, true, &Frame, &context);
        runtime.constructorReadyHandler = &Ready;
        runtime.constructorReadyNotified = false;
        const auto generation = runtime.generation.load();
        Check(!service.ReadConstructorPreparation(generation, untouched) && untouched.completedRows == 17,
              "no unpublished constructor snapshot is fabricated before the first admitted row");
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "only the producer renews its actual generation BETWEEN separate normal frames");
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
        };
        const auto before = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
        context.operation = 2;
        invoke();
        Check(context.first == ClockResult::NotInitialized && context.second == ClockResult::InvalidArgument &&
                  runtime.weaponStatManager.GetStartupState() == CWeaponStatManagerDE::StartupState::NotStarted &&
                  !std::memcmp(before.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(before)),
              "readiness and foreign-clock refusal do not start the manager or write native defaults");
        context.operation = 1;
        for (unsigned refusal : {1u, 2u})
        {
            context.refusal = refusal;
            invoke();
            Check(context.first != ClockResult::Ok && context.progress.completedRows == 0xaaaa && !runtime.stopping.load() &&
                      runtime.weaponStatManager.GetStartupState() == CWeaponStatManagerDE::StartupState::NotStarted &&
                      !std::memcmp(before.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(before)),
                  "wrong native GameThread/scoped GC refuses before registration, cursor acknowledgement or native effects");
        }
        context.refusal = 0;
        const auto          pin = NativeWeaponInfoProfile::Code[0];
        const unsigned char altered = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.Patch(pin.rva, &altered, 1);
        invoke();
        Check(context.first != ClockResult::Ok && !runtime.stopping.load() && !runtime.weaponStatManager.GetRegisteredLiveRows(),
              "initial bank qualification failure leaves the unattempted constructor manager untouched");
        fixture.Patch(pin.rva, pin.bytes.data(), pin.size);
        invoke();
        Check(context.first == ClockResult::Ok && context.second == ClockResult::Busy && context.progress.completedRows == 1 &&
                  context.progress.registeredRows == 1 && !context.progress.complete && !context.progress.failed,
              "one frame advances exactly one captured shared-schedule row and a second same-frame advance refuses");
        context.operation = 0;
        // Later rows must never replay an acknowledged row, including a native
        // edit made between separate preparation callbacks.
        auto row0 = fixture.Get<NativeWeaponInfoValues>(NativeWeaponInfoProfile::BankRva);
        row0.targetRange = 909.25f;
        fixture.Put(NativeWeaponInfoProfile::BankRva, row0);
        const auto targetRows = std::size_t{22};
        while (runtime.weaponStatManager.GetCompletedStartupRows() < targetRows)
        {
            const auto prior = runtime.weaponStatManager.GetCompletedStartupRows();
            invoke();
            Check(!runtime.stopping.load() && runtime.weaponStatManager.GetCompletedStartupRows() == prior + 1,
                  "separate natural frames advance the same retained manager cursor exactly once without lease renewal inside work");
        }
        const auto* pistolDefaults = MtaGame::CWeaponStatDefaults::Select(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        Check(pistolDefaults && pistolDefaults->model >= 0, "actual next shared default has the already qualified model prerequisite");
        const auto cell = NativeWeaponInfoExtraProfile::ModelTableRva + static_cast<std::size_t>(pistolDefaults->model) * sizeof(std::uintptr_t);
        const auto savedModel = fixture.Get<std::uintptr_t>(cell);
        const auto waitingBank = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
        fixture.Put(cell, std::uintptr_t{0});
        invoke();
        Check(!runtime.stopping.load() && !context.readyCalls && runtime.weaponStatManager.GetCompletedStartupRows() == 22 &&
                  runtime.weaponStatManager.GetRegisteredLiveRows() == 22 &&
                  runtime.weaponStatManager.GetStartupState() == CWeaponStatManagerDE::StartupState::Running &&
                  !std::memcmp(waitingBank.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(waitingBank)),
              "missing next-row model prerequisites wait before registration/native prefix and never replay completed rows");
        fixture.Put(cell, savedModel);
        if (!scenario)
            while (runtime.weaponStatManager.GetCompletedStartupRows() < NativeWeaponInfoProfile::RowCount)
            {
                const auto prior = runtime.weaponStatManager.GetCompletedStartupRows();
                invoke();
                Check(!runtime.stopping.load() && runtime.weaponStatManager.GetCompletedStartupRows() == prior + 1,
                      "restored native dependency resumes exactly the next unacknowledged shared row");
            }
        if (scenario)
        {
            const auto row = NativeWeaponInfoProfile::BankRva + 22 * NativeWeaponInfoProfile::RowBytes;
            ClockTestFaults::weaponRowAddress = fixture.image + row;
            ClockTestFaults::weaponWriteAddress =
                fixture.image + row + (scenario == 1 ? offsetof(NativeWeaponInfoValues, weaponRange) : offsetof(NativeWeaponInfoValues, fireOffset));
            if (scenario == 1)
                ClockTestFaults::failWeaponReadback.store(true);
            else
                ClockTestFaults::weaponAfterAtomic = []
                {
                    // A real accepted component loses its normal500ms lease.
                    // Later cleanup must not invoke the constructor client.
                    runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
                };
            invoke();
            NativeWeaponConstructorProgress failed;
            Check(runtime.stopping.load() && runtime.weaponStatManager.GetStartupState() == CWeaponStatManagerDE::StartupState::Failed &&
                      service.ReadConstructorPreparation(generation, failed) && failed.failed && !failed.complete && failed.completedRows == 22 &&
                      failed.registeredRows == 23 && !context.readyCalls,
                  "an entered row failure publishes the retained registered prefix and terminal failure without readiness or replay");
            const auto accepted = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
            const auto frames = context.frames;
            if (scenario == 2)
            {
                Check(runtime.weaponVectorRecovery.load() && runtime.weaponVectorGeneration.load() == generation && !service.End(),
                      "a partial vector retains this startup owner and blocks End until its exact component debt retires");
                auto foreignReceiver = fixture.frameOwner;
                fixture.SetFrameReceiver(foreignReceiver.data());
                Check(FrameReceiver(foreignReceiver.data()), "foreign test receiver independently passes current normal-frame identity qualification");
                fixture.invokeFrame(foreignReceiver.data(), 0.125f);
                Check(runtime.weaponVectorRecovery.load() && runtime.weaponVectorGeneration.load() == generation && context.frames == frames &&
                          !service.End() && !std::memcmp(accepted.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(accepted)),
                      "another qualified receiver in the same generation cannot retire or inherit the original receiver's vector journal");
                fixture.SetFrameReceiver(fixture.frameOwner.data());
                runtime.weaponVectorGeneration.store(generation + 1);
                fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
                Check(runtime.weaponVectorRecovery.load() && context.frames == frames && !service.End() &&
                          !std::memcmp(accepted.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(accepted)),
                      "matching receiver without the journal's exact generation also refuses cleanup without native effects");
                runtime.weaponVectorGeneration.store(generation);
                fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
                Check(!runtime.weaponVectorRecovery.load() && !runtime.weaponVectorGeneration.load() && !runtime.weaponVectorFrameReceiver &&
                          context.frames == frames,
                      "a stopped normal frame retires only its old vector journal without another client callback or constructor row");
                const auto after = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
                Check(after[22].weaponRange == accepted[22].weaponRange && after[22].targetRange == accepted[22].targetRange &&
                          !std::memcmp(after[22].fireOffset, before[22].fireOffset, sizeof(after[22].fireOffset)),
                      "existing component-transaction recovery preserves accepted scalar prefix and conditionally retires only its own partial vector");
                std::puts("Production constructor startup-frame vector recovery fixture completed");
            }
            else
            {
                fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
                Check(context.frames == frames && !std::memcmp(accepted.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(accepted)),
                      "a stopped failed constructor never re-enters client/default schedule or rolls back accepted scalar intent");
                std::puts("Production constructor persistent row failure fixture completed");
            }
        }
        else
        {
            Check(context.readyCalls == 1 && context.manager == &runtime.weaponStatManager && runtime.constructorReadyNotified,
                  "all80 complete rows notify the actual constructor-service consumer exactly once");
            MtaGame::CWeaponStartupPolicy::Initialize(
                [&](const MtaGame::CWeaponStartupPolicy::Entry& entry)
                {
                    auto* stat = context.manager->GetWeaponStats(entry.statType, entry.skill);
                    Check(stat && stat->GetWeaponType() == entry.statType && stat->GetWeaponSkillLevel() == entry.skill,
                          "actual complete manager contains every genuine shared standard/poor/pro/special identity");
                });
            const auto completed = fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva);
            invoke();
            Check(context.readyCalls == 1 && completed[0].targetRange == 909.25f &&
                      !std::memcmp(completed.data(), fixture.Get<Bank>(NativeWeaponInfoProfile::BankRva).data(), sizeof(completed)),
                  "completed preparation does not replay default rows, duplicate ready notification or overwrite later native edits");
            NativeWeaponConstructorProgress progress;
            Check(service.ReadConstructorPreparation(generation, progress) && progress.complete && !progress.failed && progress.completedRows == 80 &&
                      progress.registeredRows == 80,
                  "producer observes coherent value-only completed progress without obtaining an SDK pointer or write callback");
            bool        foreignRefused = false;
            std::thread foreign([&] { foreignRefused = !service.ReadConstructorPreparation(generation, progress); });
            foreign.join();
            Check(foreignRefused && !service.ReadConstructorPreparation(generation + 1, progress),
                  "foreign threads and other generations cannot inherit preparation receipt authority");
            std::puts("Production bounded constructor weapon preparation owner fixture completed");
        }
        FinishApplicationOwned(service);
        NativeWeaponConstructorProgress retired{17, 19, true, true};
        Check(!service.ReadConstructorPreparation(generation, retired) && retired.completedRows == 17 && retired.registeredRows == 19,
              "End withdraws preparation observation while leaving the runtime's one-shot manager/native effects intact");
        ClockTestFaults::weaponWriteAddress = ClockTestFaults::weaponRowAddress = 0;
        ClockReaderTestHost::image = 0;
    }

    void TimerJournal(ClockImageFixture& fixture)
    {
        fixture.AdmitWeaponPins();
        fixture.AdmitWeaponVectorPins();
        ClockReaderTestHost::image = fixture.image;
        NativeClockService  service;
        WeaponVectorContext context;
        context.operation = 2;
        context.slot = 77;
        const auto rowRva = NativeWeaponInfoProfile::BankRva + context.slot * NativeWeaponInfoProfile::RowBytes;
        const auto before = fixture.Get<NativeWeaponInfoValues>(rowRva);
        BeginOwned(service, fixture, false, true, true, &WeaponVectorHandler, &context);
        ClockTestFaults::weaponWriteAddress = fixture.image + rowRva + offsetof(NativeWeaponInfoValues, fireOffset);
        ClockTestFaults::weaponAfterAtomic = []
        {
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
            // Keep the old timer route's immediate same-callback cleanup
            // pending, so its retained next-callback identity is exercised.
            if (!::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::weaponWriteAddress), sizeof(LONG), PAGE_NOACCESS,
                                  &ClockTestFaults::weaponOldProtection))
                std::abort();
        };
        fixture.invokeTimer();
        DWORD ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::weaponWriteAddress), sizeof(LONG), ClockTestFaults::weaponOldProtection, &ignored) !=
                  FALSE,
              "restore only the owned timer fixture page deliberately made unreadable at its accepted CAS");
        Check(runtime.stopping.load() && runtime.weaponVectorGeneration.load() == runtime.generation.load() && runtime.weaponVectorRecovery.load() &&
                  !runtime.weaponVectorFrameReceiver,
              "existing timing callback retains its accepted partial vector under the unchanged timing generation");
        const auto calls = context.calls;
        fixture.invokeTimer();
        Check(!runtime.weaponVectorRecovery.load() && !runtime.weaponVectorGeneration.load() && context.calls == calls &&
                  !std::memcmp(before.fireOffset, fixture.Get<NativeWeaponInfoValues>(rowRva).fireOffset, sizeof(before.fireOffset)),
              "old timing route conditionally retires its exact journal after expiry without replaying its client callback");
        FinishOwned(service, fixture);
        ClockTestFaults::weaponWriteAddress = 0;
        ClockReaderTestHost::image = 0;
        std::puts("Production existing timing weapon-vector recovery regression fixture completed");
    }
}
