/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited GetRpClump dispatch with no pointee adoption
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedMetadataDE.h"
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "PedRenderFixture.generated.h"
static_assert(std::is_same_v<decltype(&ped_render_fixture::PedRenderFixture::GetRpClump), decltype(&CPedMetadataDE::GetRpClump)>);
static_assert(std::is_abstract_v<CPedMetadataDE> && !std::is_abstract_v<ped_render_fixture::PedRenderFixture>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Ped render SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("GetRpClump") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativePedVitalsAccess, NativePedMetadataAccess
    {
        NativePedReferenceToken        received{};
        const NativePedReferenceToken* callerAddress = nullptr;
        const NativePedReferenceToken* boundAddress = nullptr;
        std::uintptr_t                 supplied = 0;
        ClockResult                    status = ClockResult::Ok;
        unsigned                       renders = 0, metadataCalls = 0, vitalCalls = 0;
        bool                           aliased = false, stableAcrossCallback = true;
        void (*callback)(void*) noexcept = nullptr;
        void*       context = nullptr;
        ClockResult ReadPedRender(const NativePedReferenceToken& selected, std::uintptr_t& output) noexcept override
        {
            ++renders;
            received = selected;
            aliased = aliased || &selected == callerAddress || &selected == boundAddress;
            const auto before = selected;
            if (callback)
                callback(context);
            stableAcrossCallback = stableAcrossCallback && Same(before, selected);
            // A refusal may touch output. It must never become a fabricated
            // successful null/tag or prompt a second field/profile lookup.
            output = supplied;
            return status;
        }
        ClockResult ReadPedMetadata(const NativePedReferenceToken&, NativePedMetadata&) noexcept override
        {
            ++metadataCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadPedVital(const NativePedReferenceToken&, NativePedVitalField, float&) noexcept override
        {
            ++vitalCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult SetPedVital(const NativePedReferenceToken&, NativePedVitalField, float) noexcept override
        {
            ++vitalCalls;
            return ClockResult::NotInitialized;
        }
    };
    struct MissingRender final : NativePedMetadataAccess
    {
        unsigned    metadataCalls = 0;
        ClockResult ReadPedMetadata(const NativePedReferenceToken&, NativePedMetadata& output) noexcept override
        {
            ++metadataCalls;
            output = {};
            return ClockResult::Ok;
        }
    };
    struct ReentrantContext
    {
        NativePedReferenceToken* caller = nullptr;
        CEntity*                 other = nullptr;
        std::uintptr_t           observed = 0;
        unsigned                 calls = 0;
    };
    void RebindCallerAndReadOther(void* opaque) noexcept
    {
        auto& value = *static_cast<ReentrantContext*>(opaque);
        *value.caller = {};
        ++value.calls;
        value.observed = reinterpret_cast<std::uintptr_t>(value.other->GetRpClump());
    }
}
int main()
{
    NativePedReferenceToken caller;
    caller.version = (std::numeric_limits<std::uint64_t>::max)() - 13;
    caller.ownerGeneration = (std::numeric_limits<std::uint64_t>::max)() - 17;
    caller.slot = 0xfffffffeu;
    const auto original = caller;
    Owner      owner;
    owner.callerAddress = &caller;
    ped_render_fixture::PedRenderFixture receiver(owner, owner, caller);
    owner.boundAddress = receiver.TokenAddressForTest();
    CEntity& entity = receiver;
    CPed&    ped = receiver;
    caller = {};
    Check(owner.renders == 0 && owner.metadataCalls == 0 && owner.vitalCalls == 0);
    // The getter is an opaque source field observation: no alignment, target
    // dereference, model0, nonnull, or container-schema predicate belongs here.
    for (const auto value :
         {std::uintptr_t{0}, std::uintptr_t{3}, static_cast<std::uintptr_t>(0x12345678abcde000ull), (std::numeric_limits<std::uintptr_t>::max)()})
    {
        owner.supplied = value;
        auto before = owner.renders;
        Check(reinterpret_cast<std::uintptr_t>(entity.GetRpClump()) == value);
        Check(owner.renders == before + 1 && Same(owner.received, original));
        before = owner.renders;
        Check(reinterpret_cast<std::uintptr_t>(ped.GetRpClump()) == value && owner.renders == before + 1);
    }
    NativePedReferenceToken otherToken;
    otherToken.version = 3;
    otherToken.ownerGeneration = 0x1020304050607080ull;
    otherToken.slot = 7;
    Owner other;
    other.supplied = static_cast<std::uintptr_t>(0x34567890ffffd000ull);
    ped_render_fixture::PedRenderFixture peer(other, other, otherToken);
    other.boundAddress = peer.TokenAddressForTest();
    caller = original;
    ReentrantContext context{&caller, &peer, 0, 0};
    owner.callback = &RebindCallerAndReadOther;
    owner.context = &context;
    owner.supplied = static_cast<std::uintptr_t>(0x45678901bbbbc000ull);
    const auto before = owner.renders;
    Check(reinterpret_cast<std::uintptr_t>(entity.GetRpClump()) == owner.supplied);
    Check(owner.renders == before + 1 && context.calls == 1 && Same(owner.received, original));
    Check(caller.version == 0 && context.observed == other.supplied && other.renders == 1 && Same(other.received, otherToken));
    Check(!owner.aliased && !other.aliased && owner.stableAcrossCallback && other.stableAcrossCallback);
    owner.callback = nullptr;
    for (std::uint32_t code = static_cast<std::uint32_t>(ClockResult::InvalidArgument); code <= static_cast<std::uint32_t>(ClockResult::OriginalException);
         ++code)
    {
        owner.status = static_cast<ClockResult>(code);
        const auto calls = owner.renders;
        Refuses([&] { entity.GetRpClump(); }, owner.status);
        Check(owner.renders == calls + 1 && Same(owner.received, original));
    }
    MissingRender                        missing;
    ped_render_fixture::PedRenderFixture unconfigured(owner, missing, original);
    Refuses([&] { unconfigured.GetRpClump(); }, ClockResult::NotInitialized);
    Check(missing.metadataCalls == 0);
    const auto oldRenders = owner.renders, oldPeer = other.renders;
    {
        ped_render_fixture::PedRenderFixture unused(owner, owner, original);
    }
    Check(owner.renders == oldRenders && other.renders == oldPeer);
    Check(owner.metadataCalls == 0 && owner.vitalCalls == 0 && other.metadataCalls == 0 && other.vitalCalls == 0);
    std::puts("Actual ped render SDK fixture completed");
    std::printf("Passed %u ped render SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
