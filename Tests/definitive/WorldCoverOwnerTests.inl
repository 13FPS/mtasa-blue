/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual cover Runtime/resource ACKs with final original-body doubles
 ****************************************************************************/
namespace ClockWorldCoverOwnerTest
{
    mta::game_de::ClockResult Invoke(mta::game_de::NativeWorldCoverCalls::Operation operation, std::uintptr_t image, std::uintptr_t entity,
                                     mta::game_de::NativeWorldCoverCalls::Receipt& receipt) noexcept
    {
        using namespace mta::game_de;
        if (!current || current->image != image)
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        if (operation == NativeWorldCoverCalls::Operation::RemoveForEntity)
        {
            ++removes;
            if (!current->Remove(entity))
            {
                invalidInvocation = true;
                return ClockResult::InvalidArgument;
            }
            if (failAfterEffect)
            {
                receipt.exception = 0xe0560001;
                return ClockResult::OriginalException;
            }
        }
        else
        {
            ++clears;
            if (!current->Clear())
            {
                invalidInvocation = true;
                return ClockResult::InvalidArgument;
            }
        }
        return ClockResult::Ok;
    }
    // This explicit checkpoint wrapper still calls the genuine current Runtime
    // owner. Only the third check after entry refuses: the first two belong to
    // the completed raw post-ACK sample, the third is the next pre-entry fence.
    // It schedules known refusal without doubling a native write or image read.
    mta::game_de::ClockResult OwnerCheckpoint() noexcept
    {
        const auto result = mta::game_de::CheckTimingOwner();
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        if (knownRefusal && removes == 1 && ++checksAfterFirst == 3)
            return mta::game_de::ClockResult::ReadFailure;
        return result;
    }
}
namespace WorldCoverOwnerFixture
{
    namespace CoverTestP = NativeWorldCoverProfile;
    namespace CoverTestW = NativeWorldIplFixture;
    class Game final : public lifecycle_fixture::NoPresentationGameBase<CGameModelDummiesResetDE>
    {
        using Base = lifecycle_fixture::NoPresentationGameBase<CGameModelDummiesResetDE>;

    public:
        Game(const NativeGameTimingReader& t, CClockDE& c, NativeGameTimingAccess& o, const NativeStreamingFileIdsReader& s, const NativeGameStateReader& g)
            : Base(t, c, o, s, g)
        {
        }
        void RemoveCoversForFixture() { this->RemoveWorldCovers(); }

    protected:
        void RequireResetReady() override { Refuse(); }
        void RestoreDefaultObjectGroupProperties() override { Refuse(); }
        void ResetModelTextureDictionaries() override { Refuse(); }
        void RequireWorldLifecycleReady() override { Refuse(); }
        void RemoveWorldPlants() override { Refuse(); }
        void DestroyWorldStencilShadows() override { Refuse(); }
        void DropWorldBuildingCaches() override { Refuse(); }

    private:
        [[noreturn]] static void Refuse() { throw std::runtime_error("unqualified complete Cover world dependency"); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_abstract_v<CGameWorldCoverDE>);
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite cover owner lifetime is selected");
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
        CoverTestW::State                    world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeWorldCoverFixture::State       covers(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready() && covers.Ready(), "owned cover table, static reference bank and typed selected native pools are allocated");
        world.Reset();
        covers.Reset();
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockWorldCoverOwnerTest::current = &covers;
        ClockWorldCoverOwnerTest::removes = ClockWorldCoverOwnerTest::clears = ClockWorldCoverOwnerTest::checksAfterFirst = 0;
        ClockWorldCoverOwnerTest::failAfterEffect = ClockWorldCoverOwnerTest::invalidInvocation = ClockWorldCoverOwnerTest::knownRefusal = false;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "cover controller preserves the shipping executable refusal before explicit owned-image test admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "cover requests use the real current post-application Runtime");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "cover work runs on the actual distinct admitted native engine thread");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one cover action consumes one qualified engine callback");
        };

        NativeGameTimingReader       timing;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        nativeState;
        auto                         game = std::make_unique<Game>(timing, runtime.clock, runtime.access, streaming, nativeState);
        Check(runtime.access.CheckWorldCoverCleanupReady() != ClockResult::Ok && !runtime.worldCovers.Pending(),
              "producer cannot borrow Cover cleanup authority or create intent");
        const auto raw = [](std::uintptr_t address, std::size_t bytes)
        {
            std::vector<unsigned char> result(bytes);
            std::memcpy(result.data(), reinterpret_cast<void*>(address), bytes);
            return result;
        };
        const auto unaffectedPoint = raw(covers.Point(4), CoverTestP::PointBytes), inactivePoint = raw(covers.Point(5), CoverTestP::PointBytes),
                   unaffectedRef = raw(covers.Reference(11), CoverTestP::RefBytes), unaffectedNode = raw(covers.Node(20), CoverTestP::DoublePoolStride);
        const auto assertClean = [&]
        {
            Check(NativeWorldCoverFixture::State::Read<std::uint32_t>(fixture.image + CoverTestP::CountRva) == 1 &&
                      NativeWorldCoverFixture::State::Read<std::uintptr_t>(fixture.image + CoverTestP::ListRootRva) == 0,
                  "only listed owner points retire; unrelated active cover remains counted");
            Check(NativeWorldCoverFixture::State::Read<std::uintptr_t>(fixture.image + CoverTestP::FreeRefRootRva) == covers.Reference(7) &&
                      NativeWorldCoverFixture::State::Read<std::uintptr_t>(covers.Reference(7)) == covers.Reference(9) &&
                      NativeWorldCoverFixture::State::Read<std::uintptr_t>(covers.Reference(9)) == covers.Reference(5) &&
                      NativeWorldCoverFixture::State::Read<std::uintptr_t>(covers.Reference(5)) == covers.Reference(0),
                  "actual static references retire in the genuine point/list traversal order");
            Check(NativeWorldCoverFixture::State::Read<std::uintptr_t>(covers.entities.buildings.Row(0), CoverTestP::EntityRefsOffset) == covers.Reference(6) &&
                      covers.nodes.flags[7] == 0x83 && covers.nodes.flags[2] == 0x84 && covers.nodes.flags[15] == 0x85 &&
                      NativeWorldCoverFixture::State::Read<std::int32_t>(covers.nodes.Address(), CoverTestP::PoolCursorOffset) == 2,
                  "unrelated reference survives while actual native pool generations/cursor retire");
            Check(raw(covers.Point(4), CoverTestP::PointBytes) == unaffectedPoint && raw(covers.Point(5), CoverTestP::PointBytes) == inactivePoint &&
                      raw(covers.Reference(11), CoverTestP::RefBytes) == unaffectedRef && raw(covers.Node(20), CoverTestP::DoublePoolStride) == unaffectedNode,
                  "unlisted active point/ref/node and inactive stale-owner payload survive exactly");
        };
        if (!retained && !unknown)
        {
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    Check(owner.CheckWorldCoverCleanupReady() == ClockResult::Ok,
                          "actual typed Cover/list/reference graph is ready under the current engine owner");
                    const auto saved = NativeWorldCoverFixture::State::Read<std::uintptr_t>(covers.Node(7));
                    CoverTestW::Put(covers.Node(7), CoverTestP::NodeItemOffset, std::uintptr_t{0x1234});
                    Check(owner.RemoveAllWorldCovers() != ClockResult::Ok && !runtime.worldCovers.Pending() && ClockWorldCoverOwnerTest::removes == 0 &&
                              ClockWorldCoverOwnerTest::clears == 0,
                          "unsupported listed owner refuses the entire itinerary before effects");
                    CoverTestW::Put(covers.Node(7), CoverTestP::NodeItemOffset, saved);
                    game->RemoveCoversForFixture();
                    Check(ClockWorldCoverOwnerTest::removes == 3 && ClockWorldCoverOwnerTest::clears == 1 && runtime.worldCovers.CanDiscard(),
                          "actual inherited CGame Cover hook reaches genuine Runtime domain and acknowledges exactly three originals plus list clear");
                    assertClean();
                    covers.Reset();
                    ClockWorldCoverOwnerTest::removes = ClockWorldCoverOwnerTest::clears = 0;
                });
        }
        if (unknown)
        {
            ClockWorldCoverOwnerTest::failAfterEffect = true;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    Check(owner.RemoveAllWorldCovers() != ClockResult::Ok && runtime.worldCovers.Pending() && runtime.worldCovers.Uncertain() &&
                              ClockWorldCoverOwnerTest::removes == 1 && !ClockWorldCoverOwnerTest::clears,
                          "entered original failure retains actual point/reference prefix as unknown");
                },
                false);
        }
        else
        {
            ClockWorldCoverOwnerTest::knownRefusal = true;
            pulse(
                [&](NativeGameTimingAccess&)
                {
                    const auto result = runtime.worldCovers.RemoveAll(runtime.image, runtime.generation.load(), &ClockWorldCoverOwnerTest::OwnerCheckpoint,
                                                                      &CheckApplicationTimingOwner);
                    Check(result == ClockResult::ReadFailure && runtime.worldCovers.Pending() && !runtime.worldCovers.Uncertain() &&
                              ClockWorldCoverOwnerTest::removes == 1 && ClockWorldCoverOwnerTest::clears == 0,
                          "controlled genuine owner checkpoint refuses the second pre-entry fence after first native resource ACK");
                });
            ClockWorldCoverOwnerTest::knownRefusal = false;
            if (!retained)
                pulse(
                    [&](NativeGameTimingAccess& owner)
                    {
                        Check(owner.RemoveAllWorldCovers() == ClockResult::Ok && ClockWorldCoverOwnerTest::removes == 3 &&
                                  ClockWorldCoverOwnerTest::clears == 1 && runtime.worldCovers.CanDiscard(),
                              "explicit retry validates exact current resources and resumes suffix without replaying first owner");
                        assertClean();
                    });
        }
        if (retained || unknown)
        {
            const auto points = raw(fixture.image + CoverTestP::TableRva, CoverTestP::PointCapacity * CoverTestP::PointBytes);
            const auto refs = raw(fixture.image + CoverTestP::RefBankRva, CoverTestP::RefCapacity * CoverTestP::RefBytes);
            Check(!service.End(generation), "accepted known/unknown Cover prefix refuses actual End");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.worldCovers.Pending() && ClockWorldCoverOwnerTest::removes == 1 &&
                      !ClockWorldCoverOwnerTest::clears && raw(fixture.image + CoverTestP::TableRva, points.size()) == points &&
                      raw(fixture.image + CoverTestP::RefBankRva, refs.size()) == refs,
                  "stopped cleanup/new Begin retain exact Cover prefix without replay or unrelated point clearing");
            engine.Stop();
            game.reset();
            ClockWorldCoverOwnerTest::current = nullptr;
            ClockReaderTestHost::image = previousImage;
            std::puts(unknown ? "Production world cover unknown completed" : "Production world cover retained completed");
            return;
        }
        Check(!ClockWorldCoverOwnerTest::invalidInvocation, "all declared original calls were the explicit native-body doubles");
        game.reset();
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.worldCovers.CanDiscard(), "complete Cover resource ACK allows actual owner retirement");
        NativeClockService  next;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = next.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation, "acknowledged Cover cleanup permits a distinct next owner generation");
        engine.Pulse(next, nextState.generation);
        if (!next.End(nextState.generation))
            engine.Pulse(next, nextState.generation, true);
        Check(next.End(nextState.generation), "next no-effect Cover owner retires cleanly");
        engine.Stop();
        ClockWorldCoverOwnerTest::current = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts("Production world cover healthy completed");
    }
}
