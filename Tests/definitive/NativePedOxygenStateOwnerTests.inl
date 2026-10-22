/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Selected-ped oxygen and named ducking share registered authority
 ****************************************************************************/
// Extend the existing native reference fixture after current rotation. Query
// and breath owners reuse its genuine cell bank/pool, never a fake SDK ped.
{
    namespace Oxygen = NativePedOxygenProfile;
    namespace Duck = NativePedDuckingProfile;
    for (const auto& pin : Duck::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : Oxygen::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const auto control = fixture.image + Oxygen::PlayerControlRva;
    admit(Oxygen::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
    ClockPedOxygenOwnerTest::stores = 0;
    ClockPedOxygenOwnerTest::cell = 0;
    ClockPedOxygenOwnerTest::loseToken = false;
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "ducking query acquires the same genuinely registered generic ped token");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew current registered-ped query callback");
            fixture.invokeTimer();
        };
        context.duckingOutput = true;
        Check(runtime.access.ReadPedDucking(context.token, context.duckingOutput) != ClockResult::Ok && context.duckingOutput,
              "retained token between callbacks cannot manufacture successful false ducking output");
        ClockResult foreign = ClockResult::Ok;
        bool        foreignOutput = true;
        std::thread other([&] { foreign = runtime.access.ReadPedDucking(context.token, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput, "foreign-thread ducking query preserves output without native admission");
        context.operation = 7;
        store(Duck::FlagsOffset, 0xffffffffu ^ Duck::DuckingMask);
        invoke();
        Check(context.result == ClockResult::Ok && !context.duckingOutput,
              "actual callback returns false from the named ducking bit even when every unrelated flag is set");
        store(Duck::FlagsOffset, Duck::DuckingMask);
        invoke();
        Check(context.result == ClockResult::Ok && context.duckingOutput && raw(Duck::FlagsOffset) == Duck::DuckingMask,
              "actual selected-ped IsDucking fact reads true without changing native flags or inferring task presence");
        const auto valid = context.token;
        ++context.token.version;
        context.duckingOutput = true;
        invoke();
        Check(context.result != ClockResult::Ok && context.duckingOutput && !runtime.stopping.load(),
              "stale query token preserves output and does not consume the unrelated valid owner");
        context.token = valid;
        context.operation = 5;
        context.output = 123;
        invoke();
        Check(context.result != ClockResult::Ok && context.output == 123 && !ClockPedOxygenOwnerTest::stores && !runtime.stopping.load(),
              "generic ped without proved player-data ownership refuses oxygen without current-player substitution");
        retire(service);
    }
    // The initial breath subset is constructor-proved player row0. The
    // selected token must name that row's actual ped and inline data; current
    // player selection is deliberately invalid and cannot be a fallback.
    std::array<unsigned char, Oxygen::RowBytes> row{};
    const auto                                  pedAddress = reinterpret_cast<std::uintptr_t>(pool.objects);
    std::memcpy(row.data(), &pedAddress, sizeof(pedAddress));
    admit(Oxygen::BankRva, row.data(), row.size(), PAGE_READWRITE);
    const unsigned char unrelatedCurrentPlayer = 0xff;
    admit(NativePlayerInfoProfile::PlayerIndexRva, &unrelatedCurrentPlayer, sizeof(unrelatedCurrentPlayer), PAGE_READWRITE);
    const auto playerVtable = fixture.image + Oxygen::PlayerVtableRva;
    std::memcpy(pool.objects, &playerVtable, sizeof(playerVtable));
    const auto inlineData = fixture.image + Oxygen::BankRva + Oxygen::InlineDataOffset;
    std::memcpy(pool.objects + Oxygen::PlayerDataOffset, &inlineData, sizeof(inlineData));
    store(Oxygen::PlayerTagOffset, 0);
    const auto breathRva = Oxygen::BankRva + Oxygen::InlineDataOffset + Oxygen::BreathOffset;
    const auto staminaRva = Oxygen::BankRva + Oxygen::InlineDataOffset + 0x24;
    fixture.Put(breathRva, std::uint32_t{0x3f800001});
    fixture.Put(breathRva + 4, std::uint32_t{0x44556677});
    fixture.Put(staminaRva, std::uint32_t{0x42a50000});
    for (unsigned scenario = 0; scenario != 2; ++scenario)
    {
        context.operation = 0;
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &PedPhysicalScalarsOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok, "oxygen owner acquires a fresh registered exact player ped lifetime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew selected-player raw oxygen callback");
            fixture.invokeTimer();
        };
        if (!scenario)
        {
            context.output = 123;
            Check(runtime.access.ReadPedOxygenLevel(context.token, context.output) != ClockResult::Ok && context.output == 123 &&
                      runtime.access.SetPedOxygenLevel(context.token, 2) != ClockResult::Ok && !ClockPedOxygenOwnerTest::stores,
                  "a retained selected-player token cannot supply breath output or mutation between callbacks");
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.SetPedOxygenLevel(context.token, 2); });
            other.join();
            Check(foreign != ClockResult::Ok && !ClockPedOxygenOwnerTest::stores, "foreign thread cannot borrow a selected player's native breath row");
            context.operation = 5;
            const std::uintptr_t missing = 0;
            std::memcpy(pool.objects + Oxygen::PlayerDataOffset, &missing, sizeof(missing));
            invoke();
            Check(context.result == ClockResult::NotInitialized && context.output == 123 && !runtime.stopping.load(),
                  "missing selected player data refuses before effects and preserves caller output");
            std::memcpy(pool.objects + Oxygen::PlayerDataOffset, &inlineData, sizeof(inlineData));
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x3f800001 && !runtime.playerInfo.IsBound(),
                  "oxygen reads selected inline breath with invalid current-player index and no separate PlayerInfo binding");
            context.operation = 6;
            context.target = fromBits(0x7fc12345);
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<std::uint32_t>(breathRva) == 0x7fc12345 &&
                      fixture.Get<std::uint32_t>(staminaRva) == 0x42a50000 && fixture.Get<std::uint32_t>(breathRva + 4) == 0x44556677 &&
                      ClockPedOxygenOwnerTest::stores == 1,
                  "actual oxygen CAS preserves raw NaN payload and unrelated stamina/neighbor bytes");
            const auto valid = context.token;
            ++context.token.version;
            context.operation = 5;
            context.output = 123;
            invoke();
            Check(context.result != ClockResult::Ok && context.output == 123 && !runtime.stopping.load() && ClockPedOxygenOwnerTest::stores == 1,
                  "stale breath token refuses publication without fabricating oxygen or replaying a prior write");
            context.token = valid;
            context.operation = 6;
            context.target = fromBits(0x80000000);
            invoke();
            context.operation = 5;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x80000000, "actual oxygen owner reads back accepted raw signed zero");
            retire(service);
            Check(fixture.Get<std::uint32_t>(breathRva) == 0x80000000, "normal owner retirement preserves accepted breath intent");
        }
        else
        {
            const auto stores = ClockPedOxygenOwnerTest::stores, unlinks = ClockPedReferenceOwnerTest::unregisters;
            const auto node = ClockPedReferenceOwnerTest::node;
            ClockPedOxygenOwnerTest::cell = ClockPedReferenceOwnerTest::cell;
            ClockPedOxygenOwnerTest::loseToken = true;
            context.operation = 6;
            context.target = fromBits(0xffc07654);
            invoke();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedOxygenOwnerTest::stores == stores + 1 &&
                      fixture.Get<std::uint32_t>(breathRva) == 0xffc07654 && !ClockPedOxygenOwnerTest::loseToken,
                  "accepted oxygen write followed by token loss stops exact owner and preserves its raw breath effect");
            Check(runtime.pedReferences.IsRetained() && !service.End(), "native node address still owns the lost-token oxygen cell retirement debt");
            fixture.invokeTimer();
            Check(ClockPedOxygenOwnerTest::stores == stores + 1 && ClockPedReferenceOwnerTest::unregisters == unlinks && !service.End(),
                  "stopping callbacks do not replay breath mutation or reclaim native-visible reference storage");
            // Supply the established ResolveReferences address-retirement
            // postcondition only; no accepted breath value is restored.
            ClockPedReferenceOwnerTest::Put(node + 8, std::uintptr_t{0});
            const auto head = ClockPedReferenceOwnerTest::Get<std::uintptr_t>(fixture.image + R::FreeHeadRva);
            ClockPedReferenceOwnerTest::Put(node, head);
            ClockPedReferenceOwnerTest::Put(fixture.image + R::FreeHeadRva, node);
            ClockPedReferenceOwnerTest::Put(pedAddress + R::EntityHeadOffset, std::uintptr_t{0});
            fixture.invokeTimer();
            retire(service);
            Check(ClockPedOxygenOwnerTest::stores == stores + 1 && fixture.Get<std::uint32_t>(breathRva) == 0xffc07654 &&
                      fixture.Get<std::uint32_t>(staminaRva) == 0x42a50000 && fixture.Get<std::uint32_t>(breathRva + 4) == 0x44556677,
                  "native address retirement completes without oxygen rollback, replay or unrelated player-data changes");
        }
    }
    ClockPedOxygenOwnerTest::cell = 0;
    ClockPedOxygenOwnerTest::loseToken = false;
    std::puts("Production native oxygen callback owner fixture completed");
    std::puts("Production native ducking callback owner fixture completed");
}
