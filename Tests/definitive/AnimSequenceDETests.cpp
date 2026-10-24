/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual abstract sequence SDK dispatch and source-owned union policy
 ****************************************************************************/
#include "AnimationSequenceDEFixture.h"
#include "../../Client/game_de/NativeAnimationSequenceLogicalState.h"
#include <array>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
    using namespace mta::game_de;
    using animation_sequence_fixture::AnimationSequenceReceiver;
    unsigned checks = 0;

    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }

    bool Same(const NativeAnimationSequenceReferenceToken& a, const NativeAnimationSequenceReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.firstTag == b.firstTag && a.index == b.index;
    }

    struct Owner final : NativeAnimationSequenceAccess
    {
        enum Channel : unsigned
        {
            Initialize,
            Bone,
            Read,
            Hash
        };
        NativeAnimationSequenceReferenceToken expected{}, seen{};
        NativeAnimationSequenceValues         values{};
        ClockResult                           result = ClockResult::Ok;
        std::uint32_t                         hash = 0;
        std::int32_t                          lastBone = 0;
        std::array<unsigned, 4>               calls{};
        bool                                  correctIdentity = true;

        void Observe(const NativeAnimationSequenceReferenceToken& token, Channel channel) noexcept
        {
            seen = token;
            correctIdentity = correctIdentity && Same(token, expected);
            ++calls[channel];
        }
        unsigned Total() const noexcept { return calls[0] + calls[1] + calls[2] + calls[3]; }
        void     Reset() noexcept
        {
            calls = {};
            correctIdentity = true;
        }
        ClockResult InitializeAnimationSequence(const NativeAnimationSequenceReferenceToken& token) noexcept override
        {
            Observe(token, Initialize);
            return result;
        }
        ClockResult SetAnimationSequenceBoneTag(const NativeAnimationSequenceReferenceToken& token, std::int32_t value) noexcept override
        {
            Observe(token, Bone);
            lastBone = value;
            return result;
        }
        ClockResult ReadAnimationSequence(const NativeAnimationSequenceReferenceToken& token, NativeAnimationSequenceValues& output) noexcept override
        {
            Observe(token, Read);
            output = values;
            return result;
        }
        ClockResult ReadAnimationSequenceHash(const NativeAnimationSequenceReferenceToken& token, std::uint32_t& output) noexcept override
        {
            Observe(token, Hash);
            output = hash;
            return result;
        }
    };

    template <class Operation>
    void Refused(Owner& owner, const char* name, Owner::Channel channel, ClockResult status, Operation operation)
    {
        owner.Reset();
        owner.result = status;
        bool refused = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE custom animation sequence owner refused ") + name + ": " + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "typed owner refusal was hidden or lost method/status context");
        Check(owner.Total() == 1 && owner.calls[channel] == 1, "refusal retried, queried another channel or attempted an inverse");
        Check(owner.correctIdentity, "refusal changed the copied issued-row identity");
    }

    void LogicalPolicy()
    {
        NativeAnimationSequenceLogicalState state;
        std::uint32_t                       hash = 0x12345678u;
        std::uint16_t                       bone = 0x4321u;
        Check(!state.TryReadHash(hash) && hash == 0x12345678u, "fresh undefined hash became a synthetic value");
        Check(!state.TryReadBoneTag(bone) && bone == 0x4321u, "fresh undefined bone changed output");
        Check(state.DefinedBits() == 0 && !state.UsesBoneTag(), "fresh logical state invented a defined union");
        state.Initialize();
        Check(state.TryReadBoneTag(bone) && bone == 0xffffu, "Initialize did not establish exactly low WORD ffff");
        Check(state.DefinedBits() == 0xffffu && !state.TryReadHash(hash), "Initialize defined uninitialized high hash bits");
        Check(!state.UsesBoneTag(), "Initialize retained the old bone selector");
        state.SetBoneTag(-1);
        Check(!state.UsesBoneTag() && state.DefinedBits() == 0xffffu, "full-width -1 changed logical state");
        state.SetBoneTag(65535);
        Check(state.UsesBoneTag() && state.TryReadBoneTag(bone) && bone == 0xffffu, "65535 was conflated with full-width -1");
        Check(!state.TryReadHash(hash), "SetBoneTag invented the unknown upper WORD");
        state.SetNameHash(0x89abcdefu);
        Check(state.TryReadHash(hash) && hash == 0x89abcdefu && state.DefinedBits() == 0xffffffffu, "logical SetName hash lost full raw bits");
        Check(state.UsesBoneTag(), "original logical SetName unexpectedly cleared the selector");
        Check(state.TryReadBoneTag(bone) && bone == 0xcdefu, "GetBoneTag did not preserve actual raw union low WORD after SetName");
        state.Initialize();
        Check(state.TryReadHash(hash) && hash == 0x89abffffu && !state.UsesBoneTag(), "Initialize did not preserve defined high hash bits");
        const std::array<std::int32_t, 7> input{
            {0, 1, 65535, 65536, -65536, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()}};
        for (auto value : input)
        {
            state.SetBoneTag(value);
            Check(state.TryReadHash(hash) && hash == (0x89ab0000u | static_cast<std::uint16_t>(value)),
                  "SetBoneTag did not preserve high bits/modulo low bits");
            Check(state.TryReadBoneTag(bone) && bone == static_cast<std::uint16_t>(value) && state.UsesBoneTag(), "logical bone/selector mismatch");
        }
        auto copy = state;
        state.SetNameHash(0);
        Check(copy.TryReadHash(hash) && hash == 0x89abffffu && copy.UsesBoneTag(), "copy lost logical value, definedness or selector");
        copy.Initialize();
        Check(state.TryReadHash(hash) && hash == 0 && state.UsesBoneTag(), "copied logical value shared mutable CPU storage");
        std::puts("Actual MTA logical sequence union policy completed");
    }

    void DefaultRefusal()
    {
        NativeAnimationSequenceAccess         missing;
        NativeAnimationSequenceReferenceToken token{};
        NativeAnimationSequenceValues         output{0x1000u, 0x2000u, 0x1234u, 0x5678u, true};
        std::uint32_t                         hash = 0x89abcdefu;
        Check(missing.InitializeAnimationSequence(token) == ClockResult::NotInitialized, "default Initialize did not refuse");
        Check(missing.SetAnimationSequenceBoneTag(token, -1) == ClockResult::NotInitialized, "default SetBoneTag did not refuse");
        Check(missing.ReadAnimationSequence(token, output) == ClockResult::NotInitialized && output.interfaceAddress == 0x1000u &&
                  output.keyFrames == 0x2000u && output.frameCount == 0x1234u && output.boneTag == 0x5678u && output.bigChunk,
              "default row read changed the caller's output");
        Check(missing.ReadAnimationSequenceHash(token, hash) == ClockResult::NotInitialized && hash == 0x89abcdefu,
              "default hash read changed the caller's output");
    }

    void ActualDispatch()
    {
        Owner                                 owner;
        const auto                            max = (std::numeric_limits<std::uintptr_t>::max)();
        NativeAnimationSequenceReferenceToken original{0xfedcba9876543210ull, max - 0x1000u, (std::numeric_limits<std::size_t>::max)() - 7u};
        owner.expected = original;
        AnimationSequenceReceiver receiver(owner, original);
        CAnimBlendSequence&       sdk = receiver;
        Check(owner.Total() == 0, "CPU construction called the native owner");
        original = {};
        sdk.Initialize();
        Check(owner.calls[Owner::Initialize] == 1 && owner.correctIdentity, "Initialize failed actual base dispatch/copied token");
        const std::array<std::int32_t, 8> inputs{
            {-1, 0, 1, 65535, 65536, -65536, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()}};
        for (auto value : inputs)
        {
            owner.Reset();
            sdk.SetBoneTag(value);
            Check(owner.Total() == 1 && owner.calls[Owner::Bone] == 1 && owner.lastBone == value && owner.correctIdentity,
                  "SetBoneTag narrowed, filtered or retried the original full-width int");
        }
        for (std::uint32_t value : {0u, 1u, 0x80000000u, 0xffffffffu})
        {
            owner.Reset();
            owner.hash = value;
            Check(sdk.GetHash() == value && owner.Total() == 1 && owner.calls[Owner::Hash] == 1 && owner.correctIdentity,
                  "GetHash filtered raw32 bits or queried physical name/row state");
        }
        for (std::uint16_t value : {std::uint16_t{0}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
        {
            owner.values = {max - 63u, max - 127u, value, value, value != 0};
            owner.Reset();
            Check(sdk.GetBoneTag() == value, "GetBoneTag changed logical low WORD");
            Check(sdk.GetKeyFramesCount() == value, "GetKeyFramesCount sign-extended or filtered WORD");
            Check(sdk.IsBigChunkForAllSequences() == (value != 0), "big chunk observation changed");
            Check(reinterpret_cast<std::uintptr_t>(sdk.GetKeyFrames()) == max - 127u, "keyframe opaque pointer was narrowed or dereferenced");
            Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == max - 63u, "same-backend opaque row pointer was narrowed or dereferenced");
            Check(owner.Total() == 5 && owner.calls[Owner::Read] == 5 && owner.correctIdentity, "row getters cached state or used another channel");
        }
        owner.values.keyFrames = 0;
        Check(sdk.GetKeyFrames() == nullptr, "successful native null frame pointer was replaced");

        const std::array<ClockResult, 14> failures{
            {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
             ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
             ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (auto status : failures)
        {
            Refused(owner, "Initialize", Owner::Initialize, status, [&] { sdk.Initialize(); });
            Refused(owner, "SetBoneTag", Owner::Bone, status, [&] { sdk.SetBoneTag(-1); });
            Refused(owner, "GetHash", Owner::Hash, status, [&] { sdk.GetHash(); });
            Refused(owner, "GetBoneTag", Owner::Read, status, [&] { sdk.GetBoneTag(); });
            Refused(owner, "GetKeyFrames", Owner::Read, status, [&] { sdk.GetKeyFrames(); });
            Refused(owner, "GetKeyFramesCount", Owner::Read, status, [&] { sdk.GetKeyFramesCount(); });
            Refused(owner, "IsBigChunkForAllSequences", Owner::Read, status, [&] { sdk.IsBigChunkForAllSequences(); });
            Refused(owner, "GetInterface", Owner::Read, status, [&] { sdk.GetInterface(); });
        }
        owner.result = ClockResult::Ok;
        owner.Reset();
        NativeAnimationSequenceReferenceToken secondToken{3, 0x2000u, 9};
        AnimationSequenceReceiver             second(owner, secondToken);
        owner.expected = secondToken;
        second.Initialize();
        Check(Same(owner.seen, secondToken), "second receiver reused another row's token");
        owner.expected = {0xfedcba9876543210ull, max - 0x1000u, (std::numeric_limits<std::size_t>::max)() - 7u};
        sdk.Initialize();
        Check(Same(owner.seen, owner.expected) && owner.correctIdentity, "another receiver mutated the first copied identity");
        owner.Reset();
        bool destroyed = false;
        struct Tracking final : AnimationSequenceReceiver
        {
            Tracking(Owner& owner, const NativeAnimationSequenceReferenceToken& token, bool& flag) : AnimationSequenceReceiver(owner, token), flag(flag) {}
            ~Tracking() override { flag = true; }
            bool& flag;
        };
        std::unique_ptr<CAnimBlendSequence> wrapper = std::make_unique<Tracking>(owner, owner.expected, destroyed);
        wrapper.reset();
        Check(destroyed && owner.Total() == 0, "base destruction queried native state or adopted row/frame cleanup");
        std::puts("Actual MTA issued-row sequence SDK fixture completed");
    }
}

int main()
{
    try
    {
        LogicalPolicy();
        DefaultRefusal();
        ActualDispatch();
        std::printf("Passed %u custom animation sequence SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Custom animation sequence SDK check failed: %s\n", error.what());
        return 1;
    }
}
