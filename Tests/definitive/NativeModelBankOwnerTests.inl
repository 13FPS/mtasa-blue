/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Model slots are observed through the existing actual callback owner
 ****************************************************************************/
namespace
{
    struct ModelBankOwnerContext
    {
        std::uint32_t                index = 0;
        NativeModelBankReader::Value value;
        ClockResult                  result = ClockResult::NotInitialized;
    };
    void ModelBankOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<ModelBankOwnerContext*>(opaque);
        context.result = access.ReadModelBankEntry(context.index, context.value);
    }
    void NativeModelBankOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativeModelBankProfile;
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
                  "save exact model-bank/code facts in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "model observation retains real immutable code and writable image bank contracts");
        };
        for (const auto& pin : NativePedPoolProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        std::vector<std::uintptr_t> bank(P::ModelCount);
        bank[3] = 0xffff000000000003ull;
        bank.back() = 1;
        admit(P::BankRva, bank.data(), P::BankBytes, PAGE_READWRITE);
        const std::uint32_t state = 9, marker = 0;
        const auto          thread = GetCurrentThreadId();
        const unsigned char initialized = 1;
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        ClockReaderTestHost::image = fixture.image;
        ClockModelBankOwnerTest::advanceOnRead = false;
        ModelBankOwnerContext context;
        context.index = 3;
        context.value.nativeModelInfo = 123;
        context.value.slotAddress = 456;
        DefaultPedIntelligenceOwner absent;
        Check(absent.ReadModelBankEntry(3, context.value) == ClockResult::NotInitialized && context.value.nativeModelInfo == 123 &&
                  runtime.access.ReadModelBankEntry(3, context.value) == ClockResult::NotInitialized && context.value.slotAddress == 456,
              "default/unbound model observations explicitly refuse and preserve caller output");
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelBankOwnerHandler, &context);
        Check(runtime.access.ReadModelBankEntry(3, context.value) != ClockResult::Ok && context.value.nativeModelInfo == 123,
              "a producer cannot turn knowledge of the global model bank into callback admission");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew one natural owner between immediate model-bank observations");
            fixture.invokeTimer();
        };
        for (const auto index : {std::uint32_t{0}, std::uint32_t{3}, P::ModelCount - 1})
        {
            context.index = index;
            invoke();
            Check(context.result == ClockResult::Ok && context.value.modelIndex == index &&
                      context.value.slotAddress == fixture.image + P::BankRva + index * P::SlotBytes && context.value.nativeModelInfo == bank[index] &&
                      !runtime.stopping.load(),
                  "actual callback observes absent, opaque noncanonical and final in-range model associations without dereferencing model objects");
        }
        for (const auto index : {P::ModelCount, std::uint32_t{0xffff}, std::uint32_t{0x10000}, std::uint32_t{0xffffffff}})
        {
            context.index = index;
            context.value.nativeModelInfo = 123;
            context.value.slotAddress = 456;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && context.value.nativeModelInfo == 123 && context.value.slotAddress == 456 &&
                      !runtime.stopping.load(),
                  "full unsigned SDK indexes are refused before native read rather than narrowed, masked or mistaken for a ped WORD");
        }
        context.index = 3;
        ClockResult                  foreign = ClockResult::Ok;
        NativeModelBankReader::Value foreignOutput;
        foreignOutput.nativeModelInfo = 789;
        std::thread other([&] { foreign = runtime.access.ReadModelBankEntry(3, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput.nativeModelInfo == 789, "foreign threads receive no native model-bank observation grant");
        fixture.Put(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(thread + 1));
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeModelInfo == 123 && !runtime.stopping.load(),
              "real native GameThread identity remains required for the same callback owner");
        fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
        fixture.Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{1});
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeModelInfo == 123,
              "native GC exclusion remains required without inventing a model object lease");
        fixture.Put(NativeBorrowedGcProfile::markerRva, marker);
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{7});
        invoke();
        Check(context.result != ClockResult::Ok && context.value.nativeModelInfo == 123, "native non-PLAYING state supplies no qualified model observation");
        fixture.Put(NativeGameStateProfile::stateRva, state);
        ClockModelBankOwnerTest::slot = fixture.image + P::BankRva + 3 * P::SlotBytes;
        ClockModelBankOwnerTest::advanced = 0x222;
        ClockModelBankOwnerTest::advanceOnRead = true;
        const auto changes = ClockModelBankOwnerTest::changes;
        invoke();
        Check(context.result == ClockResult::BindingChanged && context.value.nativeModelInfo == 123 && context.value.slotAddress == 456 &&
                  ClockModelBankOwnerTest::changes == changes + 1 && !runtime.stopping.load(),
              "an actual guarded image read can observe native slot advancement, but its mismatching bookend cannot publish output or stop a read-only owner");
        Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "read-only bank observations retire existing callback/module ownership without model allocation, native references or SDK publication");
        Check(fixture.Get<std::uintptr_t>(P::BankRva + 3 * P::SlotBytes) == ClockModelBankOwnerTest::advanced &&
                  runtime.access.ReadModelBankEntry(3, context.value) != ClockResult::Ok && context.value.nativeModelInfo == 123,
              "retirement preserves independent native bank advancement and grants no later model lifetime");
        ClockModelBankOwnerTest::slot = ClockModelBankOwnerTest::advanced = 0;
        ClockModelBankOwnerTest::advanceOnRead = false;
        ClockReaderTestHost::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native model-bank callback owner fixture completed");
    }
}
