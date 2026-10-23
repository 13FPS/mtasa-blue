/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the one-shot ground query through the real Runtime owner
 *****************************************************************************/
namespace WorldGroundOwnerFixture
{
    struct Saved
    {
        std::uintptr_t             rva;
        std::vector<unsigned char> bytes;
        DWORD                      protection;
    };
    struct Writes
    {
        ClockImageFixture& fixture;
        std::vector<Saved> saved;
        static void        Apply(void* opaque, std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            auto&                    self = *static_cast<Writes*>(opaque);
            MEMORY_BASIC_INFORMATION info{};
            SIZE_T                   copied = 0;
            Saved                    old{rva, std::vector<unsigned char>(size), 0};
            Check(::VirtualQuery(reinterpret_cast<void*>(self.fixture.image + rva), &info, sizeof(info)) == sizeof(info) &&
                      ::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(self.fixture.image + rva), old.bytes.data(), size, &copied) &&
                      copied == size,
                  "save each bounded owned world query image span before fixture initialization");
            old.protection = info.Protect;
            self.saved.push_back(std::move(old));
            self.fixture.Patch(rva, bytes, size);
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(self.fixture.image + rva), size, protection, &ignored) != FALSE,
                  "world query fixture retains genuine code RO/RX and native data RW requirements");
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            Apply(this, rva, &value, sizeof(value), protection);
        }
        void Restore()
        {
            for (auto at = saved.rbegin(); at != saved.rend(); ++at)
            {
                fixture.Patch(at->rva, at->bytes.data(), at->bytes.size());
                DWORD ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + at->rva), at->bytes.size(), at->protection, &ignored) != FALSE,
                      "retire owned query fixture spans without changing the original image protections");
            }
            saved.clear();
        }
    };
    struct Callback
    {
        std::function<void(NativeGameTimingAccess&)> action;
        static void                                  Invoke(NativeGameTimingAccess& access, void* opaque) { static_cast<Callback*>(opaque)->action(access); }
    };
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    void Retire(NativeClockService& service, ClockImageFixture& fixture)
    {
        // This read-looking native operation owns no CClock mutation. End can
        // finish immediately or require the existing native timer to drain.
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "same native timer drains remaining query owner hook debt");
        }
        Check(!service.IsRetained() && !runtime.timingHandler && !runtime.timingContext && !runtime.inFlight.load() && !runtime.admissionSerial.load() &&
                  !runtime.admissionGeneration.load() && !callbackDepth && !timingCallbackGeneration && !timingTimerScope &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "completed native query retirement releases callback, current generation and real shared service admission");
        Check(CheckBinding(true), "query owner cleanup restores exact original hook bytes");
    }
    void Run(ClockImageFixture& fixture)
    {
        namespace Host = ClockWorldGroundOwnerTest;
        namespace P = NativeWorldGroundProfile;
        Writes     writes{fixture, {}};
        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph                     graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &Writes::Apply, &writes);
        NativeWorldGroundFixture::Roots                          roots(fixture.image, &Writes::Apply, &writes);
        roots.InitializePins();
        // Match the existing standalone owner fixtures: the current-image
        // seam must select this SEC_IMAGE before genuine native GT/GC/world
        // checks run. Its default deliberately selects the real test process,
        // which cannot satisfy a query against this owned synthetic image.
        const auto priorReaderImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Check(ClockReaderTestHost::Module(nullptr) == reinterpret_cast<HMODULE>(fixture.image),
              "owned current-image selection is established before native query admission");
        Host::image = fixture.image;
        CVector                            position;
        const std::array<std::uint32_t, 3> positionBits{0x80000000, 0x3f800001, 0xc3964000};
        std::memcpy(&position, positionBits.data(), sizeof(position));
        Host::source = &position;
        struct DefaultGroundAccess final : NativeWorldGroundAccess
        {
        };
        DefaultGroundAccess absent;
        float               refused = -173.25f;
        const auto          refusedBits = Bits(refused);
        Check(absent.FindWorldGroundZ(position, refused) == ClockResult::NotInitialized && Bits(refused) == refusedBits,
              "older default owner preserves caller scalar and cannot manufacture a native ground miss");
        for (unsigned scenario = 0; scenario != 4; ++scenario)
        {
            graph.Reset();
            roots.Reset();
            writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Host::level = graph.Level();
            Host::scenario = scenario;
            Host::copied = false;
            Host::reenter = nullptr;
            const auto  calls = Host::calls, effects = Host::effects;
            Callback    callback;
            ClockResult result = ClockResult::NotInitialized;
            float       value = refused;
            bool        caught = false;
            unsigned    callbackCount = 0;
            callback.action = [&](NativeGameTimingAccess& access)
            {
                ++callbackCount;
                result = access.FindWorldGroundZ(position, value);
            };
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &Callback::Invoke, &callback);
            Check(runtime.access.FindWorldGroundZ(position, value) != ClockResult::Ok && Bits(value) == refusedBits && Host::calls == calls,
                  "a retained service alone cannot lend native ground-query callback admission");
            if (!scenario)
            {
                ClockResult foreign = ClockResult::Ok;
                std::thread other([&] { foreign = runtime.access.FindWorldGroundZ(position, value); });
                other.join();
                Check(foreign != ClockResult::Ok && Bits(value) == refusedBits && Host::calls == calls,
                      "foreign thread cannot inherit native query world or current generation");
                writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                fixture.invokeTimer();
                Check(result != ClockResult::Ok && Bits(value) == refusedBits && !runtime.stopping.load() && Host::calls == calls,
                      "native state other than loaded9 refuses before query entry and leaves current owner usable");
                writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
                writes.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                fixture.invokeTimer();
                // Observe every failed conjunct before selecting a correction:
                // a skipped/expired callback must not look like an inner Busy.
                LONG marker = 0;
                std::memcpy(&marker, reinterpret_cast<void*>(fixture.image + NativeBorrowedGcProfile::markerRva), sizeof(marker));
                const auto observedNow = GetTickCount64();
                std::fprintf(stderr,
                             "Ground GC observation: result=%u busy=%u value=%08x expected=%08x stopping=%d active=%d failure=%u nativeCalls=%u before=%u "
                             "callbacks=%u marker=%ld\n",
                             static_cast<unsigned>(result), static_cast<unsigned>(ClockResult::Busy), Bits(value), refusedBits,
                             static_cast<int>(runtime.stopping.load()), static_cast<int>(runtime.active.load()), static_cast<unsigned>(runtime.failure.load()),
                             Host::calls, calls, callbackCount, marker);
                std::fprintf(stderr,
                             "Ground GC owner observation: bound=%d timingMode=%d timerHooked=%d timerReady=%d handler=%d qualified=%llu elapsed=%llu "
                             "leaseAge=%llu maxDuration=%u\n",
                             static_cast<int>(runtime.bound.load()), static_cast<int>(runtime.timingMode), static_cast<int>(runtime.timerHooked.load()),
                             static_cast<int>(runtime.timerReady.load()), static_cast<int>(runtime.timingHandler != nullptr),
                             static_cast<unsigned long long>(runtime.counters.qualified.load()), static_cast<unsigned long long>(observedNow - runtime.started),
                             static_cast<unsigned long long>(observedNow - runtime.refreshed.load()), runtime.maximumDuration);
                Check(result == ClockResult::Busy && Bits(value) == refusedBits && !runtime.stopping.load() && Host::calls == calls,
                      "actual native GC guard refuses before entry without consuming or stopping this owner");
                writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
                Check(NativeStreamingCompletionContext::TryEnter(), "hold existing shared completion gate for independent owner refusal");
                fixture.invokeTimer();
                NativeStreamingCompletionContext::Leave();
                Check(result == ClockResult::Busy && Bits(value) == refusedBits && !runtime.stopping.load() && Host::calls == calls,
                      "query reuses the shared streaming completion gate rather than creating a parallel lock");
            }
            world_ground_fixture::WorldGroundFixture world(runtime.access);
            CWorld&                                  sdk = world;
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    value = sdk.FindGroundZFor3DPosition(&position);
                }
                catch (const std::runtime_error& error)
                {
                    caught = std::string(error.what()).find("FindGroundZFor3DPosition") != std::string::npos;
                }
            };
            if (scenario == 3)
            {
                Host::reenter = +[]() noexcept
                {
                    float nestedOutput = -173.25f;
                    Host::nested = runtime.access.FindWorldGroundZ(*Host::source, nestedOutput);
                    Check(Host::nested == ClockResult::Busy && Bits(nestedOutput) == Bits(-173.25f),
                          "recursive ground intent refuses without publishing a nested scalar");
                    Check(runtime.access.LoadStreamingScene(Host::source) == ClockResult::Busy &&
                              runtime.access.RequestStreamingModel(3, 6) == ClockResult::Busy && !runtime.stopping.load(),
                          "same shared native query scope blocks scene/request reentry before any second native operation");
                };
            }
            Check(service.Renew(runtime.generation.load()), "renew actual owner for deliberate one-shot SDK ground query");
            fixture.invokeTimer();
            Check(Host::calls == calls + 1 && Host::effects == effects + 1 && Host::entry == P::EntryRva && Host::copied &&
                      std::memcmp(&Host::received, positionBits.data(), sizeof(position)) == 0 &&
                      std::memcmp(&position, positionBits.data(), sizeof(position)) == 0,
                  "actual CWorld dispatch enters the genuine scalar native ABI exactly once with private bit-preserving XYZ");
            const bool failed = scenario == 1 || scenario == 2;
            Check(failed ? caught && Bits(value) == refusedBits && runtime.stopping.load() : !caught && Bits(value) == 0x42f68001 && !runtime.stopping.load(),
                  "entered exception or lost world stops the same owner and preserves scalar; normal native return remains exact");
            Retire(service, fixture);
            Check(Host::calls == calls + 1 && Host::effects == effects + 1,
                  "hook retirement neither repeats the native ground query nor rolls back its accepted effect");
        }
        Host::image = Host::level = Host::entry = 0;
        Host::source = nullptr;
        Host::reenter = nullptr;
        Host::scenario = 0;
        writes.Restore();
        ClockReaderTestHost::image = priorReaderImage;
        std::puts("Production native whole ground query callback owner fixture completed");
        std::puts("Production actual CWorld ground query SDK callback fixture completed");
    }
}
