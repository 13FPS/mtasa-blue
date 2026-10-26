/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded progress execution through actual native owner and SDK
 ****************************************************************************/
namespace ClockAnimationAssociationProgressTest
{
    unsigned      calls = 0;
    bool          unknownAck = false;
    std::uint32_t argumentBits = 0;
    ClockResult   Invoke(NativeAnimationAssociationCalls::ProgressFunction function, std::uintptr_t receiver, float absoluteTime,
                         std::uint32_t& exception) noexcept
    {
        ++calls;
        std::memcpy(&argumentBits, &absoluteTime, sizeof(argumentBits));
        ClockPrivateAnimationAssociationTest::targetsValid =
            ClockPrivateAnimationAssociationTest::targetsValid &&
            reinterpret_cast<std::uintptr_t>(function) == ClockPrivateAnimationAssociationTest::image + NativeAnimationAssociationProgressProfile::SetTimeRva &&
            receiver;
        const auto result = AnimationAssociationStorageDetail::InvokeProgress(function, receiver, absoluteTime, exception);
        if (result == ClockResult::Ok && unknownAck)
        {
            // Lose only the acknowledgement after the actual body returned.
            // No copied body is asked to unwind a hardware exception.
            unknownAck = false;
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return result;
    }
}
namespace AnimationAssociationProgressOwnerFixture
{
    namespace Hook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace ProgressHook = ClockAnimationAssociationProgressTest;
    using SourceBank = PrivateAnimationAssociationOwnerFixture::SourceBank;
    using Token = NativeAnimationAssociationReferenceToken;
    using Values = NativeAnimationAssociationValues;
    unsigned scenario = 0;
    using NodeBytes = std::array<unsigned char, 40>;
    NodeBytes Node(std::uintptr_t address)
    {
        NodeBytes bytes{};
        std::memcpy(bytes.data(), reinterpret_cast<void*>(address), bytes.size());
        return bytes;
    }
    bool Near(float actual, float expected, float tolerance = 0.00002f)
    {
        return actual >= expected - tolerance && actual <= expected + tolerance;
    }
    struct FrameBank
    {
        std::array<std::uintptr_t, 2>             frames{};
        std::array<std::vector<unsigned char>, 2> before{};
        bool                                      compressed = false;
        unsigned                                  count = 0;
        FrameBank(SourceBank& source, bool packed, unsigned frameCount) : compressed(packed), count(frameCount)
        {
            for (unsigned sequence = 0; sequence != 2; ++sequence)
            {
                const bool        root = sequence == 1;
                const std::size_t stride = compressed ? (root ? 16u : 10u) : (root ? 32u : 20u);
                before[sequence].resize(count * stride);
                frames[sequence] = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, before[sequence].size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                Check(frames[sequence] != 0, "frame fixture owns a bounded payload without claiming engine frame adoption or free authority");
                for (unsigned frame = 0; frame != count; ++frame)
                {
                    const auto at = frames[sequence] + frame * stride;
                    if (compressed)
                    {
                        const std::array<std::int16_t, 4> quaternion =
                            frame == 0 ? std::array<std::int16_t, 4>{0, 0, 0, 4096} : std::array<std::int16_t, 4>{-3072, 0, 0, -2048};
                        std::memcpy(reinterpret_cast<void*>(at), quaternion.data(), sizeof(quaternion));
                        Hook::Store(at + 8, static_cast<std::int16_t>(frame == 0 ? 0 : 60));
                        if (root)
                        {
                            const std::array<std::int16_t, 3> translation{11, -23, 37};
                            std::memcpy(reinterpret_cast<void*>(at + 10), translation.data(), sizeof(translation));
                        }
                    }
                    else
                    {
                        const std::array<float, 4> quaternion = frame == 0   ? std::array<float, 4>{0, 0, 0, 1}
                                                                : frame == 1 ? std::array<float, 4>{0.6f, 0, 0, 0.8f}
                                                                             : std::array<float, 4>{0, 0.6f, 0, 0.8f};
                        std::memcpy(reinterpret_cast<void*>(at), quaternion.data(), sizeof(quaternion));
                        Hook::Store(at + 0x10, frame == 0 ? 0.0f : 1.0f);
                        if (root)
                        {
                            const std::array<float, 3> translation{11, -23, 37};
                            std::memcpy(reinterpret_cast<void*>(at + 20), translation.data(), sizeof(translation));
                        }
                    }
                }
                std::memcpy(before[sequence].data(), reinterpret_cast<void*>(frames[sequence]), before[sequence].size());
                const auto row = source.sequences + sequence * 80;
                Hook::Store(row + 0x42, static_cast<std::uint16_t>(1u | (root ? 2u : 0u) | (compressed ? 4u : 0u)));
                Hook::Store(row + 0x44, static_cast<std::int16_t>(count));
                Hook::Store(row + 0x48, frames[sequence]);
            }
            source.saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x12, static_cast<unsigned char>(compressed));
            source.saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x13, static_cast<unsigned char>(compressed));
            source.saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, 2.0f);
            source.saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x20, std::uintptr_t{0});
        }
        void CheckPayloads(bool flipped)
        {
            for (unsigned sequence = 0; sequence != 2; ++sequence)
            {
                auto expected = before[sequence];
                if (compressed && flipped && count > 1)
                {
                    const std::size_t                 stride = sequence == 1 ? 16u : 10u;
                    const std::array<std::int16_t, 4> quaternion{3072, 0, 0, 2048};
                    std::memcpy(expected.data() + stride, quaternion.data(), sizeof(quaternion));
                }
                Check(!std::memcmp(expected.data(), reinterpret_cast<void*>(frames[sequence]), expected.size()),
                      "genuine progress writes only compressed END quaternion8 and preserves times translations and all uncompressed payload bytes");
            }
        }
        ~FrameBank()
        {
            // Source-only allocations remain alive until every known native
            // association retires; their release never acknowledges its debt.
            for (const auto frame : frames)
                if (frame)
                    VirtualFree(reinterpret_cast<void*>(frame), 0, MEM_RELEASE);
        }
    };
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    template <class Action>
    void Throws(Action action, const char* why)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, why);
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        admit(NativeStreamingLoadAllProfile::Code);
        for (const auto& pin : NativeStartupOwnerProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeStartupWorldProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeUObjectMembershipProfile::pins);
        for (const auto& pin : NativeGameStateProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        // The direct fixture does not run the old AnimationBlocks wrapper.
        // Its read-only bank foundation still validates these exact pool pins.
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
        // These genuine dispatch cells share executable text pages. Match the
        // established bank fixture preparation without replaying its cases.
        saved.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                     static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                     static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        admit(NativeAnimationBlockRefProfile::Code);
        admit(NativeAnimationGroupArrayProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::ReadCode);
        admit(NativeAnimationAssociationProfile::Code);
        // Length has its own genuine field/profile prerequisites. Installing
        // them here never adds those dependencies to the old scalar reader.
        admit(NativeAnimationHierarchyReadProfile::Code);
        for (const auto& pin : NativeAnimationAssociationProgressProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, &pin.bytes[0], pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationAssociationProgressProfile::ReadOnlyData)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, &pin.bytes[0], pin.size, PAGE_READONLY);
        const auto mathModule = GetModuleHandleW(L"ucrtbase.dll");
        // These two exact IAT slots are part of the saved import closure.
        // Their destinations remain actual system math, with no mirrored
        // quaternion calculation or float callback substituted by the test.
        for (const auto& binding : NativeAnimationAssociationProgressProfile::MathBindings)
        {
            const auto function = mathModule ? GetProcAddress(mathModule, binding.symbol) : nullptr;
            Check(function != nullptr, "genuine copied math thunks use the fixture process's actual loaded system CRT exports");
            saved.Global(binding.iatRva, reinterpret_cast<std::uintptr_t>(function), PAGE_READONLY);
        }
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        SourceBank source(fixture, saved);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::constructorCalls = Hook::destructorCalls = Hook::exchangeCalls = Hook::heapAllocations = Hook::heapFrees = 0;
        Hook::targetsValid = Hook::snapshotsCopied = true;
        Hook::heapFailed = false;
        Hook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "association progress scope pins the actual callback module once for this process");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew a bounded real callback before each single progress semantic operation");
            fixture.invokeTimer();
        };
        auto& access = static_cast<NativeAnimationAssociationAccess&>(runtime.access);

        const auto execute = [&](auto action)
        {
            callback.action = [&](NativeGameTimingAccess&) { action(); };
            invoke();
            // Do not retain the local action parameter between callbacks.
            callback.action = [](NativeGameTimingAccess&) {};
        };
        ProgressHook::calls = 0;
        ProgressHook::unknownAck = false;
        const auto create = [&](Token& token, std::unique_ptr<PrivateAnimationAssociationReceiver>& observer)
        {
            execute(
                [&]
                {
                    Check(access.CreatePrivateAnimationAssociation(generation, token) == ClockResult::Ok && token.ownerGeneration == generation && token.serial,
                          "progress scope issues genuine private unconstructed104 storage");
                });
            execute(
                [&]
                {
                    observer = std::make_unique<PrivateAnimationAssociationReceiver>(access, token);
                    Check(observer->Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children)) != nullptr &&
                              host.Contains(Hook::lastReceiver) && Hook::Load<std::uint16_t>(source.counter) == 18,
                          "actual SDK constructor executes copied native bodies and holds the original bank reference");
                });
        };
        const auto retire = [&](const Token& token, std::unique_ptr<PrivateAnimationAssociationReceiver>& observer)
        {
            const auto frees = host.freeCalls, destructors = Hook::destructorCalls;
            observer.reset();
            Check(host.freeCalls == frees, "CPU progress observer destruction cannot acknowledge native retirement");
            execute(
                [&]
                {
                    Check(access.RetirePrivateAnimationAssociation(token) == ClockResult::Ok && !access.HasPendingPrivateAnimationAssociations(),
                          "accepted progress caches and shared frame flips retain exactly-once native retirement authority");
                });
            Check(host.freeCalls == frees + 2 && Hook::destructorCalls == destructors + 1 && Hook::Load<std::uint16_t>(source.counter) == 17,
                  "genuine deleting destructor balances node and outer frees and the original bank reference once");
        };
        if (scenario)
        {
            FrameBank                                            frames(source, true, 2);
            Token                                                token;
            std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
            create(token, observer);
            const auto receiver = Hook::lastReceiver, nodes = Hook::Load<std::uintptr_t>(receiver + 0x20);
            const auto constructors = Hook::constructorCalls, destructors = Hook::destructorCalls, frees = host.freeCalls;
            execute(
                [&]
                {
                    ProgressHook::unknownAck = true;
                    Check(access.SetPrivateAnimationAssociationCurrentProgress(token, Bits(0.5f)) == ClockResult::OriginalException,
                          "lost progress ACK follows the actual complete native parent child leaf and math execution");
                });
            frames.CheckPayloads(true);
            const auto after = Node(nodes);
            Check(ProgressHook::calls == 1 && Hook::Load<std::uint32_t>(receiver + 0x38) == Bits(1.0f) && Hook::Load<std::int16_t>(nodes + 8) == 1 &&
                      Hook::Load<std::int16_t>(nodes + 10) == 0 && access.HasPendingPrivateAnimationAssociations() &&
                      runtime.privateAssociations.RecoveryPending() && runtime.privateAssociations.IsRetained() && service.IsRetained() &&
                      runtime.modulePinned.load() && host.freeCalls == frees && Hook::Load<std::uint16_t>(source.counter) == 18,
                  "unknown entered progress retains the actual changed cache payload bank resources and callback module");
            Check(!service.End(), "unknown progress cannot report a completed producer shutdown");
            fixture.invokeTimer();
            Check(!service.End() && ProgressHook::calls == 1 && Hook::constructorCalls == constructors && Hook::destructorCalls == destructors &&
                      host.freeCalls == frees && Node(nodes) == after && Hook::Load<std::uint32_t>(receiver + 0x38) == Bits(1.0f) &&
                      Hook::Load<std::uint16_t>(source.counter) == 18 && runtime.modulePinned.load() &&
                      NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "natural End drainer never replays progress frees unknown resources or copies a stale node baseline back");
            frames.CheckPayloads(true);
            std::puts("Production animation association progress unknown effects retained");
            std::printf("Passed %u production clock service checks\n", checks.load());
            std::fflush(nullptr);
            // This process owns unresolved effects. Preserve every fixture and
            // hook until OS exit instead of running fake restoration/free ACKs.
            std::_Exit(0);
        }
        for (const bool compressed : {false, true})
        {
            FrameBank                                            frames(source, compressed, compressed ? 2u : 3u);
            Token                                                token;
            std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
            create(token, observer);
            const auto receiver = Hook::lastReceiver, nodes = Hook::Load<std::uintptr_t>(receiver + 0x20);
            const auto progress = [&](float fraction, bool sdk, std::int16_t end, std::int16_t start, float delta, float expectedTime)
            {
                std::array<NodeBytes, 5> previous{};
                for (unsigned index = 0; index != previous.size(); ++index)
                    previous[index] = Node(nodes + index * 40);
                std::array<unsigned char, 104> receiverBefore{};
                std::memcpy(receiverBefore.data(), reinterpret_cast<void*>(receiver), receiverBefore.size());
                const auto entries = ProgressHook::calls;
                execute(
                    [&]
                    {
                        if (sdk)
                            observer->SetCurrentProgress(fraction);
                        else
                        {
                            NativeAnimationAssociationCalls::Receipt receipt;
                            Check(runtime.privateAssociations.SetCurrentProgress(fixture.image, generation, token, Bits(fraction), &CheckTimingOwner,
                                                                                 &receipt) == ClockResult::Ok &&
                                      receipt.entered && receipt.returned && receipt.accepted && receipt.verified && receipt.progressed &&
                                      !receipt.progressUnknown && receipt.progressBits == Bits(fraction) && receipt.absoluteTimeBits == Bits(2.0f * fraction),
                                  "native controller verifies the joint raw fraction product and acknowledged progress transition");
                        }
                    });
                Check(ProgressHook::calls == entries + 1 && ProgressHook::argumentBits == Bits(2.0f * fraction),
                      "one fraction action enters the genuine absolute-time body once with one current-duration product");
                const auto timeBits = Bits(expectedTime);
                std::memcpy(receiverBefore.data() + 0x38, &timeBits, sizeof(timeBits));
                Check(!std::memcmp(receiverBefore.data(), reinterpret_cast<void*>(receiver), receiverBefore.size()),
                      "native parent changes only receiver time38 while preserving links identity flags and ownership fields");
                for (unsigned index = 0; index != previous.size(); ++index)
                {
                    const auto at = nodes + index * 40;
                    auto       expected = previous[index];
                    if (Hook::Load<std::uintptr_t>(at + 0x10))
                    {
                        Check(Hook::Load<std::int16_t>(at + 8) == end && Hook::Load<std::int16_t>(at + 10) == start && Near(Hook::Load<float>(at + 12), delta),
                              "positive root and child nodes retain exact signed END/start orientation and remaining delta");
                        const float angle = compressed ? 1.0471975512f : start == 0 ? 0.6435011088f : 0.8762980612f;
                        const float reciprocal = compressed ? 1.1547005384f : start == 0 ? 1.6666666667f : 1.3014480157f;
                        Check(Near(Hook::Load<float>(at), angle) && Near(Hook::Load<float>(at + 4), reciprocal),
                              "normal orientation executes genuine acosf and sinf imports into both native cache floats");
                        // Every changed prefix field was asserted above. Copy
                        // only that verified prefix into the unchanged-tail
                        // expectation; never waive the full node snapshot.
                        std::memcpy(expected.data(), reinterpret_cast<void*>(at), 16);
                    }
                    Check(Node(at) == expected, "null nodes are untouched and active nodes preserve sequence backpointer tail and padding bytes");
                }
                frames.CheckPayloads(compressed);
                execute(
                    [&]
                    {
                        Values current;
                        Check(access.ReadPrivateAnimationAssociation(token, current) == ClockResult::Ok && current.currentTimeBits == timeBits &&
                                  Bits(observer->GetCurrentProgress()) == timeBits && !runtime.privateAssociations.RecoveryPending(),
                              "accepted node cache and deduplicated shared quaternion ledger requalify through native and actual const SDK reads");
                    });
            };
            progress(0.0f, false, 1, 0, 1.0f, 0.0f);
            progress(0.5f, true, 1, 0, compressed ? 1.0f : 0.0f, 1.0f);
            progress(1.0f, true, compressed ? 1 : 2, compressed ? 0 : 1, compressed ? 1.0f : 0.0f, 2.0f);
            execute([&] { observer->SetFlags(static_cast<short>(0x4012)); });
            progress(1.0f, true, 1, 0, 1.0f, 0.0f);
            execute([&] { observer->SetFlags(static_cast<short>(0x4010)); });
            progress(1.25f, true, compressed ? 1 : 2, compressed ? 0 : 1, compressed ? 1.0f : 0.0f, 2.0f);
            execute([&] { observer->RestrictToBones(std::bitset<64>{9}); });
            Check(!Hook::Load<std::uintptr_t>(nodes + 2 * 40 + 0x10), "mask acknowledges null suppression for the duplicate root sequence node");
            progress(0.5f, true, 1, 0, compressed ? 1.0f : 0.0f, 1.0f);
            const auto refused = [&](const char* why, std::uint32_t bits)
            {
                const auto entries = ProgressHook::calls, time = Hook::Load<std::uint32_t>(receiver + 0x38);
                execute(
                    [&]
                    {
                        NativeAnimationAssociationCalls::Receipt receipt;
                        // Exercise the real controller's refusal without asking
                        // Runtime's conservative historical constructed flag to
                        // change the shared producer stop policy for this fixture.
                        const auto result = runtime.privateAssociations.SetCurrentProgress(fixture.image, generation, token, bits, &CheckTimingOwner, &receipt);
                        Check(result != ClockResult::Ok && !receipt.entered && !receipt.progressed && !receipt.progressUnknown, why);
                    });
                Check(ProgressHook::calls == entries && Hook::Load<std::uint32_t>(receiver + 0x38) == time,
                      "bounded preflight refusal occurs before the genuine parent's first time38 store");
            };
            refused("nonfinite fraction refuses without inventing a normalized or sanitized progress", 0x7fc12345u);
            execute([&] { observer->SetFlags(static_cast<short>(0x4012)); });
            saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, 0.0f);
            refused("zero duration loop cannot enter the native subtract loop", Bits(1.0f));
            saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, -2.0f);
            refused("negative duration loop cannot enter the native subtract loop", Bits(1.0f));
            saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, 2.0f);
            refused("huge duration product refuses the bounded wrap plan without fmod or clamping", Bits(1000000.0f));
            execute([&] { observer->SetFlags(static_cast<short>(0x4010)); });
            if (compressed)
            {
                saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, 0.0f);
                refused("cold compressed parent conversion has no issued cache or frame mutation authority", Bits(0.5f));
                saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x18, 2.0f);
            }
            else
            {
                saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x12, static_cast<unsigned char>(1));
                refused("running-compressed alone cannot grant cold conversion cache ownership", Bits(0.5f));
                saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + 0x12, static_cast<unsigned char>(0));
            }
            execute(
                [&]
                {
                    auto foreign = token;
                    ++foreign.ownerGeneration;
                    NativeAnimationAssociationCalls::Receipt receipt;
                    const auto                               entries = ProgressHook::calls;
                    Check(runtime.privateAssociations.SetCurrentProgress(fixture.image, generation, foreign, Bits(0.5f), &CheckTimingOwner, &receipt) ==
                                  ClockResult::OwnerConflict &&
                              !receipt.entered && ProgressHook::calls == entries,
                          "foreign generation cannot obtain native progress authority");
                });
            Check(service.Renew(generation), "renew before independent foreign-thread request without extending the callback lease");
            ClockResult workerResult = ClockResult::Ok;
            const auto  entries = ProgressHook::calls;
            std::thread worker([&] { workerResult = access.SetPrivateAnimationAssociationCurrentProgress(token, Bits(0.5f)); });
            worker.join();
            Check(workerResult == ClockResult::NotInitialized && ProgressHook::calls == entries,
                  "foreign runtime thread preserves legacy timing TLS-first refusal before native game-thread checks");
            execute(
                [&]
                {
                    Values value;
                    Check(access.ReadPrivateAnimationAssociation(token, value) == ClockResult::Ok && !runtime.stopping.load() &&
                              !runtime.privateAssociations.RecoveryPending(),
                          "restored preflight domains retain the acknowledged cache ledger and ordinary read contract");
                });
            retire(token, observer);
        }
        // One-frame paths intentionally share an index and write zero delta;
        // they must not inherit the constructor's FFFFFFFF index sentinel.
        for (const bool compressed : {false, true})
        {
            FrameBank                                            frames(source, compressed, 1);
            Token                                                token;
            std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
            create(token, observer);
            const auto nodes = Hook::Load<std::uintptr_t>(Hook::lastReceiver + 0x20);
            execute([&] { observer->SetCurrentProgress(0.5f); });
            for (const unsigned index : {0u, 2u, 3u})
                Check(Hook::Load<std::uint32_t>(nodes + index * 40 + 8) == 0 && Hook::Load<std::uint32_t>(nodes + index * 40 + 12) == 0 &&
                          Hook::Load<std::uint64_t>(nodes + index * 40) == 0,
                      "single-frame compressed and float paths set0/0 zero delta and exact zero angle cache");
            frames.CheckPayloads(false);
            execute(
                [&]
                {
                    Values value;
                    Check(access.ReadPrivateAnimationAssociation(token, value) == ClockResult::Ok && value.currentTimeBits == Bits(1.0f),
                          "single-frame cache state remains accepted by the same lifetime owner");
                });
            retire(token, observer);
        }
        Check(Hook::constructorCalls == 4 && Hook::destructorCalls == 4 && Hook::heapAllocations == 4 && Hook::heapFrees == 8 && Hook::targetsValid &&
                  Hook::snapshotsCopied && !Hook::heapFailed && !runtime.privateAssociations.IsRetained(),
              "four focused frame-format lifetimes each construct and retire exactly once without an old suite replay");
        Check(!service.End(), "producer shutdown requests natural hook retirement after known resources were acknowledged");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.privateAssociations.IsRetained() && runtime.modulePinned.load() &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "natural callback releases admission after all real native progress lifetimes retire");
        ClockReaderTestHost::image = previousImage;
        Hook::image = 0;
        Host::active = nullptr;
        saved.Restore();
        std::puts("Production native animation association progress owner fixture completed");
        std::puts("Production actual animation association progress SDK owner fixture completed");
        std::puts("Production copied progress parent children leaf math imports and dynamic lifetime bodies executed in owned fixture");
    }
}
