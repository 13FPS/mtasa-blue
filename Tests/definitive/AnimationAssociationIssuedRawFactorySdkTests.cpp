/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual causal raw-factory refusals and opaque identity transport
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "AnimationAssociationRawFactoryManagerFixture.generated.h"
using namespace mta::game_de;
static_assert(
    std::is_same_v<decltype(&AnimationAssociationRawFactoryManagerReceiver::GetAnimBlendAssociation), decltype(&CAnimManagerDE::GetAnimBlendAssociation)>);
static_assert(std::has_virtual_destructor_v<CAnimBlendAssociation> && !std::is_destructible_v<CAnimBlendAssociationDE>);
namespace
{
    bool                   failFacadeAllocation = false;
    unsigned               checks = 0, facadeAllocations = 0, facadeLive = 0;
    std::array<void*, 256> facadeStorage{};
}
void* operator new(std::size_t bytes)
{
    if (failFacadeAllocation && bytes == sizeof(CAnimBlendAssociationDE))
    {
        failFacadeAllocation = false;
        throw std::bad_alloc();
    }
    auto* const p = std::malloc(bytes ? bytes : 1);
    if (!p)
        throw std::bad_alloc();
    if (bytes == sizeof(CAnimBlendAssociationDE))
        for (auto& slot : facadeStorage)
            if (!slot)
            {
                slot = p;
                ++facadeAllocations;
                ++facadeLive;
                break;
            }
    return p;
}
void operator delete(void* p) noexcept
{
    if (p)
        for (auto& slot : facadeStorage)
            if (slot == p)
            {
                slot = nullptr;
                --facadeLive;
                break;
            }
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    ::operator delete(p);
}
namespace
{
    using Token = NativeAnimationAssociationReferenceToken;
    using Raw = CAnimBlendAssociationSAInterface*;
    void Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool Same(const Token& a, const Token& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial;
    }
    template <class F>
    void Refuses(F&& f, ClockResult expected, const char* operation)
    {
        bool refused = false;
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            const std::string text = e.what(), tail = ": " + std::to_string(static_cast<unsigned>(expected));
            refused =
                text.find(operation) != std::string::npos && text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "actual SDK refusal preserves operation and owner result");
    }
    constexpr std::array<ClockResult, 14> failures{ClockResult::InvalidArgument, ClockResult::NotInitialized,   ClockResult::WrongThread,
                                                   ClockResult::BindingChanged,  ClockResult::ReadFailure,      ClockResult::WriteFailure,
                                                   ClockResult::RestoreFailure,  ClockResult::PatchFailure,     ClockResult::Busy,
                                                   ClockResult::Expired,         ClockResult::UnsupportedBuild, ClockResult::OwnerConflict,
                                                   ClockResult::Stopped,         ClockResult::OriginalException};
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationAssociationAccess
    {
        ClockResult    resolveStatus = ClockResult::NotInitialized, identityStatus = ClockResult::Ok;
        unsigned       resolves = 0, identityReads = 0, scalarReads = 0, creates = 0, copies = 0, retires = 0, frees = 0;
        std::uint64_t  sentGeneration = 0;
        std::uintptr_t sentCandidate = 0, resultAddress = 0, identityAddress = 0;
        Token          resultToken{}, sentToken{};
        Raw*           mutateArgument = nullptr;
        Token*         mutateCallerToken = nullptr;
        const Token*   forbiddenCallerToken = nullptr;
        bool           aliased = false;
        ClockResult    ResolvePrivateAnimationAssociationInterface(std::uint64_t generation, std::uintptr_t candidate,
                                                                   NativeAnimationAssociationIssuedCopy& out) noexcept override
        {
            ++resolves;
            sentGeneration = generation;
            sentCandidate = candidate;
            if (mutateArgument)
                *mutateArgument = nullptr;
            out.token = resultToken;
            out.interfaceAddress = resultAddress;
            // A provider spy cannot construct a genuine same-Entry code view.
            // An empty view is useful only to verify malformed-result refusal.
            return resolveStatus;
        }
        ClockResult ReadPrivateAnimationAssociationInterface(const Token& token, std::uintptr_t& out) noexcept override
        {
            ++identityReads;
            sentToken = token;
            aliased = aliased || &token == forbiddenCallerToken;
            if (mutateCallerToken)
                *mutateCallerToken = {};
            out = identityAddress;
            return identityStatus;
        }
        ClockResult ReadPrivateAnimationAssociation(const Token&, NativeAnimationAssociationValues&) noexcept override
        {
            ++scalarReads;
            return ClockResult::ReadFailure;
        }
        ClockResult CreatePrivateAnimationAssociation(std::uint64_t, Token&) noexcept override
        {
            ++creates;
            return ClockResult::NotInitialized;
        }
        ClockResult CopyPrivateAnimationAssociationFromGroup(const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t,
                                                             NativeAnimationAssociationIssuedCopy&) noexcept override
        {
            ++copies;
            return ClockResult::NotInitialized;
        }
        ClockResult RetirePrivateAnimationAssociation(const Token&) noexcept override
        {
            ++retires;
            return ClockResult::NotInitialized;
        }
        ClockResult FreePrivateAnimationAssociationNodeArray(const Token&) noexcept override
        {
            ++frees;
            return ClockResult::NotInitialized;
        }
    };
    struct BorrowedProbe final : CAnimBlendAssociationDE
    {
        unsigned& destroyed;
        BorrowedProbe(NativeAnimationAssociationAccess& owner, const Token& token, unsigned& count) noexcept
            : CAnimBlendAssociationDE(owner, token), destroyed(count)
        {
        }
        ~BorrowedProbe() override { ++destroyed; }
    };
    void Cases()
    {
        constexpr std::uint64_t                       generation = 0xfedcba9876543210ull;
        const auto                                    candidate = (~std::uintptr_t{0}) - 64;
        Owner                                         owner;
        AnimationAssociationRawFactoryManagerReceiver manager(owner, generation);
        struct Missing final : NativeAnimationManagerAccess
        {
        } missing;
        AnimationAssociationRawFactoryManagerReceiver absent(missing, 0), zero(owner, 0);
        const auto                                    beforeNull = facadeAllocations;
        failFacadeAllocation = true;
        Check(!manager.GetAnimBlendAssociation(nullptr) && !owner.resolves && failFacadeAllocation && facadeAllocations == beforeNull,
              "source null returns before owner call and CPU allocation");
        Check(!absent.GetAnimBlendAssociation(nullptr) && !zero.GetAnimBlendAssociation(nullptr) && failFacadeAllocation,
              "source null also precedes missing-provider and zero-generation checks");
        failFacadeAllocation = false;
        for (const auto bits : {std::uintptr_t{1}, static_cast<std::uintptr_t>(0x12345678abcde000ull), candidate})
        {
            Raw caller = reinterpret_cast<Raw>(bits);
            owner.mutateArgument = &caller;
            const auto calls = owner.resolves, live = facadeLive;
            Refuses([&] { (void)manager.GetAnimBlendAssociation(caller); }, ClockResult::NotInitialized, "GetAnimBlendAssociation");
            Check(owner.resolves == calls + 1 && owner.sentGeneration == generation && owner.sentCandidate == bits && !caller && facadeLive == live,
                  "full pointer and generation copied before callback; refused CPU reservation released");
        }
        owner.mutateArgument = nullptr;
        for (const auto failure : failures)
        {
            owner.resolveStatus = failure;
            const auto calls = owner.resolves, live = facadeLive;
            Refuses([&] { (void)manager.GetAnimBlendAssociation(reinterpret_cast<Raw>(candidate)); }, failure, "GetAnimBlendAssociation");
            Check(owner.resolves == calls + 1 && facadeLive == live, "one resolver refusal, no retry or leaked CPU product");
        }
        owner.resolveStatus = ClockResult::Ok;
        struct Malformed
        {
            Token          token;
            std::uintptr_t raw;
        };
        for (const auto& value : {Malformed{{generation, 0}, candidate}, Malformed{{0, 7}, candidate}, Malformed{{generation + 1, 7}, candidate},
                                  Malformed{{generation, 7}, 0}, Malformed{{generation, 7}, candidate + 1}, Malformed{{generation, 7}, candidate}})
        {
            owner.resultToken = value.token;
            owner.resultAddress = value.raw;
            const auto calls = owner.resolves, live = facadeLive;
            Refuses([&] { (void)manager.GetAnimBlendAssociation(reinterpret_cast<Raw>(candidate)); }, ClockResult::BindingChanged, "GetAnimBlendAssociation");
            Check(owner.resolves == calls + 1 && facadeLive == live, "malformed or empty-code success is never raw adoption");
        }
        const auto calls = owner.resolves;
        failFacadeAllocation = true;
        bool oom = false;
        try
        {
            (void)manager.GetAnimBlendAssociation(reinterpret_cast<Raw>(candidate));
        }
        catch (const std::bad_alloc&)
        {
            oom = true;
        }
        Check(oom && !failFacadeAllocation && owner.resolves == calls, "CPU reservation OOM precedes code-view resolver");
        Refuses([&] { (void)absent.GetAnimBlendAssociation(reinterpret_cast<Raw>(candidate)); }, ClockResult::NotInitialized, "GetAnimBlendAssociation");
        Refuses([&] { (void)zero.GetAnimBlendAssociation(reinterpret_cast<Raw>(candidate)); }, ClockResult::NotInitialized, "GetAnimBlendAssociation");
        struct Default final : NativeAnimationManagerAccess, NativeAnimationAssociationAccess
        {
        } defaults;
        AnimationAssociationRawFactoryManagerReceiver defaultManager(defaults, generation);
        Refuses([&] { (void)defaultManager.GetAnimBlendAssociation(reinterpret_cast<Raw>(candidate)); }, ClockResult::NotInitialized,
                "GetAnimBlendAssociation");

        Token      callerToken{generation, 0xffffffffffffffffull};
        const auto original = callerToken;
        unsigned   destroyed = 0;
        {
            BorrowedProbe          product(owner, callerToken, destroyed);
            CAnimBlendAssociation& sdk = product;
            callerToken = {};
            owner.forbiddenCallerToken = &callerToken;
            owner.mutateCallerToken = &callerToken;
            for (const auto bits : {std::uintptr_t{1}, static_cast<std::uintptr_t>(0x12345678abcde000ull), candidate})
            {
                owner.identityAddress = bits;
                const auto reads = owner.identityReads;
                Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == bits, "opaque interface tag preserves full pointer width");
                Check(owner.identityReads == reads + 1 && !owner.scalarReads && Same(owner.sentToken, original),
                      "one explicit identity channel, no constructed scalar or vptr read");
            }
            Check(!callerToken.serial && !owner.aliased, "immutable facade token never aliases mutable caller storage");
            owner.mutateCallerToken = nullptr;
            owner.identityAddress = 0;
            const auto reads = owner.identityReads;
            Refuses([&] { (void)sdk.GetInterface(); }, ClockResult::BindingChanged, "GetInterface");
            Check(owner.identityReads == reads + 1 && !owner.scalarReads, "successful null identity is malformed without scalar fallback");
            owner.identityAddress = candidate;
            for (const auto failure : failures)
            {
                owner.identityStatus = failure;
                const auto before = owner.identityReads;
                Refuses([&] { (void)sdk.GetInterface(); }, failure, "GetInterface");
                Check(owner.identityReads == before + 1 && !owner.scalarReads && Same(owner.sentToken, original),
                      "identity refusal has no retry or constructed-state fallback");
            }
        }
        Check(destroyed == 1, "protected internal provider-borrow CPU product remains deletable without native cleanup");
        {
            BorrowedProbe product(defaults, original, destroyed);
            Refuses([&] { (void)product.GetInterface(); }, ClockResult::NotInitialized, "GetInterface");
        }
        Check(!owner.scalarReads && !owner.creates && !owner.copies && !owner.retires && !owner.frees,
              "raw factory, identity transport and CPU deletion invoke no native construction/copy/free/retire");
        std::puts("Actual issued raw association factory SDK fixture completed");
        std::puts("Actual association opaque identity channel SDK fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u issued raw association factory SDK checks (%zu-bit; no successful code view forged)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u issued raw factory SDK checks: %s\n", checks, e.what());
        return 1;
    }
}
