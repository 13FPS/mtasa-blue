/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current raw movement group through the real registered ped owner
 *****************************************************************************/
namespace PedMoveAnimOwnerFixture
{
    void Run(ClockImageFixture& fixture)
    {
        namespace P = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace M = NativePedMetadataProfile;
        namespace Move = NativePedMoveAnimProfile;
        namespace Host = ClockPedMoveAnimOwnerTest;
        struct Saved
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
        };
        std::vector<Saved> saved;
        const auto         admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            Saved  prior{rva, std::vector<unsigned char>(size)};
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), prior.bytes.data(), size, &copied) && copied == size,
                  "save only bounded pointer-cell proof spans in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "native reference owner preserves real code, relocated-slot and mutable-bank page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : M::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : Move::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto moveGroup = fixture.image + Move::GroupRva;
        admit(Move::GroupSlotRva, &moveGroup, sizeof(moveGroup), PAGE_READONLY);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : P::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativePlayerInfoProfile::BankRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        std::vector<unsigned char> nodes(P::ReferenceCount * P::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < P::ReferenceCount; ++i)
        {
            const auto next = fixture.image + P::ReferencesRva + (i + 1) * P::ReferenceBytes;
            std::memcpy(nodes.data() + i * P::ReferenceBytes, &next, sizeof(next));
        }
        admit(P::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + P::ReferencesRva;
        admit(P::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        // Reuse the actual handle-bank fixture. This service needs no SDK ped,
        // ambient player, matrix or actor graph to register one qualified row.
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + P::PlayerVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[P::TypeOffset] = 0xab;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        const std::uint16_t model = 0xffff, neighbour = 0x5678;
        std::memcpy(pool.objects + M::ModelOffset, &model, sizeof(model));
        std::memcpy(pool.objects + M::ModelOffset + sizeof(model), &neighbour, sizeof(neighbour));
        ped_rebuild_fixture::Game         game;
        ped_rebuild_fixture::TaskScenario tasks;
        ped_rebuild_fixture::WeaponLookup lookup;
        using Player = player_move_anim_fixture::PlayerMoveAnimFixture;
        const auto makePlayer = [&](const NativePedReferenceToken& selected)
        {
            return std::make_unique<Player>(game, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access,
                                            runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access,
                                            runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access,
                                            runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access, runtime.access,
                                            runtime.access, selected, 0x12345678, tasks, lookup, nullptr);
        };

        const auto ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        Host::address = ped + Move::GroupOffset;
        Host::flagAddress = reinterpret_cast<std::uintptr_t>(pool.flags);
        Host::Reset();
        Check(NativePedMoveAnimReader::Revalidate({fixture.image}), "new group reader admits actual code/table pins over the existing owned image");
        struct DefaultAccess final : NativePedMoveAnimAccess
        {
        } absent;
        constexpr std::int32_t sentinel = 0x1badcafe;
        std::int32_t           output = sentinel;
        Check(absent.ReadPedMoveAnim({}, output) == ClockResult::NotInitialized && output == sentinel,
              "optional movement observation preserves output rather than inventing a default group");
        NativePedPoolReader::Value unused;
        Check(NativePedMoveAnimReader({fixture.image}, nullptr).Read(unused, output) == ClockResult::NotInitialized && output == sentinel,
              "readable raw storage supplies no native callback authority by itself");
        NativePedReferenceToken           token;
        ClockResult                       result = ClockResult::NotInitialized;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); };
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok && runtime.pedReferences.IsRetained(), "movement observation borrows the one genuine registered native cell");
        Host::cell = ClockPedReferenceOwnerTest::cell;
        const auto originalToken = token;
        auto       sdk = makePlayer(token);
        Check(sdk->GetWanted() == nullptr && static_cast<CPed*>(static_cast<CPlayerPed*>(sdk.get())) == static_cast<CPed*>(sdk.get()),
              "actual player composition retains one CPed identity and explicit borrowed nullable Wanted view");
        sdk->FixtureCustomMoveAnim(137);
        Check(sdk->GetCustomMoveAnim() == 137 && Host::reads == 0, "custom animation cache is independent CPU bookkeeping, not an engine group read");
        Check(runtime.access.ReadPedMoveAnim(token, output) != ClockResult::Ok && output == sentinel && Host::reads == 0,
              "a token cannot lend movement observation authority between callbacks");
        std::thread foreign([&] { result = runtime.access.ReadPedMoveAnim(token, output); });
        foreign.join();
        Check(result != ClockResult::Ok && output == sentinel && Host::reads == 0 && !runtime.stopping.load(),
              "foreign thread refuses before reading the group or stopping its owner");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only the current move-observation callback generation");
            fixture.invokeTimer();
        };
        const auto bits = [](std::int32_t value)
        {
            std::uint32_t result = 0;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        };
        const auto store = [&](std::uint32_t value) { std::memcpy(pool.objects + Move::GroupOffset, &value, sizeof(value)); };
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedMoveAnim(token, output); };
        for (const auto value : std::array<std::uint32_t, 7>{0, 138, 139, 255, 0x7fffffff, 0x80000000, 0xffffffff})
        {
            store(value);
            std::array<unsigned char, Ped::ObjectStride> before;
            std::memcpy(before.data(), pool.objects, before.size());
            Host::Reset();
            output = sentinel;
            invoke();
            Check(result == ClockResult::Ok && bits(output) == value && Host::reads == 2 && !std::memcmp(before.data(), pool.objects, before.size()),
                  "current registered group preserves every signed32 bit without enum filtering, model lookup, cache substitution or native mutation");
        }
        for (const auto table : {P::PedVtableRva, P::PlayerVtableRva})
        {
            const auto selected = fixture.image + table;
            std::memcpy(pool.objects, &selected, sizeof(selected));
            store(0xffffffff);
            Host::Reset();
            callback.action = [&](NativeGameTimingAccess&)
            {
                NativePedPoolReader::Value current;
                result = NativePedPoolReader({fixture.image}, &CheckTimingOwner).Resolve(1, current);
                if (result == ClockResult::Ok)
                    result = NativePedMoveAnimReader({fixture.image}, &CheckTimingOwner).Read(current, output);
            };
            output = sentinel;
            invoke();
            Check(result == ClockResult::Ok && bits(output) == 0xffffffff && Host::reads == 2,
                  "primitive reuses both exact known native ped identities without an ambient player row");
        }
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedMoveAnim(token, output); };
        for (const auto fail : {1u, 2u})
        {
            store(138);
            Host::Reset();
            Host::failAt = fail;
            output = sentinel;
            invoke();
            Check(result == ClockResult::ReadFailure && output == sentinel && Host::reads == fail && !runtime.stopping.load(),
                  "first or final raw OS-copy failure preserves output without native-call failure debt or owner Stop");
        }
        Host::Reset();
        store(138);
        Host::changeAt = 1;
        Host::replacement = 139;
        output = sentinel;
        invoke();
        Check(result == ClockResult::OwnerConflict && output == sentinel && Host::reads == 2 && !runtime.stopping.load(),
              "unstable raw group between snapshots refuses without pretending a cached animation is current");
        Host::Reset();
        store(138);
        Host::recycleAt = 1;
        output = sentinel;
        invoke();
        Check(result != ClockResult::Ok && output == sentinel && Host::reads == 1 && !runtime.stopping.load(),
              "native pool generation loss after the first copy prevents publication");
        pool.flags[0] = 1;
        Host::Reset();
        ++token.ownerGeneration;
        output = sentinel;
        invoke();
        Check(result != ClockResult::Ok && output == sentinel && Host::reads == 0 && !runtime.stopping.load(),
              "a foreign CPU owner generation cannot borrow current native bytes");
        token = originalToken;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            const auto generation = timingCallbackGeneration;
            ++timingCallbackGeneration;
            result = access.ReadPedMoveAnim(token, output);
            timingCallbackGeneration = generation;
        };
        output = sentinel;
        invoke();
        Check(result != ClockResult::Ok && output == sentinel && Host::reads == 0, "current thread with a foreign callback generation is still refused");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedMoveAnim(token, output); };
        Host::Reset();
        Host::callerToken = &token;
        Host::changeCaller = true;
        output = sentinel;
        invoke();
        Check(result == ClockResult::Ok && output == 138 && token.version == originalToken.version + 1,
              "owner snapshots caller token before a synchronous read can mutate the caller argument");
        token = originalToken;
        Host::Reset();
        bool      threw = false;
        eMoveAnim sdkOutput = static_cast<eMoveAnim>(0);
        callback.action = [&](NativeGameTimingAccess&)
        {
            try
            {
                sdkOutput = static_cast<CPlayerPed&>(*sdk).GetMoveAnim();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        // The unfixed common enum's language-defined range is 0..255. Raw
        // negative/high-bit preservation was tested above through int32.
        for (const auto value : {0u, 138u, 139u, 255u})
        {
            store(value);
            Host::Reset();
            threw = false;
            invoke();
            Check(!threw && static_cast<int>(sdkOutput) == static_cast<int>(value) && Host::reads == 2 && sdk->GetCustomMoveAnim() == 137,
                  "actual CPlayerPed getter observes the native group and ignores a different custom override cache");
        }
        std::puts("Production actual CPlayerPed move-animation SDK callback fixture completed");
        store(138);
        Host::Reset();
        Host::loseAt = 2;
        output = sentinel;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedMoveAnim(token, output); };
        invoke();
        Check(result != ClockResult::Ok && output == sentinel && Host::reads == 2 && !runtime.stopping.load(),
              "final independent registered-cell bookend refuses even though both primitive scalar copies succeeded");
        // The fixture restores only its own existing weak word for real unlink;
        // no production token, scalar observation or accepted intent is replayed.
        ClockPedReferenceOwnerTest::Put(ClockPedReferenceOwnerTest::cell, ped);
        Host::Reset();
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(!runtime.pedReferences.IsRetained(), "movement observation retires the same sole native cell controller");
        output = sentinel;
        Check(runtime.access.ReadPedMoveAnim(originalToken, output) != ClockResult::Ok && output == sentinel,
              "retired generation cannot publish a cached group after End");
        NativeClockService      nextService;
        NativePedReferenceToken nextToken;
        callback.action = [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, nextToken); };
        BeginOwned(nextService, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(result == ClockResult::Ok && nextToken.ownerGeneration != originalToken.ownerGeneration,
              "a separate current generation reacquires the same native row without renewing an old token");
        callback.action = [&](NativeGameTimingAccess& access) { result = access.ReadPedMoveAnim(originalToken, output); };
        Check(nextService.Renew(runtime.generation.load()), "renew new generation for stale-token observation refusal");
        fixture.invokeTimer();
        Check(result != ClockResult::Ok && output == sentinel && Host::reads == 0 && !runtime.stopping.load(),
              "same reused native address does not revive the prior CPU generation");
        callback.action = [&](NativeGameTimingAccess&)
        {
            threw = false;
            try
            {
                (void)sdk->GetMoveAnim();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        };
        Check(nextService.Renew(runtime.generation.load()), "renew current generation for actual stale SDK refusal");
        fixture.invokeTimer();
        Check(threw && Host::reads == 0, "old actual SDK wrapper retains its immutable original registration identity");
        WorldGroundOwnerFixture::Retire(nextService, fixture);
        sdk.reset();
        Check(!tasks.eventCount && !lookup.count && !runtime.pedReferences.IsRetained(),
              "read-only SDK path invokes no task/weapon factory and releases no foreign native lifetime");
        Host::Reset();
        Host::address = Host::cell = Host::flagAddress = 0;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native registered move-animation observation owner fixture completed");
    }
}
