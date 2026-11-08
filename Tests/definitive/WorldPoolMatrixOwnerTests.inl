/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual matrix intent/controller/Runtime with labelled entry doubles
 ****************************************************************************/
namespace ClockWorldPoolMatrixOwnerTest
{
    mta::game_de::ClockResult Invoke(mta::game_de::NativeWorldPoolEntryCalls::Entry original, void* entity, std::uint32_t& exception) noexcept
    {
        using namespace mta::game_de;
        namespace P = NativeWorldPoolMatrixProfile;
        exception = 0;
        const auto function = reinterpret_cast<std::uintptr_t>(original), target = reinterpret_cast<std::uintptr_t>(entity);
        if (!current || image != current->image || (target != current->buildings.Row(0) && target != current->dummies.Row(0)))
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        if (function == image + P::RemoveMatrixRva)
        {
            ++removes;
            if (current->Remove(target))
                return ClockResult::Ok;
        }
        else if (function == image + P::AllocateStaticRva)
        {
            ++allocations;
            if (current->Allocate(target))
            {
                const auto matrix = current->Pointer(target, 0x18);
                std::memcpy(allocatedTail.data(), reinterpret_cast<void*>(matrix + 0x40), allocatedTail.size());
                if (failAllocationAfterEffect)
                {
                    exception = 0xe0550001;
                    return ClockResult::OriginalException;
                }
                return ClockResult::Ok;
            }
        }
        invalidInvocation = true;
        return ClockResult::InvalidArgument;
    }
}
namespace WorldPoolMatrixOwnerFixture
{
    namespace P = NativeWorldPoolMatrixProfile;
    namespace M = NativeWorldPoolMatrixMemory;
    namespace W = NativeWorldIplFixture;
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite matrix intent lifetime is selected");
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
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                   &WorldGroundOwnerFixture::Writes::Apply, &writes);
        W::State                             world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeWorldPoolMatrixFixture::State  matrices(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready() && matrices.Ready(), "owned typed Building/Dummy pools and full native matrix bank were allocated");
        // The same genuine native-shaped heap/profile support as the isolated
        // primitive tests is installed before the matrix bank's own roots.
        world.Reset();
        matrices.Reset();
        matrices.Move(0, M::List::Dynamic, matrices.buildings.Row(0));
        matrices.Move(1, M::List::Static, matrices.dummies.Row(0));
        W::Put(matrices.Node(0), 0x30, 25.0f);
        W::Put(matrices.Node(0), 0x34, -7.0f);
        W::Put(matrices.Node(0), 0x38, 9.0f);
        W::Put(matrices.Node(1), 0x30, -3.0f);
        W::Put(matrices.Node(1), 0x34, 15.0f);
        W::Put(matrices.Node(1), 0x38, 20.0f);
        // Source consumers do not establish any bool semantics at48. Nonzero
        // values and padding must survive detach, allocation and payload copy.
        W::Put(matrices.Node(0), P::Opaque48Offset, std::uint64_t{0xa500000000000001});
        W::Put(matrices.Node(1), P::Opaque48Offset, std::uint64_t{0xff12341234123456});
        M::Transform original{}, dummyOriginal{};
        std::memcpy(original.data(), reinterpret_cast<void*>(matrices.Node(0)), original.size());
        std::memcpy(dummyOriginal.data(), reinterpret_cast<void*>(matrices.Node(1)), dummyOriginal.size());
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockWorldPoolMatrixOwnerTest::current = &matrices;
        ClockWorldPoolMatrixOwnerTest::image = fixture.image;
        ClockWorldPoolMatrixOwnerTest::removes = ClockWorldPoolMatrixOwnerTest::allocations = 0;
        ClockWorldPoolMatrixOwnerTest::failAllocationAfterEffect = ClockWorldPoolMatrixOwnerTest::invalidInvocation = false;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "matrix controller preserves the shipping executable refusal before explicit owned-image test admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "matrix requests use the real current post-application Runtime");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "matrix work runs on the actual distinct admitted native engine thread");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one matrix action consumes one qualified engine callback");
        };
        M::Transform output;
        output.fill(0x77);
        const auto sentinel = output;
        bool       present = true;
        Check(runtime.access.ReadWorldPoolMatrix(M::Kind::Building, 0, output, present) != ClockResult::Ok && output == sentinel && present &&
                  !runtime.worldPoolMatrices.Pending(),
              "producer read refusal preserves both outputs and creates no matrix intent");
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.ReadWorldPoolMatrix(M::Kind::Building, 0, output, present) == ClockResult::Ok && present && output == original &&
                          !runtime.worldPoolMatrices.Pending(),
                      "real typed pool/bank/list read observes the native owner transform without taking a fake lease");
                Check(owner.DetachWorldPoolMatrix(M::Kind::Building, 0) == ClockResult::Ok && runtime.worldPoolMatrices.Pending() &&
                          !runtime.worldPoolMatrices.Uncertain() && matrices.Pointer(matrices.buildings.Row(0), 0x18) == 0 &&
                          ClockWorldPoolMatrixOwnerTest::removes == 1,
                      "real controller admits exact list/row effects of the labelled RemoveMatrix native-original double");
                Check(owner.DetachWorldPoolMatrix(M::Kind::Building, 0) == ClockResult::Ok && ClockWorldPoolMatrixOwnerTest::removes == 1,
                      "repeated known detach does not recapture defaults or replay the native entry");
                output = sentinel;
                present = true;
                Check(owner.ReadWorldPoolMatrix(M::Kind::Building, 0, output, present) == ClockResult::Ok && !present && output == sentinel,
                      "a current no-matrix observation preserves transform output while the independent detached intent remains retained");
            });
        if (retained || unknown)
        {
            if (unknown)
            {
                ClockWorldPoolMatrixOwnerTest::failAllocationAfterEffect = true;
                pulse(
                    [&](NativeGameTimingAccess& owner)
                    {
                        Check(owner.RestoreWorldPoolMatrix(M::Kind::Building, 0) != ClockResult::Ok && runtime.worldPoolMatrices.Uncertain() &&
                                  runtime.worldPoolMatrices.Pending() && ClockWorldPoolMatrixOwnerTest::allocations == 1 &&
                                  matrices.Pointer(matrices.buildings.Row(0), 0x18) != 0,
                              "entered allocation failure retains its actual static-list prefix without pretending the payload restored");
                    },
                    false);
            }
            const auto matrix = matrices.Pointer(matrices.buildings.Row(0), 0x18);
            const auto removes = ClockWorldPoolMatrixOwnerTest::removes, allocations = ClockWorldPoolMatrixOwnerTest::allocations;
            Check(!service.End(generation), "known detach or entered-unknown allocation refuses actual owner End");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.worldPoolMatrices.Pending() &&
                      matrices.Pointer(matrices.buildings.Row(0), 0x18) == matrix && ClockWorldPoolMatrixOwnerTest::removes == removes &&
                      ClockWorldPoolMatrixOwnerTest::allocations == allocations,
                  "stopped-frame cleanup/new Begin retain matrix intent without automatic removal, allocation or payload replay");
            engine.Stop();
            ClockWorldPoolMatrixOwnerTest::current = nullptr;
            ClockReaderTestHost::image = previousImage;
            std::puts(unknown ? "Production world pool matrix unknown completed" : "Production world pool matrix retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.DetachWorldPoolMatrix(M::Kind::Dummy, 0) == ClockResult::Ok && ClockWorldPoolMatrixOwnerTest::removes == 2,
                      "a genuine occupied Dummy slot owns its distinct matrix intent");
                const auto freeHead = matrices.Pointer(matrices.Head(M::List::Free), P::NextOffset);
                W::Put(freeHead, P::AttachedOffset, std::uintptr_t{0x1111});
                Check(owner.RestoreWorldPoolMatrix(M::Kind::Building, 0) == ClockResult::UnsupportedBuild && ClockWorldPoolMatrixOwnerTest::allocations == 0 &&
                          runtime.worldPoolMatrices.Pending() && !runtime.worldPoolMatrices.Uncertain(),
                      "unqualified attached RW pointer on the actual free head refuses before allocator fallback or another node can be selected");
                W::Put(freeHead, P::AttachedOffset, std::uintptr_t{0});
                Check(owner.RestoreWorldPoolMatrix(M::Kind::Building, 0) == ClockResult::Ok && ClockWorldPoolMatrixOwnerTest::allocations == 1,
                      "explicit restoration consumes the current qualified free head into native static list2");
                const auto restored = matrices.Pointer(matrices.buildings.Row(0), 0x18);
                Check(restored == matrices.Node(1) && std::memcmp(reinterpret_cast<void*>(restored), original.data(), original.size()) == 0 &&
                          std::memcmp(reinterpret_cast<void*>(restored + 0x40), ClockWorldPoolMatrixOwnerTest::allocatedTail.data(),
                                      ClockWorldPoolMatrixOwnerTest::allocatedTail.size()) == 0,
                      "real production64-byte payload copy restores original transform while preserving the new allocation's complete opaque/owner/link tail");
                Check(owner.RestoreAllWorldPoolMatrices() == ClockResult::Ok && !runtime.worldPoolMatrices.Pending() &&
                          ClockWorldPoolMatrixOwnerTest::allocations == 2,
                      "explicit remaining-matrix restoration retires the actual second intent without repeating the first");
                Check(owner.ReadWorldPoolMatrix(M::Kind::Dummy, 0, output, present) == ClockResult::Ok && present && output == dummyOriginal &&
                          !ClockWorldPoolMatrixOwnerTest::invalidInvocation,
                      "current Dummy static matrix has its original exact transform and every original invocation was the declared final double");
            });
        auto        foreign = sentinel;
        bool        foreignPresent = true;
        ClockResult foreignResult = ClockResult::Ok;
        std::thread outsider([&] { foreignResult = runtime.access.ReadWorldPoolMatrix(M::Kind::Building, 0, foreign, foreignPresent); });
        outsider.join();
        Check(foreignResult != ClockResult::Ok && foreign == sentinel && foreignPresent && !runtime.worldPoolMatrices.Pending(),
              "foreign threads cannot borrow matrix bank or entity admission");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.worldPoolMatrices.CanDiscard(), "acknowledged static restoration permits actual native owner retirement");
        NativeClockService  next;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = next.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation, "a clean matrix domain permits a distinct next owner generation");
        engine.Pulse(next, nextState.generation);
        if (!next.End(nextState.generation))
            engine.Pulse(next, nextState.generation, true);
        Check(next.End(nextState.generation), "next no-effect matrix owner retires normally");
        engine.Stop();
        ClockWorldPoolMatrixOwnerTest::current = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production world pool matrix healthy completed");
    }
}
