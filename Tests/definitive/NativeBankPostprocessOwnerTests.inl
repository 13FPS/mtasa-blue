/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual bank SDK quaternion/time postprocessing and retained effects
 ****************************************************************************/
#include "../../Client/game_de/NativePrivateAnimationHierarchyPostProcessProfile.h"
namespace BankPostprocessOwnerFixture
{
    namespace Bank = BankDecompressionOwnerFixture;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    namespace Calls = NativeAnimationHierarchyUncompressCalls;
    namespace Conversion = NativeAnimationHierarchyDecompressProfile;
    namespace Post = NativePrivateAnimationHierarchyPostProcessProfile;
    namespace Old = AnimationHierarchyUncompressOwnerFixture;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    using Operation = Calls::Operation;
    unsigned                               scenario = 0;
    Operation                              operation = Operation::QuaternionFlips;
    std::uintptr_t                         image = 0, hierarchy = 0;
    NativeAnimationHierarchyReferenceToken token;
    unsigned                               calls = 0, executed = 0, mode = 0;
    bool                                   valid = true, nestedBusy = false, gateBusy = false, retained = false;
    std::uintptr_t                         Entry()
    {
        return operation == Operation::QuaternionFlips ? Post::QuaternionRva : Post::TimeRva;
    }
    ClockResult Invoke(Calls::Function function, std::uintptr_t requested, std::uint32_t& exception) noexcept
    {
        ++calls;
        valid = valid && reinterpret_cast<std::uintptr_t>(function) == image + Entry() && requested == hierarchy &&
                CheckApplicationTimingOwner() == ClockResult::Ok;
        nestedBusy = runtime.access.UncompressAnimationHierarchy(token) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        retained = runtime.hierarchyUncompress.IsRetained() && NativeLifetimeRetained();
        if (!valid)
            return ClockResult::BindingChanged;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        // Both exact original entries are allocation-free. Execute their whole
        // bodies through the production SEH wrapper with no leaf substitute.
        const auto result = mta::game_de::AnimationHierarchyUncompressDetail::InvokeOriginal(function, requested, exception);
        if (result != ClockResult::Ok)
            return result;
        ++executed;
        if (mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 3)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 4)
        {
            const auto rows = Bytes::Load<std::uintptr_t>(requested + 8);
            const auto frames = Bytes::Load<std::uintptr_t>(rows + 0x48);
            const auto field = frames + (operation == Operation::QuaternionFlips ? 0 : 16);
            Bytes::Store(field, Bytes::Load<std::uint32_t>(field) ^ 1u);
        }
        return ClockResult::Ok;
    }
    struct Inputs
    {
        Bank::AllocationHost                      backing;
        std::uintptr_t                            rows, h;
        bool                                      shared;
        std::array<std::uintptr_t, 2>             pointers{};
        std::array<std::vector<unsigned char>, 2> before, expected;
        std::array<unsigned char, 160>            beforeRows{};
        std::array<unsigned char, 40>             beforeH{}, expectedH{};
        Inputs(std::uintptr_t rows, std::uintptr_t h, bool shared) : rows(rows), h(h), shared(shared)
        {
            pointers[0] = backing.Own(shared ? 188 : 128, true);
            pointers[1] = shared ? pointers[0] + 128 : backing.Own(60, true);
            const float times[2][4] = {{0.5f, 1.5f, 2.5f, 4.0f}, {0.25f, 2.0f, 5.0f, 0.0f}};
            const float deltas[2][4] = {{0.5f, 1.0f, 1.0f, 1.5f}, {0.25f, 1.75f, 3.0f, 0.0f}};
            const float quaternions[4][4] = {{1, 0, 0, 0}, {-1, 0, 0, 0}, {0.5f, 0.5f, 0, 0}, {-0.5f, -0.5f, 0, 0}};
            for (unsigned row = 0; row != 2; ++row)
            {
                const unsigned count = row ? 3 : 4, stride = row ? 20 : 32;
                before[row].resize(count * stride);
                expected[row].resize(count * stride);
                std::memset(reinterpret_cast<void*>(rows + row * 80), 0x5a, 80);
                Bytes::Store(rows + row * 80 + 0x42, static_cast<std::uint16_t>((row ? 1 : 3) | (shared ? 8 : 0)));
                Bytes::Store(rows + row * 80 + 0x44, static_cast<std::uint16_t>(count));
                Bytes::Store(rows + row * 80 + 0x48, pointers[row]);
                for (unsigned key = 0; key != count; ++key)
                    for (unsigned word = 0; word != stride / 4; ++word)
                    {
                        const float   value = word < 4    ? (row ? (word == 3 ? (key ? -1.0f : 1.0f) : 0.0f) : quaternions[key][word])
                                              : word == 4 ? times[row][key]
                                              : word == 5 ? -32.0f
                                              : word == 6 ? 31.9990234375f
                                                          : 1.0f;
                        std::uint32_t bits = 0;
                        std::memcpy(&bits, &value, 4);
                        std::memcpy(before[row].data() + key * stride + word * 4, &bits, 4);
                        // The oracle names known hemisphere changes, including
                        // all sign-zero lanes. It does not run a dot predictor.
                        if (operation == Operation::QuaternionFlips && word < 4 && (row ? key != 0 : key == 1 || key == 3))
                            bits ^= 0x80000000u;
                        if (operation == Operation::TotalTime && word == 4)
                            std::memcpy(&bits, &deltas[row][key], 4);
                        std::memcpy(expected[row].data() + key * stride + word * 4, &bits, 4);
                    }
                std::memcpy(reinterpret_cast<void*>(pointers[row]), before[row].data(), before[row].size());
            }
            Bytes::Store(h + 8, rows);
            Bytes::Store(h + 0x10, std::uint16_t{2});
            Bytes::Store(h + 0x12, static_cast<unsigned char>(0));
            Bytes::Store(h + 0x13, static_cast<unsigned char>(0));
            Bytes::Store(h + 0x18, std::uint32_t{0x3fa00000});
            std::memcpy(beforeRows.data(), reinterpret_cast<const void*>(rows), beforeRows.size());
            std::memcpy(beforeH.data(), reinterpret_cast<const void*>(h), beforeH.size());
            expectedH = beforeH;
            if (operation == Operation::TotalTime)
            {
                const std::uint32_t five = 0x40a00000;
                std::memcpy(expectedH.data() + 0x18, &five, 4);
            }
        }
        void CheckAfter() const
        {
            Check(!std::memcmp(beforeRows.data(), reinterpret_cast<const void*>(rows), beforeRows.size()),
                  "whole original postprocessor preserves every row field, flag and native frame pointer");
            Check(!std::memcmp(expectedH.data(), reinterpret_cast<const void*>(h), expectedH.size()),
                  "whole H preserves opaque fields/cache link with only the exact native time effect");
            for (unsigned row = 0; row != 2; ++row)
                Check(!std::memcmp(expected[row].data(), reinterpret_cast<const void*>(pointers[row]), expected[row].size()),
                      "actual original produces every expected float bit, signed zero and timestamp delta");
            Check(backing.Live(pointers[0]) && (shared || backing.Live(pointers[1])),
                  "postprocessing preserves the fixture's live frame backing without adopting or replacing its ownership");
        }
    };
    ApplicationFrameFunction originalFrame = nullptr;
    unsigned                 originals = 0;
    bool                     originalReturned = false;
    void                     Original(void* receiver, float delta)
    {
        ++originals;
        originalReturned = false;
        Check(!timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver && !frameCallbackSerial &&
                  runtime.access.UncompressAnimationHierarchy(token) != ClockResult::Ok,
              "whole original application work has no lent bank postprocess access");
        originalFrame(receiver, delta);
        originalReturned = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        // The real frame root is shared with the typed startup/world graph.
        // Admit frame code first, then preserve that graph's actual receiver.
        fixture.AdmitApplicationPins();
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
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
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
        admit(NativeAnimationHierarchyReadProfile::Code);
        // Real private allocation/init/read/retirement shares the existing
        // manager and cache0 producer proof; this is not a fake H facade.
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        admit(NativeAnimationSequenceRowProfile::Code);
        admit(NativeAnimationSequenceFrameBindProfile::Code);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Profile::HierarchyTimeRva, Profile::HierarchyTimeBytes.data(), Profile::HierarchyTimeBytes.size(),
                                               PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0x4bc80ac, Profile::TimeScaleBytes.data(), Profile::TimeScaleBytes.size(), PAGE_READONLY);
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        PrivateAnimationAssociationOwnerFixture::SourceBank source(fixture, saved);
        WorldGroundOwnerFixture::Writes::Apply(&saved, NativeAnimationHierarchyCacheRemoveProfile::SourceRva,
                                               NativeAnimationHierarchyCacheRemoveProfile::SourceBytes.data(),
                                               NativeAnimationHierarchyCacheRemoveProfile::SourceBytes.size(), PAGE_EXECUTE_READ);

        admit(Conversion::Code);
        admit(NativeModelBankProfile::Code);
        admit(NativeModelLoadedProfile::Code);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::QuaternionScaleRva, Conversion::QuaternionScaleBytes.data(), 4, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::PositionScaleRva, Conversion::PositionScaleBytes.data(), 4, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::TimeScaleRva, Conversion::TimeScaleBytes.data(), 4, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::SignMask.rva, Conversion::SignMask.bytes, Conversion::SignMask.size, PAGE_READONLY);
        const auto streamRva = NativeModelLoadedProfile::ArrayRva + NativeStreamingResourceProfile::IfpFirst * NativeModelLoadedProfile::RowStride;
        std::array<unsigned char, 16> stream{};
        stream[NativeModelLoadedProfile::FlagsOffset] = 0x96;
        stream[NativeModelLoadedProfile::StateOffset] = 1;
        WorldGroundOwnerFixture::Writes::Apply(&saved, streamRva, stream.data(), stream.size(), PAGE_READWRITE);
        graph.Reset();
        fixture.Seed();
        fixture.SetFrameReceiver(root.data());
        saved.Global(NativeApplicationFrameProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto priorImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        image = fixture.image;
        hierarchy = source.hierarchy;
        calls = executed = mode = originals = 0;
        valid = true;
        ClockHierarchyUncompressOwnerTest::bankInvoke = &Invoke;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService    service;
        ClockServiceRequestV1 request;
        if (scenario != 3)
            request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked && !rejected.modulePinned,
              "destructive bank owner starts with the genuine shipping wrong-executable refusal");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.generation > rejected.generation && admitted.bound && admitted.modulePinned &&
                  (scenario == 3 ? runtime.timerHooked.load() : runtime.frameHooked.load() && !runtime.timerHooked.load() && !runtime.timerReady.load()),
              "fresh actual public Begin admits the exact native owner mode without synthetic runtime admission");
        const auto generation = admitted.generation;
        if (scenario != 3)
        {
            originalFrame = runtime.originalFrame;
            runtime.originalFrame = &Original;
        }
        unsigned   handled = 0;
        const auto action = [&](const auto& operation)
        {
            const auto before = handled;
            callback.action = [&](NativeGameTimingAccess&)
            {
                ++handled;
                Check(scenario == 3 ||
                          (originalReturned && originals >= handled && CheckApplicationTimingOwner() == ClockResult::Ok && !runtime.timerReady.load()),
                      "same existing timing access is lent only after the actual normal original frame returns");
                operation();
            };
            Check(service.Renew(generation), "renew the actual public bank conversion owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            if (scenario == 3)
                fixture.invokeTimer();
            else
                fixture.invokeFrame(root.data(), 0.25f);
            pulse.Stop();
            Check(handled == before + 1 && !pulse.unexpectedFailures.load(), "actual qualified callback executes exactly one requested bank action");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        AnimationManagerUncompressReceiver   manager(runtime.access, runtime.access, runtime.access, generation);
        CAnimManager&                        sdk = manager;
        std::unique_ptr<CAnimBlendHierarchy> facade;
        action(
            [&]
            {
                Check(runtime.access.ResolveBankAnimationHierarchyAt(generation, 0, token) == ClockResult::Ok && token.locator == hierarchy,
                      "actual manager bank origin resolves this current H without private frame-storage adoption");
                facade = sdk.GetAnimation(0);
                Check(facade && reinterpret_cast<std::uintptr_t>(facade->GetInterface()) == hierarchy,
                      "actual SDK factory publishes the current native-bank facade");
            });
        const auto refuses = [&](const auto& operation, const char* why)
        {
            bool refused = false;
            try
            {
                operation();
            }
            catch (const std::exception&)
            {
                refused = true;
            }
            Check(refused, why);
        };
        const auto finish = [&]
        {
            if (scenario != 3)
                runtime.originalFrame = originalFrame;
            bool done = service.End();
            for (unsigned attempt = 0; !done && attempt != 3; ++attempt)
            {
                if (scenario == 3)
                    fixture.invokeTimer();
                else
                    fixture.invokeFrame(root.data(), 0.25f);
                done = service.End();
            }
            return done;
        };

        operation = scenario == 1 || scenario == 6 || scenario == 7 || scenario == 9 || scenario == 11 ? Operation::TotalTime : Operation::QuaternionFlips;
        const auto command = [&]
        {
            if (operation == Operation::QuaternionFlips)
                facade->RemoveQuaternionFlips();
            else
                facade->CalculateTotalTime();
        };
        if (scenario <= 1)
        {
            for (unsigned variant = 0; variant != 4; ++variant)
            {
                Old::CacheFixture cache(saved, image, hierarchy, 1, 0);
                if (!(variant & 2))
                    Bytes::Store(hierarchy + 0x20, std::uintptr_t{0});
                Inputs                        inputs(source.sequences, hierarchy, (variant & 1) != 0);
                std::array<unsigned char, 24> cacheBytes{};
                std::memcpy(cacheBytes.data(), reinterpret_cast<const void*>(cache.Node(0)), cacheBytes.size());
                const auto before = calls, beforeExecuted = executed;
                const auto sse = _mm_getcsr() & 0xffc0u;
                action(command);
                inputs.CheckAfter();
                Check(!std::memcmp(cacheBytes.data(), reinterpret_cast<const void*>(cache.Node(0)), cacheBytes.size()),
                      "postprocessor leaves the untraversed cache row unchanged");
                const auto& receipt = runtime.hierarchyUncompressReceipt;
                Check(calls == before + 1 && executed == beforeExecuted + 1 && valid && nestedBusy && gateBusy && retained && receipt.operation == operation &&
                          receipt.postprocessPath && !receipt.decompressionPath && receipt.entered && receipt.returned && receipt.effectsVerified &&
                          !receipt.debt && receipt.result == ClockResult::Ok && receipt.hierarchy == hierarchy && receipt.generation == generation &&
                          receipt.entryRva == Entry() && receipt.sequences == 2 && receipt.preparedFrames == 7 &&
                          receipt.modifiedFrames == (operation == Operation::QuaternionFlips ? 4u : 5u) && !receipt.convertedFrames && !receipt.cacheStores &&
                          receipt.beforeTimeBits == 0x3fa00000 && receipt.afterTimeBits == (operation == Operation::TotalTime ? 0x40a00000u : 0x3fa00000u) &&
                          !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load() && (_mm_getcsr() & 0xffc0u) == sse,
                      "actual SDK bank postprocess verifies exact whole original effects under the real frame owner");
            }
            if (scenario == 1)
                for (unsigned edge = 0; edge != 3; ++edge)
                {
                    Old::CacheFixture   cache(saved, image, hierarchy, 1, 0);
                    Inputs              inputs(source.sequences, hierarchy, false);
                    const std::uint32_t lastTimes[3][2] = {{0xbf800000, 0xc0000000}, {0, 0x80000000}, {0x80000000, 0}};
                    // Each row has one positive key. Its first timestamp is
                    // also its last, so no delta store is permitted. The
                    // original's positive-zero duration seed must survive
                    // negative or either signed-zero terminal timestamp.
                    for (unsigned row = 0; row != 2; ++row)
                    {
                        Bytes::Store(source.sequences + row * 80 + 0x44, std::uint16_t{1});
                        Bytes::Store(inputs.pointers[row] + 16, lastTimes[edge][row]);
                        std::memcpy(inputs.before[row].data(), reinterpret_cast<const void*>(inputs.pointers[row]), inputs.before[row].size());
                        inputs.expected[row] = inputs.before[row];
                    }
                    std::memcpy(inputs.beforeRows.data(), reinterpret_cast<const void*>(source.sequences), inputs.beforeRows.size());
                    inputs.expectedH = inputs.beforeH;
                    const std::uint32_t zero = 0;
                    std::memcpy(inputs.expectedH.data() + 0x18, &zero, 4);
                    const auto before = calls;
                    action(command);
                    inputs.CheckAfter();
                    const auto& receipt = runtime.hierarchyUncompressReceipt;
                    Check(calls == before + 1 && receipt.operation == Operation::TotalTime && receipt.postprocessPath && receipt.effectsVerified &&
                              receipt.result == ClockResult::Ok && !receipt.debt && receipt.sequences == 2 && receipt.preparedFrames == 2 &&
                              !receipt.modifiedFrames && !receipt.convertedFrames && receipt.beforeTimeBits == 0x3fa00000 && !receipt.afterTimeBits,
                          "whole original single-key native duration keeps positive zero and preserves negative/signed-zero key timestamps");
                }
            const std::uintptr_t unused[] = {0, 1, (std::numeric_limits<std::uintptr_t>::max)()};
            const std::uint32_t  initialTimes[] = {0, 0x80000000u, 0x3fa00000u, 0x7fc12345u, 0x7f800000u, 0xff800000u};
            for (unsigned compressed = 0; compressed != 2; ++compressed)
                for (const auto pointer : unused)
                    for (const auto time : initialTimes)
                    {
                        Old::CacheFixture cache(saved, image, hierarchy, 1, 0);
                        Bytes::Store(hierarchy + 8, pointer);
                        Bytes::Store(hierarchy + 0x10, std::uint16_t{0});
                        Bytes::Store(hierarchy + 0x12, static_cast<unsigned char>(compressed));
                        Bytes::Store(hierarchy + 0x13, static_cast<unsigned char>(0xcd));
                        Bytes::Store(hierarchy + 0x18, time);
                        std::array<unsigned char, 40>  expectedHeader{};
                        std::array<unsigned char, 24>  cacheBytes{};
                        std::array<unsigned char, 160> rowBytes{};
                        std::memcpy(expectedHeader.data(), reinterpret_cast<const void*>(hierarchy), expectedHeader.size());
                        std::memcpy(cacheBytes.data(), reinterpret_cast<const void*>(cache.Node(0)), cacheBytes.size());
                        std::memcpy(rowBytes.data(), reinterpret_cast<const void*>(source.sequences), rowBytes.size());
                        if (operation == Operation::TotalTime)
                        {
                            const std::uint32_t zero = 0;
                            std::memcpy(expectedHeader.data() + 0x18, &zero, sizeof(zero));
                        }
                        const auto before = calls;
                        const auto sse = _mm_getcsr() & 0xffc0u;
                        action(command);
                        const auto& receipt = runtime.hierarchyUncompressReceipt;
                        Check(calls == before + 1 && executed == calls && valid && nestedBusy && gateBusy && retained && receipt.operation == operation &&
                                  receipt.postprocessPath && receipt.entered && receipt.returned && receipt.effectsVerified && !receipt.debt &&
                                  receipt.result == ClockResult::Ok && receipt.entryRva == Entry() && !receipt.sequences && !receipt.preparedFrames &&
                                  !receipt.modifiedFrames && !receipt.convertedFrames && !receipt.cacheStores && receipt.beforeTimeBits == time &&
                                  receipt.afterTimeBits == (operation == Operation::TotalTime ? 0u : time) &&
                                  receipt.noOp == (operation == Operation::QuaternionFlips || time == 0) && (_mm_getcsr() & 0xffc0u) == sse,
                              "actual empty bank original uses no child/capacity/frame authority and reports exact raw Htime effects");
                        Check(!std::memcmp(expectedHeader.data(), reinterpret_cast<const void*>(hierarchy), expectedHeader.size()) &&
                                  !std::memcmp(cacheBytes.data(), reinterpret_cast<const void*>(cache.Node(0)), cacheBytes.size()) &&
                                  !std::memcmp(rowBytes.data(), reinterpret_cast<const void*>(source.sequences), rowBytes.size()),
                              "empty originals preserve unused child tags, encoding/custom flags, cache and all untraversed row bytes");
                        Check(!runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load(),
                              "verified empty bank operation leaves the current owner usable");
                    }
            std::puts(operation == Operation::QuaternionFlips ? "Production bank empty quaternion whole original fixture completed"
                                                              : "Production bank empty time whole original fixture completed");
            facade.reset();
            Check(finish() && !service.IsRetained(), "verified native bank postprocess cleanly retires without private storage adoption");
            std::puts(scenario == 0 ? "Production bank postprocess quaternion actual SDK owner fixture completed"
                                    : "Production bank postprocess time actual SDK owner fixture completed");
            std::puts(scenario == 0 ? "Production bank postprocess exact quaternion signed-zero effects fixture completed"
                                    : "Production bank postprocess exact timestamp and Htime effects fixture completed");
        }
        else if (scenario <= 3)
        {
            for (unsigned kind = 0; kind != 2; ++kind)
            {
                operation = kind ? Operation::TotalTime : Operation::QuaternionFlips;
                Inputs     inputs(source.sequences, hierarchy, false);
                const auto check = [&](const auto& invoke)
                {
                    const auto                     before = calls;
                    std::array<unsigned char, 40>  h{};
                    std::array<unsigned char, 160> rows{};
                    std::memcpy(h.data(), reinterpret_cast<const void*>(hierarchy), h.size());
                    std::memcpy(rows.data(), reinterpret_cast<const void*>(source.sequences), rows.size());
                    std::array<std::vector<unsigned char>, 2> frames = inputs.before;
                    for (unsigned row = 0; row != 2; ++row)
                        std::memcpy(frames[row].data(), reinterpret_cast<const void*>(inputs.pointers[row]), frames[row].size());
                    action([&] { refuses(invoke, "unsupported current bank state refuses SDK postprocessing before original entry"); });
                    Check(calls == before && !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load() &&
                              !std::memcmp(h.data(), reinterpret_cast<const void*>(hierarchy), h.size()) &&
                              !std::memcmp(rows.data(), reinterpret_cast<const void*>(source.sequences), rows.size()),
                          "refused postprocess changes neither H nor row ownership");
                    for (unsigned row = 0; row != 2; ++row)
                        Check(!std::memcmp(frames[row].data(), reinterpret_cast<const void*>(inputs.pointers[row]), frames[row].size()),
                              "refused postprocess preserves every live source float byte");
                };
                if (scenario == 3)
                    check(command);
                else
                {
                    saved.Global(streamRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(0));
                    check(command);
                    saved.Global(streamRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(1));
                    saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::LoadedOffset, static_cast<unsigned char>(0));
                    check(command);
                    saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::LoadedOffset, static_cast<unsigned char>(1));
                    Bytes::Store(hierarchy + 0x12, static_cast<unsigned char>(1));
                    check(command);
                    Bytes::Store(hierarchy + 0x12, static_cast<unsigned char>(0));
                    Bytes::Store(hierarchy + 0x10, std::uint16_t{0xffff});
                    check(command);
                    Bytes::Store(hierarchy + 0x10, std::uint16_t{2});
                    Bytes::Store(source.sequences + 0x42, std::uint16_t{7});
                    check(command);
                    Bytes::Store(source.sequences + 0x42, std::uint16_t{3});
                    Bytes::Store(source.sequences + 0x44, std::uint16_t{0});
                    check(command);
                    Bytes::Store(source.sequences + 0x44, std::uint16_t{4});
                    Bytes::Store(source.sequences - 8, std::uint64_t{1});
                    check(command);
                    Bytes::Store(source.sequences - 8, std::uint64_t{2});
                    const auto child = Bytes::Load<std::uintptr_t>(source.sequences + 80 + 0x48);
                    Bytes::Store(source.sequences + 80 + 0x48, inputs.pointers[0]);
                    check(command);
                    Bytes::Store(source.sequences + 80 + 0x48, child);
                    const auto field = inputs.pointers[0] + (kind ? 16 : 0);
                    const auto original = Bytes::Load<std::uint32_t>(field);
                    Bytes::Store(field, std::uint32_t{0x7fc00000});
                    check(command);
                    Bytes::Store(field, original);
                    const auto& pin = Post::Code[kind];
                    saved.Global(pin.rva + pin.size - 1, static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1), PAGE_EXECUTE_READ);
                    check(command);
                    saved.Global(pin.rva + pin.size - 1, pin.bytes[pin.size - 1], PAGE_EXECUTE_READ);
                    check(
                        [&]
                        {
                            const auto serial = frameCallbackSerial;
                            --frameCallbackSerial;
                            try
                            {
                                command();
                            }
                            catch (...)
                            {
                                frameCallbackSerial = serial;
                                throw;
                            }
                            frameCallbackSerial = serial;
                        });
                    refuses(command, "outside actual callback the producer cannot borrow current bank frame-write access");
                }
            }
            facade.reset();
            Check(finish() && !service.IsRetained() && !calls, "all current bank pre-entry refusals retire without native effects");
            std::puts(scenario == 3 ? "Production bank postprocess default timer refusal fixture completed"
                                    : "Production bank postprocess coupled current bank refusals fixture completed");
        }
        else
        {
            Inputs inputs(source.sequences, hierarchy, false);
            mode = scenario == 4 || scenario == 6 ? 1 : scenario == 5 || scenario == 7 ? 2 : scenario == 8 || scenario == 9 ? 3 : 4;
            action([&] { refuses(command, "entered unknown postprocessing cannot publish a successful SDK result"); });
            const auto receipt = runtime.hierarchyUncompressReceipt;
            Check(calls == 1 && executed == (mode == 1 ? 0u : 1u) && receipt.operation == operation && receipt.postprocessPath && receipt.entered &&
                      !receipt.effectsVerified && receipt.debt && runtime.hierarchyUncompress.RecoveryPending() && NativeLifetimeRetained() &&
                      runtime.stopping.load(),
                  "uncertain original/post-owner/effects retains the real current bank plan and module fence");
            if (mode == 3)
                Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
            std::array<unsigned char, 40>             h{};
            std::array<unsigned char, 160>            rows{};
            std::array<std::vector<unsigned char>, 2> frames = inputs.before;
            std::memcpy(h.data(), reinterpret_cast<const void*>(hierarchy), h.size());
            std::memcpy(rows.data(), reinterpret_cast<const void*>(source.sequences), rows.size());
            for (unsigned row = 0; row != 2; ++row)
                std::memcpy(frames[row].data(), reinterpret_cast<const void*>(inputs.pointers[row]), frames[row].size());
            refuses(command, "retained postprocess facade never replays native work");
            facade.reset();
            Check(!finish() && service.IsRetained() && calls == 1, "End and natural frames retain unknown postprocess effects without replay or repair");
            Check(!std::memcmp(h.data(), reinterpret_cast<const void*>(hierarchy), h.size()) &&
                      !std::memcmp(rows.data(), reinterpret_cast<const void*>(source.sequences), rows.size()),
                  "retained H and row effects remain observed rather than rolled back");
            for (unsigned row = 0; row != 2; ++row)
                Check(!std::memcmp(frames[row].data(), reinterpret_cast<const void*>(inputs.pointers[row]), frames[row].size()),
                      "retained float effects remain unchanged through shutdown");
            const char* debtNames[] = {"quaternion-before", "quaternion-after", "time-before",         "time-after",
                                       "quaternion-owner",  "time-owner",       "quaternion-mismatch", "time-mismatch"};
            std::printf("Production bank postprocess %s retained debt fixture completed\n", debtNames[scenario - 4]);
        }
        ClockHierarchyUncompressOwnerTest::bankInvoke = nullptr;
        image = hierarchy = 0;
        originalFrame = nullptr;
        ClockReaderTestHost::image = priorImage;
        Host::active = nullptr;
        saved.Restore();
    }
}
