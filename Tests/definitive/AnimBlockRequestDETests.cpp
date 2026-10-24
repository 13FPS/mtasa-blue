/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Test actual shared animation request and borrowed SDK dispatch
 *****************************************************************************/
#include "../../Client/game_de/CAnimBlockRequestDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <game/CAnimBlockRequestPolicy.h>
#include <game/CGame.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached animation request fixture\n", stderr);
        std::abort();
    }
}
#include "AnimBlockRequestFixture.generated.h"

namespace
{
    using namespace mta::game_de;
#if defined(NDEBUG)
    constexpr bool Assertions = false;
#else
    constexpr bool Assertions = true;
#endif
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }
    struct Failure : std::runtime_error
    {
        explicit Failure(int stage) : std::runtime_error("existing animation SDK owner refused"), stage(stage) {}
        int stage;
    };
    template <class Operation>
    void FailsAt(Operation operation, int stage)
    {
        try
        {
            operation();
        }
        catch (const Failure& failure)
        {
            Check(failure.stage == stage, "exact original SDK/native effect exception propagates");
            return;
        }
        Check(false, "required original exception cannot be swallowed");
    }
    template <class Operation>
    void Refuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, "required borrowed SDK receiver refuses explicitly");
            return;
        }
        Check(false, "unavailable borrowed SDK/native owner cannot silently succeed");
    }
    // Policy callables must be well-formed returning operations under /WX.
    // A runtime guard still fails forbidden calls without teaching MSVC that
    // a template's following statements can never be reached.
    volatile bool forbiddenPolicyCall = true;
    void          UnexpectedPolicyCall()
    {
        if (forbiddenPolicyCall)
            fixture::Unqualified();
    }
    void SharedPolicyChecks()
    {
        for (auto rawType : std::array<std::uint32_t, 3>{1u, 2u, 0xffffffffu})
        {
            unsigned         observations = 0, indices = 0, bases = 0, refs = 0;
            std::vector<int> effects;
            MtaGame::CAnimBlockRequestPolicy::Request(
                rawType, false,
                [&]
                {
                    ++observations;
                    return false;
                },
                [&]
                {
                    ++indices;
                    return -3;
                },
                [&]
                {
                    ++bases;
                    return 10u;
                },
                [&](std::uint32_t) { UnexpectedPolicyCall(); },
                [&](std::uint32_t model, std::uint32_t flags)
                {
                    Check(model == 7 && flags == 0x04, "raw nonblocking intents preserve DWORD index conversion and exact flags");
                    effects.push_back(1);
                },
                [&]
                {
                    ++refs;
                    effects.push_back(2);
                },
                [&](bool) { UnexpectedPolicyCall(); },
                [&](std::uint32_t) -> bool
                {
                    UnexpectedPolicyCall();
                    return false;
                });
            Check(observations == 1 && indices == 1 && bases == 1 && refs == 1 && effects == std::vector<int>({1, 2}),
                  "every raw nonblocking intent has only request then genuine ref effect, without enum fabrication or blocking lookup");
        }
        for (bool allow : {false, true})
        {
            unsigned         observations = 0, hasModelReads = 0;
            std::vector<int> effects;
            MtaGame::CAnimBlockRequestPolicy::Request(
                static_cast<std::uint32_t>(BLOCKING), allow, [&] { return ++observations > 1; }, [] { return -1; }, [] { return 1u; },
                [&](std::uint32_t model) -> CModelInfo*
                {
                    Check(model == 0, "DWORD model arithmetic wraps rather than clamps");
                    effects.push_back(1);
                    return nullptr;
                },
                [&](std::uint32_t model, std::uint32_t flags)
                {
                    Check(model == 0 && flags == 0x14, "blocking request keeps exact model and flags");
                    effects.push_back(2);
                },
                [&] { effects.push_back(3); },
                [&](bool priority)
                {
                    Check(priority, "blocking animation load uses priority-only true");
                    effects.push_back(4);
                },
                [&](std::uint32_t model)
                {
                    Check(model == 0, "standard assertion observes the selected model");
                    ++hasModelReads;
                    return true;
                });
            Check(effects == std::vector<int>({1, 2, 3, 4}), "unused nullable lookup precedes request/ref/load without becoming an availability gate");
            const auto asserted = Assertions && !allow;
            Check(observations == (asserted ? 2u : 1u) && hasModelReads == (asserted ? 1u : 0u),
                  "NDEBUG and allowBlockingFail suppress assertion observations themselves");
        }
        unsigned observations = 0;
        MtaGame::CAnimBlockRequestPolicy::Request(
            0u, false,
            [&]
            {
                ++observations;
                return true;
            },
            []() -> int
            {
                UnexpectedPolicyCall();
                return 0;
            },
            []() -> std::uint32_t
            {
                UnexpectedPolicyCall();
                return 0;
            },
            [](std::uint32_t) { UnexpectedPolicyCall(); }, [](std::uint32_t, std::uint32_t) { UnexpectedPolicyCall(); }, [] { UnexpectedPolicyCall(); },
            [](bool) { UnexpectedPolicyCall(); },
            [](std::uint32_t) -> bool
            {
                UnexpectedPolicyCall();
                return false;
            });
        Check(observations == 1, "already loaded block short-circuits before index/base/lookup/request/ref/load");
        std::puts("Production shared animation block request policy fixture completed");
    }

    struct Trace
    {
        std::vector<int> effects;
        unsigned         gameReads = 0, baseReads = 0, lookups = 0, refs = 0, blockReads = 0, streamQueries = 0;
        std::uint32_t    model = 0, flags = 0;
        std::uint32_t    base = 25000;
        int              failStage = 0;
        bool             blockLoaded = false, streamed = false, loadCompletes = true;
        ClockResult      blockStatus = ClockResult::Ok, requestStatus = ClockResult::Ok, loadStatus = ClockResult::Ok;
    };
    class BlockOwner final : public NativeAnimationBlockAccess
    {
    public:
        BlockOwner(Trace& trace, const NativeAnimationBlockIdentity& identity) : trace(trace), expected(identity) {}
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity& identity, NativeAnimationBlockValues& output) noexcept override
        {
            ++trace.blockReads;
            if (trace.blockStatus != ClockResult::Ok)
                return trace.blockStatus;
            if (identity.index != expected.index || identity.name != expected.name || identity.ownerGeneration != expected.ownerGeneration)
                return ClockResult::BindingChanged;
            NativeAnimationBlockValues value;
            value.index = expected.index;
            value.loaded = trace.blockLoaded ? 1 : 0;
            output = value;
            return ClockResult::Ok;
        }
        Trace&                             trace;
        const NativeAnimationBlockIdentity expected;
    };
    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        explicit StreamingOwner(Trace& trace) : trace(trace) {}
        ClockResult RequestStreamingModel(std::uint32_t model, std::uint32_t flags) noexcept override
        {
            trace.effects.push_back(2);
            trace.model = model;
            trace.flags = flags;
            return trace.requestStatus;
        }
        ClockResult LoadStreamingModels(bool priority) noexcept override
        {
            trace.effects.push_back(priority ? 4 : -4);
            if (trace.loadStatus != ClockResult::Ok)
                return trace.loadStatus;
            if (trace.loadCompletes)
                trace.blockLoaded = trace.streamed = true;
            return ClockResult::Ok;
        }
        ClockResult ReadStreamingModelLoaded(std::uint32_t model, NativeModelLoadedReader::Value& output) noexcept override
        {
            ++trace.streamQueries;
            trace.model = model;
            NativeModelLoadedReader::Value value;
            value.streamedIn = trace.streamed;
            output = value;
            return ClockResult::Ok;
        }
        Trace& trace;
    };
    class Streaming final : public fixture::StreamingFixtureBase
    {
    public:
        explicit Streaming(NativeStreamingModelAccess& access) : StreamingFixtureBase(access) {}
    };
    class Game final : public fixture::GameFixtureBase
    {
    public:
        explicit Game(Trace& trace) : trace(trace) {}
        CStreaming* GetStreaming() override
        {
            ++trace.gameReads;
            return streaming;
        }
        std::uint32_t GetBaseIDforIFP() override
        {
            ++trace.baseReads;
            return trace.base;
        }
        CModelInfo* GetModelInfo(DWORD model, bool canBeInvalid) override
        {
            Check(!canBeInvalid, "blocking animation lookup keeps the original default validity argument");
            ++trace.lookups;
            trace.model = model;
            trace.effects.push_back(1);
            if (trace.failStage == 1)
                throw Failure(1);
            // Classic ignores this pointer; null is not a model fallback.
            return nullptr;
        }
        Trace&      trace;
        CStreaming* streaming = nullptr;
    };
    class Block final : public fixture::BlockFixtureBase
    {
    public:
        Block(NativeAnimationBlockAccess& native, const NativeAnimationBlockIdentity& identity, CGame& game, Trace& trace)
            : BlockFixtureBase(native, identity, game), trace(trace)
        {
        }
        void AddRef() override
        {
            ++trace.refs;
            trace.effects.push_back(3);
            if (trace.failStage == 3)
                throw Failure(3);
        }
        Trace& trace;
    };
    NativeAnimationBlockIdentity Identity()
    {
        NativeAnimationBlockIdentity value;
        value.index = 5;
        value.name[0] = 'w';
        value.name[1] = 'a';
        value.name[2] = 'l';
        value.name[3] = 'k';
        value.ownerGeneration = 42;
        return value;
    }
    struct Scenario
    {
        Trace                        trace;
        NativeAnimationBlockIdentity identity = Identity();
        BlockOwner                   blockOwner{trace, identity};
        StreamingOwner               streamingOwner{trace};
        Streaming                    streaming{streamingOwner};
        Game                         game{trace};
        Block                        block{blockOwner, identity, game, trace};
        Scenario() { game.streaming = &streaming; }
    };
    static_assert(!std::is_abstract_v<Block> && std::is_base_of_v<CAnimBlock, Block>);
    static_assert(!std::is_abstract_v<CAnimBlockRequestDE>);

    void SdkChecks()
    {
        {
            Scenario s;
            s.trace.blockLoaded = true;
            CAnimBlock& sdk = s.block;
            sdk.Request(BLOCKING, false);
            Check(s.trace.blockReads == 1 && !s.trace.baseReads && !s.trace.gameReads && s.trace.effects.empty(),
                  "actual loaded SDK block returns before game service lookup and all effects");
        }
        for (bool allow : {false, true})
        {
            Scenario s;
            // In release deliberately leave completion false: standard assert
            // expressions must not become mandatory native observations.
            s.trace.loadCompletes = Assertions && !allow;
            CAnimBlock& sdk = s.block;
            sdk.Request(BLOCKING, allow);
            Check(s.trace.effects == std::vector<int>({1, 2, 3, 4}) && s.trace.model == 25005 && s.trace.flags == 0x14 && s.trace.lookups == 1 &&
                      s.trace.refs == 1 && s.trace.baseReads == 1,
                  "actual CGame/streaming/block SDK virtuals preserve unused lookup then request/ref/priorityload");
            const auto asserted = Assertions && !allow;
            Check(s.trace.blockReads == (asserted ? 3u : 2u) && s.trace.streamQueries == (asserted ? 1u : 0u) && s.trace.gameReads == (asserted ? 3u : 2u),
                  "actual borrowed owner assertion reads occur only in assertion-enabled non-allowing build");
        }
        {
            Scenario s;
            s.trace.base = 0xfffffffeu;
            CAnimBlock& sdk = s.block;
            sdk.Request(NON_BLOCKING, false);
            Check(s.trace.effects == std::vector<int>({2, 3}) && s.trace.model == 3 && s.trace.flags == 0x04 && s.trace.refs == 1 && s.trace.blockReads == 2 &&
                      !s.trace.lookups && !s.trace.streamQueries && s.trace.gameReads == 1,
                  "actual nonblocking SDK preserves DWORD wrap and skips unused lookup/blockingload/assertions");
        }
        {
            Scenario s;
            s.trace.failStage = 1;
            FailsAt([&] { s.block.Request(BLOCKING, true); }, 1);
            Check(s.trace.effects == std::vector<int>{1} && !s.trace.refs && !s.trace.gameReads,
                  "blocking unused lookup exception precedes request/ref/load and is not suppressed");
        }
        {
            Scenario s;
            s.game.streaming = nullptr;
            Refuses([&] { s.block.Request(BLOCKING, true); });
            Check(s.trace.effects == std::vector<int>{1} && !s.trace.refs && s.trace.gameReads == 1,
                  "missing mandatory streaming receiver retains lookup prefix and refuses request");
        }
        {
            Scenario s;
            s.trace.requestStatus = ClockResult::NotInitialized;
            Refuses([&] { s.block.Request(BLOCKING, true); });
            Check(s.trace.effects == std::vector<int>({1, 2}) && !s.trace.refs,
                  "genuine streaming request refusal prevents native AddRef and blockingload without rollback");
        }
        {
            Scenario s;
            s.trace.failStage = 3;
            FailsAt([&] { s.block.Request(BLOCKING, true); }, 3);
            Check(s.trace.effects == std::vector<int>({1, 2, 3}) && s.trace.refs == 1 && s.trace.gameReads == 1,
                  "entered required native AddRef failure retains accepted request/ref prefix and never loads/retries");
        }
        {
            Scenario s;
            s.trace.loadStatus = ClockResult::OriginalException;
            Refuses([&] { s.block.Request(BLOCKING, false); });
            Check(s.trace.effects == std::vector<int>({1, 2, 3, 4}) && s.trace.refs == 1 && s.trace.blockReads == 2 && !s.trace.streamQueries,
                  "genuine load failure preserves accepted AddRef and stops before standard assertions");
        }
        {
            Scenario s;
            s.trace.blockStatus = ClockResult::BindingChanged;
            Refuses([&] { s.block.Request(NON_BLOCKING, true); });
            Check(s.trace.blockReads == 1 && !s.trace.baseReads && s.trace.effects.empty(),
                  "native block owner refusal cannot masquerade as an unloaded block and start effects");
        }
        std::puts("Production actual animation block request SDK fixture completed");
    }
}

int main()
{
    SharedPolicyChecks();
    SdkChecks();
    std::printf("Passed %u actual animation request SDK checks (%zu-bit; assertions=%u)\n", checks, sizeof(void*) * 8, Assertions ? 1u : 0u);
    return EXIT_SUCCESS;
}
