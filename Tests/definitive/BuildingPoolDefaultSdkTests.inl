/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep Reset's original capacity lookup separate from its live pool
 ****************************************************************************/
class DefaultOwner final : public Owner
{
public:
    std::uint32_t value = 14000;
    ClockResult   result = ClockResult::Ok;
    unsigned      reads = 0;
    bool          afterLiveSize = false;
    Trace*        trace = nullptr;
    ClockResult   ReadDefaultBuildingPoolCapacity(std::uint32_t& output) noexcept override
    {
        ++reads;
        if (trace)
        {
            afterLiveSize = !trace->events.empty() && trace->events.back() == 110;
            // This noexcept transport reports a result. It must not call
            // the intentionally throwing policy-event spy.
            trace->events.push_back(111);
        }
        if (result == ClockResult::Ok)
            output = value;
        return result;
    }
};
using DefaultBase = lifecycle_fixture::NoPresentationGameBase<CGameBuildingPoolDefaultDE>;
class DefaultGame final : public DefaultBase
{
    Trace& trace;

public:
    using ConstructorServices = CGameDE::ConstructorServices;
    Fire      fire;
    Camera    camera;
    Hud       hud;
    Buildings buildings;
    Dummies   dummies;
    Pools     pools;
    template <class... Prepared>
    DefaultGame(const NativeGameTimingReader& reader, CClockDE& clock, Owner& owner, const NativeStreamingFileIdsReader& streaming,
                const NativeGameStateReader& state, Trace& t, Prepared&&... prepared)
        : DefaultBase(reader, clock, owner, streaming, state, std::forward<Prepared>(prepared)...),
          trace(t),
          fire(t),
          camera(t),
          hud(t),
          buildings(t),
          dummies(t),
          pools(buildings, dummies)
    {
    }
    SystemState::Enum GetSystemState() override { return trace.playing ? SystemState::GS_PLAYING_GAME : SystemState::GS_INIT_ONCE; }
    CFireManager*     GetFireManager() override { return &fire; }
    CCamera*          GetCamera() override { return &camera; }
    CHud*             GetHud() override { return &hud; }
    CPools*           GetPools() const noexcept override { return const_cast<Pools*>(&pools); }
    void              Pause(bool paused) override
    {
        if (paused)
            throw Failure(-94);
        trace.Call(4);
    }
    int                     DefaultCapacity() const { return this->GetResetBuildingPoolCapacity(); }
    NativeGameTimingAccess* Writer() noexcept { return &this->NativeWrites(); }
    NativeGameTimingAccess* Writer() const noexcept { return &this->NativeWrites(); }
    bool                    Removed() const noexcept { return this->IsGameWorldRemoved(); }
    TaskSimpleBeHitHandler* Hit() const noexcept { return this->GetTaskSimpleBeHitHandler(); }

protected:
    // Explicit test-policy spies are not production native hooks. The
    // generated omission probes separately prove every hook remains pure.
    void RequireResetReady() override
    {
        ++trace.resetPreflights;
        if (!trace.ready)
            throw Failure(-1);
    }
    void ResetVehicleModelDummies() override { trace.Call(7); }
    void RestoreObjectModelGroups() override { trace.Call(8); }
    void RestoreDefaultObjectGroupProperties() override { trace.Call(9); }
    void ResetVehicleModelWheelSizes() override { trace.Call(10); }
    void ResetModelTextureDictionaries() override { trace.Call(11); }
    void RequireWorldLifecycleReady() override
    {
        ++trace.worldPreflights;
        if (!trace.ready)
            throw Failure(-1);
    }
    void DisableWorldIplStreaming() override { trace.Call(100); }
    void RemoveWorldCovers() override { trace.Call(101); }
    void RemoveWorldPlants() override { trace.Call(102); }
    void DestroyWorldStencilShadows() override { trace.Call(103); }
    void DropWorldBuildingCaches() override { trace.Call(106); }
    void EnableWorldIplStreaming(MtaGame::CGameWorldPolicy::IplFilter filter) override
    {
        if (!filter || filter({'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'}) || !filter({}))
            throw Failure(-95);
        trace.Call(109);
    }
};

static_assert(std::is_base_of_v<CGameBuildingPoolDefaultDE, DefaultGame> && std::is_convertible_v<DefaultGame*, CGame*> && !std::is_abstract_v<DefaultGame>);
template <class Call>
void RefusesCapacity(Call&& call, ClockResult expected)
{
    bool refused = false;
    try
    {
        call();
    }
    catch (const std::runtime_error& error)
    {
        const auto        status = std::string("(status ") + std::to_string(static_cast<std::uint32_t>(expected)) + ")";
        const std::string message = error.what();
        refused = message.find("DE CGame reset building-pool default capacity unavailable ") == 0 && message.find(status) != std::string::npos;
    }
    Check(refused, "the real default hook preserves its provider result and operation in the existing explicit refusal");
}
void BuildingDefaults()
{
    // Default-refusing base channels cannot accidentally publish a guessed
    // value. Only the specialised native owner may supply authority.
    Owner         unbound;
    std::uint32_t output = 0xfeedu;
    Check(unbound.ReadDefaultBuildingPoolCapacity(output) == ClockResult::NotInitialized && output == 0xfeedu,
          "the missing capacity provider refuses without changing caller output");
    ContextWithOwner<DefaultOwner> c;
    DefaultGame                    game(c.reader, c.clock, c.owner, c.streaming, c.state, c.trace, c.owner.bundle);
    const DefaultGame&             constant = game;
    Check(game.Writer() == &c.owner && constant.Writer() == &c.owner, "const and nonconst native accessors retain the identical borrowed owner");
    Check(game.GetClock() == &c.clock && c.owner.validations == 1 && c.owner.captures == 1 && !c.owner.reads,
          "the default dependency shares the prepared clock and service graph without an eager read");
    for (const auto value : {std::uint32_t{1}, std::uint32_t{14000}, static_cast<std::uint32_t>((std::numeric_limits<int>::max)())})
    {
        c.owner.value = value;
        const auto before = c.owner.reads;
        Check(constant.DefaultCapacity() == static_cast<int>(value), "the actual const hook returns a positive int-representable native default");
        Check(c.owner.reads == before + 1, "each real const getter enters the same owner exactly once");
    }
    for (const auto value : {std::uint32_t{0}, static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) + 1u, (std::numeric_limits<std::uint32_t>::max)()})
    {
        c.owner.value = value;
        const auto before = c.owner.reads;
        RefusesCapacity([&] { constant.DefaultCapacity(); }, ClockResult::InvalidArgument);
        Check(c.owner.reads == before + 1, "zero or unrepresentable defaults refuse after one current lookup without a fallback");
    }
    c.owner.value = 14000;
    for (const auto result : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::OwnerConflict})
    {
        c.owner.result = result;
        const auto before = c.owner.reads;
        RefusesCapacity([&] { constant.DefaultCapacity(); }, result);
        Check(c.owner.reads == before + 1, "provider refusal is preserved without reusing a previously accepted default");
    }
    c.owner.result = ClockResult::Ok;
    const auto before = c.owner.reads;
    Check(constant.DefaultCapacity() == 14000, "a later read uses current authority rather than an earlier accepted transport value");
    Check(c.owner.reads == before + 1, "restored authority is freshly queried without SDK caching");
    std::puts("Actual lifecycle default capacity SDK and const owner fixture completed");
    std::puts("Actual lifecycle default capacity range and refusal transport fixture completed");

    ContextWithOwner<DefaultOwner> same;
    DefaultGame                    sameGame(same.reader, same.clock, same.owner, same.streaming, same.state, same.trace);
    same.owner.trace = &same.trace;
    sameGame.buildings.size = 14000;
    sameGame.Reset();
    Equal(same.trace.events, ResetOrder);
    Check(same.owner.reads == 1 && same.owner.afterLiveSize, "actual Reset reads mutable size before exactly one qualified original default");
    Check(sameGame.buildings.size == 14000 && sameGame.buildings.lastCapacity == -1 && !sameGame.Removed(),
          "equal live and original capacities do not enter resize and retain the restored world");

    ContextWithOwner<DefaultOwner> resized;
    DefaultGame                    resizedGame(resized.reader, resized.clock, resized.owner, resized.streaming, resized.state, resized.trace);
    resized.owner.trace = &resized.trace;
    resizedGame.buildings.size = 13;
    resizedGame.buildings.resizeResult = false;
    resizedGame.Reset();
    auto expected = ResetOrder;
    expected.push_back(110);
    expected.insert(expected.end(), Removal.begin(), Removal.end());
    expected.push_back(112);
    expected.insert(expected.end(), Restoration.begin(), Restoration.end());
    Equal(resized.trace.events, expected);
    Check(resized.owner.reads == 1 && resized.owner.afterLiveSize, "different live capacity does not replace or repeat the original-default lookup");
    Check(resizedGame.buildings.lastCapacity == 14000 && resizedGame.buildings.size == 13 && !resizedGame.Removed(),
          "actual Reset forwards the original default, ignores false resize status and restores the shared world");

    ContextWithOwner<DefaultOwner> missing;
    DefaultGame                    missingGame(missing.reader, missing.clock, missing.owner, missing.streaming, missing.state, missing.trace);
    missing.owner.trace = &missing.trace;
    missing.owner.result = ClockResult::NotInitialized;
    missingGame.buildings.size = 14000;
    RefusesCapacity([&] { missingGame.Reset(); }, ClockResult::NotInitialized);
    Equal(missing.trace.events, ResetOrder);
    Check(missing.owner.reads == 1 && missing.owner.afterLiveSize && missingGame.buildings.lastCapacity == -1 && missingGame.buildings.size == 14000 &&
              !missingGame.Removed(),
          "unavailable default retains the accepted Reset prefix even when live size happens to match the normal original");

    ContextWithOwner<DefaultOwner> invalid;
    DefaultGame                    invalidGame(invalid.reader, invalid.clock, invalid.owner, invalid.streaming, invalid.state, invalid.trace);
    invalid.owner.trace = &invalid.trace;
    invalid.owner.value = 0;
    RefusesCapacity([&] { invalidGame.Reset(); }, ClockResult::InvalidArgument);
    Equal(invalid.trace.events, ResetOrder);
    Check(invalid.owner.reads == 1 && invalid.owner.afterLiveSize && invalidGame.buildings.lastCapacity == -1 && !invalidGame.Removed(),
          "range refusal preserves the original exception prefix without entering resize or synthesising removal");

    ContextWithOwner<DefaultOwner> gated;
    DefaultGame                    gatedGame(gated.reader, gated.clock, gated.owner, gated.streaming, gated.state, gated.trace);
    gated.owner.trace = &gated.trace;
    gated.trace.playing = false;
    gatedGame.Reset();
    Check(!gated.owner.reads && gated.trace.events.empty() && !gated.trace.resetPreflights,
          "inactive Reset still returns before native capacity access or policy readiness");
    gated.trace.playing = true;
    gated.trace.ready = false;
    Fails([&] { gatedGame.Reset(); }, -1);
    Check(!gated.owner.reads && gated.trace.events.empty(), "failed existing Reset readiness grants no default-capacity access");
    std::puts("Actual lifecycle default capacity inherited Reset order fixture completed");
    std::puts("Thirteen remaining default lifecycle native hooks remain abstract");
}
