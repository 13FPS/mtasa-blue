/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact three-mode SDK transport with protected CPU-only lifetime
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "PrivateAnimationAssociationFixture.generated.h"
using namespace mta::game_de;
static_assert(!std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_default_constructible_v<CAnimBlendAssociationDE> &&
              !std::is_destructible_v<CAnimBlendAssociationDE> && !std::has_virtual_destructor_v<CAnimBlendAssociation>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::Init), decltype(&CAnimBlendAssociationDE::Init)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::InitializeForCustomAnimation),
                             decltype(&CAnimBlendAssociationDE::InitializeForCustomAnimation)>);
namespace
{
    using Operation = NativeAnimationAssociationClumpOperation;
    using Token = NativeAnimationAssociationReferenceToken;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    bool Same(const Token& a, const Token& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial;
    }
    const char* Tag(Operation op)
    {
        return op == Operation::Construct ? "Constructor(clump)" : op == Operation::Initialize ? "Init" : "InitializeForCustomAnimation";
    }
    struct Owner final : NativeAnimationAssociationAccess
    {
        Token          received{}, *callerToken = nullptr;
        Operation      operation{Operation::Construct};
        std::uintptr_t clump = 0, hierarchy = 0, returnTag = 0, *callerClump = nullptr, *callerHierarchy = nullptr;
        ClockResult    status = ClockResult::Ok;
        unsigned       calls = 0, reads = 0, freeCalls = 0, staticConstructs = 0, creates = 0, retires = 0, scalarCalls = 0;
        bool           outputsInitiallyZero = true;
        ClockResult    ApplyPrivateAnimationAssociationClump(const Token& token, Operation op, std::uintptr_t c, std::uintptr_t h,
                                                             std::uintptr_t& output) noexcept override
        {
            ++calls;
            outputsInitiallyZero = outputsInitiallyZero && output == 0;
            if (callerToken)
                *callerToken = {};
            if (callerClump)
                *callerClump = 0;
            if (callerHierarchy)
                *callerHierarchy = 0;
            received = token;
            operation = op;
            clump = c;
            hierarchy = h;
            output = returnTag;
            return status;
        }
        ClockResult ReadPrivateAnimationAssociation(const Token&, NativeAnimationAssociationValues&) noexcept override
        {
            ++reads;
            return ClockResult::ReadFailure;
        }
        ClockResult CreatePrivateAnimationAssociation(std::uint64_t, Token&) noexcept override
        {
            ++creates;
            return ClockResult::NotInitialized;
        }
        ClockResult ConstructPrivateAnimationAssociationFromStatic(const Token&, std::uintptr_t, std::uintptr_t&) noexcept override
        {
            ++staticConstructs;
            return ClockResult::NotInitialized;
        }
        ClockResult FreePrivateAnimationAssociationNodeArray(const Token&) noexcept override
        {
            ++freeCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult RetirePrivateAnimationAssociation(const Token&) noexcept override
        {
            ++retires;
            return ClockResult::NotInitialized;
        }
        ClockResult SetPrivateAnimationAssociationBlendAmount(const Token&, std::uint32_t) noexcept override
        {
            ++scalarCalls;
            return ClockResult::WriteFailure;
        }
        ClockResult SetPrivateAnimationAssociationCurrentSpeed(const Token&, std::uint32_t) noexcept override
        {
            ++scalarCalls;
            return ClockResult::WriteFailure;
        }
        ClockResult SetPrivateAnimationAssociationAnimID(const Token&, std::int16_t) noexcept override
        {
            ++scalarCalls;
            return ClockResult::WriteFailure;
        }
        ClockResult SetPrivateAnimationAssociationAnimGroup(const Token&, std::int16_t) noexcept override
        {
            ++scalarCalls;
            return ClockResult::WriteFailure;
        }
        ClockResult SetPrivateAnimationAssociationFlags(const Token&, std::int16_t) noexcept override
        {
            ++scalarCalls;
            return ClockResult::WriteFailure;
        }
    };
    std::uintptr_t Invoke(CAnimBlendAssociation& sdk, Operation op, std::uintptr_t clump, std::uintptr_t hierarchy)
    {
        auto* c = reinterpret_cast<RpClump*>(clump);
        auto* h = reinterpret_cast<CAnimBlendHierarchySAInterface*>(hierarchy);
        if (op == Operation::Construct)
            return reinterpret_cast<std::uintptr_t>(sdk.Constructor(c, h));
        if (op == Operation::InitializeCustom)
            return reinterpret_cast<std::uintptr_t>(sdk.InitializeForCustomAnimation(c, h));
        sdk.Init(c, h);
        return 0;
    }
    void Refuses(CAnimBlendAssociation& sdk, Operation op, ClockResult expected)
    {
        bool refused = false;
        try
        {
            (void)Invoke(sdk, op, 0, 0);
        }
        catch (const std::runtime_error& e)
        {
            const std::string message = e.what(), suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = message.find(Tag(op)) != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused, "one owner refusal must preserve the exact operation tag and result");
    }
    void Cases()
    {
        constexpr Token                                    original{0xfedcba9876543210ull, 0x8877665544332211ull};
        constexpr auto                                     high = (~std::uintptr_t{0}) - 0x1234u;
        const std::array<std::array<std::uintptr_t, 2>, 4> args{{{{0, 0}}, {{high, 0}}, {{0, high - 32}}, {{high, high - 64}}}};
        std::array<unsigned char, 104>                     untouched{};
        untouched.fill(0xa5);
        const auto before = untouched;
        Owner      owner;
        auto       callerToken = original;
        {
            PrivateAnimationAssociationReceiver receiver(owner, callerToken);
            CAnimBlendAssociation&              sdk = receiver;
            for (const auto op : {Operation::Construct, Operation::Initialize, Operation::InitializeCustom})
                for (std::size_t i = 0; i < args.size(); ++i)
                {
                    auto       c = args[i][0], h = args[i][1];
                    const auto copiedC = c, copiedH = h;
                    owner.callerToken = &callerToken;
                    owner.callerClump = &c;
                    owner.callerHierarchy = &h;
                    owner.returnTag = i ? high - static_cast<std::uintptr_t>(i) : 0;
                    const auto calls = owner.calls;
                    const auto returned = Invoke(sdk, op, c, h);
                    Check(owner.calls == calls + 1 && Same(owner.received, original) && owner.operation == op && owner.clump == copiedC &&
                              owner.hierarchy == copiedH && !callerToken.ownerGeneration && !c && !h && owner.outputsInitiallyZero,
                          "actual clump method sends one copied issued token/mode/fulluintptr request including nulls before callbacks");
                    Check(returned == (op == Operation::Initialize ? 0 : owner.returnTag),
                          "constructor/custom preserve owner return bits without interface fallback; void Init ignores output");
                    Check(untouched == before, "frontend never dereferences or writes raw clump/H/receiver tags");
                }
            owner.callerToken = nullptr;
            owner.callerClump = nullptr;
            owner.callerHierarchy = nullptr;
            for (const auto op : {Operation::Construct, Operation::Initialize, Operation::InitializeCustom})
                for (const auto failure :
                     {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged,
                      ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy,
                      ClockResult::Expired, ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
                {
                    owner.status = failure;
                    const auto calls = owner.calls;
                    Refuses(sdk, op, failure);
                    Check(owner.calls == calls + 1 && owner.outputsInitiallyZero && Same(owner.received, original),
                          "failed operations do not retry, reuse an old return value or redirect receiver identity");
                }
        }
        Check(!owner.reads && !owner.freeCalls && !owner.staticConstructs && !owner.creates && !owner.retires && !owner.scalarCalls && untouched == before,
              "all SDK methods and concrete CPU destruction avoid default setters, old-node free, native creation/retirement and second interface reads");
        struct Defaults final : NativeAnimationAssociationAccess
        {
        };
        Defaults                            absent;
        PrivateAnimationAssociationReceiver unconfigured(absent, original);
        for (const auto op : {Operation::Construct, Operation::Initialize, Operation::InitializeCustom})
            Refuses(unconfigured, op, ClockResult::NotInitialized);
        std::puts("Actual animation association three clump SDK fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u association clump SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u clump SDK checks: %s\n", checks, e.what());
        return 1;
    }
}
