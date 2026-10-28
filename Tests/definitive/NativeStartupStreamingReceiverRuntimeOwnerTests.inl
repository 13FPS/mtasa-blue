/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the actual startup streaming receiver on its retained owner
 ****************************************************************************/
{
    // The surrounding genuine special-model fixture supplies the real current
    // provider, world, name cache and heap prerequisites. Only this new branch
    // runs; the historical streaming operation matrix remains unexecuted.
    resetSpecial();
    CStreaming* const sentinel = reinterpret_cast<CStreaming*>(std::uintptr_t{0x13579});
    CStreaming*       selected = sentinel;
    Check(specialAbsent.RequireStartupStreamingReceiverReady() == ClockResult::NotInitialized &&
              specialAbsent.GetStartupStreaming(selected) == ClockResult::NotInitialized && selected == sentinel,
          "older startup streaming owners refuse without replacing the caller output");
    Check(runtime.access.RequireStartupStreamingReceiverReady() != ClockResult::Ok && runtime.access.GetStartupStreaming(selected) != ClockResult::Ok &&
              selected == sentinel && !ClockSpecialModelOwnerTest::calls,
          "unbound runtime cannot lend its real startup streaming receiver");

    const auto snapshot = [&]
    {
        std::vector<unsigned char> bytes;
        const auto                 append = [&](std::uintptr_t address, std::size_t size)
        {
            const auto begin = bytes.size();
            bytes.resize(begin + size);
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), bytes.data() + begin, size, &copied) && copied == size,
                  "snapshot only owned special-model fixture bytes");
        };
        append(modelAddress, model.size());
        append(fixture.image + row, L::RowStride);
        append(fixture.image + X::NamePoolRva, X::NamePoolBytes);
        append(fixture.image + X::CacheRva, X::CacheHeaderBytes);
        append(fixture.image + X::ReverseCacheRva, X::CacheHeaderBytes);
        append(specialHeapAddress, specialHeap.size());
        append(providerAddress, provider.size());
        return bytes;
    };
    NativeClockService service;
    CStreaming*        borrowed = nullptr;
    unsigned           callbacks = 0;
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        ++callbacks;
        const auto  before = snapshot();
        const auto  calls = ClockSpecialModelOwnerTest::calls;
        const auto  generation = runtime.generation.load();
        const auto  creations = runtime.pedModelCreations.size(), collisions = runtime.startupCollisionCopies.size();
        CStreaming *first = sentinel, *second = sentinel;
        Check(access.RequireStartupStreamingReceiverReady() == ClockResult::Ok && access.GetStartupStreaming(first) == ClockResult::Ok &&
                  access.GetStartupStreaming(second) == ClockResult::Ok && first == second && first == &runtime.startupStreaming &&
                  runtime.startupStreaming.IsOwnedBy(access),
              "actual current callback publishes its one real owned streaming receiver with stable borrowed identity");
        borrowed = first;
        // Receiver capability is model-independent. The later inherited
        // RequestSpecialModel must freshly qualify the selected model itself.
        const auto modelSlot = NativeModelBankProfile::BankRva + 3 * NativeModelBankProfile::SlotBytes;
        const auto oldModel = fixture.Get<std::uintptr_t>(modelSlot);
        fixture.Put(modelSlot, std::uintptr_t{0});
        selected = sentinel;
        Check(access.GetStartupStreaming(selected) == ClockResult::Ok && selected == borrowed,
              "receiver publication does not inspect a future model publication or issue a request as its probe");
        fixture.Put(modelSlot, oldModel);
        Check(snapshot() == before && ClockSpecialModelOwnerTest::calls == calls && runtime.generation.load() == generation &&
                  runtime.pedModelCreations.size() == creations && runtime.startupCollisionCopies.size() == collisions && !runtime.stopping.load(),
              "readiness and getter leave native model/name/cache/heap/provider bytes and startup journals unchanged");
    };
    BeginOwned(service, fixture, false, true, true, &SpecialCallback::Invoke, &specialCall);
    const auto invoke = [&]
    {
        Check(service.Renew(runtime.generation.load()), "renew startup receiver fixture between independent callback phases");
        fixture.invokeTimer();
    };
    invoke();
    Check(callbacks == 1 && borrowed == &runtime.startupStreaming && !ClockSpecialModelOwnerTest::calls,
          "one genuine timer callback qualified receiver publication without a native request");
    selected = sentinel;
    Check(runtime.access.RequireStartupStreamingReceiverReady() != ClockResult::Ok && runtime.access.GetStartupStreaming(selected) != ClockResult::Ok &&
              selected == sentinel && !runtime.stopping.load(),
          "retained runtime identity alone grants no between-callback receiver access");
    ClockResult foreignReady = ClockResult::Ok, foreignGet = ClockResult::Ok;
    CStreaming* foreignOutput = sentinel;
    std::thread foreign(
        [&]
        {
            foreignReady = runtime.access.RequireStartupStreamingReceiverReady();
            foreignGet = runtime.access.GetStartupStreaming(foreignOutput);
        });
    foreign.join();
    Check(foreignReady != ClockResult::Ok && foreignGet != ClockResult::Ok && foreignOutput == sentinel && !runtime.stopping.load() &&
              !ClockSpecialModelOwnerTest::calls,
          "foreign receiver checks preserve the real owner and the caller output");

    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        const auto before = snapshot();
        const auto expected = access.RequireStartupSpecialModelRequestReady();
        selected = sentinel;
        Check(expected != ClockResult::Ok && access.RequireStartupStreamingReceiverReady() == expected && access.GetStartupStreaming(selected) == expected &&
                  selected == sentinel && !ClockSpecialModelOwnerTest::calls && !runtime.stopping.load() && snapshot() == before,
              "current native name-prerequisite refusal reaches the getter without replacing output or mutating native state");
    };
    fixture.Put(X::NameInitializedRva, std::uint8_t{0});
    invoke();
    fixture.Put(X::NameInitializedRva, initializedNames);
    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        Check(NativeStreamingCompletionContext::TryEnter(), "receiver fixture owns the existing shared completion gate");
        selected = sentinel;
        const auto ready = access.RequireStartupStreamingReceiverReady(), get = access.GetStartupStreaming(selected);
        NativeStreamingCompletionContext::Leave();
        Check(ready == ClockResult::Busy && get == ClockResult::Busy && selected == sentinel && !ClockSpecialModelOwnerTest::calls && !runtime.stopping.load(),
              "receiver publication refuses a live real completion scope without lending its owned object");
        Check(access.GetStartupStreaming(selected) == ClockResult::Ok && selected == borrowed,
              "getter freshly requalifies the same object after the genuine loading scope retires");
    };
    invoke();
    std::puts("Production startup streaming receiver guards and fresh borrowed publication completed");

    specialCall.action = [&](NativeGameTimingAccess& access)
    {
        selected = sentinel;
        Check(access.GetStartupStreaming(selected) == ClockResult::Ok && selected == borrowed,
              "fresh getter lends the runtime object immediately before the actual startup intent");
        selected->RequestSpecialModel(3, "startup", 0x80000116u);
    };
    invoke();
    Check(ClockSpecialModelOwnerTest::calls == 1 && ClockSpecialModelOwnerTest::model == 3 && ClockSpecialModelOwnerTest::channel == 0x80000116u &&
              ClockSpecialModelOwnerTest::entry == reinterpret_cast<NativeStreamingSpecialModelCalls::NativeFunction>(fixture.image + X::EntryRva) &&
              std::strcmp(ClockSpecialModelOwnerTest::copied.data(), "startup") == 0 && fixture.Get<unsigned char>(row + L::StateOffset) == 2,
          "the actual returned concrete receiver inherits one real special-model native request with its unchanged channel intent");
    retireSpecial(service);
    selected = sentinel;
    Check(runtime.access.GetStartupStreaming(selected) != ClockResult::Ok && selected == sentinel,
          "retired owner cannot publish a receiver despite the runtime object still existing");
    bool rejected = false;
    try
    {
        borrowed->RequestSpecialModel(3, "again", 6);
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    Check(rejected && ClockSpecialModelOwnerTest::calls == 1 && std::strcmp(reinterpret_cast<const char*>(model.data() + 0xc), "startup") == 0 &&
              !service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
          "a previously borrowed receiver rechecks its retired native owner and cleanup never replays or rolls back the accepted request");
    std::puts("Production startup streaming receiver actual inherited request and clean retirement completed");
    std::puts("Production startup streaming receiver runtime fixture completed");
}
