/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual numeric factory refusal/null and CPU-only common deletion
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "AnimationAssociationNumericFactoryManagerFixture.generated.h"
#include "ClassicAssociationCpuLifetime.generated.h"
using namespace mta::game_de;
static_assert(std::has_virtual_destructor_v<CAnimBlendAssociation> && !std::is_destructible_v<CAnimBlendAssociationDE>);
static_assert(!std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_copy_constructible_v<NativeAnimationAssociationFacadeView>);
namespace
{
    bool     failFacadeAllocation = false;
    unsigned checks = 0;
}
void* operator new(std::size_t bytes)
{
    if (failFacadeAllocation && bytes == sizeof(CAnimBlendAssociationDE))
    {
        failFacadeAllocation = false;
        throw std::bad_alloc();
    }
    if (auto* p = std::malloc(bytes ? bytes : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
namespace
{
    using Token = NativeAnimationAssociationReferenceToken;
    using Group = NativeAnimationAssociationGroupReferenceToken;
    void Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    template <class F>
    void Refuses(F&& f, ClockResult expected)
    {
        bool refused = false;
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            const std::string text = e.what(), tail = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("CreateAnimAssociation") != std::string::npos && text.size() >= tail.size() &&
                      text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "actual factory refusal preserves tagged owner result");
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationAssociationGroupAccess, NativeAnimationAssociationAccess, NativeAnimationHierarchyAccess
    {
        ClockResult    resolveStatus = ClockResult::Ok, copyStatus = ClockResult::Ok;
        unsigned       resolves = 0, copies = 0, reads = 0, retires = 0, frees = 0, blocks = 0, validity = 0, hierarchyResolves = 0, hierarchyReads = 0;
        Group          sent{};
        std::uint32_t  id = 0;
        Token          resultToken{};
        std::uintptr_t resultAddress = 0;
        AssocGroupId*  callerGroup = nullptr;
        AnimationId*   callerId = nullptr;
        bool           badGroup = false;
        ClockResult    ResolveAnimationAssociationGroup(std::uint64_t gen, std::uint32_t index, Group& out) noexcept override
        {
            ++resolves;
            if (callerGroup)
                *callerGroup = 0;
            if (callerId)
                *callerId = 0;
            out = {gen, index, static_cast<std::uintptr_t>(0x4000)};
            if (badGroup)
                ++out.index;
            return resolveStatus;
        }
        ClockResult CopyPrivateAnimationAssociationFromGroup(const Group& group, std::uint32_t animation,
                                                             NativeAnimationAssociationIssuedCopy& out) noexcept override
        {
            ++copies;
            sent = group;
            id = animation;
            out.token = resultToken;
            out.interfaceAddress = resultAddress;
            return copyStatus;
        }
        ClockResult ReadPrivateAnimationAssociation(const Token&, NativeAnimationAssociationValues&) noexcept override
        {
            ++reads;
            return ClockResult::ReadFailure;
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
        ClockResult ReadAnimationAssociationGroupBlock(const Group&, NativeAnimationAssociationGroupBlock&) noexcept override
        {
            ++blocks;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationGroupCreated(std::uint64_t, std::uint32_t, bool&) noexcept override
        {
            ++validity;
            return ClockResult::ReadFailure;
        }
        ClockResult ResolvePrivateAnimationAssociationHierarchy(const Token& in, NativeAnimationHierarchyReferenceToken& out) noexcept override
        {
            ++hierarchyResolves;
            out = {in.ownerGeneration, static_cast<std::uintptr_t>(0x8800)};
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token, NativeAnimationHierarchyValues& out) noexcept override
        {
            ++hierarchyReads;
            out.interfaceAddress = token.locator;
            return ClockResult::Ok;
        }
    };
    struct BorrowedWatch final : CAnimBlendAssociationDE
    {
        unsigned& destroyed;
        BorrowedWatch(Owner& owner, const Token& token, unsigned& count) noexcept : CAnimBlendAssociationDE(owner, token), destroyed(count) {}
        ~BorrowedWatch() override { ++destroyed; }
    };
    struct ClassicWatch final : ClassicAssociationCpuLifetimeFixture
    {
        unsigned& destroyed;
        ClassicWatch(CAnimBlendAssociationSAInterface* p, unsigned& count) : ClassicAssociationCpuLifetimeFixture(p), destroyed(count) {}
        ~ClassicWatch() override { ++destroyed; }
    };
    void Cases()
    {
        constexpr std::uint64_t                           generation = 0xfedcba9876543210ull;
        Owner                                             owner;
        AnimationAssociationNumericFactoryManagerReceiver manager(owner, generation);
        for (const auto group : {std::uint32_t{0}, std::uint32_t{119}, std::uint32_t{0xffffffff}})
            for (const auto id : {std::uint32_t{0}, std::uint32_t{0xffffffff}})
            {
                AssocGroupId callerGroup = group;
                AnimationId  callerId = id;
                owner.callerGroup = &callerGroup;
                owner.callerId = &callerId;
                const auto resolves = owner.resolves, copies = owner.copies;
                auto       result = manager.CreateAnimAssociation(callerGroup, callerId);
                Check(!result && owner.resolves == resolves + 1 && owner.copies == copies + 1, "verified null preserves one actual resolve/typed call");
                Check(owner.sent.ownerGeneration == generation && owner.sent.index == group && owner.id == id && !callerGroup && !callerId,
                      "full uint32 arguments copied before callbacks, no CGame or enum/Created gate");
            }
        owner.callerGroup = nullptr;
        owner.callerId = nullptr;
        for (const auto failure :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.copyStatus = failure;
            const auto copies = owner.copies;
            Refuses([&] { (void)manager.CreateAnimAssociation(0xffffffff, 0xffffffff); }, failure);
            Check(owner.copies == copies + 1, "refusal does not retry or create a CPU wrapper from a raw result");
        }
        owner.copyStatus = ClockResult::Ok;
        for (const auto token : {Token{generation, 0}, Token{0, 1}, Token{generation, 1}, Token{generation + 1, 1}})
        {
            owner.resultToken = token;
            owner.resultAddress = 0;
            Refuses([&] { (void)manager.CreateAnimAssociation(0, 0); }, ClockResult::BindingChanged);
            owner.resultAddress = (~std::uintptr_t{0}) - 64;
            Refuses([&] { (void)manager.CreateAnimAssociation(0, 0); }, ClockResult::BindingChanged);
        }
        owner.resultToken = {};
        owner.resultAddress = 0;
        owner.badGroup = true;
        const auto beforeBad = owner.copies;
        Refuses([&] { (void)manager.CreateAnimAssociation(0, 0); }, ClockResult::BindingChanged);
        Check(owner.copies == beforeBad, "malformed physical-group token refuses before Copy");
        owner.badGroup = false;
        const auto beforeResolve = owner.resolves, beforeCopy = owner.copies;
        bool       oom = false;
        failFacadeAllocation = true;
        try
        {
            (void)manager.CreateAnimAssociation(0, 0);
        }
        catch (const std::bad_alloc&)
        {
            oom = true;
        }
        Check(oom && !failFacadeAllocation && owner.resolves == beforeResolve && owner.copies == beforeCopy, "CPU facade OOM precedes original native effects");
        struct Missing final : NativeAnimationManagerAccess
        {
        } missing;
        AnimationAssociationNumericFactoryManagerReceiver missingManager(missing, generation);
        Refuses([&] { (void)missingManager.CreateAnimAssociation(0, 0); }, ClockResult::NotInitialized);
        AnimationAssociationNumericFactoryManagerReceiver zeroManager(owner, 0);
        Refuses([&] { (void)zeroManager.CreateAnimAssociation(0, 0); }, ClockResult::NotInitialized);
        struct Defaults final : NativeAnimationManagerAccess, NativeAnimationAssociationGroupAccess, NativeAnimationAssociationAccess
        {
            ClockResult ResolveAnimationAssociationGroup(std::uint64_t g, std::uint32_t i, Group& out) noexcept override
            {
                out = {g, i, 0x4000};
                return ClockResult::Ok;
            }
        } defaults;
        AnimationAssociationNumericFactoryManagerReceiver defaultManager(defaults, generation);
        Refuses([&] { (void)defaultManager.CreateAnimAssociation(0, 0); }, ClockResult::NotInitialized);
        std::array<unsigned char, 104> raw{};
        raw.fill(0xa5);
        const auto                           old = raw;
        unsigned                             deDeleted = 0, saDeleted = 0;
        std::unique_ptr<CAnimBlendHierarchy> child;
        {
            std::unique_ptr<CAnimBlendAssociation> parent(new BorrowedWatch(owner, {generation, 7}, deDeleted));
            child = parent->GetAnimHierarchy();
            Check(child != nullptr && owner.hierarchyResolves == 1, "actual independent borrowed H child resolves jointly once");
        }
        Check(deDeleted == 1 && raw == old, "common virtual delete reaches actual DE CPU chain without native effects");
        Check(child->GetInterface() == reinterpret_cast<CAnimBlendHierarchySAInterface*>(std::uintptr_t{0x8800}),
              "old protected empty-code internal child stays an explicit provider borrow");
        child.reset();
        {
            auto*                                  tag = reinterpret_cast<CAnimBlendAssociationSAInterface*>(raw.data());
            std::unique_ptr<CAnimBlendAssociation> classic(new ClassicWatch(tag, saDeleted));
            Check(classic->GetInterface() == tag, "exact selected actual classic CPU constructor/GetInterface preserve raw tag");
        }
        Check(saDeleted == 1 && raw == old, "actual classic default CPU destructor leaves native interface unchanged");
        Check(!owner.reads && !owner.retires && !owner.frees && !owner.blocks && !owner.validity,
              "numeric frontend and CPU deletion do not read/free/retire/warm/check validity");
        std::puts("Actual numeric association factory SDK fixture completed");
        std::puts("Actual association CPU base-deletion fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u numeric association factory SDK checks (%zu-bit; no successful code view forged)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u numeric factory SDK checks: %s\n", checks, e.what());
        return 1;
    }
}
