/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real static factories/Initialize and CPU-only default base deletion
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendStaticAssociationDE.h"
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlock.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendSequence.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "StaticAssociationFactoryFixture.generated.h"
static_assert(!std::is_abstract_v<CAnimBlendStaticAssociationDE> && std::is_abstract_v<CAnimManagerDE>);
static_assert(std::has_virtual_destructor_v<CAnimBlendStaticAssociation>);
static_assert(std::is_same_v<CAnimManager::StaticAssocIntface_type, std::unique_ptr<CAnimBlendStaticAssociation>>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Static initialization SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationStaticAssociationReferenceToken& a, const NativeAnimationStaticAssociationReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.groupIndex == b.groupIndex && a.index == b.index && a.locator == b.locator;
    }
    template <class Action>
    void Refuses(Action action, ClockResult status, const char* operation)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what()), suffix = ": " + std::to_string(static_cast<unsigned>(status));
            refused = text.find(operation) != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationStaticAssociationAccess
    {
        const std::uint64_t                            generation = UINT64_MAX - 7;
        const std::uintptr_t                           address = static_cast<std::uintptr_t>(0x12345678abcde000ull);
        NativeAnimationStaticAssociationReferenceToken issued{generation, 3, 2, address}, received{};
        NativeAnimationStaticAssociationValues         values{address, 17, -2, 3, -4, static_cast<std::uintptr_t>(0x88776655aa000000ull)};
        ClockResult                                    groupStatus = ClockResult::Ok, selectStatus = ClockResult::Ok, initStatus = ClockResult::Ok;
        unsigned                                       groups = 0, ids = 0, raws = 0, reads = 0, initializes = 0, forbidden = 0;
        std::uint64_t                                  requestedGeneration = 0;
        std::uint32_t                                  requestedGroup = 0, requestedId = 0;
        std::uintptr_t                                 requestedRaw = 0, clump = 0, hierarchy = 0;
        bool                                           created = true;
        std::array<unsigned char, 32>                  engineRow{};
        ClockResult ReadAnimationGroupCreated(std::uint64_t generationValue, std::uint32_t group, bool& output) noexcept override
        {
            ++groups;
            requestedGeneration = generationValue;
            requestedGroup = group;
            if (groupStatus == ClockResult::Ok)
                output = created;
            return groupStatus;
        }
        ClockResult ResolveAnimationStaticAssociationById(std::uint64_t gen, std::uint32_t group, std::uint32_t id,
                                                          NativeAnimationStaticAssociationReferenceToken& output) noexcept override
        {
            ++ids;
            requestedGeneration = gen;
            requestedGroup = group;
            requestedId = id;
            if (selectStatus == ClockResult::Ok)
                output = issued;
            return selectStatus;
        }
        ClockResult ResolveAnimationStaticAssociationInterface(std::uint64_t gen, std::uintptr_t raw,
                                                               NativeAnimationStaticAssociationReferenceToken& output) noexcept override
        {
            ++raws;
            requestedGeneration = gen;
            requestedRaw = raw;
            if (selectStatus == ClockResult::Ok)
                output = issued;
            return selectStatus;
        }
        ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken& token,
                                                   NativeAnimationStaticAssociationValues&               output) noexcept override
        {
            ++reads;
            received = token;
            output = values;
            return ClockResult::Ok;
        }
        ClockResult InitializeAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken& token, std::uintptr_t c,
                                                         std::uintptr_t h) noexcept override
        {
            ++initializes;
            received = token;
            clump = c;
            hierarchy = h;
            return initStatus;
        }
        ClockResult ResolveAnimationStaticAssociation(std::uint64_t, std::uint32_t, std::size_t,
                                                      NativeAnimationStaticAssociationReferenceToken&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
    class Direct final : public CAnimBlendStaticAssociationDE
    {
    public:
        Direct(NativeAnimationStaticAssociationAccess& owner, const NativeAnimationStaticAssociationReferenceToken& token, unsigned* destroyed = nullptr)
            : CAnimBlendStaticAssociationDE(owner, token), destroyed(destroyed)
        {
        }
        ~Direct() override
        {
            if (destroyed)
                ++*destroyed;
        }

    private:
        unsigned* destroyed;
    };
    static_assert(std::is_same_v<decltype(&Direct::Initialize), decltype(&CAnimBlendStaticAssociationDE::Initialize)>);
}
int main()
{
    Owner                                   owner;
    StaticAssociationFactoryManagerReceiver receiver(owner, owner.generation);
    CAnimManager&                           manager = receiver;
    const auto                              group = static_cast<eAnimGroup>(3);
    const auto                              original = owner.issued;
    for (const auto id : {std::uint32_t{0}, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu})
    {
        auto result = manager.GetAnimStaticAssociation(group, static_cast<eAnimID>(id));
        Check(result && owner.requestedGroup == 3 && owner.requestedId == id && owner.requestedGeneration == owner.generation);
        Check(result->GetAnimID() == -2 && Same(owner.received, original));
        Check(result->GetNumBlendNodes() == 17 && result->GetAnimGroup() == 3 && result->GetFlags() == -4);
        Check(reinterpret_cast<std::uintptr_t>(result->GetAnimHierarchyInterface()) == owner.values.hierarchyAddress);
        Check(reinterpret_cast<std::uintptr_t>(result->GetInterface()) == original.locator && owner.forbidden == 0);
    }
    const auto beforeInvalid = owner.groups;
    Check(!manager.GetAnimStaticAssociation(eAnimGroup::ANIM_GROUP_NONE, static_cast<eAnimID>(0)) && owner.groups == beforeInvalid);
    Check(!manager.GetAnimStaticAssociation(eAnimGroup::ANIM_TOTAL_GROUPS, static_cast<eAnimID>(0)) && owner.groups == beforeInvalid);
    owner.created = false;
    const auto ids = owner.ids;
    Check(!manager.GetAnimStaticAssociation(group, static_cast<eAnimID>(0)) && owner.ids == ids);
    owner.created = true;
    const auto beforeNull = owner.raws;
    Check(!manager.GetAnimStaticAssociation(static_cast<CAnimBlendStaticAssociationSAInterface*>(nullptr)) && owner.raws == beforeNull);
    auto raw = reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(owner.address);
    auto result = manager.GetAnimStaticAssociation(raw);
    Check(result && owner.requestedRaw == owner.address && owner.requestedGeneration == owner.generation);
    // A borrowed CPU facade keeps the physical original token even if the
    // selector subsequently points elsewhere; reads never reselect by ID.
    owner.issued = {owner.generation, 5, 9, owner.address + 32};
    Check(result->GetAnimID() == -2 && Same(owner.received, original) && owner.forbidden == 0);
    owner.issued = original;
    const auto c = static_cast<std::uintptr_t>(0x8877665544332000ull), h = static_cast<std::uintptr_t>(0x1122334455667000ull);
    result->Initialize(reinterpret_cast<RpClump*>(c), reinterpret_cast<CAnimBlendHierarchySAInterface*>(h));
    Check(owner.initializes == 1 && owner.clump == c && owner.hierarchy == h && Same(owner.received, original));
    result->Initialize(nullptr, nullptr);
    Check(owner.initializes == 2 && owner.clump == 0 && owner.hierarchy == 0 && Same(owner.received, original));
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::ReadFailure,
                               ClockResult::BindingChanged, ClockResult::InvalidArgument})
    {
        owner.initStatus = failure;
        Refuses([&] { result->Initialize(reinterpret_cast<RpClump*>(c), reinterpret_cast<CAnimBlendHierarchySAInterface*>(h)); }, failure, "Initialize");
        Check(owner.clump == c && owner.hierarchy == h && Same(owner.received, original));
        owner.selectStatus = failure;
        Refuses([&] { manager.GetAnimStaticAssociation(group, static_cast<eAnimID>(0)); }, failure, "GetAnimStaticAssociation");
        Refuses([&] { manager.GetAnimStaticAssociation(raw); }, failure, "GetAnimStaticAssociation(interface)");
    }
    owner.initStatus = owner.selectStatus = ClockResult::Ok;
    owner.issued.ownerGeneration = 0;
    Refuses([&] { manager.GetAnimStaticAssociation(raw); }, ClockResult::OwnerConflict, "GetAnimStaticAssociation(interface)");
    owner.issued.locator = 0;
    Refuses([&] { manager.GetAnimStaticAssociation(group, static_cast<eAnimID>(0)); }, ClockResult::OwnerConflict, "GetAnimStaticAssociation");
    owner.issued = original;
    owner.issued.locator = 0;
    Check(!manager.GetAnimStaticAssociation(group, static_cast<eAnimID>(0)) && !manager.GetAnimStaticAssociation(raw));
    owner.issued = original;
    owner.issued.groupIndex = 4;
    Refuses([&] { manager.GetAnimStaticAssociation(group, static_cast<eAnimID>(0)); }, ClockResult::OwnerConflict, "GetAnimStaticAssociation");
    owner.issued = original;
    owner.issued.locator += 32;
    Refuses([&] { manager.GetAnimStaticAssociation(raw); }, ClockResult::OwnerConflict, "GetAnimStaticAssociation(interface)");
    owner.issued = original;
    NativeAnimationStaticAssociationAccess absent;
    Direct                                 unconfigured(absent, original);
    Refuses([&] { unconfigured.Initialize(reinterpret_cast<RpClump*>(c), reinterpret_cast<CAnimBlendHierarchySAInterface*>(h)); }, ClockResult::NotInitialized,
            "Initialize");
    NativeAnimationManagerAccess            noStatic;
    StaticAssociationFactoryManagerReceiver noOwner(noStatic, owner.generation);
    Check(!noOwner.GetAnimStaticAssociation(static_cast<CAnimBlendStaticAssociationSAInterface*>(nullptr)));
    Refuses([&] { noOwner.GetAnimStaticAssociation(raw); }, ClockResult::NotInitialized, "GetAnimStaticAssociation(interface)");
    const auto rowBefore = owner.engineRow;
    const auto readsBefore = owner.reads, initBefore = owner.initializes, selectionsBefore = owner.ids + owner.raws;
    result.reset();
    Check(owner.engineRow == rowBefore && owner.reads == readsBefore && owner.initializes == initBefore && owner.ids + owner.raws == selectionsBefore);
    unsigned                              destroyed = 0;
    CAnimManager::StaticAssocIntface_type common(new Direct(owner, original, &destroyed));
    common.reset();
    Check(destroyed == 1 && owner.engineRow == rowBefore && owner.initializes == initBefore && owner.forbidden == 0);
    std::puts("Actual static association factory SDK fixture completed");
    std::puts("Actual static association Initialize SDK fixture completed");
    std::puts("Actual static association default deletion fixture completed");
    std::printf("Passed %u static initialization SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
