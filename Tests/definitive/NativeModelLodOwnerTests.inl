/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual model distance setter/default journal and CGame reset dispatch
 ****************************************************************************/
#include "NativeModelLodMutationFixture.h"
#include "ModelLodMutationFixture.generated.h"
namespace ModelLodResetOwnerFixture
{
    namespace F = NativeModelLodMutationFixture;
    namespace Fault = ClockModelLodMutationTest;
    namespace Request = NativeStreamingRequestProfile;
    class Game final : public model_lod_lifecycle_fixture::NoPresentationGameBase<CGameModelLodResetDE>
    {
        using Base = model_lod_lifecycle_fixture::NoPresentationGameBase<CGameModelLodResetDE>;

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
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified distance lifecycle dependency"); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameModelLodResetDE> && std::is_abstract_v<CModelInfoLodDE>);
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite model lod Runtime scenario is selected");
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
        const auto original = storage->BitsAt(), secondOriginal = storage->BitsAt(1);
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
        Fault::fields = {storage->Row() + F::P::FieldOffset, storage->Row(1) + F::P::FieldOffset};
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
              "model distance mutations preserve shipping refusal before explicit owned-image admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load(), "model distance setters use the real post-application Runtime owner");
        const auto                   generation = admitted.generation;
        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        state;
        auto                         game = std::make_unique<Game>(timing, runtime.clock, runtime.access, streaming, state);
        auto                         model = std::make_unique<ModelLodMutationObserver>(3, runtime.access, runtime.access);
        CGame&                       sdkGame = *game;
        CModelInfo&                  sdkModel = *model;
        std::uint32_t                outsideBits = 0xdeadbeefu;
        Check(runtime.access.ReadModelLodDistance(3, outsideBits) != ClockResult::Ok && outsideBits == 0xdeadbeefu && !runtime.modelLods.Pending(),
              "producer-thread distance refusal preserves its output and captures no default");
        const auto pulse = [&](const auto& operation, bool active = true)
        {
            unsigned calls = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++calls;
                Check(&owner == &runtime.access && GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "model distance memory, SDK and reset execute on the same distinct genuine native owner thread");
                operation(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(calls == 1 && (!active || !runtime.stopping.load()), "one model distance action returns from one current native owner callback");
        };
        if (retained || unknown)
        {
            Fault::failAfterCommitted = unknown;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    const auto result = owner.SetModelLodDistance(3, 0x80000000u);
                    Check((unknown ? result != ClockResult::Ok : result == ClockResult::Ok) && storage->BitsAt() == 0x80000000u &&
                              runtime.modelLods.Pending() && runtime.modelLods.Uncertain() == unknown && Fault::injected == unknown &&
                              runtime.stopping.load() == unknown,
                          "a genuine distance CAS retains a known original or sticky entered post-acknowledgement uncertainty");
                },
                !unknown);
            if (unknown)
            {
                F::Store(provider, Request::ProviderCountOffset, std::int32_t{10});
                const auto counts = Fault::exchanges;
                Check(runtime.modelLods.ResetAll(fixture.image, generation, &CheckTimingOwner, &CheckApplicationTimingOwner) == ClockResult::RestoreFailure &&
                          Fault::exchanges == counts && runtime.modelLods.Pending(),
                      "an entered-unknown distance intent cannot replay even after the independent source returns to its original value");
            }
            game.reset();
            model.reset();
            const auto counts = Fault::exchanges;
            Check(!service.End(generation), "actual End refuses known and uncertain native distance defaults");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.modelLods.Pending() && storage->BitsAt() == 0x80000000u && Fault::exchanges == counts,
                  "stopped cleanup and new Begin retain actual distance debt without replaying or automatically restoring its native field");
            engine.Stop();
            std::puts(unknown ? "Production model lod unknown completed" : "Production model lod retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                const auto    check = [](bool condition, const char* message) { Check(condition, message); };
                const auto    before = Fault::exchanges;
                std::uint32_t bits = 0xdeadbeefu;
                Check(owner.ReadModelLodDistance(8, bits) == ClockResult::Ok && bits == 0 && owner.SetModelLodDistance(8, F::Bits(30.0f)) == ClockResult::Ok &&
                          !runtime.modelLods.Pending() && Fault::exchanges == before,
                      "the genuine absent association returns current zero and ignores Set without inventing a native default");
                bits = 0xdeadbeefu;
                Check(owner.ReadModelLodDistance(9, bits) == ClockResult::UnsupportedBuild && bits == 0xdeadbeefu &&
                          owner.SetModelLodDistance(9, F::Bits(30.0f)) == ClockResult::UnsupportedBuild && !runtime.modelLods.Pending(),
                      "a current unsupported receiver is a refusal with unchanged output rather than a successful absent model");
                F::MemoryRefusals(*storage, generation, &CheckTimingOwner, &CheckApplicationTimingOwner, check);
                Check(F::Bits(sdkModel.GetLODDistance()) == original && F::Bits(sdkModel.GetOriginalLODDistance()) == 0,
                      "the actual SDK reads the native current distance and separately reports no uncached model-ID default");
                sdkModel.SetLODDistance(F::Float(original), true);
                Check(runtime.modelLods.Pending() && F::Bits(sdkModel.GetOriginalLODDistance()) == original,
                      "an initial same-bits setter with real override captures the first model-ID default");
                sdkGame.ResetModelLodDistances();
                Check(!runtime.modelLods.Pending() && storage->BitsAt() == original && F::Bits(sdkModel.GetOriginalLODDistance()) == 0,
                      "the actual public CGame reset drains the same-bits default and clears its original cache");
                NativeModelLodAccess foreign;
                bool                 refused = false;
                try
                {
                    ModelLodMutationObserver mismatch(3, owner, foreign);
                }
                catch (const std::runtime_error&)
                {
                    refused = true;
                }
                Check(refused, "distance writer and all inherited model siblings must use the same actual classification owner");
                Check(sdkModel.GetModel() == 0xfedcba98u, "the observer poisons virtual GetModel so the real setter must retain its immutable SDK key");
                sdkModel.SetLODDistance(500.0f);
                Check(storage->BitsAt() == F::Bits(325.0f), "actual MTA distance policy applies its existing 325 limit before native owner access");
                sdkModel.SetLODDistance(500.0f, true);
                Check(storage->BitsAt() == F::Bits(500.0f), "the real SDK override preserves a caller's above-limit distance");
                sdkModel.SetLODDistance(-17.0f);
                Check(storage->BitsAt() == F::Bits(-17.0f), "the actual shared SDK policy preserves negative distances without a new lower clamp");
                sdkModel.SetLODDistance(F::Float(0x7f800000u));
                Check(storage->BitsAt() == F::Bits(325.0f), "positive infinity follows the existing default upper limit");
                sdkModel.SetLODDistance(F::Float(0x7f800000u), true);
                Check(storage->BitsAt() == 0x7f800000u, "the real override retains positive infinity's original Float32 representation");
                sdkModel.SetLODDistance(F::Float(0xff800000u));
                Check(storage->BitsAt() == 0xff800000u, "negative infinity remains unchanged by the actual std::min argument order");
                sdkModel.SetLODDistance(F::Float(0x80000000u));
                Check(storage->BitsAt() == 0x80000000u, "the actual SDK preserves negative zero rather than normalizing the field");
                sdkModel.SetLODDistance(F::Float(0x7fc12345u));
                Check(storage->BitsAt() == 0x7fc12345u && F::Bits(sdkModel.GetLODDistance()) == 0x7fc12345u,
                      "the real std::min argument order and raw-bit native path preserve the supplied quiet NaN payload");
                sdkGame.ResetModelLodDistances();
                Check(storage->BitsAt() == original && !runtime.modelLods.Pending(), "repeated actual SDK edits retain and restore the first raw original");
                sdkModel.SetLODDistance(100.0f);
                Check(owner.SetModelLodDistance(7, F::Bits(200.0f)) == ClockResult::Ok && storage->BitsAt() == F::Bits(200.0f),
                      "the second model ID writes the same actual metadata and captures its distinct current default");
                bits = 0;
                Check(F::Bits(sdkModel.GetOriginalLODDistance()) == original && owner.ReadOriginalModelLodDistance(7, bits) == ClockResult::Ok &&
                          bits == F::Bits(100.0f),
                      "model-ID cache keys preserve different first defaults even when their native distance field aliases");
                const auto driftCounts = Fault::exchanges;
                storage->Bind(7, storage->Row(1));
                Check(owner.ResetAllModelLodDistances() != ClockResult::Ok && runtime.modelLods.Pending() && !runtime.modelLods.Uncertain() &&
                          storage->BitsAt() == original && Fault::exchanges[0] == driftCounts[0] + 1,
                      "ordered reset acknowledges model3 before refusing retargeted alias7 and retains the uncompleted suffix");
                storage->Bind(7, storage->Row());
                const auto acknowledged = Fault::exchanges;
                sdkGame.ResetModelLodDistances();
                Check(!runtime.modelLods.Pending() && storage->BitsAt() == F::Bits(100.0f) && Fault::exchanges[0] == acknowledged[0] + 1,
                      "retry skips the acknowledged prefix and applies alias7's distinct first default as classic ascending-ID reset requires");
                sdkModel.SetLODDistance(175.0f);
                const auto ownedBits = storage->BitsAt();
                const auto externalCounts = Fault::exchanges;
                storage->Distance(F::Bits(176.0f));
                Check(owner.SetModelLodDistance(3, F::Bits(177.0f)) == ClockResult::OwnerConflict &&
                          owner.ResetAllModelLodDistances() == ClockResult::OwnerConflict && runtime.modelLods.Pending() && Fault::exchanges == externalCounts,
                      "external distance drift cannot be overwritten while a prior original and owned preimage remain pending");
                storage->Distance(ownedBits);
                sdkGame.ResetModelLodDistances();
                Check(storage->BitsAt() == F::Bits(100.0f) && !runtime.modelLods.Pending(),
                      "explicit restoration of the recorded owned preimage permits a freshly validated healthy retry");
                sdkModel.SetLODDistance(90.0f);
                storage->Bind(3, 0);
                bits = 0xdeadbeefu;
                Check(owner.ReadModelLodDistance(3, bits) == ClockResult::Ok && bits == 0 && F::Bits(sdkModel.GetOriginalLODDistance()) == F::Bits(100.0f),
                      "a freshly absent association reads current zero while GetOriginal remains the separate actual model-ID cache");
                sdkGame.ResetModelLodDistances();
                Check(!runtime.modelLods.Pending() && storage->BitsAt() == F::Bits(90.0f),
                      "the classic reset policy skips a genuinely absent bank key without dereferencing or overwriting its previous row");
                storage->Bind(3, storage->Row());
                storage->Distance(F::Bits(100.0f));
                F::KnownPrefix(*storage, generation, &CheckTimingOwner, &CheckApplicationTimingOwner, check);
                F::KnownAbsentPrefix(*storage, generation, &CheckTimingOwner, &CheckApplicationTimingOwner, check);
                Check(storage->OutsideFieldUnchanged(opaqueBefore) && storage->OutsideFieldUnchanged(secondOpaque, 1) && storage->BitsAt(1) == secondOriginal,
                      "all opaque native row bytes and independent fields survive outside the exact four-byte distance store");
            });
        std::uint32_t outside = 0xdeadbeefu;
        ClockResult   foreignResult = ClockResult::Ok;
        std::thread   foreign([&] { foreignResult = runtime.access.ReadModelLodDistance(3, outside); });
        foreign.join();
        Check(foreignResult != ClockResult::Ok && outside == 0xdeadbeefu && !runtime.modelLods.Pending(),
              "a foreign thread cannot renew the actual owner's native distance borrow or overwrite its caller's output");
        game.reset();
        model.reset();
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.modelLods.CanDiscard(), "explicit healthy distance restoration permits authoritative owner retirement");
        Check(runtime.access.SetModelLodDistance(3, F::Bits(20.0f)) != ClockResult::Ok, "a retired owner cannot renew a prior native metadata borrow");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        Fault::fields = {};
        Fault::providerCount = 0;
        writes.Restore();
        std::puts("Production model lod healthy completed");
    }
}
