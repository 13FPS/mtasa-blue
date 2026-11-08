/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise one composed model SDK, reference family and Core reporter.
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoStartupDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <functional>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif
#include <SharedUtil.h>
#include <core/CCoreInterface.h>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace model_startup_fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("Unrelated SDK method reached model startup composition fixture\n", stderr);
        std::abort();
    }
}
#include "ModelStartupCompositionFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    using Token = NativeModelBackingReferenceToken;
    using Phase = NativeModelBackingReferencePhase;
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Model startup composition check %u failed: %s\n", checks, why);
            std::exit(EXIT_FAILURE);
        }
    }
    template <class Operation>
    bool Refuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::exception&)
        {
            return true;
        }
        return false;
    }
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    bool Same(const Token& a, const Token& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial && a.modelIndex == b.modelIndex;
    }
    // These controlled capability replies are not another native controller.
    // The actual inherited SDK policies own counters, pending state, requests,
    // temporary dummy references and diagnostics throughout this test.
    class Owner final : public NativeModelClassificationAccess,
                        public NativeModelBackingReferenceAccess,
                        public NativeModelLodAccess,
                        public NativeModelTimeAccess,
                        public NativeModelWheelAccess,
                        public NativeModelObjectGroupAccess,
                        public NativeModelDummiesAccess
    {
    public:
        Token                     current{7, 11, 400};
        Phase                     phase = Phase::Bound;
        std::uint64_t             family = 11;
        CModelInfo*               model = nullptr;
        unsigned                  binds = 0, claims = 0, reads = 0, acquires = 0, releases = 0, renews = 0, effects = 0;
        unsigned                  wheelSets = 0, groupSets = 0, dummySets = 0, timeSets = 0, lodSets = 0, dummyReads = 0;
        bool                      claimed = false, keysMatch = true, policyPreserved = true, cachedDummy = false;
        int                       cpuAtAcquire = -1, cpuAtRelease = -1, cpuAtDummyRead = -1;
        ClockResult               dummyReadResult = ClockResult::Ok, mutationResult = ClockResult::Ok;
        NativeModelWheelSizes     wheels{1.0f, 2.0f};
        std::uint16_t             group = 8;
        NativeModelDummyPositions positions{};
        std::uint8_t              on = 0, off = 0;
        std::uint32_t             lod = Bits(50.0f);
        void                      Touch(std::uint32_t key) noexcept
        {
            ++effects;
            keysMatch = keysMatch && key == current.modelIndex;
        }
        ClockResult BindModelBackingReference(const Token& token) noexcept override
        {
            ++binds;
            return Same(token, current) ? ClockResult::Ok : ClockResult::OwnerConflict;
        }
        ClockResult ClaimUnmodifiedModelFamily(const Token& token) noexcept override
        {
            ++claims;
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            claimed = true;
            return ClockResult::Ok;
        }
        ClockResult ReadModelBackingReference(const Token& token, NativeModelBackingReferenceState& output) noexcept override
        {
            ++reads;
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            output = {};
            output.reference = current;
            output.phase = phase;
            output.familySerial = family;
            output.unmodifiedFamily = claimed;
            return ClockResult::Ok;
        }
        ClockResult RenewModelBackingReferenceCycle(const Token& token, Token& output) noexcept override
        {
            ++renews;
            if (!Same(token, current) || phase != Phase::Released)
                return ClockResult::OwnerConflict;
            ++current.serial;
            output = current;
            phase = Phase::Bound;
            return ClockResult::Ok;
        }
        ClockResult AcquireModelBackingReference(const Token& token) noexcept override
        {
            ++acquires;
            cpuAtAcquire = model ? model->GetRefCount() : -1;
            if (!Same(token, current) || phase != Phase::Bound)
                return ClockResult::OwnerConflict;
            phase = Phase::Held;
            return ClockResult::Ok;
        }
        ClockResult ReleaseModelBackingReference(const Token& token, NativeModelReferenceRelease::ZeroPolicy policy) noexcept override
        {
            ++releases;
            cpuAtRelease = model ? model->GetRefCount() : -1;
            policyPreserved = policyPreserved && policy == NativeModelReferenceRelease::ZeroPolicy::RemoveAtZero;
            if (!Same(token, current) || phase != Phase::Held)
                return ClockResult::OwnerConflict;
            phase = Phase::Released;
            return ClockResult::Ok;
        }
        ClockResult UnloadUnusedModelFamily(const Token& token, bool& output) noexcept override
        {
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            output = false;
            return ClockResult::Ok;
        }
        ClockResult ReadModelType(std::uint32_t key, NativeModelTypeObservation& output) noexcept override
        {
            Touch(key);
            output = {true, 6};
            return ClockResult::Ok;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t key, bool& output) noexcept override
        {
            // The real vehicle predicate admits archive membership before its
            // type query. Supply that separate controlled observation here.
            Touch(key);
            output = true;
            return ClockResult::Ok;
        }
        ClockResult ReadModelInterface(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            Touch(key);
            output = 1;
            return ClockResult::Ok;
        }
        ClockResult ReadModelRender(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            Touch(key);
            output = 1;
            return ClockResult::Ok;
        }
        ClockResult ReadModelAnimFileIndex(std::uint32_t key, std::uint32_t& output) noexcept override
        {
            Touch(key);
            output = 42;
            return ClockResult::Ok;
        }
        ClockResult ReadModelWheelSizes(std::uint32_t key, NativeModelWheelSizes& output) noexcept override
        {
            Touch(key);
            output = wheels;
            return ClockResult::Ok;
        }
        ClockResult SetModelWheelSize(std::uint32_t key, NativeModelWheelGroup selected, float value) noexcept override
        {
            Touch(key);
            ++wheelSets;
            if (mutationResult != ClockResult::Ok)
                return mutationResult;
            if (selected == NativeModelWheelGroup::Front || selected == NativeModelWheelGroup::All)
                wheels.front = value;
            if (selected == NativeModelWheelGroup::Rear || selected == NativeModelWheelGroup::All)
                wheels.rear = value;
            return ClockResult::Ok;
        }
        ClockResult ResetModelWheelSizes(std::uint32_t key, const NativeModelWheelSizes* defaults) noexcept override
        {
            Touch(key);
            wheels = defaults ? *defaults : NativeModelWheelSizes{1.0f, 2.0f};
            return ClockResult::Ok;
        }
        ClockResult ReadModelObjectPropertiesGroup(std::uint32_t key, std::uint16_t& output) noexcept override
        {
            Touch(key);
            output = group;
            return ClockResult::Ok;
        }
        ClockResult SetModelObjectPropertiesGroup(std::uint32_t key, std::uint16_t value) noexcept override
        {
            Touch(key);
            ++groupSets;
            group = value;
            return ClockResult::Ok;
        }
        ClockResult RestoreModelObjectPropertiesGroup(std::uint32_t key) noexcept override
        {
            Touch(key);
            group = 8;
            return ClockResult::Ok;
        }
        ClockResult ReadCachedModelVehicleDummyDefault(std::uint32_t key, std::uint32_t index, NativeModelDummyVector& output, bool& found) noexcept override
        {
            Touch(key);
            found = cachedDummy;
            output = positions[index];
            return ClockResult::Ok;
        }
        ClockResult ReadModelVehicleDummyPositions(std::uint32_t key, NativeModelDummyPositions& output) noexcept override
        {
            Touch(key);
            ++dummyReads;
            cpuAtDummyRead = model ? model->GetRefCount() : -1;
            if (dummyReadResult != ClockResult::Ok)
                return dummyReadResult;
            output = positions;
            return ClockResult::Ok;
        }
        ClockResult SetModelVehicleDummyPosition(std::uint32_t key, std::uint32_t index, const NativeModelDummyVector& value) noexcept override
        {
            Touch(key);
            ++dummySets;
            positions[index] = value;
            return ClockResult::Ok;
        }
        ClockResult ResetModelVehicleDummies(std::uint32_t key, bool remove) noexcept override
        {
            Touch(key);
            keysMatch = keysMatch && remove;
            positions = {};
            return ClockResult::Ok;
        }
        ClockResult SetModelTime(std::uint32_t key, std::uint8_t valueOn, std::uint8_t valueOff, bool& applied) noexcept override
        {
            Touch(key);
            ++timeSets;
            on = valueOn;
            off = valueOff;
            // This scenario is a vehicle. Preserve genuine non-TIME refusal of
            // applicability while proving the composed setter routes its bytes.
            applied = false;
            return ClockResult::Ok;
        }
        ClockResult ReadModelLodDistance(std::uint32_t key, std::uint32_t& output) noexcept override
        {
            Touch(key);
            output = lod;
            return ClockResult::Ok;
        }
        ClockResult ReadOriginalModelLodDistance(std::uint32_t key, std::uint32_t& output) noexcept override
        {
            Touch(key);
            output = Bits(50.0f);
            return ClockResult::Ok;
        }
        ClockResult SetModelLodDistance(std::uint32_t key, std::uint32_t value) noexcept override
        {
            Touch(key);
            ++lodSets;
            lod = value;
            return ClockResult::Ok;
        }
    };
    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        bool          loaded = true;
        unsigned      requests = 0, loads = 0, readyAfterLoads = 2;
        std::uint32_t requestedKey = 0, requestFlags = 0;
        ClockResult   ReadStreamingModelLoaded(std::uint32_t, NativeModelLoadedReader::Value& output) noexcept override
        {
            output = {};
            output.streamedIn = loaded;
            return ClockResult::Ok;
        }
        ClockResult RequestStreamingModel(std::uint32_t key, std::uint32_t flags) noexcept override
        {
            ++requests;
            requestedKey = key;
            requestFlags = flags;
            return ClockResult::Ok;
        }
        ClockResult LoadStreamingModels(bool) noexcept override
        {
            loaded = ++loads >= readyAfterLoads;
            return ClockResult::Ok;
        }
    };
    class Streaming final : public model_startup_fixture::StreamingFixtureBase
    {
    public:
        explicit Streaming(NativeStreamingModelAccess& owner) noexcept : StreamingFixtureBase(owner) {}
    };
    class Game final : public model_startup_fixture::GameFixtureBase
    {
    public:
        CStreaming* streaming = nullptr;
        unsigned    streamingReads = 0, lookups = 0;
        DWORD       lookupKey = 0;
        CStreaming* GetStreaming() override
        {
            ++streamingReads;
            return streaming;
        }
        std::uint32_t GetBaseIDforTXD() override { return 20000; }
        std::uint32_t GetBaseIDforIFP() override { return 30000; }
        bool          IsASyncLoadingEnabled(bool) override { return true; }
        CModelInfo*   GetModelInfo(DWORD key, bool) override
        {
            ++lookups;
            lookupKey = key;
            return nullptr;
        }
    };
    class Core final : public model_startup_fixture::CoreFixtureBase
    {
    public:
        struct Entry
        {
            uint        debug = 0, report = 0;
            std::string type, body;
        };
        std::array<Entry, 4> entries{};
        unsigned             count = 0;
        void                 LogEvent(uint debug, const char* type, const char*, const char* body, uint report) override
        {
            if (count == entries.size())
                model_startup_fixture::Unqualified();
            entries[count++] = {debug, report, type, body};
        }
    };
    class Model final : public model_startup_fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t key, NativeModelClassificationAccess& classification, NativeModelLodAccess& lod, NativeModelTimeAccess& time,
              NativeModelWheelAccess& wheel, NativeModelObjectGroupAccess& groups, NativeModelDummiesAccess& dummies, CGame& game,
              NativeModelBackingReferenceAccess& backing, const Token& token, CCoreInterface& core)
            : ModelFixtureBase(key, classification, lod, time, wheel, groups, dummies, game, backing, token, core)
        {
        }
        Model(Owner& owner, CGame& game, CCoreInterface& core)
            : Model(owner.current.modelIndex, owner, owner, owner, owner, owner, owner, game, owner, owner.current, core)
        {
        }
    };
    static_assert(std::is_abstract_v<CModelInfoStartupDE> && !std::is_abstract_v<Model> && std::is_convertible_v<Model*, CModelInfoDE*>);
    static_assert(std::is_same_v<decltype(&Model::ModelAddRef), decltype(&CModelInfoRequestDE::ModelAddRef)> &&
                  std::is_same_v<decltype(&Model::RemoveRef), decltype(&CModelInfoRequestDE::RemoveRef)> &&
                  std::is_same_v<decltype(&Model::Request), decltype(&CModelInfoRequestDE::Request)> &&
                  std::is_same_v<decltype(&Model::SetCustomModel), decltype(&CModelInfoUnmodifiedDE::SetCustomModel)>);
    struct Scenario
    {
        Owner          owner;
        StreamingOwner streamingOwner;
        Streaming      streaming{streamingOwner};
        Game           game;
        Core           core;
        Model          model{owner, game, core};
        Scenario()
        {
            game.streaming = &streaming;
            owner.model = &model;
            owner.positions[0] = {1.0f, 2.0f, 3.0f};
        }
    };
    void SharedIdentityAndCycles()
    {
        Scenario    s;
        CModelInfo& sdk = s.model;
        Check(static_cast<CModelInfo*>(&s.model) == static_cast<CModelInfo*>(static_cast<CModelInfoUnmodifiedDE*>(&s.model)) &&
                  static_cast<CModelInfoDE*>(&s.model) == static_cast<CModelInfoDE*>(static_cast<CModelInfoRequestDE*>(&s.model)),
              "all startup mixins share one actual model/request/reference SDK base");
        Check(s.owner.binds == 1 && s.owner.claims == 1 && !s.owner.effects && !s.owner.acquires && sdk.GetRefCount() == 0,
              "composition construction binds and claims once without native observations or effects");
        Check(sdk.GetModel() == 400, "the inherited actual model identity is unchanged");
        const auto position = sdk.GetVehicleDummyDefaultPosition(static_cast<VehicleDummies::Enum>(0));
        Check(position.fX == 1 && position.fY == 2 && position.fZ == 3 && s.owner.cpuAtDummyRead == 1,
              "actual dummy default borrows the same model through its temporary SDK reference");
        Check(s.owner.acquires == 1 && s.owner.releases == 1 && sdk.GetRefCount() == 0 && s.owner.phase == Phase::Released,
              "dummy default releases its one backing reference through the unmodified family");
        sdk.ModelAddRef(NON_BLOCKING, "composed cycle");
        sdk.ModelAddRef(NON_BLOCKING, "shared cycle");
        Check(sdk.GetRefCount() == 2 && s.owner.acquires == 2 && s.owner.renews == 1 && s.owner.current.serial == 12 && s.owner.family == 11,
              "later ordinary references renew the SAME family and share one new backing acquire");
        sdk.RemoveRef();
        Check(sdk.GetRefCount() == 1 && s.owner.releases == 1, "interior decrement cannot release the composed backing family");
        sdk.RemoveRef();
        sdk.ModelAddRef(NON_BLOCKING, "next composed cycle");
        Check(sdk.GetRefCount() == 1 && s.owner.acquires == 3 && s.owner.renews == 2 && s.owner.current.serial == 13,
              "another source cycle transports a distinct serial without another wrapper or Bind");
        sdk.RemoveRef();
        Check(!sdk.GetRefCount() && s.owner.releases == 3 && s.owner.cpuAtAcquire == 0 && s.owner.cpuAtRelease == 0 && s.owner.policyPreserved,
              "same-object final release preserves CPU commit order and genuine RemoveAtZero intent");

        sdk.SetVehicleWheelSize(ResizableVehicleWheelGroup::ALL_WHEELS, 3.5f);
        Check(sdk.GetVehicleWheelSize(ResizableVehicleWheelGroup::FRONT_AXLE) == 3.5f && s.owner.wheelSets == 1,
              "wheel methods dispatch on the same composed model key");
        sdk.ResetVehicleWheelSizes();
        sdk.SetObjectPropertiesGroup(0x1234);
        Check(sdk.GetObjectPropertiesGroup() == 0x1234 && s.owner.groupSets == 1, "object group mutation shares the composed native owner");
        sdk.RestoreObjectPropertiesGroup();
        sdk.SetVehicleDummyPosition(static_cast<VehicleDummies::Enum>(0), CVector(4, 5, 6));
        Check(sdk.GetVehicleDummyPosition(static_cast<VehicleDummies::Enum>(0)).fY == 5 && s.owner.dummySets == 1,
              "dummy setter and reader use the same request/reference-capable instance");
        sdk.ResetVehicleDummies(true);
        Check(!sdk.SetTime(static_cast<char>(0x80), static_cast<char>(0xff)) && s.owner.timeSets == 1 && s.owner.on == 0x80 && s.owner.off == 0xff,
              "composed time setter preserves raw char bytes and qualified non-TIME applicability");
        sdk.SetLODDistance(500.0f);
        Check(sdk.GetLODDistance() == 325.0f && sdk.GetOriginalLODDistance() == 50.0f && s.owner.lodSets == 1,
              "LOD current/default reads and actual SDK limit share the same owner");
        sdk.SetLODDistance(500.0f, true);
        Check(s.owner.lod == Bits(500.0f) && s.owner.keysMatch && s.owner.binds == 1 && s.owner.claims == 1,
              "every mutation callback keeps the original model key and complete owner without duplicate binding");
        s.owner.mutationResult = ClockResult::Stopped;
        Check(Refuses([&] { sdk.SetVehicleWheelSize(ResizableVehicleWheelGroup::ALL_WHEELS, 9.0f); }) && s.owner.wheels.front == 1.0f,
              "a composed mutation propagates native-owner refusal without a replacement success path");
    }
    void RetainedPrefixAndRealDiagnostics()
    {
        Scenario    s;
        CModelInfo& sdk = s.model;
        s.owner.dummyReadResult = ClockResult::OriginalException;
        Check(Refuses([&] { static_cast<void>(sdk.GetVehicleDummyDefaultPosition(static_cast<VehicleDummies::Enum>(0))); }),
              "dummy read refusal propagates after the actual temporary acquire");
        Check(sdk.GetRefCount() == 1 && s.owner.acquires == 1 && !s.owner.releases && s.owner.phase == Phase::Held,
              "failed dummy default retains the acknowledged reference prefix on this composed instance");
        s.owner.dummyReadResult = ClockResult::Ok;
        const auto position = sdk.GetVehicleDummyDefaultPosition(static_cast<VehicleDummies::Enum>(0));
        Check(position.fX == 1 && sdk.GetRefCount() == 1 && s.owner.cpuAtDummyRead == 2 && s.owner.acquires == 1 && !s.owner.releases,
              "explicit retry shares the retained acquire and releases only its own temporary CPU reference");
        sdk.RemoveRef();
        Check(!sdk.GetRefCount() && s.owner.releases == 1 && s.owner.phase == Phase::Released,
              "the caller explicitly retires the original retained prefix exactly once");
        s.streamingOwner.loaded = false;
        sdk.Request(BLOCKING, "composed report");
        Check(s.game.lookups == 1 && s.game.lookupKey == 30042 && s.core.count == 2,
              "actual request dependencies reach the borrowed game and real Core reporter through this composition");
        Check(s.core.entries[0].debug == 505 && s.core.entries[0].report == 0 && s.core.entries[0].type == "Model no anim" &&
                  s.core.entries[0].body == "400 (30042)",
              "missing-animation Core505 payload is preserved");
        Check(s.core.entries[1].debug == 642 && s.core.entries[1].report == 6642 && s.core.entries[1].body == "400 (composed report) (Took 2 attempts)",
              "blocking Core642/report6642 payload comes from the actual inherited request policy");
        Check(s.streamingOwner.requests == 1 && s.streamingOwner.requestedKey == 400 && s.streamingOwner.requestFlags == 0x16 && s.streamingOwner.loads == 2,
              "real streaming SDK receives the original blocking request flags and retry completion sequence");
        Check(s.owner.binds == 1 && s.owner.claims == 1 && s.owner.acquires == 1 && s.owner.renews == 0,
              "request and diagnostics do not fabricate another backing-reference cycle");
    }
    void ConstructorAndReplacementRefusals()
    {
        for (unsigned route = 0; route != 9; ++route)
        {
            Owner owner, foreign;
            Game  game;
            Core  core;
            Token token = owner.current;
            if (route == 6)
                ++token.modelIndex;
            if (route == 7)
                token.ownerGeneration = 0;
            if (route == 8)
                token.serial = 0;
            Check(Refuses(
                      [&]
                      {
                          Model model(400, owner, route == 0 ? foreign : owner, route == 1 ? foreign : owner, route == 2 ? foreign : owner,
                                      route == 3 ? foreign : owner, route == 4 ? foreign : owner, game, route == 5 ? foreign : owner, token, core);
                          (void)model;
                      }),
                  "foreign mutation/backing owner or invalid token refuses composition construction");
            Check(!owner.binds && !owner.claims && !owner.effects && !foreign.binds && !foreign.claims && !foreign.effects,
                  "every constructor affinity/token refusal precedes Bind, Claim and native effect entry");
        }
        Scenario    s;
        CModelInfo& sdk = s.model;
        const auto  reads = s.owner.reads;
        Check(Refuses([&] { static_cast<void>(sdk.SetCustomModel(nullptr)); }), "custom render replacement stays final and refused");
        Check(Refuses([&] { sdk.RestoreOriginalModel(); }), "custom render restore stays final and refused");
        Check(Refuses([&] { sdk.SetColModel(nullptr); }), "collision replacement stays final and refused");
        Check(Refuses([&] { sdk.RestoreColModel(); }), "collision restore stays final and refused");
        Check(Refuses([&] { sdk.MakeCustomModel(); }), "custom model creation stays final and refused");
        Check(s.owner.reads == reads && !s.owner.effects && !s.owner.acquires && !s.owner.releases && !sdk.GetRefCount(),
              "replacement refusal cannot invalidate the single unmodified family's native-removal proof");
    }
}
int main()
{
    static_assert(sizeof(std::uintptr_t) == 8, "The startup composition fixture requires the actual x64 SDK build");
    try
    {
        SharedIdentityAndCycles();
        RetainedPrefixAndRealDiagnostics();
        ConstructorAndReplacementRefusals();
        std::puts("Production model startup composition SDK fixture completed");
        std::printf("Passed %u model startup composition SDK checks (%u-bit; CPU provider doubles only)\n", checks,
                    static_cast<unsigned>(sizeof(std::uintptr_t) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        // Keep genuine SDK refusals visible in the owned process log instead
        // of turning a missing fixture capability into an opaque abort code.
        std::fprintf(stderr, "Model startup composition fixture refused: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
