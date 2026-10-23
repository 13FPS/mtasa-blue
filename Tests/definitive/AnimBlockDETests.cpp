/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual abstract SDK getter dispatch, raw32 count widening and refusal
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlockDE.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <stdexcept>
using namespace mta::game_de;
namespace
{
    unsigned checks = 0;
    void     Check(bool ok, const char* why)
    {
        ++checks;
        if (!ok)
        {
            std::fprintf(stderr, "FAIL %u: %s\n", checks, why);
            std::exit(1);
        }
    }
    struct Owner : NativeAnimationBlockAccess
    {
        unsigned                     calls = 0;
        ClockResult                  result = ClockResult::Ok;
        NativeAnimationBlockIdentity received{};
        NativeAnimationBlockValues   value{17, -23, 0xffffffffu, 1};
        ClockResult                  ReadAnimationBlock(const NativeAnimationBlockIdentity& identity, NativeAnimationBlockValues& output) noexcept override
        {
            ++calls;
            received = identity;
            output = value;
            return result;
        }
    };
    class Receiver final : public CAnimBlockDE
    {
    public:
        Receiver(NativeAnimationBlockAccess& access, const NativeAnimationBlockIdentity& identity) : CAnimBlockDE(access, identity) {}
        CAnimBlockSAInterface*          GetInterface() override { std::abort(); }
        char*                           GetName() override { std::abort(); }
        void                            AddRef() override { std::abort(); }
        unsigned short                  GetRefs() override { std::abort(); }
        void                            Request(EModelRequestType, bool) override { std::abort(); }
        CAnimBlendHierarchySAInterface* GetAnimationHierarchyInterface(std::size_t) override { std::abort(); }
    };
    template <class F>
    void Refused(Owner& owner, F operation)
    {
        const auto before = owner.calls;
        bool       threw = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && owner.calls == before + 1, "actual SDK getter propagates refusal once, without retry or using refused output");
    }
}
int main()
{
    NativeAnimationBlockIdentity identity;
    identity.index = 17;
    identity.name[0] = 'P';
    identity.name[1] = 'E';
    identity.name[2] = 'D';
    identity.ownerGeneration = 0xfedcba9876543210ull;
    Owner    owner;
    Receiver sdk(owner, identity);
    Check(owner.calls == 0, "construction copies identity without native reads, refs, streaming requests or writes");
    identity.name[0] = 'X';
    identity.index = 99;
    identity.ownerGeneration = 0;
    CAnimBlock& base = sdk;
    Check(base.GetIndex() == 17 && owner.calls == 1 && owner.received.index == 17 && owner.received.name[0] == 'P' &&
              owner.received.ownerGeneration == 0xfedcba9876543210ull,
          "actual SDK base dispatch preserves immutable copied full identity and callback generation");
    for (const auto raw : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{0x80}, std::uint8_t{0xff}})
    {
        owner.value.loaded = raw;
        const auto before = owner.calls;
        Check(base.IsLoaded() == (raw != 0) && owner.calls == before + 1,
              "loaded getter observes fresh raw nonzero block byte independently from count or groups");
    }
    for (const auto offset : {std::int32_t{0}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
    {
        owner.value.idOffset = offset;
        const auto before = owner.calls;
        Check(base.GetIDOffset() == offset && owner.calls == before + 1,
              "signed raw32 animation offset is not normalized or converted to an opaque hierarchy pointer");
    }
    for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{0x7fffffff}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        owner.value.animationCount = count;
        const auto before = owner.calls;
        Check(base.GetAnimationCount() == static_cast<std::size_t>(count) && owner.calls == before + 1,
              "SDK size_t zeroextends genuine classic/native32bit count without reading neighboring assoc data or signextension");
    }
    for (const auto status :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::InvalidArgument})
    {
        owner.result = status;
        Refused(owner, [&] { base.GetIndex(); });
        Refused(owner, [&] { base.IsLoaded(); });
        Refused(owner, [&] { base.GetIDOffset(); });
        Refused(owner, [&] { base.GetAnimationCount(); });
    }
    owner.result = ClockResult::Ok;
    owner.value.index = 18;
    Refused(owner, [&] { base.GetIndex(); });
    owner.value.index = 0xffffffffu;
    Refused(owner, [&] { base.GetIndex(); });
    NativeAnimationBlockIdentity bad;
    bad.name.fill('x');
    const auto before = owner.calls;
    bool       rejected = false;
    try
    {
        Receiver invalid(owner, bad);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Check(rejected && owner.calls == before, "unterminated identity refuses before native access; copied name does not implement mutable GetName");
    NativeAnimationBlockAccess unavailable;
    bad.name[0] = 0;
    Receiver missing(unavailable, bad);
    bool     refused = false;
    try
    {
        missing.GetIndex();
    }
    catch (const std::runtime_error&)
    {
        refused = true;
    }
    Check(refused, "the default native interface cannot create valid metadata from CPU fallback");
    std::puts("Actual animation block metadata SDK fixture completed");
    std::printf("Passed %u animation block SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
