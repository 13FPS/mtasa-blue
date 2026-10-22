/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native stream-state facts use the actual retained callback owner
 ****************************************************************************/
namespace
{
    struct ModelLoadedOwnerContext
    {
        std::uint32_t                  index = 0;
        NativeModelLoadedReader::Value value;
        ClockResult                    result = ClockResult::NotInitialized;
    };
    void ModelLoadedOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<ModelLoadedOwnerContext*>(opaque);
        context.result = access.ReadStreamingModelLoaded(context.index, context.value);
    }
    void NativeModelLoadedOwnedSdk(ClockImageFixture& fixture)
    {
        namespace L = NativeModelLoadedProfile;
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
                  "save only owned code/data spans before the actual stream-state callback experiment");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "stream observations preserve real immutable-code and writable-image-data contracts");
        };
        for (const auto& pin : NativePedPoolProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeModelBankProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : L::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        std::vector<unsigned char> rows(L::ArrayBytes);
        rows[3 * L::RowStride + L::FlagsOffset] = 0x96;
        rows[3 * L::RowStride + L::StateOffset] = 1;
        rows[(L::ModelCount - 1) * L::RowStride + L::FlagsOffset] = 0xff;
        rows[(L::ModelCount - 1) * L::RowStride + L::StateOffset] = 2;
        admit(L::ArrayRva, rows.data(), rows.size(), PAGE_READWRITE);
        const std::uint32_t state = 9, marker = 0;
        const auto          thread = GetCurrentThreadId();
        const unsigned char initialized = 1;
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        ClockReaderTestHost::image = fixture.image;
        ClockModelLoadedOwnerTest::advanceOnRead = false;
        ModelLoadedOwnerContext context;
        context.index = 3;
        context.value.modelIndex = 0xdeadbeef;
        context.value.rowAddress = 123;
        context.value.rawFlags = 0xab;
        context.value.rawLoadState = 0xcd;
        context.value.streamedIn = false;
        const auto untouched = [&]
        {
            return context.value.modelIndex == 0xdeadbeef && context.value.rowAddress == 123 && context.value.rawFlags == 0xab &&
                   context.value.rawLoadState == 0xcd && !context.value.streamedIn;
        };
        DefaultPedIntelligenceOwner absent;
        Check(absent.ReadStreamingModelLoaded(3, context.value) == ClockResult::NotInitialized && untouched() &&
                  runtime.access.ReadStreamingModelLoaded(3, context.value) == ClockResult::NotInitialized && untouched(),
              "default and unbound streaming owners preserve the whole output instead of inventing a loaded result");
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &ModelLoadedOwnerHandler, &context);
        Check(runtime.access.ReadStreamingModelLoaded(3, context.value) != ClockResult::Ok && untouched(),
              "a producer cannot infer stream-state access from a known row or retained runtime");
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew one existing native callback between stream-state observations");
            fixture.invokeTimer();
        };
        for (const auto index : {std::uint32_t{0}, std::uint32_t{3}, L::ModelCount - 1})
        {
            context.index = index;
            invoke();
            Check(context.result == ClockResult::Ok && context.value.modelIndex == index &&
                      context.value.rowAddress == fixture.image + L::ArrayRva + index * L::RowStride &&
                      context.value.rawFlags == rows[index * L::RowStride + L::FlagsOffset] &&
                      context.value.rawLoadState == rows[index * L::RowStride + L::StateOffset] &&
                      context.value.streamedIn == (rows[index * L::RowStride + L::StateOffset] == 1) && !runtime.stopping.load(),
                  "actual callback routes first, loaded and last-requested ordinary rows through the native state1 predicate without model publication");
        }
        const auto sentinel = [&] { context.value = {0xdeadbeef, 123, 0xab, 0xcd, false}; };
        for (const auto index : {L::ModelCount, std::uint32_t{0xffff}, std::uint32_t{0x10000}, std::uint32_t{0xffffffff}})
        {
            context.index = index;
            sentinel();
            invoke();
            Check(context.result == ClockResult::InvalidArgument && untouched() && !runtime.stopping.load(),
                  "invalid full SDK keys refuse without narrowing and leave the read-only owner usable");
        }
        context.index = 3;
        sentinel();
        ClockResult                    foreign = ClockResult::Ok;
        NativeModelLoadedReader::Value foreignOutput;
        foreignOutput.rowAddress = 456;
        std::thread other([&] { foreign = runtime.access.ReadStreamingModelLoaded(3, foreignOutput); });
        other.join();
        Check(foreign != ClockResult::Ok && foreignOutput.rowAddress == 456, "a foreign thread receives no stream-state callback grant");
        fixture.Put(NativeStartGameProfile::ThreadIdRva, static_cast<DWORD>(thread + 1));
        invoke();
        Check(context.result != ClockResult::Ok && untouched() && !runtime.stopping.load(),
              "the real native GameThread pair remains mandatory inside this actual owner");
        fixture.Put(NativeStartGameProfile::ThreadIdRva, thread);
        fixture.Put(NativeBorrowedGcProfile::markerRva, std::uint32_t{1});
        invoke();
        Check(context.result != ClockResult::Ok && untouched(), "native GC exclusion refuses a loaded-state observation");
        fixture.Put(NativeBorrowedGcProfile::markerRva, marker);
        fixture.Put(NativeGameStateProfile::stateRva, std::uint32_t{7});
        invoke();
        Check(context.result != ClockResult::Ok && untouched(), "a qualified non-playing state does not authorize model streaming facts");
        fixture.Put(NativeGameStateProfile::stateRva, state);
        ClockModelLoadedOwnerTest::address = fixture.image + L::ArrayRva + 3 * L::RowStride + L::FlagsOffset;
        ClockModelLoadedOwnerTest::advanced = {0x97, 2};
        ClockModelLoadedOwnerTest::advanceOnRead = true;
        const auto changes = ClockModelLoadedOwnerTest::changes;
        invoke();
        Check(context.result == ClockResult::BindingChanged && untouched() && ClockModelLoadedOwnerTest::changes == changes + 1 && !runtime.stopping.load(),
              "independent native state/flag advancement between actual guarded reads cannot publish stale output or stop a read-only owner");
        Check(service.End() && !service.IsRetained() && !runtime.timingHandler && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "stream queries retire the existing runtime without a second owner, native call, model lease or restoration debt");
        Check(fixture.Get<unsigned char>(L::ArrayRva + 3 * L::RowStride + L::StateOffset) == 2 &&
                  runtime.access.ReadStreamingModelLoaded(3, context.value) != ClockResult::Ok && untouched(),
              "retirement preserves native loading progress and denies later access through the expired facade");
        ClockModelLoadedOwnerTest::address = 0;
        ClockModelLoadedOwnerTest::advanceOnRead = false;
        ClockReaderTestHost::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native model-loaded callback owner fixture completed");
    }
}
