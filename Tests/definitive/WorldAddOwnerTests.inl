/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual Runtime world insertion ownership with final originals doubled
 ****************************************************************************/
namespace WorldAddOwnerFixture
{
    namespace AddP = NativeWorldAddProfile;
    namespace AddW = NativeWorldIplFixture;
    using Kind = NativeWorldAddEntryCalls::Kind;
    struct State
    {
        NativeWorldPoolMatrixFixture::State entities;
        AddW::Pool                          singles{static_cast<std::int32_t>(AddP::SingleCapacity), AddP::SingleBytes};
        AddW::Pool                          doubles{static_cast<std::int32_t>(AddP::DoubleCapacity), AddP::DoubleBytes};
        std::uintptr_t                      image = 0;
        AddW::Writer                        writer = nullptr;
        void*                               host = nullptr;
        Kind                                kind = Kind::Building;
        bool                                lod = false;
        State(std::uintptr_t selectedImage, AddW::Writer selectedWriter, void* selectedHost)
            : entities(selectedImage, selectedWriter, selectedHost), image(selectedImage), writer(selectedWriter), host(selectedHost)
        {
            Setup(Kind::Building, false, false);
        }
        bool           Ready() const noexcept { return entities.Ready() && singles.Ready() && doubles.Ready(); }
        std::uintptr_t Entity() const noexcept { return kind == Kind::Building ? entities.buildings.Row(0) : entities.dummies.Row(0); }
        AddW::Pool&    Nodes() noexcept { return kind == Kind::Dummy && !lod ? doubles : singles; }
        std::uintptr_t Head(unsigned x, unsigned y) const noexcept
        {
            const auto offset = std::size_t{y} * (lod ? AddP::LodSide : AddP::SectorSide) + x;
            return image + (lod ? AddP::LodRva + offset * 8 : AddP::SectorsRva + offset * 16 + (kind == Kind::Dummy ? 8 : 0));
        }
        template <class T>
        static T Read(std::uintptr_t address, std::size_t offset = 0) noexcept
        {
            T value{};
            std::memcpy(&value, reinterpret_cast<void*>(address + offset), sizeof(value));
            return value;
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        void Setup(Kind selectedKind, bool selectedLod, bool matrix)
        {
            if (!Ready())
                return;
            kind = selectedKind;
            lod = selectedLod;
            entities.Reset();
            singles.Reset();
            doubles.Reset();
            NativeWorldBoundRectFixture::Install(image, writer, host);
            for (const auto& pin : AddP::Code)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& scalar : AddP::Scalars)
                Global(scalar.rva, scalar.bits, PAGE_READONLY);
            for (const auto& slot : AddP::Slots)
                Global(slot.rva, image + slot.target, PAGE_READONLY);
            Global(AddP::SingleRootRva, singles.Address());
            Global(AddP::DoubleRootRva, doubles.Address());
            const std::vector<unsigned char> sectors(AddP::SectorSide * AddP::SectorSide * 16), lodHeads(AddP::LodSide * AddP::LodSide * 8);
            writer(host, AddP::SectorsRva, sectors.data(), sectors.size(), PAGE_READWRITE);
            writer(host, AddP::LodRva, lodHeads.data(), lodHeads.size(), PAGE_READWRITE);
            AddW::Put(Entity(), 0x30, lod ? AddP::LodFlag : std::uint32_t{0});
            if (matrix)
                entities.Move(0, NativeWorldPoolMatrixMemory::List::Static, Entity());
            auto&      pool = Nodes();
            const auto capacity = static_cast<std::size_t>(pool.capacity);
            std::memset(pool.flags, 0, capacity);
            pool.flags[capacity - 1] = 0x85;
            pool.flags[0] = 0xff;
            pool.flags[7] = 0x80;
            pool.flags[9] = 0x81;
            AddW::Put(pool.Address(), 0x14, pool.capacity - 2);
            AddW::Put(pool.Address(), 0x1f, static_cast<unsigned char>(0xa5));
            // Four real existing heads exercise both neighbours and the native
            // cursor's wrap/holes. They refer to the other occupied typed row;
            // this is not a claim of a complete world-membership registry.
            const auto other = kind == Kind::Building ? entities.dummies.Row(0) : entities.buildings.Row(0);
            unsigned   at = 0;
            for (unsigned y = lod ? 14u : 59u; y <= (lod ? 15u : 60u); ++y)
                for (unsigned x = lod ? 14u : 59u; x <= (lod ? 15u : 60u); ++x)
                {
                    const auto old = pool.Row(30 + at++);
                    AddW::Put(old, 0, other);
                    AddW::Put(old, 8, std::uintptr_t{0});
                    if (&pool == &doubles)
                        AddW::Put(old, 0x10, std::uintptr_t{0});
                    AddW::Put(Head(x, y), 0, old);
                }
        }
    };
}
namespace ClockWorldAddOwnerTest
{
    mta::game_de::ClockResult ReadBound(mta::game_de::NativeWorldBoundRectCalls::Entry original, std::uintptr_t entity,
                                        mta::game_de::NativeWorldBoundRectCalls::Rect& output, mta::game_de::NativeWorldBoundRectCalls::Rect*& returned,
                                        std::uint32_t& exception) noexcept
    {
        using namespace mta::game_de;
        exception = 0;
        if (!current || image != current->image || entity != current->Entity() ||
            reinterpret_cast<std::uintptr_t>(original) != image + NativeWorldBoundRectProfile::EntryRva ||
            (!current->Read<std::uintptr_t>(entity, 0x18) && current->Read<std::uint32_t>(entity, 0x14)))
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        ++bounds;
        // Only the final native read body is doubled. The real current model,
        // default box, inline +0 or matrix ownership and raw seals stay active.
        std::array<float, 6> box{};
        std::memcpy(box.data(), reinterpret_cast<void*>(image + NativeWorldBoundRectProfile::DefaultBoxRva), sizeof(box));
        const auto matrix = current->Read<std::uintptr_t>(entity, 0x18);
        output = {1000000, -1000000, -1000000, 1000000};
        const auto corner = [&](float x, float y, float z)
        {
            float px = 0, py = 0;
            if (matrix)
            {
                const float firstX = y * current->Read<float>(matrix, 0x10) + x * current->Read<float>(matrix);
                const float secondX = z * current->Read<float>(matrix, 0x20) + current->Read<float>(matrix, 0x30);
                const float firstY = x * current->Read<float>(matrix, 4) + y * current->Read<float>(matrix, 0x14);
                const float secondY = z * current->Read<float>(matrix, 0x24) + current->Read<float>(matrix, 0x34);
                px = firstX + secondX;
                py = firstY + secondY;
            }
            else
            {
                px = x + current->Read<float>(entity, 8);
                py = y + current->Read<float>(entity, 0xc);
            }
            output[0] = (std::min)(output[0], px);
            output[2] = (std::max)(output[2], px);
            output[3] = (std::min)(output[3], py);
            output[1] = (std::max)(output[1], py);
        };
        corner(box[0], box[1], box[2]);
        corner(box[3], box[4], box[5]);
        corner(box[3], box[1], box[2]);
        corner(box[0], box[4], box[5]);
        returned = &output;
        return ClockResult::Ok;
    }
    mta::game_de::ClockResult Invoke(mta::game_de::NativeWorldAddEntryCalls::Entry original, std::uintptr_t entity, std::uint32_t& exception) noexcept
    {
        using namespace mta::game_de;
        exception = 0;
        if (!current || image != current->image || entity != current->Entity() ||
            reinterpret_cast<std::uintptr_t>(original) != image + NativeWorldAddProfile::WorldAddRva || current->Read<std::uintptr_t>(entity, 0x20))
        {
            invalidInvocation = true;
            return ClockResult::InvalidArgument;
        }
        ++adds;
        NativeWorldBoundRectCalls::Rect  rectangle{};
        NativeWorldBoundRectCalls::Rect* returned = nullptr;
        const auto read = ReadBound(reinterpret_cast<NativeWorldBoundRectCalls::Entry>(image + NativeWorldBoundRectProfile::EntryRva), entity, rectangle,
                                    returned, exception);
        if (read != ClockResult::Ok || returned != &rectangle)
            return ClockResult::ReadFailure;
        // This independent whole-original double includes its nested getter.
        // It never calls production Plan/Expected to manufacture expected bytes.
        auto&      pool = current->Nodes();
        const bool lod = current->lod;
        const auto cell = [&](float coordinate)
        {
            coordinate = (std::max)(-3000.0f, (std::min)(2999.0f, coordinate));
            const float shifted = coordinate * (lod ? 0.005f : 0.02f) + (lod ? 15.0f : 60.0f);
            return lod ? static_cast<std::int32_t>(shifted) : static_cast<std::int32_t>(std::floor(shifted));
        };
        const auto                         left = cell(rectangle[0]), right = cell(rectangle[2]), bottom = cell(rectangle[3]), top = cell(rectangle[1]);
        const std::array<unsigned char, 4> cache{static_cast<unsigned char>(left), static_cast<unsigned char>(right), static_cast<unsigned char>(bottom),
                                                 static_cast<unsigned char>(top)};
        for (std::size_t at = 0; at != cache.size(); ++at)
            WorldAddOwnerFixture::AddW::Put(entity, 0x3c + at, cache[at]);
        auto cursor = current->Read<std::int32_t>(pool.Address(), 0x14);
        for (auto y = bottom; y <= top; ++y)
            for (auto x = left; x <= right; ++x)
            {
                std::int32_t searched = 0;
                do
                {
                    if (++cursor == pool.capacity)
                        cursor = 0;
                    if (++searched > pool.capacity)
                    {
                        invalidInvocation = true;
                        return ClockResult::NotInitialized;
                    }
                } while (!(pool.flags[cursor] & 0x80));
                WorldAddOwnerFixture::AddW::Put(pool.Address(), 0x14, cursor);
                pool.flags[cursor] = static_cast<unsigned char>(((pool.flags[cursor] & 0x7f) + 1) & 0x7f);
                const auto node = pool.Row(static_cast<std::size_t>(cursor));
                const auto head = current->Head(static_cast<unsigned>(x), static_cast<unsigned>(y));
                const auto old = current->Read<std::uintptr_t>(head);
                WorldAddOwnerFixture::AddW::Put(node, 0, entity);
                WorldAddOwnerFixture::AddW::Put(node, 8, old);
                if (&pool == &current->doubles)
                {
                    WorldAddOwnerFixture::AddW::Put(node, 0x10, std::uintptr_t{0});
                    if (old)
                        WorldAddOwnerFixture::AddW::Put(old, 0x10, node);
                }
                WorldAddOwnerFixture::AddW::Put(head, 0, node);
            }
        if (failAfterEffect)
        {
            exception = 0xe05d0001;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
}
namespace WorldAddOwnerFixture
{
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite healthy/refused/unknown WorldAdd Runtime lifetime is selected");
        const bool refused = scenario == 1, unknown = scenario == 2;
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, bytes, size, protection); };
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
        AddW::State                          world(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        State                                addition(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        Check(world.Ready() && addition.Ready(), "actual typed entities, metadata, static box and fixed Single/Double insertion pools are owned");
        std::vector<unsigned char> providerRows(20 * NativeStreamingRequestProfile::ProviderRowStride);
        NativeWorldBoundRectFixture::PublishProvider(reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()), graph.Game(),
                                                     reinterpret_cast<std::uintptr_t>(providerRows.data()), 20);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockWorldAddOwnerTest::current = &addition;
        ClockWorldAddOwnerTest::image = fixture.image;
        ClockWorldAddOwnerTest::adds = ClockWorldAddOwnerTest::bounds = 0;
        ClockWorldAddOwnerTest::failAfterEffect = ClockWorldAddOwnerTest::invalidInvocation = false;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 state, rejected;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "WorldAdd scope preserves shipping executable refusal before explicit owned-image admission");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "WorldAdd work uses the real post-application Runtime owner");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "WorldAdd input and originals execute on the distinct genuinely admitted engine thread");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one WorldAdd action consumes one current engine callback");
        };
        Check(runtime.access.AddWorldEntity(Kind::Building, 0) != ClockResult::Ok && !runtime.worldAdds.Pending() && !ClockWorldAddOwnerTest::adds &&
                  !ClockWorldAddOwnerTest::bounds,
              "producer-thread access cannot borrow geometry or enter either native original");
        const auto raw = [](std::uintptr_t address, std::size_t bytes)
        {
            std::vector<unsigned char> result(bytes);
            std::memcpy(result.data(), reinterpret_cast<void*>(address), bytes);
            return result;
        };
        const auto snapshot = [&]
        {
            auto&      pool = addition.Nodes();
            auto       bytes = raw(pool.Address(), pool.header.size());
            const auto append = [&](std::uintptr_t address, std::size_t count)
            {
                const auto part = raw(address, count);
                bytes.insert(bytes.end(), part.begin(), part.end());
            };
            append(pool.Row(0), static_cast<std::size_t>(pool.capacity) * pool.stride);
            append(reinterpret_cast<std::uintptr_t>(pool.flags), static_cast<std::size_t>(pool.capacity));
            append(fixture.image + AddP::SectorsRva, AddP::SectorSide * AddP::SectorSide * 16);
            append(fixture.image + AddP::LodRva, AddP::LodSide * AddP::LodSide * 8);
            append(addition.Entity(), addition.kind == Kind::Building ? addition.entities.buildings.stride : addition.entities.dummies.stride);
            return bytes;
        };
        if (!refused && !unknown)
        {
            for (unsigned kindCase = 0; kindCase != 3; ++kindCase)
                pulse(
                    [&](NativeGameTimingAccess& owner)
                    {
                        addition.Setup(kindCase ? Kind::Dummy : Kind::Building, kindCase == 2, kindCase == 2);
                        auto&      pool = addition.Nodes();
                        const auto oldAdds = ClockWorldAddOwnerTest::adds, oldBounds = ClockWorldAddOwnerTest::bounds;
                        const auto entityBytes =
                            raw(addition.Entity(), addition.kind == Kind::Building ? addition.entities.buildings.stride : addition.entities.dummies.stride);
                        Check(owner.CheckWorldEntityAddReady(addition.kind, 0) == ClockResult::Ok && !runtime.worldAdds.Pending() &&
                                  ClockWorldAddOwnerTest::adds == oldAdds && ClockWorldAddOwnerTest::bounds == oldBounds + 1,
                              "actual current typed/default-box/matrix and fixed-pool readiness performs only a read and grants no future native lease");
                        const auto before = snapshot();
                        Check(owner.AddWorldEntity(addition.kind, 0) == ClockResult::Ok && ClockWorldAddOwnerTest::adds == oldAdds + 1 &&
                                  ClockWorldAddOwnerTest::bounds == oldBounds + 3 && runtime.worldAdds.CanDiscard() && !runtime.worldAdds.Uncertain(),
                              "actual Runtime journals insertion, calls genuine preflight and whole-entry doubles, and clears only after exact resource ACK");
                        const auto capacity = static_cast<std::size_t>(pool.capacity);
                        const auto first = pool.Row(capacity - 1);
                        Check(pool.flags[capacity - 1] == 6 && pool.flags[0] == 0 && pool.flags[7] == 1 && pool.flags[9] == 2 &&
                                  addition.Read<std::int32_t>(pool.Address(), 0x14) == 9 && addition.Read<unsigned char>(pool.Address(), 0x1f) == 0xa5 &&
                                  addition.Read<std::uintptr_t>(first) == addition.Entity() && addition.Read<std::uintptr_t>(first, 8) == pool.Row(30) &&
                                  addition.Read<std::uintptr_t>(pool.Row(9), 8) == pool.Row(33) && snapshot() != before,
                              "Building Single, Dummy Double and Dummy LOD Single consume exact wrap/holes and preserve opaque cursor padding");
                        const unsigned low = addition.lod ? 14u : 59u, high = low + 1;
                        Check(addition.Read<std::uintptr_t>(addition.Head(low, low)) == first &&
                                  addition.Read<std::uintptr_t>(addition.Head(high, high)) == pool.Row(9) &&
                                  (&pool != &addition.doubles || addition.Read<std::uintptr_t>(pool.Row(33), 0x10) == pool.Row(9)),
                              "all four native cells publish selected nodes and Double old-head predecessors point back exactly");
                        auto                               expectedEntity = entityBytes;
                        const std::array<unsigned char, 4> cache{static_cast<unsigned char>(low), static_cast<unsigned char>(high),
                                                                 static_cast<unsigned char>(low), static_cast<unsigned char>(high)};
                        std::copy(cache.begin(), cache.end(), expectedEntity.begin() + 0x3c);
                        Check(raw(addition.Entity(), expectedEntity.size()) == expectedEntity,
                              "native insertion changes only the four cached entity ranges and preserves typed row, actor, model and transform bytes");
                    });
        }
        else if (refused)
        {
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    // The genuine read can complete before an exhausted node
                    // budget refuses. This still has no engine-write debt.
                    auto& pool = addition.Nodes();
                    std::memset(pool.flags, 0, static_cast<std::size_t>(pool.capacity));
                    const auto bytes = snapshot();
                    Check(owner.AddWorldEntity(addition.kind, 0) == ClockResult::NotInitialized && !runtime.worldAdds.Pending() &&
                              !runtime.worldAdds.Uncertain() && !ClockWorldAddOwnerTest::adds && ClockWorldAddOwnerTest::bounds == 1 && snapshot() == bytes,
                          "successful genuine read plus exhausted native fixed-pool budget refuses before any WorldAdd write and clears CPU intent");
                },
                false);
        }
        else
        {
            ClockWorldAddOwnerTest::failAfterEffect = true;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    Check(owner.AddWorldEntity(addition.kind, 0) != ClockResult::Ok && runtime.worldAdds.Pending() && runtime.worldAdds.Uncertain() &&
                              ClockWorldAddOwnerTest::adds == 1 && ClockWorldAddOwnerTest::bounds == 2 && addition.singles.flags[9] == 2 &&
                              addition.Read<std::int32_t>(addition.singles.Address(), 0x14) == 9,
                          "entered whole-original failure after a real four-node insertion prefix retains actual Runtime intent as unknown");
                },
                false);
            const auto bytes = snapshot();
            const auto originalCount = ClockWorldAddOwnerTest::adds, boundCount = ClockWorldAddOwnerTest::bounds;
            Check(!service.End(generation), "unknown WorldAdd domain refuses actual owner End");
            engine.Pulse(service, generation, true);
            NativeClockService  next;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool beganAgain = next.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !beganAgain && runtime.worldAdds.Pending() && runtime.worldAdds.Uncertain() &&
                      ClockWorldAddOwnerTest::adds == originalCount && ClockWorldAddOwnerTest::bounds == boundCount && snapshot() == bytes,
                  "stopped cleanup and new Begin retain unknown insertion without replay, rollback, grid/node writes or renewed native borrows");
            engine.Stop();
            ClockWorldAddOwnerTest::current = nullptr;
            ClockReaderTestHost::image = previousImage;
            std::puts("Production world add unknown completed");
            return;
        }
        Check(!ClockWorldAddOwnerTest::invalidInvocation, "every called native original was the explicit correctly typed final body double");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.worldAdds.CanDiscard(), "healthy ACK or untouched preentry refusal permits actual owner End");
        NativeClockService  next;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = next.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation, "actual acknowledged/refused WorldAdd lifetime permits a distinct owner generation");
        engine.Pulse(next, nextState.generation);
        if (!next.End(nextState.generation))
            engine.Pulse(next, nextState.generation, true);
        Check(next.End(nextState.generation), "next no-effect WorldAdd generation retires normally");
        engine.Stop();
        ClockWorldAddOwnerTest::current = nullptr;
        ClockReaderTestHost::image = previousImage;
        writes.Restore();
        std::puts(refused ? "Production world add refused completed" : "Production world add healthy completed");
    }
}
