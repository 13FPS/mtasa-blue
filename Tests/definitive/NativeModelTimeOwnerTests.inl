/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual timed-model setter/default journal and CGame reset dispatch
 ****************************************************************************/
#include "NativeModelTimeMutationFixture.h"
#include "ModelTimeMutationFixture.generated.h"
namespace ModelTimeResetOwnerFixture
{
    namespace F = NativeModelTimeMutationFixture;
    namespace Fault = ClockModelTimeMutationTest;
    namespace Request = NativeStreamingRequestProfile;
    class Game final : public model_time_lifecycle_fixture::NoPresentationGameBase<CGameModelTimeResetDE>
    {
        using Base = model_time_lifecycle_fixture::NoPresentationGameBase<CGameModelTimeResetDE>;

    public:
        Game(const NativeGameTimingReader& timing, CClockDE& clock, NativeGameTimingAccess& owner, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state)
            : Base(timing, clock, owner, streaming, state)
        {
        }

    protected:
        void RequireResetReady() override { Refuse(); }
        void RestoreDefaultObjectGroupProperties() override { Refuse(); }
        void ResetModelTextureDictionaries() override { Refuse(); }
        void RequireWorldLifecycleReady() override { Refuse(); }
        void RemoveWorldCovers() override { Refuse(); }
        void RemoveWorldPlants() override { Refuse(); }
        void DestroyWorldStencilShadows() override { Refuse(); }
        void DropWorldBuildingCaches() override { Refuse(); }

    private:
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified timed-model lifecycle dependency"); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameModelTimeResetDE> && std::is_abstract_v<CModelInfoTimeDE>);
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite model time Runtime scenario is selected");
        const bool retained = scenario == 1, unknown = scenario == 2;
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* value, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, value, bytes, protection); };
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
        code(NativePedPoolProfile::Code);
        code(NativeModelBankProfile::Code);
        code(Request::Code);
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        auto graph = std::make_unique<NativeStreamingLoadAllFixture::Graph>(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                                            &WorldGroundOwnerFixture::Writes::Apply, &writes);
        std::vector<unsigned char> providerRows(10 * Request::ProviderRowStride);
        const auto                 provider = graph->Game(), rows = reinterpret_cast<std::uintptr_t>(providerRows.data());
        F::Store(reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), Request::ProviderOffset, provider);
        F::Store(provider, Request::ProviderRowsOffset, rows);
        F::Store(provider, Request::ProviderCountOffset, std::int32_t{10});
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        apply(NativeModelBankProfile::BankRva, bank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        auto       storage = std::make_unique<F::Storage>(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        const auto original = storage->Tuple(), secondOriginal = storage->Tuple(1);
        const auto opaqueBefore = storage->RowBytes(), secondOpaque = storage->RowBytes(1);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Fault::fields = {storage->Row() + F::P::TimeOffset, storage->Row(1) + F::P::TimeOffset};
        Fault::exchanges = {};
        Fault::providerCount = provider + Request::ProviderCountOffset;
        Fault::failAfterCommitted = Fault::injected = false;
        Fault::afterExchange = nullptr;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "timed-model mutations preserve shipping refusal before explicit owned-image admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load(), "timed-model setters use the real post-application Runtime owner");
        const auto                   generation = admitted.generation;
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        state;
        auto                         game = std::make_unique<Game>(timing, runtime.clock, runtime.access, streaming, state);
        auto                         model = std::make_unique<ModelTimeMutationObserver>(3, runtime.access, runtime.access);
        CGame&                       sdkGame = *game;
        CModelInfo&                  sdkModel = *model;
        bool                         applied = true;
        Check(runtime.access.SetModelTime(3, 1, 2, applied) != ClockResult::Ok && applied && !runtime.modelTimes.Pending(),
              "producer-thread setter refusal preserves the caller's applied output and captures no default");
        const auto pulse = [&](const auto& operation, bool active = true)
        {
            unsigned calls = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++calls;
                Check(&owner == &runtime.access && GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "timed-model memory, SDK and reset execute on the same distinct genuine native owner thread");
                operation(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(calls == 1 && (!active || !runtime.stopping.load()), "one timed-model action returns from one current native owner callback");
        };
        if (retained || unknown)
        {
            Fault::failAfterCommitted = unknown;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    bool       changed = false;
                    const auto result = owner.SetModelTime(3, 0x80, 0xff, changed);
                    Check((unknown ? result != ClockResult::Ok : result == ClockResult::Ok) && storage->Hours() == 0xff80 && runtime.modelTimes.Pending() &&
                              runtime.modelTimes.Uncertain() == unknown && Fault::injected == unknown && storage->Tuple()[2] == original[2] &&
                              storage->Tuple()[3] == original[3],
                          "a genuine hour CAS retains either a known original or entered post-acknowledgement uncertainty while preserving OtherModel16");
                },
                !unknown);
            if (unknown)
            {
                F::Store(provider, Request::ProviderCountOffset, std::int32_t{10});
                const auto counts = Fault::exchanges;
                Check(runtime.modelTimes.ResetAll(fixture.image, generation, &CheckTimingOwner, &CheckApplicationTimingOwner) == ClockResult::RestoreFailure &&
                          Fault::exchanges == counts && runtime.modelTimes.Pending(),
                      "entered-unknown intent is sticky and cannot replay a CAS even after its independent source is restored");
            }
            game.reset();
            model.reset();
            const auto counts = Fault::exchanges;
            Check(!service.End(generation), "actual End refuses known or uncertain timed-model defaults");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.modelTimes.Pending() && storage->Hours() == 0xff80 && Fault::exchanges == counts,
                  "stopped-frame cleanup and a new Begin retain timed-model debt without replaying or automatically restoring native hours");
            engine.Stop();
            std::puts(unknown ? "Production model time unknown completed" : "Production model time retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                const auto check = [](bool condition, const char* message) { Check(condition, message); };
                F::MemoryRefusals(*storage, generation, &CheckTimingOwner, &CheckApplicationTimingOwner, check);
                bool       noTime = true;
                const auto before = Fault::exchanges;
                Check(owner.SetModelTime(8, 1, 2, noTime) == ClockResult::Ok && !noTime && owner.SetModelTime(9, 1, 2, noTime) == ClockResult::Ok && !noTime &&
                          !runtime.modelTimes.Pending() && Fault::exchanges == before,
                      "qualified absent and non-time models return false without a default or native CAS");
                char on = 0, off = 0;
                Check(sdkModel.GetTime(on, off) && static_cast<unsigned char>(on) == original[0] && static_cast<unsigned char>(off) == original[1],
                      "the actual model SDK getter observes both native hour bytes through its same classification owner");
                Check(sdkModel.SetTime(static_cast<char>(original[0]), static_cast<char>(original[1])) && runtime.modelTimes.Pending(),
                      "an initial same-hours SetTime still captures the first default exactly like the classic MTA policy");
                sdkGame.ResetModelTimes();
                Check(!runtime.modelTimes.Pending() && storage->Tuple() == original, "the actual public CGame reset method drains that same-hours default");
                NativeModelTimeAccess foreign;
                bool                  refused = false;
                try
                {
                    ModelTimeMutationObserver mismatch(3, owner, foreign);
                }
                catch (const std::runtime_error&)
                {
                    refused = true;
                }
                Check(refused, "a model setter cannot borrow a different writer from the classification and sibling model owners");
                Check(sdkModel.GetModel() == 0xfedcba98u && sdkModel.SetTime(static_cast<char>(0x80), static_cast<char>(0xff)) && storage->Hours() == 0xff80,
                      "the real setter retains raw high-bit chars and its immutable model key despite a poisoned virtual GetModel");
                Check(owner.SetModelTime(7, 5, 19, applied) == ClockResult::Ok && applied && storage->Hours() == 0x1305 && runtime.modelTimes.Pending(),
                      "a second bank key aliases the same metadata storage without replacing its first tuple");
                const auto driftCounts = Fault::exchanges;
                storage->Bind(7, storage->Row(1));
                Check(owner.ResetAllModelTimes() != ClockResult::Ok && runtime.modelTimes.Pending() && !runtime.modelTimes.Uncertain() &&
                          storage->Hours() == 0x1305 && Fault::exchanges == driftCounts,
                      "a retargeted last-proven alias route refuses restoration while retaining the original default");
                storage->Bind(7, storage->Row());
                auto external = storage->Tuple();
                external[0] ^= 1;
                storage->Tuple(external);
                Check(owner.SetModelTime(3, 2, 3, applied) == ClockResult::OwnerConflict && owner.ResetAllModelTimes() == ClockResult::OwnerConflict &&
                          runtime.modelTimes.Pending() && Fault::exchanges == driftCounts,
                      "external hour drift cannot be overwritten by Set or Reset while a first default is retained");
                external[0] ^= 1;
                storage->Tuple(external);
                external[2] ^= 1;
                storage->Tuple(external);
                Check(owner.ResetAllModelTimes() == ClockResult::OwnerConflict && runtime.modelTimes.Pending() && Fault::exchanges == driftCounts,
                      "changed OtherModel16 pairing cannot borrow the retained first default's mutable hours");
                external[2] ^= 1;
                storage->Tuple(external);
                Check(owner.SetModelTime(7, 0xff, 0x80, applied) == ClockResult::Ok && applied && storage->Hours() == 0x80ff,
                      "explicit restoration to the journal's acknowledged preimage permits another real alias write");
                sdkGame.ResetModelTimes();
                Check(!runtime.modelTimes.Pending() && storage->Tuple() == original && storage->Tuple(1) == secondOriginal,
                      "one first tuple survives repeated writes and aliases and is restored by actual CGame dispatch");
                F::KnownPrefix(*storage, generation, &CheckTimingOwner, &CheckApplicationTimingOwner, check);
                Check(storage->RowBytes() == opaqueBefore && storage->RowBytes(1) == secondOpaque,
                      "all native opaque bytes and the pairing word survive setter/reset activity outside the two hour bytes");
            });
        bool        outside = true;
        ClockResult foreignResult = ClockResult::Ok;
        std::thread foreign([&] { foreignResult = runtime.access.SetModelTime(3, 1, 2, outside); });
        foreign.join();
        Check(foreignResult != ClockResult::Ok && outside && !runtime.modelTimes.Pending(), "a foreign thread cannot renew the setter's native borrow");
        game.reset();
        model.reset();
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.modelTimes.CanDiscard(), "explicit healthy restoration permits authoritative timed-model owner retirement");
        outside = true;
        Check(runtime.access.SetModelTime(3, 1, 2, outside) != ClockResult::Ok && outside, "a retired owner cannot renew an earlier timed-model tuple borrow");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        Fault::fields = {};
        Fault::providerCount = 0;
        writes.Restore();
        std::puts("Production model time healthy completed");
    }
}
