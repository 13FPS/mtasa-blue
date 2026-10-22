/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual registered-ped owner for complete MTA fighting-style storage
 ****************************************************************************/
// The established occupied-ped/ref-cell fixture owns this callback, pool and
// retirement machinery. No current-player or script-reset policy is substituted.
{
    namespace Combat = NativePedFightingStyleProfile;
    for (const auto& pin : Combat::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    ClockPedCombatOwnerTest::host = {};
    ClockPedCombatOwnerTest::failAfter = false;
    struct DefaultCombatOwner final : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    } absent;
    std::uint8_t untouched = 0xfa;
    Check(absent.ReadPedFightingStyle(context.token, untouched) == ClockResult::NotInitialized && untouched == 0xfa &&
              absent.SetPedFightingStyle(context.token, 251, 255) == ClockResult::NotInitialized,
          "timing-only access refuses combat output and effects without a registered native callback owner");
    const auto behaviorBefore = raw(NativePedBleedingProfile::FlagsOffset);
    const auto bikeBefore = raw(NativePedBikeFlagsProfile::FlagsOffset);
    const auto shotBefore = raw(NativePedShotFlagsProfile::FlagsOffset);
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "combat owner acquires the same real selected generic-ped reference lifetime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew actual fighting-style callback");
            fixture.invokeTimer();
        };
        context.styleOutput = 0xfa;
        Check(runtime.access.ReadPedFightingStyle(context.token, context.styleOutput) != ClockResult::Ok && context.styleOutput == 0xfa &&
                  runtime.access.SetPedFightingStyle(context.token, 251, 255) != ClockResult::Ok && !ClockPedCombatOwnerTest::host.stores,
              "retained combat token outside a callback grants neither a read result nor style-write authority");
        ClockResult foreign = ClockResult::Ok;
        std::thread other([&] { foreign = runtime.access.SetPedFightingStyle(context.token, 251, 255); });
        other.join();
        Check(foreign != ClockResult::Ok && !ClockPedCombatOwnerTest::host.stores, "foreign thread cannot borrow selected-ped combat mutation scope");
        store(Combat::WordOffset, 0xa5801166u);
        context.operation = 16;
        invoke();
        Check(context.result == ClockResult::Ok && context.styleOutput == 0x11 && !runtime.playerInfo.IsBound(),
              "actual combat getter returns the selected raw style without a current-player or enum fallback");
        context.operation = 17;
        context.styleInput = 0x11;
        context.styleExtra = 6;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa5801166u && !ClockPedCombatOwnerTest::host.stores,
              "classic same-style early return performs no extra-bit effect and no conditional write");
        context.styleInput = 0xf1;
        context.styleExtra = 1;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa581f166u && ClockPedCombatOwnerTest::host.stores == 1,
              "changed style ORs eligible extra bit1 while preserving existing extra and both adjacent bytes");
        context.styleInput = 0xf2;
        context.styleExtra = 6;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa5a1f266u && ClockPedCombatOwnerTest::host.stores == 2,
              "eligible extra6 is ORed without the native script command's different reset or move mapping");
        context.styleInput = 0xf3;
        context.styleExtra = 0;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa5a1f366u,
              "extra0 changes raw style while leaving the existing extra byte intact");
        context.styleInput = 0xf4;
        context.styleExtra = 7;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa5a1f466u, "extra7 has no invented wrapped bit or reset effect");
        const auto priorStores = ClockPedCombatOwnerTest::host.stores;
        ClockPedCombatOwnerTest::host.neighborOnce = true;
        context.styleInput = 0xf5;
        context.styleExtra = 3;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa5a5f567u && ClockPedCombatOwnerTest::host.stores == priorStores + 2 &&
                  !ClockPedCombatOwnerTest::host.neighborOnce,
              "only an unaccepted neighbor conflict retries; the later accepted style/extra intent retains that neighbor writer");
        const auto token = context.token;
        ++context.token.version;
        context.operation = 16;
        context.styleOutput = 0xfa;
        invoke();
        Check(context.result != ClockResult::Ok && context.styleOutput == 0xfa && !runtime.stopping.load(),
              "stale selected token cannot publish guessed fighting style or retire unrelated ownership");
        context.operation = 17;
        invoke();
        Check(context.result != ClockResult::Ok && raw(Combat::WordOffset) == 0xa5a5f567u && !runtime.stopping.load(),
              "stale token refuses before entering a combat write");
        context.token = token;
        context.styleInput = 0xf6;
        context.styleExtra = 255;
        invoke();
        Check(context.result == ClockResult::Ok && raw(Combat::WordOffset) == 0xa5a5f667u,
              "full extra byte255 reaches unchanged MTA no-extra policy rather than a wrapped native reset");
        const auto stores = ClockPedCombatOwnerTest::host.stores;
        retire(service);
        Check(ClockPedCombatOwnerTest::host.stores == stores && raw(Combat::WordOffset) == 0xa5a5f667u,
              "normal reference retirement never restores or replays accepted combat data");
    }
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "accepted combat failure uses its own newly acquired token generation");
        store(Combat::WordOffset, 0xc3802277u);
        auto& host = ClockPedCombatOwnerTest::host;
        host.cell = ClockPedReferenceOwnerTest::cell;
        host.loseToken = true;
        const auto stores = host.stores, unlinks = ClockPedReferenceOwnerTest::unregisters;
        const auto node = ClockPedReferenceOwnerTest::node;
        context.operation = 17;
        context.styleInput = 0xee;
        context.styleExtra = 4;
        Check(service.Renew(runtime.generation.load()), "renew before accepted combat loses only final token acknowledgement");
        fixture.invokeTimer();
        Check(context.result != ClockResult::Ok && runtime.stopping.load() && host.stores == stores + 1 && raw(Combat::WordOffset) == 0xc388ee77u &&
                  !host.loseToken,
              "accepted style and extra followed by token loss Stop the same owner while retaining every accepted and unowned bit");
        Check(runtime.pedReferences.IsRetained() && !service.End(), "still-native-visible combat cell retains address-retirement debt");
        fixture.invokeTimer();
        Check(host.stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
              "stopping callbacks do not replay combat intent or reclaim still-visible reference storage");
        ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
        const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
        ClockPedReferenceOwnerTest::Put(node, head);
        ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
        ClockPedReferenceOwnerTest::Put(reinterpret_cast<std::uintptr_t>(pool.objects) + R::EntityHeadOffset, std::uintptr_t{0});
        fixture.invokeTimer();
        retire(service);
        Check(host.stores == stores + 1 && raw(Combat::WordOffset) == 0xc388ee77u, "reference bank debt resolution retires without combat rollback or replay");
        host.cell = 0;
    }
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "uncertain acknowledgement uses another genuine combat callback generation");
        store(Combat::WordOffset, 0xd4803377u);
        const auto stores = ClockPedCombatOwnerTest::host.stores;
        ClockPedCombatOwnerTest::failAfter = true;
        context.operation = 17;
        context.styleInput = 0xed;
        context.styleExtra = 2;
        Check(service.Renew(runtime.generation.load()), "renew before combat acknowledgement faults after its actual DWORD exchange");
        fixture.invokeTimer();
        ClockPedCombatOwnerTest::failAfter = false;
        Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedCombatOwnerTest::host.stores == stores + 1 &&
                  raw(Combat::WordOffset) == 0xd482ed77u,
              "unknown partial combat effect stops the owner without retrying or restoring the previous native word");
        retire(service);
        Check(ClockPedCombatOwnerTest::host.stores == stores + 1 && raw(Combat::WordOffset) == 0xd482ed77u,
              "cleanup cannot replay an unacknowledged but persistent combat effect");
    }
    Check(raw(NativePedBleedingProfile::FlagsOffset) == behaviorBefore && raw(NativePedBikeFlagsProfile::FlagsOffset) == bikeBefore &&
              raw(NativePedShotFlagsProfile::FlagsOffset) == shotBefore,
          "style/extra storage never writes independently owned bleeding/stay, bike or shot flags");
    std::puts("Production native fighting-style callback owner fixture completed");
}
