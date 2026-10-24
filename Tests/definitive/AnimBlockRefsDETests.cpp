/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual block ref SDK dispatch coupled to MTA's shared request policy
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlockDE.h"
#include "../../Client/game_de/CAnimBlockRequestDE.h"
#include <game/CAnimBlockRequestPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
class CModelInfo;
using namespace mta::game_de;
#include "AnimBlockRefsFixture.generated.h"

namespace
{
    using Receiver = animation_block_refs_fixture::RefsReceiver;
    unsigned checks = 0;
#if defined(NDEBUG)
    constexpr bool Assertions = false;
#else
    constexpr bool Assertions = true;
#endif
    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    bool Same(const NativeAnimationBlockIdentity& a, const NativeAnimationBlockIdentity& b) noexcept
    {
        return a.index == b.index && a.name == b.name && a.ownerGeneration == b.ownerGeneration;
    }
    NativeAnimationBlockIdentity Identity()
    {
        NativeAnimationBlockIdentity id;
        id.index = 17;
        id.name = {'p', 'e', 'd', 0, 0x81, 0x82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfe};
        id.ownerGeneration = 0xfedcba9876543210ull;
        return id;
    }
    struct Owner final : NativeAnimationBlockAccess
    {
        explicit Owner(const NativeAnimationBlockIdentity& id) : expected(id) {}
        const NativeAnimationBlockIdentity  expected;
        NativeAnimationBlockIdentity        received{};
        const NativeAnimationBlockIdentity* forbiddenCaller = nullptr;
        unsigned                            reads = 0, adds = 0, metadata = 0, names = 0;
        std::uint16_t                       refs = 0;
        bool                                loaded = false, aliased = false, stable = true, commitOnFailure = false;
        ClockResult                         readResult = ClockResult::Ok, addResult = ClockResult::Ok, metadataResult = ClockResult::Ok;
        std::vector<int>*                   effects = nullptr;
        void (*callback)(void*) noexcept = nullptr;
        void*       context = nullptr;
        ClockResult ReadAnimationBlockRefs(const NativeAnimationBlockIdentity& identity, std::uint16_t& output) noexcept override
        {
            ++reads;
            received = identity;
            aliased = aliased || &identity == forbiddenCaller;
            const auto before = identity;
            if (callback)
                callback(context);
            stable = stable && Same(before, identity);
            output = refs;
            return Same(identity, expected) ? readResult : ClockResult::BindingChanged;
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity& identity) noexcept override
        {
            ++adds;
            received = identity;
            aliased = aliased || &identity == forbiddenCaller;
            const auto before = identity;
            if (callback)
                callback(context);
            stable = stable && Same(before, identity);
            if (!Same(identity, expected))
                return ClockResult::BindingChanged;
            if (effects)
                effects->push_back(3);
            if (addResult == ClockResult::Ok || commitOnFailure)
                refs = static_cast<std::uint16_t>(refs + 1u);
            return addResult;
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity& identity, NativeAnimationBlockValues& output) noexcept override
        {
            ++metadata;
            if (!Same(identity, expected))
                return ClockResult::BindingChanged;
            output.index = expected.index;
            output.loaded = loaded ? 1 : 0;
            return metadataResult;
        }
        ClockResult ReadAnimationBlockName(const NativeAnimationBlockIdentity&, char*&) noexcept override
        {
            ++names;
            return ClockResult::NotInitialized;
        }
    };
    struct Callback
    {
        NativeAnimationBlockIdentity* caller = nullptr;
        CAnimBlock*                   peer = nullptr;
        unsigned                      calls = 0;
        unsigned short                observed = 0;
    };
    void MutateCallerAndReadPeer(void* opaque) noexcept
    {
        auto& value = *static_cast<Callback*>(opaque);
        ++value.calls;
        *value.caller = {};
        value.observed = value.peer->GetRefs();
    }
    template <class Operation>
    bool Refused(Operation operation, const char* method, ClockResult result)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            return text.find(method) != std::string::npos && text.find(": " + std::to_string(static_cast<unsigned>(result))) != std::string::npos;
        }
        return false;
    }
    void SdkChecks()
    {
        auto       caller = Identity();
        const auto expected = caller;
        auto       peerId = caller;
        peerId.index = 8;
        peerId.ownerGeneration = 13;
        Owner first(expected), second(peerId);
        first.forbiddenCaller = &caller;
        Receiver    receiver(first, caller), peer(second, peerId);
        CAnimBlock& sdk = receiver;
        Check(!first.reads && !first.adds && !first.metadata && !second.reads, "construction copies identity without native reference effects or observations");
        caller.index = 99;
        caller.name.fill('x');
        caller.ownerGeneration = 0;
        for (const auto value : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x7fff}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
        {
            first.refs = value;
            const auto before = first.reads;
            Check(sdk.GetRefs() == value && first.reads == before + 1 && Same(first.received, expected) && !first.aliased && !first.metadata && !first.names,
                  "actual GetRefs returns each raw unsigned WORD once with the copied name/index/generation and no metadata or loaded query");
        }
        first.metadataResult = ClockResult::Stopped;
        first.refs = 0xffff;
        sdk.AddRef();
        Check(first.adds == 1 && first.refs == 0 && Same(first.received, expected) && !first.metadata && !first.names,
              "actual AddRef forwards once to its selected owner even while unloaded and never supplies a CPU counter, loaded gate or metadata fallback");
        second.refs = 0x8001;
        Callback callback{&caller, &peer};
        first.callback = &MutateCallerAndReadPeer;
        first.context = &callback;
        const auto reads = first.reads;
        Check(sdk.GetRefs() == 0 && first.reads == reads + 1 && callback.calls == 1 && callback.observed == 0x8001 && second.reads == 1 &&
                  Same(first.received, expected) && Same(second.received, peerId) && first.stable && !first.aliased,
              "nested peer reads and mutation of the original caller preserve the per-call copied receiver identity");
        const auto adds = first.adds;
        sdk.AddRef();
        Check(first.adds == adds + 1 && first.refs == 1 && callback.calls == 2 && second.reads == 2 && first.stable && Same(first.received, expected),
              "the void increment path also snapshots identity once across callback and never redirects to another receiver");
        first.callback = nullptr;
        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.readResult = first.addResult = status;
            const auto beforeRead = first.reads, beforeAdd = first.adds;
            const auto beforeRefs = first.refs;
            Check(Refused([&] { (void)sdk.GetRefs(); }, "GetRefs", status) && first.reads == beforeRead + 1 && first.adds == beforeAdd,
                  "each GetRefs owner refusal propagates its exact operation/code once without using refused output");
            Check(
                Refused([&] { sdk.AddRef(); }, "AddRef", status) && first.adds == beforeAdd + 1 && first.refs == beforeRefs && !first.metadata && !first.names,
                "each AddRef refusal propagates once without readback, retry, counter fabrication or inverse operation");
        }
        NativeAnimationBlockAccess unavailable;
        Receiver                   missing(unavailable, expected);
        Check(Refused([&] { missing.AddRef(); }, "AddRef", ClockResult::NotInitialized) &&
                  Refused([&] { (void)missing.GetRefs(); }, "GetRefs", ClockResult::NotInitialized),
              "both new default access methods refuse instead of synthesizing reference success or a zero count");
        first.readResult = first.addResult = ClockResult::Ok;
        const auto beforeLifetime = first.adds + first.reads;
        {
            Receiver temporary(first, expected);
        }
        Check(first.adds + first.reads == beforeLifetime && sdk.GetRefs() == first.refs && second.refs == 0x8001,
              "retiring a borrowed facade does not decrement native refs or mutate another block's counter");
        std::puts("Actual animation block refs SDK fixture completed");
    }
    struct BoundaryFailure : std::runtime_error
    {
        explicit BoundaryFailure(unsigned stage) : std::runtime_error("instrumented request/load boundary"), stage(stage) {}
        unsigned stage;
    };
    struct Ordering
    {
        NativeAnimationBlockIdentity id = Identity();
        Owner                        owner{id};
        Receiver                     receiver{owner, id};
        std::vector<int>             effects;
        unsigned                     baseReads = 0, lookupReads = 0, requestCalls = 0, loadCalls = 0, loadedQueries = 0, fail = 0;
        bool                         streamed = false;
        Ordering() { owner.effects = &effects; }
        void Request(EModelRequestType type, bool allow)
        {
            // These are explicit policy boundaries, not a positive CGame or
            // CStreaming factory. Ref operations use the actual SDK receiver.
            MtaGame::CAnimBlockRequestPolicy::Request(
                static_cast<std::uint32_t>(type), allow, [&] { return receiver.IsLoaded(); }, [&] { return receiver.GetIndex(); },
                [&]
                {
                    ++baseReads;
                    return 25575u;
                },
                [&](std::uint32_t model) -> CModelInfo*
                {
                    ++lookupReads;
                    effects.push_back(1);
                    Check(model == 25592, "shared blocking lookup keeps exact IFP base-plus-index arithmetic");
                    if (fail == 1)
                        throw BoundaryFailure(1);
                    return nullptr;
                },
                [&](std::uint32_t model, std::uint32_t flags)
                {
                    ++requestCalls;
                    effects.push_back(2);
                    Check(model == 25592 && flags == (type == BLOCKING ? 0x14u : 0x04u), "shared request boundary receives exact resource and policy flags");
                    if (fail == 2)
                        throw BoundaryFailure(2);
                },
                [&] { receiver.AddRef(); },
                [&](bool priority)
                {
                    ++loadCalls;
                    effects.push_back(4);
                    Check(priority && owner.adds == 1 && owner.refs == 1, "priority load observes the real SDK AddRef already accepted exactly once");
                    if (fail == 4)
                        throw BoundaryFailure(4);
                    owner.loaded = streamed = true;
                },
                [&](std::uint32_t model)
                {
                    ++loadedQueries;
                    Check(model == 25592, "shared standard assertion uses the same resource index");
                    return streamed;
                });
        }
    };
    void RequestOrderingChecks()
    {
        {
            Ordering s;
            s.owner.loaded = true;
            s.owner.refs = 9;
            s.Request(BLOCKING, false);
            Check(s.owner.metadata == 1 && s.owner.refs == 9 && !s.owner.adds && !s.baseReads && s.effects.empty(),
                  "loaded short circuit invokes no ref, request, lookup or load boundary");
        }
        {
            Ordering s;
            s.Request(NON_BLOCKING, true);
            Check(s.effects == std::vector<int>({2, 3}) && s.owner.adds == 1 && s.receiver.GetRefs() == 1 && !s.lookupReads && !s.loadCalls,
                  "nonblocking shared policy couples request then actual SDK AddRef with no blocking or fake game factory");
        }
        for (bool allow : {false, true})
        {
            Ordering s;
            s.Request(BLOCKING, allow);
            Check(s.effects == std::vector<int>({1, 2, 3, 4}) && s.owner.adds == 1 && s.receiver.GetRefs() == 1 && s.lookupReads == 1 && s.loadCalls == 1 &&
                      s.loadedQueries == (Assertions && !allow ? 1u : 0u),
                  "blocking shared policy preserves nullable lookup/request/actual SDK ref/load order and standard assertion observation elision");
        }
        for (unsigned failed : {1u, 2u, 4u})
        {
            Ordering s;
            s.fail = failed;
            bool threw = false;
            try
            {
                s.Request(BLOCKING, true);
            }
            catch (const BoundaryFailure& error)
            {
                threw = error.stage == failed;
            }
            const std::vector<int> expected = failed == 1 ? std::vector<int>{1} : failed == 2 ? std::vector<int>{1, 2} : std::vector<int>{1, 2, 3, 4};
            Check(threw && s.effects == expected && s.owner.adds == (failed == 4 ? 1u : 0u) && s.receiver.GetRefs() == (failed == 4 ? 1u : 0u),
                  "a boundary exception preserves its accepted prefix and neither adds an early ref nor rolls back a completed native ref");
        }
        {
            Ordering s;
            s.owner.addResult = ClockResult::WriteFailure;
            s.owner.commitOnFailure = true;
            Check(Refused([&] { s.Request(BLOCKING, true); }, "AddRef", ClockResult::WriteFailure) && s.effects == std::vector<int>({1, 2, 3}) &&
                      s.owner.adds == 1 && s.receiver.GetRefs() == 1 && !s.loadCalls,
                  "unknown or accepted AddRef failure stops the shared itinerary before load without replay or inverse decrement");
        }
        std::puts("Shared animation block request ref ordering fixture completed");
    }
}
static_assert(std::is_abstract_v<CAnimBlockDE> && std::is_abstract_v<CAnimBlockRequestDE>);
static_assert(std::is_same_v<decltype(&CAnimBlockRequestDE::AddRef), void (CAnimBlockDE::*)()>);
static_assert(std::is_same_v<decltype(&CAnimBlockRequestDE::GetRefs), unsigned short (CAnimBlockDE::*)()>);
int main()
{
    try
    {
        SdkChecks();
        RequestOrderingChecks();
        std::printf("Passed %u animation block refs SDK/shared-policy checks (%zu-bit; assertions=%u)\n", checks, sizeof(void*) * 8, Assertions ? 1u : 0u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
