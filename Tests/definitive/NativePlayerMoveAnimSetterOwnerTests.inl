/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual registered-player scalar intent under the existing hook owner
 ****************************************************************************/
namespace ClockMoveAnimSetterOwnerTest
{
    unsigned                calls = 0, accepted = 0, mode = 0;
    std::uintptr_t          address = 0, worldField = 0, worldValue = 0;
    NativePedReferenceToken retireToken;
    ClockResult             retireResult = ClockResult::NotInitialized;
    LONG                    Exchange(volatile LONG* target, LONG desired, LONG expected) noexcept
    {
        ++calls;
        address = reinterpret_cast<std::uintptr_t>(target);
        if (mode == 1)
            RaiseException(0xe0424901, 0, 0, nullptr);
        if (mode == 4)
            ::InterlockedExchange(target, expected ^ 0x20);
        const LONG prior = ::InterlockedCompareExchange(target, desired, expected);
        if (prior == expected)
            ++accepted;
        if (mode == 2)
        {
            const std::uintptr_t missing = 0;
            std::memcpy(reinterpret_cast<void*>(worldField), &missing, sizeof(missing));
        }
        if (mode == 3)
            retireResult = runtime.access.ReleasePedReference(retireToken);
        return prior;
    }
}
namespace PlayerMoveAnimSetterOwnerFixture
{
    using namespace PlayerMoveAnimHookOwnerFixture;
    namespace Hook = ClockMoveAnimSetterOwnerTest;
    struct LateProbe
    {
        unsigned                         calls = 0, trigger = 0;
        WorldGroundOwnerFixture::Writes* writes = nullptr;
        std::uintptr_t                   rva = 0;
        std::size_t                      size = 0;
        DWORD                            protection = PAGE_READWRITE;
        std::array<unsigned char, 8>     bytes{};
    };
    LateProbe*  late = nullptr;
    ClockResult LateAfterApplication() noexcept
    {
        const auto result = CheckApplicationTimingOwner();
        if (result == ClockResult::Ok && late && ++late->calls == late->trigger)
            WorldGroundOwnerFixture::Writes::Apply(late->writes, late->rva, late->bytes.data(), late->size, late->protection);
        return result;
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, bytes, size, protection); };
        const auto code = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        const auto typed = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        // Native world admission and the normal application caller share
        // RootRva5725750. Their one owned receiver must carry BOTH roles;
        // publishing a different frame object would discard the typed graph.
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::State allocations;
        allocations.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        code(Ped::Code);
        code(R::Code);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        code(NativeAnimationBlockProfile::Code);
        writes.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                      static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        writes.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                      static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        writes.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        code(NativeAnimationBlockRefProfile::Code);
        code(NativeAnimationGroupArrayProfile::Code);
        code(NativeAnimationStaticAssociationProfile::Code);
        code(NativeAnimationStaticAssociationProfile::ReadCode);
        code(NativeAnimationHierarchyReadProfile::Code);
        code(NativeAnimationAssociationProfile::Code);
        writes.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        OwnerSources sources(fixture, writes);
        writes.Global(Ped::RootRva, std::uintptr_t{0});
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto                                               ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        alignas(std::uintptr_t) std::array<unsigned char, 0x100> model{};
        NativePedRebuildFixture::State                           state;
        Check(state.Ready(), "own the existing intelligence/task pools without publishing SDK task products");
        state.Reset(fixture.image, ped, reinterpret_cast<std::uintptr_t>(model.data()), 0, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        // Rebuild's general render fixture has a synthetic list sentinel. This
        // distinct current clump deliberately models the original's valid EMPTY list.
        Store(state.Container(), std::uintptr_t{0});
        Store(ped, fixture.image + Player::PlayerVtableRva);
        pool.objects[R::TypeOffset] = 3;
        Store(ped + NativePedMoveAnimProfile::GroupOffset, std::int32_t{0});
        code(NativePedMoveAnimProfile::Code);
        writes.Global(NativePedMoveAnimProfile::GroupSlotRva, fixture.image + NativePedMoveAnimProfile::GroupRva, PAGE_READONLY);
        // Other aggregate fixtures export their own P alias; this setup needs
        // only the exact whole-Reapply profile inherited from the hook fixture.
        code(mta::game_de::NativePlayerMoveAnimReapplyProfile::Pins);
        apply(HP::ProcessContextRva, HP::ProcessContext.data(), HP::ProcessContext.size(), PAGE_EXECUTE_READ);
        apply(HP::ClothesConsumerRva, HP::ClothesConsumer.data(), HP::ClothesConsumer.size(), PAGE_EXECUTE_READ);
        apply(NativePedRebuildProfile::EntryRva, NativePedRebuildProfile::Entry.data(), NativePedRebuildProfile::Entry.size(), PAGE_EXECUTE_READ);
        for (const auto& pin : RF::Pins)
            apply(pin.rva, pin.bytes, pin.size, PAGE_READONLY);
        apply(RF::ExceptionDirectoryRva, RF::Entries, sizeof(RF::Entries), PAGE_READONLY);
        IMAGE_DOS_HEADER dos{};
        std::memcpy(&dos, reinterpret_cast<void*>(fixture.image), sizeof(dos));
        IMAGE_NT_HEADERS64 nt{};
        std::memcpy(&nt, reinterpret_cast<void*>(fixture.image + dos.e_lfanew), sizeof(nt));
        const IMAGE_DATA_DIRECTORY exceptionDirectory{RF::ExceptionDirectoryRva, static_cast<DWORD>(sizeof(RF::Entries))};
        apply(static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory) +
                  IMAGE_DIRECTORY_ENTRY_EXCEPTION * sizeof(IMAGE_DATA_DIRECTORY),
              &exceptionDirectory, sizeof(exceptionDirectory), PAGE_READONLY);
        ClockMoveAnimHookOwnerTest::rangeImage = fixture.image;
        ClockMoveAnimHookOwnerTest::rangeImageBytes = nt.OptionalHeader.SizeOfImage;
        std::array<unsigned char, 2 * Player::RowBytes> players{};
        apply(Player::BankRva, players.data(), players.size(), PAGE_READWRITE);
        const auto publish = [&](std::int32_t selector)
        {
            const auto row = fixture.image + Player::BankRva + static_cast<std::uintptr_t>(selector) * Player::RowBytes;
            writes.Global(Player::BankRva, std::uintptr_t{0});
            writes.Global(Player::BankRva + Player::RowBytes, std::uintptr_t{0});
            writes.Global(Player::BankRva + static_cast<std::uintptr_t>(selector) * Player::RowBytes, ped);
            Store(ped + 0x5f0, row + 8);
            Store(ped + 0x7e0, selector);
        };
        publish(0);
        std::vector<unsigned char> references(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t index = 0; index + 1 != R::ReferenceCount; ++index)
        {
            const auto next = fixture.image + R::ReferencesRva + (index + 1) * R::ReferenceBytes;
            std::memcpy(references.data() + index * R::ReferenceBytes, &next, sizeof(next));
        }
        apply(R::ReferencesRva, references.data(), references.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        graph.Reset();
        // Only the player publication/control assets needed by these existing
        // readers are seeded; no complete CGame or model/streaming factory is made.
        for (std::size_t i = 0; i != 3; ++i)
        {
            const auto& pin = Player::Code[i];
            apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        }
        writes.Global(Player::PlayerVtableRva + 0x50, fixture.image + Player::PlayerControlRva, PAGE_READONLY);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image, previousReferenceImage = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockMoveAnimHookOwnerTest::failDelete = ClockMoveAnimHookOwnerTest::deleteCalls = 0;
        ClockMoveAnimHookOwnerTest::injectPartial = ClockMoveAnimHookOwnerTest::partialInjected = 0;
        ClockMoveAnimHookOwnerTest::originalAction = {};
        ClockMoveAnimHookOwnerTest::rebuildAction = {};
        auto& reapply = ClockPlayerMoveAnimReapplyOwnerTest::scenario;
        reapply = 0;
        ClockPlayerMoveAnimReapplyOwnerTest::image = fixture.image;
        ClockPlayerMoveAnimReapplyOwnerTest::ped = ped;
        ClockPlayerMoveAnimReapplyOwnerTest::calls = ClockPlayerMoveAnimReapplyOwnerTest::effects = 0;
        ClockPlayerMoveAnimReapplyOwnerTest::callerToken = ClockPlayerMoveAnimReapplyOwnerTest::retireToken = nullptr;
        ClockPlayerMoveAnimReapplyOwnerTest::after = nullptr;
        Hook::calls = Hook::accepted = Hook::mode = 0;
        Hook::address = ped + NativePedMoveAnimProfile::GroupOffset;
        Hook::worldField = graph.Level() + NativeStartupWorldProfile::levelWorldOffset;
        Hook::worldValue = graph.World();
        Hook::retireToken = {};
        Hook::retireResult = ClockResult::NotInitialized;
        struct OldProvider final : NativePedMoveAnimAccess
        {
        } absent;
        Check(absent.WriteCurrentPlayerMoveAnimGroup({}, 55) == ClockResult::NotInitialized,
              "older providers refuse the new scalar effect without inventing setter readiness");
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.modulePinned,
              "new scalar fixture preserves shipping executable refusal before hook admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load() && CheckBinding(), "new scalar uses actual public frame/timing admission");
        const auto generation = admitted.generation;
        const auto pulse = [&](const char* label, std::function<void(NativeGameTimingAccess&)> action)
        {
            callback.action = [&, label, action = std::move(action)](NativeGameTimingAccess& access)
            {
                const auto at = GetTickCount64();
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "scalar executes only on the genuine distinct after-application owner");
                action(access);
                std::printf("MoveAnim setter phase %s elapsed %llu ms\n", label, static_cast<unsigned long long>(GetTickCount64() - at));
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
        };
        NativePedReferenceToken token;
        ClockResult             result = ClockResult::NotInitialized;
        pulse("acquire", [&](NativeGameTimingAccess& access) { result = access.AcquirePedReference(1, token); });
        Check(result == ClockResult::Ok && runtime.pedReferences.IsRetained(), "actual registered owner issues current player token");
        const auto fixedToken = token;
        pulse("hooks",
              [&](NativeGameTimingAccess& access)
              {
                  Check(access.EnablePlayerMoveAnimHooks() == ClockResult::Ok && runtime.playerMoveAnimHooks.Ready(),
                        "new setter borrows genuine two-site ownership through actual peer hold");
              });
        pulse("unpublished",
              [&](NativeGameTimingAccess& access)
              {
                  Check(access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55) == ClockResult::OwnerConflict && !Hook::calls,
                        "a current token without a published custom intent cannot write");
              });
        pulse("publish",
              [&](NativeGameTimingAccess& access)
              {
                  Check(access.PublishPlayerMoveAnimOverride(fixedToken, 55) == ClockResult::Ok && runtime.playerMoveAnimHooks.MatchesOverride(fixedToken, 55),
                        "actual current token and exact custom intent are published before any scalar write");
              });
        Check(runtime.access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55) != ClockResult::Ok && !Hook::calls,
              "producer thread refuses before the native CAS");
        std::thread foreign([&] { result = runtime.access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55); });
        foreign.join();
        Check(result != ClockResult::Ok && !Hook::calls, "foreign thread cannot borrow the scalar writer");
        std::printf("Production player move animation setter actual runtime completed\n");
        std::printf("Production player move animation setter matched intent completed\n");
        if (scenario == 0)
        {
            pulse("range-and-token-refusals",
                  [&](NativeGameTimingAccess& access)
                  {
                      auto stale = fixedToken;
                      ++stale.version;
                      Check(access.WriteCurrentPlayerMoveAnimGroup(stale, 55) != ClockResult::Ok && !Hook::calls, "stale full token refuses before write");
                      Check(access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 56) == ClockResult::OwnerConflict && !Hook::calls,
                            "a different published custom value cannot authorize this store");
                      for (const auto bad : {0, -1, 9999})
                          Check(access.WriteCurrentPlayerMoveAnimGroup(fixedToken, bad) == ClockResult::InvalidArgument && !Hook::calls,
                                "default and invalid requests never invoke a native field exchange");
                  });
            pulse("source-refusal",
                  [&](NativeGameTimingAccess& access)
                  {
                      const auto&         pin = NativePedMoveAnimProfile::Code[0];
                      const unsigned char foreignByte = 0xcc;
                      apply(pin.rva, &foreignByte, 1, PAGE_EXECUTE_READ);
                      result = access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55);
                      apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
                      Check(result != ClockResult::Ok && !Hook::calls && !runtime.playerMoveAnimGroupWrite.IsRetained() && !runtime.stopping.load(),
                            "changed source refuses before native entry without fabricating debt");
                  });
            pulse("world-refusal",
                  [&](NativeGameTimingAccess& access)
                  {
                      Store(Hook::worldField, std::uintptr_t{0});
                      result = access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55);
                      Store(Hook::worldField, Hook::worldValue);
                      Check(result != ClockResult::Ok && !Hook::calls && !runtime.playerMoveAnimGroupWrite.IsRetained() && !runtime.stopping.load(),
                            "missing current world refuses before field mutation");
                  });
            pulse("null-child-fact",
                  [&](NativeGameTimingAccess& access)
                  {
                      // The actual complete Runtime lends this separate sibling
                      // interface; the timing ABI itself does not inherit it.
                      auto* groups = dynamic_cast<NativeAnimationAssociationGroupAccess*>(&access);
                      Check(groups != nullptr, "actual Runtime exposes its existing group endpoint");
                      NativeAnimationAssociationGroupReferenceToken group;
                      Check(groups->ResolveAnimationAssociationGroup(generation, 55, group) == ClockResult::Ok, "resolve real current group for null fact");
                      bool present = false;
                      Check(groups->ReadAnimationAssociationGroupChildArrayPresent(group, present) == ClockResult::Ok && present,
                            "genuine nonnull child-array parent fact is explicit");
                      Store(group.locator + 8, std::uintptr_t{0});
                      present = true;
                      Check(groups->ReadAnimationAssociationGroupChildArrayPresent(group, present) == ClockResult::Ok && !present,
                            "genuine null child array is successful false, not provider refusal");
                      Check(access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55) != ClockResult::Ok && !Hook::calls,
                            "null children cannot supply hierarchy authority for a native store");
                      Store(group.locator + 8, sources.children);
                      const auto&         pin = NativeAnimationGroupArrayProfile::Code[0];
                      const unsigned char foreignByte = 0xcc;
                      apply(pin.rva, &foreignByte, 1, PAGE_EXECUTE_READ);
                      present = true;
                      result = groups->ReadAnimationAssociationGroupChildArrayPresent(group, present);
                      apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
                      Check(result != ClockResult::Ok && present, "failed null-fact provider preserves caller output rather than reporting false");
                  });
            for (unsigned drift = 0; drift != 2; ++drift)
                pulse(drift ? "late-source" : "late-pool",
                      [&](NativeGameTimingAccess& access)
                      {
                          NativePedPoolReader::Value current;
                          Check(access.ResolvePedReference(fixedToken, current) == ClockResult::Ok, "late bookend uses actual issued identity");
                          LateProbe probe;
                          probe.trigger = 3;
                          const auto& pin = NativePedMoveAnimProfile::Code[0];
                          probe.writes = &writes;
                          probe.rva = drift ? pin.rva : Ped::RootRva;
                          probe.size = drift ? 1 : sizeof(std::uintptr_t);
                          probe.protection = drift ? PAGE_EXECUTE_READ : PAGE_READWRITE;
                          probe.bytes[0] = drift ? 0xcc : 0;
                          late = &probe;
                          NativePlayerMoveAnimGroupWrite::Controller local;
                          NativePlayerMoveAnimGroupWrite::Receipt    observed;
                          result = local.Write(fixture.image, current, 55, &CheckTimingOwner, &LateAfterApplication, &QualifyMoveAnimHookGroup, &observed);
                          late = nullptr;
                          if (drift)
                              apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
                          else
                              writes.Global(Ped::RootRva, current.pool);
                          Check(result != ClockResult::Ok && probe.calls == probe.trigger && !observed.entered && !local.IsRetained() && !Hook::calls,
                                "final genuine owner callback replacement is caught before exchange");
                      });
            std::printf("Production player move animation setter refusals completed\n");
            for (const std::int32_t selector : {0, 1})
            {
                const auto desired = selector ? 56 : 55;
                publish(selector);
                pulse("publish-current-player",
                      [&](NativeGameTimingAccess& access)
                      {
                          Check(access.PublishPlayerMoveAnimOverride(fixedToken, desired) == ClockResult::Ok,
                                "both qualified player publications use the same actual token");
                      });
                pulse("write-current-player",
                      [&](NativeGameTimingAccess& access)
                      {
                          std::array<unsigned char, Ped::ObjectStride> before{}, after{};
                          std::memcpy(before.data(), reinterpret_cast<void*>(ped), before.size());
                          const auto calls = Hook::calls;
                          result = access.WriteCurrentPlayerMoveAnimGroup(fixedToken, desired);
                          std::memcpy(after.data(), reinterpret_cast<void*>(ped), after.size());
                          Check(result == ClockResult::Ok && Hook::calls == calls + 1 && Hook::address == ped + 0x6e0 &&
                                    runtime.playerMoveAnimGroupWriteReceipt.acknowledged && !runtime.playerMoveAnimGroupWrite.IsRetained(),
                                "actual Runtime acknowledges exactly one aligned current-player CAS");
                          for (std::size_t i = 0; i != before.size(); ++i)
                              if (i < NativePedMoveAnimProfile::GroupOffset || i >= NativePedMoveAnimProfile::GroupOffset + 4)
                                  Check(before[i] == after[i], "native DWORD store preserves every neighboring ped byte");
                          std::int32_t raw = -1;
                          Check(access.ReadPedMoveAnim(fixedToken, raw) == ClockResult::Ok && raw == desired, "real getter observes the stored raw DWORD");
                      });
                pulse("whole-reapply-after-write",
                      [&](NativeGameTimingAccess& access)
                      {
                          const auto calls = ClockPlayerMoveAnimReapplyOwnerTest::calls;
                          Check(access.ReapplyCurrentPlayerMoveAnim(fixedToken) == ClockResult::Ok && ClockPlayerMoveAnimReapplyOwnerTest::calls == calls + 1,
                                "the existing whole original consumes the successfully written current group once through its owned invoke seam");
                      });
            }
            pulse("rejected-cas",
                  [&](NativeGameTimingAccess& access)
                  {
                      Hook::mode = 4;
                      const auto calls = Hook::calls;
                      result = access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 56);
                      Hook::mode = 0;
                      Check(result == ClockResult::OwnerConflict && Hook::calls == calls + 1 && !runtime.playerMoveAnimGroupWriteReceipt.accepted &&
                                !runtime.playerMoveAnimGroupWrite.IsRetained() && !runtime.stopping.load(),
                            "one rejected CAS preserves the concurrent field and has no unknown write debt");
                      Store(ped + 0x6e0, std::int32_t{56});
                  });
            std::printf("Production player move animation setter bookends completed\n");
        }
        else
        {
            Hook::mode = scenario;
            Hook::retireToken = fixedToken;
            pulse("entered-uncertainty",
                  [&](NativeGameTimingAccess& access)
                  {
                      const auto calls = Hook::calls;
                      result = access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55);
                      Store(Hook::worldField, Hook::worldValue);
                      NativePlayerMoveAnimGroupWrite::Receipt retained;
                      Check(result != ClockResult::Ok && Hook::calls == calls + 1 && runtime.playerMoveAnimGroupWrite.IsRetained() &&
                                runtime.playerMoveAnimGroupWrite.Read(retained) && retained.unknown && retained.entered && !retained.acknowledged &&
                                retained.nativePed == ped && retained.desired == 55 && runtime.stopping.load(),
                            "first entered native scalar uncertainty survives in the real Runtime journal");
                      if (scenario == 1)
                          Check(!retained.returned && !retained.accepted, "unreturned exchange is not a claimed accepted write");
                      else
                          Check(retained.returned && retained.accepted, "accepted CAS remains recorded after a lost bookend");
                      if (scenario == 3)
                          Check(Hook::retireResult == ClockResult::Ok, "native-token cleanup may precede post-write acknowledgement");
                      else
                          Check(access.ReleasePedReference(fixedToken) == ClockResult::Ok,
                                "stopped callback retires its actual private reference without dropping debt");
                      Check(runtime.playerMoveAnimGroupWrite.IsRetained() && !runtime.pedReferences.IsRetained(),
                            "scalar debt is independent of weak-token retirement");
                  });
            const auto calls = Hook::calls;
            Check(runtime.access.WriteCurrentPlayerMoveAnimGroup(fixedToken, 55) != ClockResult::Ok && Hook::calls == calls && !service.End(),
                  "stopped producer cannot replay an unknown scalar or complete End");
            NativeClockService  another;
            ClockServiceStateV1 denied;
            Check(!another.BeginGameTiming(request, denied, nullptr, nullptr) && runtime.playerMoveAnimGroupWrite.IsRetained(),
                  "a new generation cannot discard the retained scalar prefix");
            Check(!service.End() && Hook::calls == calls, "repeated End performs no second scalar exchange");
            std::printf("Production player move animation setter quarantine completed\n");
        }
        Hook::mode = 0;
        if (scenario == 0)
        {
            pulse("release",
                  [&](NativeGameTimingAccess& access)
                  {
                      Check(access.WithdrawPlayerMoveAnimOverride(fixedToken) == ClockResult::Ok && access.ReleasePedReference(fixedToken) == ClockResult::Ok,
                            "healthy native scalar keeps normal explicit snapshot/reference retirement");
                  });
            bool ended = service.End();
            if (!ended && !runtime.playerMoveAnimHooks.IsRetained())
            {
                engine.Pulse(service, generation, true);
                ended = service.End();
            }
            Check(ended && !service.IsRetained() && !runtime.playerMoveAnimGroupWrite.IsRetained(),
                  "acknowledged scalar needs no End rollback or retained module owner");
        }
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferenceImage;
        ClockMoveAnimHookOwnerTest::rangeImage = 0;
        ClockMoveAnimHookOwnerTest::rangeImageBytes = 0;
        std::printf("Production player move animation setter owner completed\n");
    }
}
