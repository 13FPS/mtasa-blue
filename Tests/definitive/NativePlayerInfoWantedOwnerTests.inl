/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real callback admission for player intents and complete CWanted SDK
 *****************************************************************************/
namespace
{
    struct PlayerWantedOwnerContext
    {
        unsigned      operation = 0, refusal = 0, sdkIndex = 0;
        std::int32_t  money = 0x12345678, value = -321;
        std::int16_t  food = -1234;
        bool          instant = false, tired = true, threw = false;
        std::uint32_t level = 3, observed = 0xdeadbeef;
        float         floatOutput = 123.0f;
        std::uint8_t  drunk = 7;
        CPlayerInfo*  player = reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
        CWanted*      wanted = reinterpret_cast<CWanted*>(std::uintptr_t{1});
        ClockResult   result = ClockResult::NotInitialized;
    };

    void PlayerWantedOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<PlayerWantedOwnerContext*>(opaque);
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        context.threw = false;
        try
        {
            switch (context.operation)
            {
                case 0:
                    context.result = access.ReadPlayerMoney(context.money);
                    break;
                case 1:
                    context.result = access.SetPlayerMoney(context.value, context.instant);
                    break;
                case 2:
                    context.result = access.SetPlayerLastTimeEaten(context.food);
                    break;
                case 3:
                    context.result = access.SetPlayerDoesNotGetTired(context.tired);
                    break;
                case 4:
                    context.result = access.GetWanted(context.wanted);
                    break;
                case 5:
                    context.result = access.ReadWantedLevel(context.observed);
                    break;
                case 6:
                    context.wanted->SetWantedLevel(context.level);
                    context.result = ClockResult::Ok;
                    break;
                case 7:
                    context.wanted->SetWantedLevelNoFlash(context.level);
                    context.result = ClockResult::Ok;
                    break;
                case 8:
                    context.result = access.SetWantedLevel(context.level, context.instant);
                    break;
                case 9:
                    context.observed = static_cast<unsigned char>(context.wanted->GetWantedLevel());
                    context.result = ClockResult::Ok;
                    break;
                case 10:
                    context.result = access.GetPlayerInfo(context.player);
                    break;
                case 11:
                    context.money = context.player->GetPlayerMoney();
                    context.result = ClockResult::Ok;
                    break;
                case 12:
                    context.player->SetPlayerMoney(context.value, context.instant);
                    context.result = ClockResult::Ok;
                    break;
                case 13:
                    context.player->SetLastTimeEaten(context.food);
                    context.result = ClockResult::Ok;
                    break;
                case 14:
                    context.player->SetDoesNotGetTired(context.tired);
                    context.result = ClockResult::Ok;
                    break;
                case 15:
                    context.wanted = context.player->GetWanted();
                    context.result = ClockResult::Ok;
                    break;
                case 16:
                    switch (context.sdkIndex)
                    {
                        case 0:
                            context.floatOutput = context.player->GetFPSMoveHeading();
                            break;
                        case 1:
                            context.observed = context.player->GetCamDrunkLevel();
                            break;
                        case 2:
                            context.player->SetCamDrunkLevel(context.drunk);
                            break;
                        case 3:
                            context.observed = context.player->GetCarTwoWheelCounter();
                            break;
                        case 4:
                            context.floatOutput = context.player->GetCarTwoWheelDist();
                            break;
                        case 5:
                            context.observed = context.player->GetCarLess3WheelCounter();
                            break;
                        case 6:
                            context.observed = context.player->GetBikeRearWheelCounter();
                            break;
                        case 7:
                            context.floatOutput = context.player->GetBikeRearWheelDist();
                            break;
                        case 8:
                            context.observed = context.player->GetBikeFrontWheelCounter();
                            break;
                        case 9:
                            context.floatOutput = context.player->GetBikeFrontWheelDist();
                            break;
                        case 10:
                            context.observed = context.player->GetMaxHealth();
                            break;
                        case 11:
                            context.observed = context.player->GetMaxArmor();
                            break;
                        case 12:
                            context.observed = context.player->CanDoDriveBy() ? 1u : 0u;
                            break;
                        default:
                            std::abort();
                    }
                    context.result = ClockResult::Ok;
                    break;
                default:
                    std::abort();
            }
        }
        catch (const std::runtime_error&)
        {
            context.threw = true;
            context.result = ClockResult::WriteFailure;
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativePlayerInfoWantedOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativePlayerInfoProfile;
        namespace W = NativeWantedProfile;
        namespace X = NativePlayerInfoExtraProfile;
        static_assert(!std::is_abstract_v<CPlayerInfoDE> && !std::is_abstract_v<CWantedDE>);
        namespace S = NativeGameStateProfile;
        // Save every replaced instruction span: several proof fragments share
        // pages with earlier owned caller stubs, which must remain executable
        // only in their original fixture form after this cohort retires.
        struct SavedPin
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
        };
        std::vector<SavedPin> savedPins;
        const auto            admit = [&](std::uintptr_t rva, const unsigned char* bytes, std::size_t size, bool code)
        {
            SavedPin saved{rva, std::vector<unsigned char>(size)};
            SIZE_T   copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), saved.bytes.data(), size, &copied) && copied == size,
                  "save only each bounded owned instruction span before player/wanted qualification");
            savedPins.push_back(std::move(saved));
            fixture.Patch(rva, bytes, size);
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, code ? PAGE_EXECUTE_READ : PAGE_READONLY, &ignored) != FALSE,
                  "qualify exact player/wanted code on real executable image pages without running its instructions");
        };
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, true);
        for (const auto& pin : W::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, true);
        for (const auto& pin : X::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, true);
        for (const auto& pin : S::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code);
        const auto row = fixture.Get<std::array<unsigned char, P::RowBytes>>(P::BankRva);
        const auto oldIndex = fixture.Get<unsigned char>(P::PlayerIndexRva);
        const auto oldState = fixture.Get<std::uint32_t>(S::stateRva);
        const auto oldTimer = fixture.Get<std::uint32_t>(W::TimerRva);
        const auto oldMaximum = fixture.Get<std::uint32_t>(W::MaximumLevelRva);
        const auto oldChaos = fixture.Get<std::int32_t>(W::MaximumChaosRva);
        const auto oldControl = fixture.Get<std::uintptr_t>(P::PlayerVtableRva + 0x50);
        const auto writable = [&](std::uintptr_t rva, std::size_t bytes)
        {
            DWORD prior = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), bytes, PAGE_READWRITE, &prior) != FALSE,
                  "admit only this owned fixture's actual native player/wanted data pages");
        };
        writable(P::BankRva, P::RowBytes);
        writable(P::PlayerIndexRva, 1);
        writable(S::stateRva, 4);
        writable(W::TimerRva, 4);
        writable(W::MaximumLevelRva, 8);
        fixture.statsPed.fill(0);
        fixture.statsData.fill(0);
        const auto store = [](auto& bytes, std::size_t offset, const auto& value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
        store(fixture.statsPed, 0, fixture.image + P::PlayerVtableRva);
        store(fixture.statsPed, 0x5f0, fixture.image + P::BankRva + 8);
        store(fixture.statsPed, 0x7e0, std::int32_t{0});
        store(fixture.statsData, W::LevelOffset, std::uint32_t{1});
        store(fixture.statsData, W::TimestampOffset, std::uint32_t{55});
        const auto wantedAddress = reinterpret_cast<std::uintptr_t>(fixture.statsData.data());
        fixture.Put(P::BankRva, reinterpret_cast<std::uintptr_t>(fixture.statsPed.data()));
        fixture.Put(W::WantedRootRva, wantedAddress);
        fixture.Put(P::PlayerIndexRva, static_cast<unsigned char>(0));
        fixture.Put(S::stateRva, std::uint32_t{9});
        fixture.Put(P::BankRva + P::MoneyOffset, std::int32_t{123});
        fixture.Put(P::BankRva + P::DisplayMoneyOffset, std::int32_t{77});
        fixture.Put(W::TimerRva, std::uint32_t{5000});
        fixture.Put(W::MaximumLevelRva, std::uint32_t{6});
        fixture.Put(W::MaximumChaosRva, std::int32_t{6900});
        fixture.Put(P::BankRva + X::FpsMoveHeadingOffset, -1.25f);
        fixture.Put(P::BankRva + X::CarTwoWheelCounterOffset, std::uint32_t{0x80000001});
        fixture.Put(P::BankRva + X::CarTwoWheelDistanceOffset, 10.25f);
        fixture.Put(P::BankRva + X::CarLess3WheelCounterOffset, std::uint32_t{0x80000002});
        fixture.Put(P::BankRva + X::BikeRearWheelCounterOffset, std::uint32_t{0x80000003});
        fixture.Put(P::BankRva + X::BikeRearWheelDistanceOffset, -20.5f);
        fixture.Put(P::BankRva + X::BikeFrontWheelCounterOffset, std::uint32_t{0x80000004});
        fixture.Put(P::BankRva + X::BikeFrontWheelDistanceOffset, 30.75f);
        fixture.Put(P::BankRva + X::CamDrunkLevelOffset, std::uint8_t{4});
        fixture.Put(P::BankRva + X::CamDrunkFadeOffset, std::uint8_t{9});
        fixture.Put(P::BankRva + X::MaxHealthOffset, std::uint8_t{250});
        fixture.Put(P::BankRva + X::MaxArmorOffset, std::uint8_t{251});
        fixture.Put(P::BankRva + X::CanDoDriveByOffset, std::uint8_t{2});
        const auto control = fixture.image + P::PlayerControlRva;
        fixture.Patch(P::PlayerVtableRva + 0x50, &control, sizeof(control));
        DWORD ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + P::PlayerVtableRva + 0x50), 8, PAGE_READONLY, &ignored) != FALSE,
              "the loaded player uses the independently pinned actual read-only control vtable");
        ClockReaderTestHost::image = ClockWantedOwnerTest::image = fixture.image;
        ClockWantedOwnerTest::calls = 0;
        ClockWantedOwnerTest::result = ClockResult::Ok;
        PlayerWantedOwnerContext context;
        Check(runtime.access.ReadPlayerMoney(context.money) == ClockResult::NotInitialized && context.money == 0x12345678 &&
                  runtime.access.GetWanted(context.wanted) == ClockResult::NotInitialized && context.wanted == reinterpret_cast<CWanted*>(std::uintptr_t{1}) &&
                  runtime.access.GetPlayerInfo(context.player) == ClockResult::NotInitialized &&
                  context.player == reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1}),
              "unbound Runtime player and wanted services preserve caller outputs and provide no native lease");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "the actual player/wanted callback and hooks retire without a native object rewind");
            }
            Check(!service.IsRetained() && !runtime.playerInfo.IsBound() && !runtime.playerInfoGeneration && !runtime.timingHandler && !runtime.timingContext &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "retirement resets borrowed player binding only after the real callback owner has retired");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PlayerWantedOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew player/wanted scope between individual natural callbacks");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.money == 123 && runtime.playerInfo.IsBound() &&
                      runtime.playerInfoGeneration == runtime.generation.load(),
                  "the real admitted callback binds one exact native player row per generation");
            context.operation = 10;
            invoke();
            Check(context.result == ClockResult::Ok && context.player == &runtime.playerInfoSdk,
                  "all eighteen methods and the dependent wanted service qualify before publishing the actual complete Runtime CPlayerInfo");
            auto* const playerIdentity = context.player;
            invoke();
            Check(context.result == ClockResult::Ok && context.player == playerIdentity,
                  "fresh callbacks publish stable actual SDK identity without retaining the previously observed native player row");
            context.operation = 11;
            invoke();
            Check(context.result == ClockResult::Ok && context.money == 123, "actual CPlayerInfo money getter dispatches through the retained native owner");
            context.operation = 12;
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<std::int32_t>(P::BankRva + P::MoneyOffset) == context.value &&
                      fixture.Get<std::int32_t>(P::BankRva + P::DisplayMoneyOffset) == 77,
                  "noninstant signed money intent preserves the observed native display value through actual pair CAS");
            context.value = -7654321;
            context.instant = true;
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<std::int32_t>(P::BankRva + P::MoneyOffset) == context.value &&
                      fixture.Get<std::int32_t>(P::BankRva + P::DisplayMoneyOffset) == context.value,
                  "instant money intent commits both actual signed members without clamping or a cached row");
            context.operation = 13;
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<std::int16_t>(P::BankRva + P::LastTimeEatenOffset) == context.food,
                  "native short-width last-eaten intent retains signed SDK semantics");
            context.operation = 14;
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<unsigned char>(P::BankRva + P::DoesNotGetTiredOffset) == 1,
                  "never-tired uses the proved native byte without touching adjacent player data");
            context.operation = 16;
            for (context.sdkIndex = 0; context.sdkIndex < 13; ++context.sdkIndex)
            {
                invoke();
                bool exact = false;
                switch (context.sdkIndex)
                {
                    case 0:
                        exact = context.floatOutput == -1.25f;
                        break;
                    case 1:
                        exact = context.observed == 4;
                        break;
                    case 2:
                        exact = fixture.Get<std::uint8_t>(P::BankRva + X::CamDrunkLevelOffset) == 7 &&
                                fixture.Get<std::uint8_t>(P::BankRva + X::CamDrunkFadeOffset) == 0;
                        break;
                    case 3:
                        exact = context.observed == 0x80000001;
                        break;
                    case 4:
                        exact = context.floatOutput == 10.25f;
                        break;
                    case 5:
                        exact = context.observed == 0x80000002;
                        break;
                    case 6:
                        exact = context.observed == 0x80000003;
                        break;
                    case 7:
                        exact = context.floatOutput == -20.5f;
                        break;
                    case 8:
                        exact = context.observed == 0x80000004;
                        break;
                    case 9:
                        exact = context.floatOutput == 30.75f;
                        break;
                    case 10:
                        exact = context.observed == 250;
                        break;
                    case 11:
                        exact = context.observed == 251;
                        break;
                    case 12:
                        exact = context.observed == 1;
                        break;
                    default:
                        std::abort();
                }
                Check(context.result == ClockResult::Ok && !context.threw && exact,
                      "each remaining actual CPlayerInfo SDK method preserves qualified native widths, finite values and classic camera/ability semantics");
            }
            context.sdkIndex = 2;
            context.drunk = 0;
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<std::uint8_t>(P::BankRva + X::CamDrunkLevelOffset) == 0 &&
                      fixture.Get<std::uint8_t>(P::BankRva + X::CamDrunkFadeOffset) == 1,
                  "actual camera reset uses the classic level-zero fade intent through one native short pair CAS");
            context.operation = 15;
            invoke();
            Check(context.result == ClockResult::Ok && context.wanted == &runtime.wanted,
                  "fresh loaded-player qualification publishes the exact complete Runtime CWanted SDK object");
            auto* const stable = context.wanted;
            invoke();
            Check(context.result == ClockResult::Ok && context.wanted == stable,
                  "subsequent callbacks borrow the same actual SDK object while independently requalifying its current native wanted object");
            context.operation = 6;
            context.level = 2;
            invoke();
            Check(context.result == ClockResult::Ok && !context.threw && ClockWantedOwnerTest::calls == 1 && ClockWantedOwnerTest::entry == W::SetRva &&
                      ClockWantedOwnerTest::receiver == wantedAddress && ClockWantedOwnerTest::level == 2,
                  "actual CWanted setter reaches only the correctly typed native function and currently qualified receiver");
            context.operation = 7;
            context.level = 4;
            const auto stamp = [&]
            {
                std::uint32_t value = 0;
                std::memcpy(&value, fixture.statsData.data() + W::TimestampOffset, 4);
                return value;
            };
            store(fixture.statsData, W::TimestampOffset, std::uint32_t{777});
            const auto beforeStamp = stamp();
            invoke();
            Check(context.result == ClockResult::Ok && !context.threw && ClockWantedOwnerTest::calls == 2 && stamp() == beforeStamp,
                  "actual NoFlash retains native reaction effects while narrowly restoring only the attributable timer stamp");
            context.operation = 9;
            invoke();
            Check(context.result == ClockResult::Ok && context.observed == 4, "actual CWanted getter requalifies the native level on the current callback");
            const auto   calls = ClockWantedOwnerTest::calls;
            std::int32_t money = 1234567;
            CWanted*     output = reinterpret_cast<CWanted*>(std::uintptr_t{1});
            Check(runtime.access.ReadPlayerMoney(money) != ClockResult::Ok && money == 1234567 && runtime.access.GetWanted(output) != ClockResult::Ok &&
                      output == reinterpret_cast<CWanted*>(std::uintptr_t{1}) && runtime.access.SetWantedLevel(5, false) != ClockResult::Ok &&
                      ClockWantedOwnerTest::calls == calls,
                  "producer calls cannot reuse prior player binding or wanted publication as callback authority");
            CPlayerInfo* unavailablePlayer = reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
            Check(runtime.access.GetPlayerInfo(unavailablePlayer) != ClockResult::Ok && unavailablePlayer == reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1}),
                  "complete SDK publication also refuses outside the current callback and preserves caller identity");
            bool        foreignRefused = false;
            std::thread foreign(
                [&]
                {
                    CPlayerInfo* player = reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
                    foreignRefused = runtime.access.SetPlayerMoney(7, true) != ClockResult::Ok && runtime.access.SetWantedLevel(5, false) != ClockResult::Ok &&
                                     runtime.access.GetPlayerInfo(player) != ClockResult::Ok && player == reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
                });
            foreign.join();
            Check(foreignRefused && ClockWantedOwnerTest::calls == calls, "a foreign thread cannot mutate player money or invoke wanted reactions");
            context.operation = 0;
            for (unsigned refusal = 1; refusal <= 4; ++refusal)
            {
                context.refusal = refusal;
                context.money = 0x12345678;
                invoke();
                Check(context.result != ClockResult::Ok && context.money == 0x12345678,
                      "stale generation/admission, nested and expired callback scopes preserve player outputs");
                context.operation = 10;
                context.player = reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
                invoke();
                Check(context.result != ClockResult::Ok && context.player == reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1}),
                      "the complete player factory requires the same fresh callback rather than cached qualification");
                context.operation = 0;
            }
            context.refusal = 0;
            const auto generation = runtime.playerInfoGeneration;
            runtime.playerInfoGeneration = generation + 1;
            invoke();
            Check(context.result == ClockResult::OwnerConflict, "a player reader bound to another generation never supplies a native write lease");
            runtime.playerInfoGeneration = generation;
            for (const std::uint32_t state : {6u, 7u, 8u})
            {
                fixture.Put(S::stateRva, state);
                context.operation = 8;
                invoke();
                Check(context.result == ClockResult::NotInitialized && ClockWantedOwnerTest::calls == calls,
                      "native wanted effects require real playing9 and cannot be admitted by a startup transition state");
            }
            fixture.Put(S::stateRva, std::uint32_t{9});
            fixture.Put(P::BankRva, std::uintptr_t{0});
            invoke();
            Check(context.result == ClockResult::NotInitialized && ClockWantedOwnerTest::calls == calls,
                  "state9 without the actual loaded ped remains unavailable before any native wanted call");
            fixture.Put(P::BankRva, reinterpret_cast<std::uintptr_t>(fixture.statsPed.data()));
            fixture.Put(W::WantedRootRva, std::uintptr_t{0});
            context.operation = 4;
            context.wanted = reinterpret_cast<CWanted*>(std::uintptr_t{1});
            invoke();
            Check(context.result == ClockResult::NotInitialized && context.wanted == reinterpret_cast<CWanted*>(std::uintptr_t{1}),
                  "missing native wanted storage refuses output rather than publishing a fake zero-level object");
            context.operation = 10;
            context.player = reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
            invoke();
            Check(context.result == ClockResult::NotInitialized && context.player == reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1}),
                  "complete player publication refuses when its native wanted dependency is absent");
            fixture.Put(W::WantedRootRva, wantedAddress);
            retire(service);
            Check(fixture.Get<std::int32_t>(P::BankRva + P::MoneyOffset) == -7654321 &&
                      fixture.Get<std::int16_t>(P::BankRva + P::LastTimeEatenOffset) == context.food &&
                      fixture.Get<unsigned char>(P::BankRva + P::DoesNotGetTiredOffset) == 1,
                  "End preserves accepted player intents instead of replaying an old row snapshot");
        }
        for (unsigned failure = 0; failure < 3; ++failure)
        {
            fixture.Put(S::stateRva, std::uint32_t{9});
            fixture.Put(W::TimerRva, std::uint32_t{5000});
            context.operation = 8;
            context.level = failure + 1;
            store(fixture.statsData, W::TimestampOffset, static_cast<std::uint32_t>(40 + failure));
            context.instant = failure == 2;
            ClockWantedOwnerTest::result = failure == 0 ? ClockResult::OriginalException : ClockResult::Ok;
            if (failure == 1)
                ClockWantedOwnerTest::after = [] { runtime.admissionGeneration.fetch_add(1); };
            if (failure == 2)
                ClockWantedOwnerTest::after = []
                {
                    const std::uint32_t later = 5001;
                    SIZE_T              copied = 0;
                    if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(runtime.image + NativeWantedProfile::TimerRva), &later, 4,
                                              &copied) ||
                        copied != 4)
                        std::abort();
                };
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PlayerWantedOwnerHandler, &context);
            const auto calls = ClockWantedOwnerTest::calls;
            fixture.invokeTimer();
            const auto expected = failure == 0 ? ClockResult::OriginalException : ClockResult::OwnerConflict;
            Check(context.result == expected && ClockWantedOwnerTest::calls == calls + 1 && runtime.stopping.load(),
                  "entered native exception, revoked post-call scope or foreign NoFlash timer stops this exact owner without replay");
            std::uint32_t level = 0;
            std::memcpy(&level, fixture.statsData.data() + W::LevelOffset, 4);
            Check(level == context.level && runtime.access.SetWantedLevel(6, false) != ClockResult::Ok && ClockWantedOwnerTest::calls == calls + 1,
                  "already-entered native wanted effects survive refusal and cannot be automatically retried");
            retire(service);
            if (failure == 2)
                Check(fixture.Get<std::uint32_t>(W::TimerRva) == 5001, "NoFlash refusal preserves an intervening native timer advance through End");
        }
        context.money = 0x12345678;
        context.player = reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1});
        context.wanted = reinterpret_cast<CWanted*>(std::uintptr_t{1});
        Check(runtime.access.ReadPlayerMoney(context.money) == ClockResult::NotInitialized && context.money == 0x12345678 &&
                  runtime.access.GetWanted(context.wanted) == ClockResult::NotInitialized && context.wanted == reinterpret_cast<CWanted*>(std::uintptr_t{1}) &&
                  runtime.access.GetPlayerInfo(context.player) == ClockResult::NotInitialized &&
                  context.player == reinterpret_cast<CPlayerInfo*>(std::uintptr_t{1}),
              "retired callback services cannot resurrect player/wanted pointers or overwrite caller output");
        ClockWantedOwnerTest::result = ClockResult::Ok;
        ClockWantedOwnerTest::after = nullptr;
        ClockReaderTestHost::image = ClockWantedOwnerTest::image = 0;
        fixture.Put(P::BankRva, row);
        fixture.Put(P::PlayerIndexRva, oldIndex);
        fixture.Put(S::stateRva, oldState);
        fixture.Put(W::TimerRva, oldTimer);
        fixture.Put(W::MaximumLevelRva, oldMaximum);
        fixture.Put(W::MaximumChaosRva, oldChaos);
        fixture.Patch(P::PlayerVtableRva + 0x50, &oldControl, sizeof(oldControl));
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + P::PlayerVtableRva + 0x50), 8, PAGE_READONLY, &ignored) != FALSE,
              "restore only fixture control-table protection after complete native owner retirement");
        for (auto pin = savedPins.rbegin(); pin != savedPins.rend(); ++pin)
            fixture.Patch(pin->rva, pin->bytes.data(), pin->bytes.size());
        std::puts("Production native player-info and complete Wanted SDK owner fixture completed");
    }
}
