/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual retained Rain owner and complete CWeather SDK publication
 ****************************************************************************/
// Included in the ordinary production Runtime fixture. The sparse SEC_IMAGE
// supplies exact native bytes, but no archive/native serializer or RX leaf is
// executed. Real OS page/atomic/stack/peer ownership still runs unchanged.
namespace
{
    struct RainOwnerContext
    {
        unsigned                     operation = 1, refusal = 0, calls = 0, field = 0;
        ClockResult                  result = ClockResult::NotInitialized;
        CWeather*                    sdk = nullptr;
        CWeather*                    published = nullptr;
        float                        value = 0.625f, observed = 0;
        bool                         threw = false, accepted = false;
        NativeWeatherRainLeaf::State leaf;
    };

    void RainOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<RainOwnerContext*>(opaque);
        ++context.calls;
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
        context.accepted = false;
        try
        {
            if (context.operation == 1)
                context.result = access.GetWeather(context.published);
            if (context.operation == 2)
            {
                context.sdk->SetAmountOfRain(context.value);
                context.accepted = true;
            }
            if (context.operation == 3)
            {
                context.sdk->ResetAmountOfRain();
                context.accepted = true;
            }
            if (context.operation == 4)
            {
                context.observed = context.sdk->GetAmountOfRain();
                context.accepted = true;
            }
            if (context.operation == 5)
            {
                const std::array<bool (CWeather::*)(float), 7> setters{&CWeather::SetWetRoads, &CWeather::SetFoggyness, &CWeather::SetFog,
                                                                       &CWeather::SetRainFog,  &CWeather::SetWaterFog,  &CWeather::SetSandstorm,
                                                                       &CWeather::SetRainbow};
                context.accepted = (context.sdk->*setters[context.field])(context.value);
            }
        }
        catch (const std::runtime_error&)
        {
            context.threw = true;
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeRainOwnerAndWeatherSdk(ClockImageFixture& fixture)
    {
        namespace Rain = NativeWeatherRainProfile;
        namespace Fields = NativeWeatherScalarProfile;
        static_assert(!std::is_abstract_v<CWeatherOverridesDE> && NativeWeatherOverrideAtomicProfile::Windows.size() == 37);
        fixture.AdmitWeatherPins();
        fixture.AdmitWeatherScalarPins();
        fixture.AdmitRainPins();
        ClockReaderTestHost::image = fixture.image;
        const auto bits = [](float value)
        {
            std::uint32_t output = 0;
            std::memcpy(&output, &value, sizeof(output));
            return output;
        };
        auto        context = std::make_unique<RainOwnerContext>();
        auto* const sentinel = reinterpret_cast<CWeather*>(std::uintptr_t{1});
        context->published = sentinel;
        Check(runtime.access.GetWeather(context->published) == ClockResult::NotInitialized && context->published == sentinel,
              "unbound GetWeather preserves output instead of publishing a partial/default SDK product");
        NativeClockService service;
        const auto         begin = [&]
        {
            context->operation = 1;
            context->refusal = 0;
            context->published = sentinel;
            BeginOwned(service, fixture, false, true, true, &RainOwnerHandler, context.get());
        };
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only the actual callback owner between Rain SDK intents");
            fixture.invokeTimer();
        };
        const auto retire = [&]
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "same natural callback retires the complete Rain owner before hooks and SDK context release");
            }
            Check(!service.IsRetained() && !WeatherOverrideRetained() && !runtime.weatherOverridePeers.Held() && !runtime.peersRetained.load() &&
                      !runtime.timingHandler && !runtime.timingContext && !runtime.hooked.load() && !runtime.timerHooked.load() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "Rain retirement leaves no code, RX leaf, RW descriptor, accumulator, held peer or hook/context lifetime debt");
        };
        const auto captureLeaf = [&]
        {
            std::uint64_t active = 99, foreign = 99;
            Check(runtime.weatherOverridePlatform.ReadRain(context->leaf, active, foreign) && context->leaf.prepared && !active && !foreign,
                  "actual Win32 Rain platform retains the exact near RX leaf and separately writable immutable descriptor");
            MEMORY_BASIC_INFORMATION code{}, data{};
            Check(::VirtualQuery(reinterpret_cast<void*>(context->leaf.leafEntry), &code, sizeof(code)) == sizeof(code) &&
                      ::VirtualQuery(context->leaf.context, &data, sizeof(data)) == sizeof(data) && code.Type == MEM_PRIVATE && data.Type == MEM_PRIVATE &&
                      code.Protect == PAGE_EXECUTE_READ && data.Protect == PAGE_READWRITE &&
                      reinterpret_cast<std::uintptr_t>(code.AllocationBase) == context->leaf.allocationBase &&
                      reinterpret_cast<std::uintptr_t>(data.AllocationBase) == context->leaf.allocationBase,
                  "native CALL targets owned RX bytes while descriptor/count mutations remain on a separate private RW page");
            Check(fixture.Get<std::array<unsigned char, 8>>(Rain::EntryRva) == context->leaf.plan.entry,
                  "one aligned64 native word publishes the real CALL/JMP plan after the descriptor and leaf are retained");
        };
        begin();
        Check(runtime.access.GetWeather(context->published) != ClockResult::Ok && context->published == sentinel,
              "producer adoption/binding cannot manufacture immediate CWeather callback authority");
        invoke();
        Check(context->result == ClockResult::Ok && context->published == &runtime.weatherSdk,
              "admitted GetWeather publishes the actual complete Runtime CWeather object through its existing SDK interface");
        context->sdk = context->published;
        invoke();
        Check(context->published == context->sdk, "the complete weather SDK identity stays stable across admitted callbacks");
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
        {
            context->refusal = refusal;
            context->operation = 1;
            context->published = sentinel;
            invoke();
            Check(context->result != ClockResult::Ok && context->published == sentinel && !WeatherOverrideRetained(),
                  "stale/nested/foreign-admission/expired GetWeather preserves caller output and acquires no override lifetime");
            context->operation = 2;
            invoke();
            Check(context->threw && !context->accepted && !WeatherOverrideRetained(),
                  "the real void Rain SDK setter reports refused scope instead of falsely accepting a delayed pointer call");
        }
        context->refusal = 0;
        context->operation = 2;
        context->value = 0.75f;
        invoke();
        Check(context->accepted && !context->threw && fixture.Get<std::uint32_t>(Fields::Fields[0]) == bits(context->value) &&
                  runtime.weatherOverrides.HasReferences() && !runtime.weatherOverrides.NeedsRecovery(),
              "the complete actual Rain SDK setter suppresses direct and serialized native writes under one existing engine owner");
        captureLeaf();
        const auto allocation = context->leaf.allocationBase;
        context->value = -0.0f;
        invoke();
        captureLeaf();
        Check(context->accepted && context->leaf.allocationBase == allocation && fixture.Get<std::uint32_t>(Fields::Fields[0]) == bits(-0.0f),
              "repeated Rain SDK intent requalifies its owned entry and reuses exact leaf/allocation identity without clamping");
        context->operation = 4;
        invoke();
        Check(context->accepted && !context->threw && bits(context->observed) == bits(-0.0f),
              "the real Rain SDK getter accepts only exact same-generation owned serializer and direct-writer bytes");
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                CWeather*  output = sentinel;
                const auto result = runtime.access.GetWeather(output);
                bool       threw = false;
                try
                {
                    context->sdk->SetAmountOfRain(0.125f);
                }
                catch (const std::runtime_error&)
                {
                    threw = true;
                }
                foreignRefused = result != ClockResult::Ok && output == sentinel && threw;
            });
        foreign.join();
        Check(foreignRefused && fixture.Get<std::uint32_t>(Fields::Fields[0]) == bits(-0.0f),
              "a foreign thread holding the real complete SDK pointer inherits neither publication nor native write permission");
        // A counted Body is simulated by its actual retained counter only;
        // the focused bridge fixture separately executes all direction paths.
        // Reset must refuse before its entry CAS, not restore stock d55 bytes
        // and only later notice that a real Body can still return there.
        InterlockedExchange64(&context->leaf.context->activeCalls, 1);
        context->operation = 3;
        invoke();
        Check(context->threw && !context->accepted && runtime.weatherOverrides.NeedsRecovery() && service.IsRetained() &&
                  fixture.Get<std::array<unsigned char, 8>>(Rain::EntryRva) == context->leaf.plan.entry,
              "active Body refuses entry restoration before either instruction word can invalidate its native return continuation");
        InterlockedExchange64(&context->leaf.context->activeCalls, 0);
        retire();
        Check(fixture.Get<std::array<unsigned char, 8>>(Rain::EntryRva) == NativeWeatherRainBridge::StockEntry &&
                  fixture.Get<std::uint32_t>(Fields::Fields[0]) == bits(-0.0f),
              "normal callback retirement restores stock serializer flow while preserving successful Rain intent");
        MEMORY_BASIC_INFORMATION released{};
        Check(::VirtualQuery(reinterpret_cast<void*>(allocation), &released, sizeof(released)) == sizeof(released) && released.State == MEM_FREE,
              "drained Rain leaf lifetime releases its retained reservation base rather than its interior entry or descriptor");

        begin();
        invoke();
        Check(context->result == ClockResult::Ok && context->published == context->sdk,
              "a fresh owner generation republishes the same Runtime object only under fresh callback admission");
        context->operation = 2;
        context->value = 0.625f;
        invoke();
        Check(context->accepted, "Rain is active before all seven other native writer groups coexist");
        for (unsigned field = 0; field < 7; ++field)
        {
            context->operation = 5;
            context->field = field;
            context->value = 2.0f + static_cast<float>(field);
            invoke();
            Check(context->accepted && !runtime.weatherOverrides.NeedsRecovery(),
                  "all eight actual SDK groups coexist, including frozen Rain/Sandstorm displacement-prefix neighbors");
        }
        context->operation = 1;
        invoke();
        Check(context->result == ClockResult::Ok && context->published == context->sdk,
              "GetWeather revalidates the full combined code view with37 direct words and the independently journaled serializer entry");
        retire();
        Check(fixture.Get<std::uint32_t>(Fields::Fields[0]) == bits(0.625f), "End retires all38 code words without rewinding any accepted Rain scalar");

        begin();
        invoke();
        context->operation = 2;
        context->value = 0.5f;
        invoke();
        captureLeaf();
        const auto ownedEntry = context->leaf.plan.entry;
        auto       changed = ownedEntry;
        changed[7] ^= 1;
        fixture.Patch(Rain::EntryRva, changed.data(), changed.size());
        context->operation = 1;
        context->published = sentinel;
        invoke();
        Check(context->result != ClockResult::Ok && context->published == sentinel && runtime.weatherOverrides.NeedsRecovery() && !service.End(),
              "foreign serializer bytes are refused rather than blindly originalized by the Rain or scalar code view");
        fixture.Patch(Rain::EntryRva, ownedEntry.data(), ownedEntry.size());
        retire();

        begin();
        invoke();
        context->operation = 2;
        context->value = 0.25f;
        invoke();
        captureLeaf();
        InterlockedExchange64(&context->leaf.context->unqualifiedArchive, 1);
        fixture.Put(Fields::Fields[0], 0.9375f);
        context->operation = 1;
        context->published = sentinel;
        invoke();
        // The real timer drains recoverable debt after the handler returns.
        // Require the terminal refusal/supersession latch here, without
        // demanding that already-drained code references remain artificially
        // pending until the producer's later End call.
        Check(context->result == ClockResult::OwnerConflict && context->published == sentinel && runtime.stopping.load() &&
                  runtime.weatherOverrideSuperseded.load() && runtime.failure.load() == ClockResult::OwnerConflict,
              "a foreign archive preserves native progression and latches terminal refusal even when this same callback drains its code debt");
        retire();
        Check(
            fixture.Get<std::uint32_t>(Fields::Fields[0]) == bits(0.9375f) && State(service).failure == ClockResult::OwnerConflict && !State(service).restored,
            "terminal native archive supersession retires hooks without falsely claiming the later Rain value was restored");
        context->published = sentinel;
        Check(runtime.access.GetWeather(context->published) == ClockResult::NotInitialized && context->published == sentinel,
              "retired complete CWeather publication preserves output and cannot resurrect the old generation");
        ClockReaderTestHost::image = 0;
        std::puts("Production native complete Rain weather SDK owner fixture completed");
    }
}
