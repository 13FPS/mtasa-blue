/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real MTA wheel policy, SDK dispatch and owner-cache orchestration
 *****************************************************************************/
#include <game/CModelWheelSizePolicy.h>
#if defined(_WIN64)
    // The shared MTA policy is portable; the DE owner graph deliberately requires
    // x64. Keep that native ABI requirement intact while checking classic x86 reuse.
    #include "../../Client/game_de/CModelInfoWheelDE.h"
    #include "../../Client/game_de/NativeModelWheelDomain.h"
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN64)
    #include "ModelWheelFixture.generated.h"

using namespace mta::game_de;
#endif
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
#if defined(_WIN64)
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    float Float(std::uint32_t bits)
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    ClockResult Allowed() noexcept
    {
        return ClockResult::Ok;
    }
    template <class Action>
    void Refuses(Action&& action, const char* name, ClockResult status)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message = error.what(), suffix = ": " + std::to_string(static_cast<std::uint32_t>(status));
            refused = message.find(name) != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused, "actual SDK wheel override preserves the owner's explicit refusal");
    }
    struct WheelOwner final : NativeModelClassificationAccess, NativeModelWheelAccess
    {
        NativeModelWheelSizes    sizes{1.25f, 0.75f};
        NativeModelWheelGroup    group = NativeModelWheelGroup::Front;
        std::uint32_t            key = 0, scalar = 0;
        unsigned                 reads = 0, sets = 0, resets = 0, classifications = 0;
        bool                     explicitReset = false;
        ClockResult              status = ClockResult::Ok;
        std::pair<float, float>* mutateCaller = nullptr;
        ClockResult              ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++classifications;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelWheelSizes(std::uint32_t model, NativeModelWheelSizes& output) noexcept override
        {
            ++reads;
            key = model;
            output = sizes;
            return status;
        }
        ClockResult SetModelWheelSize(std::uint32_t model, NativeModelWheelGroup selected, float value) noexcept override
        {
            ++sets;
            key = model;
            group = selected;
            scalar = Bits(value);
            return status;
        }
        ClockResult ResetModelWheelSizes(std::uint32_t model, const NativeModelWheelSizes* value) noexcept override
        {
            ++resets;
            key = model;
            explicitReset = value != nullptr;
            if (mutateCaller)
                *mutateCaller = {99.0f, 98.0f};
            if (value)
                sizes = *value;
            return status;
        }
    };
    void Sdk()
    {
        WheelOwner owner, unrelated;
        bool       rejected = false;
        try
        {
            ModelWheelObserver bad(400, owner, unrelated);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        Check(rejected && !owner.reads && !unrelated.sets, "separate classification and wheel owners cannot compose a facade");
        ModelWheelObserver concrete(400, owner, owner);
        CModelInfo&        model = concrete;
        Check(model.GetModel() == 0xfedcba98, "fixture exposes a deliberately poisoned virtual model key");
        Check(model.GetVehicleWheelSize(ResizableVehicleWheelGroup::FRONT_AXLE) == 1.25f && owner.key == 400, "front getter uses immutable native key");
        Check(model.GetVehicleWheelSize(ResizableVehicleWheelGroup::REAR_AXLE) == 0.75f && owner.key == 400, "rear getter uses immutable native key");
        Check(model.GetVehicleWheelSize(ResizableVehicleWheelGroup::ALL_WHEELS) == 0.0f && owner.reads == 3,
              "ALL is ignored by the getter after its owner read");
        const auto nan = Float(0x7fc12345);
        model.SetVehicleWheelSize(ResizableVehicleWheelGroup::ALL_WHEELS, nan);
        Check(owner.key == 400 && owner.group == NativeModelWheelGroup::All && owner.scalar == 0x7fc12345,
              "SDK forwards ALLff and quiet-NaN payload without clamping");
        model.SetVehicleWheelSize(static_cast<ResizableVehicleWheelGroup::Enum>(2), Float(0x80000000));
        Check(static_cast<std::uint32_t>(owner.group) == 2 && owner.scalar == 0x80000000,
              "unknown group and signed zero reach the real owner policy unchanged");
        model.ResetVehicleWheelSizes();
        Check(owner.key == 400 && !owner.explicitReset, "default Reset retains the null/default-map intent");
        std::pair<float, float> defaults{2.0f, 3.0f};
        owner.mutateCaller = &defaults;
        model.ResetVehicleWheelSizes(&defaults);
        owner.mutateCaller = nullptr;
        Check(owner.explicitReset && owner.sizes.front == 2.0f && owner.sizes.rear == 3.0f && defaults.first == 99.0f,
              "explicit defaults are copied before calling an owner that can mutate caller state");
        for (std::uint32_t code = 1; code <= static_cast<std::uint32_t>(ClockResult::OriginalException); ++code)
        {
            owner.status = static_cast<ClockResult>(code);
            Refuses([&] { static_cast<void>(model.GetVehicleWheelSize(ResizableVehicleWheelGroup::FRONT_AXLE)); }, "GetVehicleWheelSize", owner.status);
            Refuses([&] { model.SetVehicleWheelSize(ResizableVehicleWheelGroup::FRONT_AXLE, 0.0f); }, "SetVehicleWheelSize", owner.status);
            Refuses([&] { model.ResetVehicleWheelSizes(); }, "ResetVehicleWheelSizes", owner.status);
        }
        Check(owner.classifications == 0, "wheel facade adds no duplicate virtual classifier or load prerequisite");
    }
#endif
    void Policy()
    {
        using P = MtaGame::CModelWheelSizePolicy;
        unsigned front = 0, rear = 0;
        Check(P::Get(
                  0,
                  [&]
                  {
                      ++front;
                      return 4.0f;
                  },
                  [&]
                  {
                      ++rear;
                      return 5.0f;
                  }) == 4.0f &&
                  front == 1 && rear == 0,
              "shared getter reads only its selected front field");
        Check(P::Get(
                  0xff,
                  [&]
                  {
                      ++front;
                      return 4.0f;
                  },
                  [&]
                  {
                      ++rear;
                      return 5.0f;
                  }) == 0.0f &&
                  front == 1 && rear == 0,
              "shared getter ignores ALL without either field callback");
        std::string sequence;
        Check(P::Apply(
                  0xff, 7, [&](int) { sequence += 'F'; }, [&](int) { sequence += 'R'; }) &&
                  sequence == "FR",
              "shared ALL setter preserves front-then-rear native callback order");
        Check(!P::Apply(2, 7, [&](int) { sequence += 'X'; }, [&](int) { sequence += 'Y'; }) && sequence == "FR", "unknown setter group invokes no store");
        std::map<unsigned, std::pair<float, float>> cache;
        unsigned                                    captures = 0;
        P::Remember(cache, 1u,
                    [&]
                    {
                        ++captures;
                        return std::make_pair(1.0f, 2.0f);
                    });
        P::Remember(cache, 1u,
                    [&]
                    {
                        ++captures;
                        return std::make_pair(8.0f, 9.0f);
                    });
        Check(captures == 1 && cache.at(1).first == 1.0f, "MTA default map remembers the first pair once per model");
        std::pair<float, float> copied;
        Check(P::Take(cache, 1u, copied) && cache.empty() && copied.first == 1.0f && copied.second == 2.0f,
              "default is copied before erasing its owning node, leaving valid erase-before-write values");
        Check(!P::Take(cache, 1u, copied), "empty repeated reset has no default to consume");
        cache.emplace(1, std::make_pair(1.0f, 2.0f));
        cache.emplace(2, std::make_pair(3.0f, 4.0f));
        unsigned callbacks = 0;
        Check(!P::RestoreAll(cache,
                             [&](unsigned key, auto&)
                             {
                                 ++callbacks;
                                 Check(cache.size() == 2, "bulk callbacks retain the whole MTA cache");
                                 return key != 2;
                             }) &&
                  cache.size() == 2,
              "incomplete backend bulk reset retains its default evidence");
        Check(P::RestoreAll(cache,
                            [&](unsigned, auto&)
                            {
                                ++callbacks;
                                return true;
                            }) &&
                  cache.empty() && callbacks == 4,
              "successful bulk reset clears defaults after all callbacks");
    }
}

#if defined(_WIN64)
// Explicit native-memory doubles isolate the real Domain's CPU ownership and
// cache/error itinerary. The separate Runtime fixture executes actual qualified
// Memory/Domain/CAS against owned SEC_IMAGE data; this binary invokes no engine.
namespace FixtureMemory
{
    std::map<std::uint32_t, NativeModelWheelMemory::Value> rows;
    unsigned                                               captures = 0, writes = 0, failWriteAt = 0;
    ClockResult                                            captureStatus = ClockResult::Ok, writeStatus = ClockResult::Ok;
    bool                                                   acceptedUnknown = false;
    std::function<void()>                                  captureAction;
    std::vector<std::uint32_t>                             written;
    NativeModelWheelMemory::Value                          Row(std::uint32_t key)
    {
        NativeModelWheelMemory::Value value;
        value.generation = 1;
        value.vehicle = true;
        value.model.modelIndex = key;
        value.model.nativeModelInfo = 0x10000 + key * 0x100;
        value.model.slotAddress = 0x1000 + key * 8;
        value.bits = {Bits(1.0f), Bits(2.0f)};
        return value;
    }
    void Fresh()
    {
        rows.clear();
        rows.emplace(3, Row(3));
        rows.emplace(4, Row(4));
        captures = writes = failWriteAt = 0;
        captureStatus = writeStatus = ClockResult::Ok;
        acceptedUnknown = false;
        captureAction = {};
        written.clear();
    }
}
namespace mta::game_de::NativeModelWheelMemory
{
    Origin Identity(const Value& value) noexcept
    {
        Origin origin;
        origin.generation = value.generation;
        origin.model = value.model.modelIndex;
        origin.metadata = value.model.nativeModelInfo;
        origin.slot = value.model.slotAddress;
        return origin;
    }
    bool SameIdentity(const Origin& origin, const Value& value) noexcept
    {
        return value.vehicle && origin.generation == value.generation && origin.model == value.model.modelIndex &&
               origin.metadata == value.model.nativeModelInfo && origin.slot == value.model.slotAddress;
    }
    bool SameIdentity(const Value& first, const Value& second) noexcept
    {
        return SameIdentity(Identity(first), second);
    }
    ClockResult Capture(std::uintptr_t, std::uint64_t generation, std::uint32_t key, CheckOwner owner, CheckOwner afterApplication, Value& output) noexcept
    {
        using namespace FixtureMemory;
        ++captures;
        if (captureAction)
        {
            auto action = std::move(captureAction);
            captureAction = {};
            action();
        }
        if (captureStatus != ClockResult::Ok)
            return captureStatus;
        if (!owner || !afterApplication || owner() != ClockResult::Ok || afterApplication() != ClockResult::Ok)
            return ClockResult::WrongThread;
        const auto found = rows.find(key);
        Value      value;
        value.generation = generation;
        value.model.modelIndex = key;
        if (found != rows.end())
        {
            value = found->second;
            value.generation = generation;
        }
        output = value;
        return ClockResult::Ok;
    }
    ClockResult Write(std::uintptr_t, const Value& before, const std::array<std::uint32_t, 2>& desired, CheckOwner, CheckOwner, Receipt& receipt,
                      Exchange) noexcept
    {
        using namespace FixtureMemory;
        ++writes;
        written.push_back(before.model.modelIndex);
        receipt.before = before;
        receipt.desired = desired;
        const auto result = failWriteAt == writes ? ClockResult::WriteFailure : writeStatus;
        receipt.result = result;
        if (acceptedUnknown)
        {
            rows[before.model.modelIndex].bits = desired;
            receipt.entered = receipt.accepted = receipt.uncertain = true;
            return ClockResult::OriginalException;
        }
        if (result != ClockResult::Ok)
            return result;
        receipt.entered = receipt.accepted = true;
        rows[before.model.modelIndex].bits = desired;
        return ClockResult::Ok;
    }
}
namespace
{
    void Domain()
    {
        using namespace FixtureMemory;
        const auto front = NativeModelWheelGroup::Front, rear = NativeModelWheelGroup::Rear, all = NativeModelWheelGroup::All;
        Fresh();
        NativeModelWheelDomain basic;
        Check(basic.CanDiscard() && basic.Set(1, 1, 3, front, 5.0f, Allowed, Allowed) == ClockResult::Ok && basic.Pending(),
              "accepted setter retains first defaults with its owner");
        Check(basic.Set(1, 1, 3, rear, 6.0f, Allowed, Allowed) == ClockResult::Ok && rows[3].bits == std::array<std::uint32_t, 2>{Bits(5), Bits(6)},
              "repeated setter updates selected fields");
        Check(basic.ResetOne(1, 1, 3, nullptr, Allowed, Allowed) == ClockResult::Ok && rows[3].bits == std::array<std::uint32_t, 2>{Bits(1), Bits(2)} &&
                  basic.CanDiscard(),
              "single default reset restores first pair and retires cache debt");
        const auto completedWrites = writes;
        Check(basic.ResetOne(1, 1, 3, nullptr, Allowed, Allowed) == ClockResult::Ok && writes == completedWrites, "repeated reset has no invented write");
        Check(basic.Set(1, 1, 3, static_cast<NativeModelWheelGroup>(2), 7, Allowed, Allowed) == ClockResult::Ok && basic.Pending() && writes == completedWrites,
              "ignored group still captures the default without a native store");
        Check(basic.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok && basic.CanDiscard(), "explicit healthy bulk reset retires ignored-group cache");
        const auto nan = Float(0x7fc12345);
        Check(basic.Set(1, 1, 3, all, nan, Allowed, Allowed) == ClockResult::Ok && rows[3].bits == std::array<std::uint32_t, 2>{0x7fc12345, 0x7fc12345},
              "domain preserves quiet-NaN bits for both fields");
        NativeModelWheelSizes read;
        Check(basic.Read(1, 1, 3, Allowed, Allowed, read) == ClockResult::Ok && Bits(read.front) == 0x7fc12345, "domain getter returns current raw pair");
        Check(basic.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok, "NaN metadata stays resettable without finite clamps");
        Check(basic.Set(1, 1, 3, front, Float(0x80000000), Allowed, Allowed) == ClockResult::Ok && rows[3].bits[0] == 0x80000000,
              "signed zero remains an accepted raw setter value");
        NativeModelWheelSizes explicitPair{8, 9};
        Check(basic.ResetOne(1, 1, 3, &explicitPair, Allowed, Allowed) == ClockResult::Ok && basic.Pending() && rows[3].bits[0] == Bits(8),
              "explicit reset pair does not consume cached original defaults");
        Check(basic.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok && rows[3].bits[0] == Bits(1), "bulk still has the original pair after explicit reset");
        rows[3].vehicle = false;
        const auto beforeSkip = writes;
        Check(basic.Set(1, 1, 3, all, 4, Allowed, Allowed) == ClockResult::Ok && !basic.Pending() && writes == beforeSkip,
              "qualified nonvehicle setter has no default or write");
        Check(basic.Read(1, 1, 3, Allowed, Allowed, read) == ClockResult::Ok && Bits(read.front) == 0 && Bits(read.rear) == 0,
              "qualified nonvehicle getter supplies classic zero");

        Fresh();
        NativeModelWheelDomain identity;
        Check(identity.Set(1, 1, 3, all, 4, Allowed, Allowed) == ClockResult::Ok, "identity fixture captures a genuine first default");
        const auto beforeIdentity = writes;
        Check(identity.Set(1, 2, 3, all, 5, Allowed, Allowed) == ClockResult::BindingChanged && identity.Pending() && writes == beforeIdentity,
              "new generation cannot reuse old defaults");
        Check(identity.ResetAll(1, 2, Allowed, Allowed) == ClockResult::BindingChanged && writes == beforeIdentity,
              "foreign generation cannot clear or replay reset debt");
        rows[3].model.nativeModelInfo += 8;
        Check(identity.ResetAll(1, 1, Allowed, Allowed) == ClockResult::BindingChanged && identity.Pending(), "replaced native row refuses cached defaults");
        rows[3].model.nativeModelInfo -= 8;
        Check(identity.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok && identity.CanDiscard(),
              "same current generation and row can explicitly finish healthy reset");

        Fresh();
        NativeModelWheelDomain skipped;
        Check(skipped.Set(1, 1, 3, front, 4, Allowed, Allowed) == ClockResult::Ok, "bulk-skip fixture owns defaults");
        rows[3].vehicle = false;
        const auto beforeMissing = writes;
        Check(skipped.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok && skipped.CanDiscard() && writes == beforeMissing,
              "qualified absent/nonvehicle bulk skip clears its logical default without a store");

        Fresh();
        NativeModelWheelDomain refused;
        Check(refused.Set(1, 1, 3, front, 4, Allowed, Allowed) == ClockResult::Ok, "refusal fixture first accepts native intent");
        writeStatus = ClockResult::WriteFailure;
        Check(refused.ResetOne(1, 1, 3, nullptr, Allowed, Allowed) == ClockResult::WriteFailure && refused.Pending() && !refused.Uncertain(),
              "erase-before-store refusal retains a separate known reset obligation");
        const auto beforeRetired = writes;
        Check(refused.Set(1, 1, 3, rear, 9, Allowed, Allowed) == ClockResult::RestoreFailure && writes == beforeRetired,
              "a new setter cannot replace pending retired intent");
        writeStatus = ClockResult::Ok;
        Check(refused.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok && refused.CanDiscard() && rows[3].bits[0] == Bits(1),
              "explicit healthy drain restores the separately retained original");

        Fresh();
        NativeModelWheelDomain prefix;
        Check(prefix.Set(1, 1, 3, all, 4, Allowed, Allowed) == ClockResult::Ok && prefix.Set(1, 1, 4, all, 5, Allowed, Allowed) == ClockResult::Ok,
              "two models retain independent first defaults");
        failWriteAt = writes + 2;
        Check(prefix.ResetAll(1, 1, Allowed, Allowed) == ClockResult::WriteFailure && prefix.Pending(), "bulk keeps an incomplete accepted prefix alive");
        failWriteAt = 0;
        const auto prefixWrites = writes;
        Check(prefix.ResetAll(1, 1, Allowed, Allowed) == ClockResult::Ok && writes == prefixWrites + 1 && prefix.CanDiscard(),
              "explicit retry skips its already acknowledged model prefix");

        Fresh();
        NativeModelWheelDomain unknown;
        acceptedUnknown = true;
        Check(unknown.Set(1, 1, 3, all, 4, Allowed, Allowed) == ClockResult::OriginalException && unknown.Pending() && unknown.Uncertain(),
              "accepted unknown write retains sticky exact intent");
        acceptedUnknown = false;
        const auto uncertainWrites = writes;
        Check(unknown.Set(1, 1, 3, front, 7, Allowed, Allowed) == ClockResult::RestoreFailure &&
                  unknown.ResetOne(1, 1, 3, nullptr, Allowed, Allowed) == ClockResult::RestoreFailure &&
                  unknown.ResetAll(1, 1, Allowed, Allowed) == ClockResult::RestoreFailure && writes == uncertainWrites && !unknown.CanDiscard(),
              "unknown native prefix is never rolled back, replayed, discarded or cleared by another generation");

        Fresh();
        NativeModelWheelDomain active;
        captureAction = [&]
        {
            Check(active.Pending() && !active.CanDiscard(), "producer summary stays conservative while the real domain operation is active");
            Check(active.Set(1, 1, 3, all, 8, Allowed, Allowed) == ClockResult::Busy, "reentrant mutation cannot borrow the same CPU default map");
        };
        Check(active.Read(1, 1, 3, Allowed, Allowed, read) == ClockResult::Ok && active.CanDiscard(),
              "read-only active scope retires without fabricated default debt");
        captureStatus = ClockResult::BindingChanged;
        read = {17, 18};
        Check(active.Read(1, 1, 3, Allowed, Allowed, read) == ClockResult::BindingChanged && read.front == 17 && read.rear == 18,
              "read refusal preserves the caller's whole pair");
    }
}
#endif
int main()
{
    try
    {
        Policy();
        std::puts("Shared MTA model wheel policy fixture completed");
#if defined(_WIN64)
        static_assert(std::is_abstract_v<CModelInfoWheelDE> && !std::is_abstract_v<ModelWheelObserver>);
        Sdk();
        Domain();
        std::puts("Actual model wheel SDK/domain fixture completed");
        std::printf("Passed %u actual MTA model wheel checks (%u-bit; native memory doubled)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
#else
        std::printf("Passed %u shared MTA model wheel policy checks (%u-bit; no DE native graph)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
#endif
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Model wheel SDK/domain fixture failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
