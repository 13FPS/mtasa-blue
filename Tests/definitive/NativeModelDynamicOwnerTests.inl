/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual Runtime and SDK dynamic predicate on three admitted families.
 *****************************************************************************/
#include "NativeModelDynamicFixture.h"
#include "ModelDynamicFixture.generated.h"
namespace ModelDynamicOwnerFixture
{
    namespace F = NativeModelDynamicFixture;
    namespace Request = NativeStreamingRequestProfile;
    class MissingOwner final : public NativeModelClassificationAccess
    {
    };
    template <class Operation>
    bool Refuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        return false;
    }
    void Run(ClockImageFixture& fixture)
    {
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
        // Large graph/storage stay heap-owned. Their identities remain alive
        // through actual callbacks and authoritative owner retirement.
        auto graph = std::make_unique<NativeStreamingLoadAllFixture::Graph>(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                                            &WorldGroundOwnerFixture::Writes::Apply, &writes);
        std::vector<unsigned char> providerRows(16 * Request::ProviderRowStride);
        const auto                 provider = graph->Game(), rows = reinterpret_cast<std::uintptr_t>(providerRows.data());
        F::Store(reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), Request::ProviderOffset, provider);
        F::Store(provider, Request::ProviderRowsOffset, rows);
        F::Store(provider, Request::ProviderCountOffset, std::int32_t{16});
        std::vector<std::uintptr_t> bank(NativeModelBankProfile::ModelCount);
        apply(NativeModelBankProfile::BankRva, bank.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
        auto storage = std::make_unique<F::Storage>(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "dynamic predicate preserves shipping admission refusal");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.bound && runtime.frameHooked.load(), "dynamic predicate uses the actual post-application Runtime owner");
        const auto generation = admitted.generation;
        const auto pulse = [&](const auto& operation)
        {
            unsigned calls = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++calls;
                Check(&owner == &runtime.access && GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "predicate memory and SDK execute on the real distinct current-owner thread");
                operation(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(calls == 1 && !runtime.stopping.load(), "one read-only predicate action uses one current owner callback");
        };
        bool output = true;
        Check(runtime.access.ReadModelDynamic(3, output) != ClockResult::Ok && output, "producer-thread predicate refusal preserves its output");
        MissingOwner         missing;
        ModelDynamicObserver missingModel(3, missing);
        Check(Refuses([&] { static_cast<void>(missingModel.IsDynamic()); }), "missing native predicate authority throws rather than fabricating false");
        const auto classifierCalls = ClockModelClassificationOwnerTest::calls;
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                const std::array<std::pair<std::uint32_t, std::uintptr_t>, 3> families{
                    {{3, storage->Row()}, {5, fixture.image + F::T::PoolRva}, {6, fixture.image + F::V::PoolRva}}};
                for (const auto& selected : families)
                {
                    ModelDynamicObserver model(selected.first, owner);
                    CModelInfo&          sdk = model;
                    for (const auto word : {std::uint16_t{0}, std::uint16_t{0xffff}, std::uint16_t{0x8000}, std::uint16_t{0xfffe}})
                    {
                        storage->Word(selected.second, word);
                        std::array<unsigned char, 0x60> before{};
                        std::memcpy(before.data(), reinterpret_cast<void*>(selected.second), before.size());
                        output = word == 0xffff;
                        Check(owner.ReadModelDynamic(selected.first, output) == ClockResult::Ok && output == (word != 0xffff),
                              "each admitted family preserves exact unsigned WORD sentinel semantics");
                        Check(sdk.IsDynamic() == (word != 0xffff) && sdk.GetModel() == selected.first,
                              "actual inherited SDK IsDynamic forwards this same model key to the current Runtime");
                        Check(std::memcmp(before.data(), reinterpret_cast<void*>(selected.second), before.size()) == 0,
                              "native predicate and SDK dispatch leave every selected metadata byte unchanged");
                    }
                }
                output = true;
                Check(owner.ReadModelDynamic(7, output) == ClockResult::Ok && !output, "qualified current null model association is false");
                for (const auto key : {std::uint32_t{8}, std::uint32_t{9}, static_cast<std::uint32_t>(F::B::ModelCount)})
                    for (const bool sentinel : {false, true})
                    {
                        output = sentinel;
                        Check(owner.ReadModelDynamic(key, output) != ClockResult::Ok && output == sentinel,
                              "heap, second raw-type3 family and invalid bank key refuse without changing either boolean preimage");
                    }
                ModelDynamicObserver unsupported(8, owner);
                Check(Refuses([&] { static_cast<void>(unsupported.IsDynamic()); }), "unsupported present SDK receiver does not become a static model");
                storage->Global(F::A::PoolCountRva, std::int32_t{1});
                output = true;
                Check(owner.ReadModelDynamic(4, output) != ClockResult::Ok && output, "an aligned typed row outside the allocated prefix refuses");
                storage->Global(F::A::PoolCountRva, std::int32_t{2});
                storage->Word(storage->Row(), 0);
            });
        pulse(
            [&](NativeGameTimingAccess&)
            {
                F::Gate gate{&CheckTimingOwner, &CheckApplicationTimingOwner};
                F::Gate::current = &gate;
                output = false;
                Check(F::M::Read(fixture.image, generation, 3, &F::Gate::Owner, &F::Gate::After, output) == ClockResult::Ok && output,
                      "direct read borrows the same real owner callbacks before fault calibration");
                const auto applications = gate.applications;
                Check(applications > 0 && gate.owners > 0, "calibrate real application/owner bookends without fixed callback counts");
                const auto patch = [&](std::uintptr_t at, const void* value, std::size_t bytes)
                {
                    if (at >= fixture.image && at - fixture.image < NativePedPoolProfile::ImageSize)
                        fixture.Patch(at - fixture.image, value, bytes);
                    else
                        std::memcpy(reinterpret_cast<void*>(at), value, bytes);
                };
                const auto drift = [&](std::uintptr_t at, const auto& changed)
                {
                    using Value = std::decay_t<decltype(changed)>;
                    Value before{};
                    std::memcpy(&before, reinterpret_cast<void*>(at), sizeof(before));
                    for (const bool sentinel : {false, true})
                    {
                        gate.owners = gate.applications = 0;
                        gate.mutateAt = applications;
                        gate.change = [&] { patch(at, &changed, sizeof(changed)); };
                        output = sentinel;
                        Check(F::M::Read(fixture.image, generation, 3, &F::Gate::Owner, &F::Gate::After, output) != ClockResult::Ok && output == sentinel,
                              "post-owner raw identity or WORD drift refuses without publishing either boolean preimage");
                        patch(at, &before, sizeof(before));
                    }
                    gate.change = nullptr;
                };
                drift(storage->Row() + F::A::FieldOffset, std::uint16_t{1});
                drift(fixture.image + F::B::BankRva + 3 * F::B::SlotBytes, storage->Row(1));
                drift(fixture.image + F::A::PoolCountRva, std::int32_t{3});
                drift(provider + Request::ProviderCountOffset, std::int32_t{15});
                drift(provider + Request::ProviderRowsOffset, rows + Request::ProviderRowStride);
                drift(reinterpret_cast<std::uintptr_t>(graph->items.data()) + 0x10, std::uint32_t{777});
                drift(fixture.image + F::T::VtableRva + 0x40, fixture.image + F::V::InitializeRva);
                drift(fixture.image + F::A::TypeGetterRva, static_cast<unsigned char>(0x90));
                drift(fixture.image + NativeStartGameProfile::ThreadIdRva, DWORD{0});
                // Neighbor flags are not the property. Their independent change
                // must not turn a read-only WORD predicate into whole-row ownership.
                const auto    flagsAt = storage->Row() + 0x2a;
                std::uint16_t flags = 0;
                std::memcpy(&flags, reinterpret_cast<void*>(flagsAt), sizeof(flags));
                gate.owners = gate.applications = 0;
                gate.change = [&] { F::Store(flagsAt, 0, static_cast<std::uint16_t>(flags ^ 1)); };
                output = false;
                Check(F::M::Read(fixture.image, generation, 3, &F::Gate::Owner, &F::Gate::After, output) == ClockResult::Ok && output,
                      "independent neighboring WORD flags may change without revoking the stable property predicate");
                F::Store(flagsAt, 0, flags);
                gate.change = nullptr;
                F::Gate::current = nullptr;
            });
        Check(ClockModelClassificationOwnerTest::calls == classifierCalls && !runtime.modelObjectGroups.Pending() && !runtime.modelLods.Pending() &&
                  !runtime.modelTimes.Pending(),
              "predicate invokes no native classifier and creates no mutation/default journal");
        ClockResult foreignResult = ClockResult::Ok;
        output = false;
        std::thread foreign([&] { foreignResult = runtime.access.ReadModelDynamic(3, output); });
        foreign.join();
        Check(foreignResult != ClockResult::Ok && !output, "foreign thread cannot borrow the predicate's current owner");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation), "read-only predicate permits authoritative normal Runtime retirement");
        output = true;
        Check(runtime.access.ReadModelDynamic(3, output) != ClockResult::Ok && output, "retired generation cannot republish an old predicate observation");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production model dynamic predicate owner completed");
    }
}
