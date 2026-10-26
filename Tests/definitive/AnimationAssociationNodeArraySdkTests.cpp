/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real SDK node-release dispatch without a second native ACK model
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include "PrivateAnimationAssociationFixture.generated.h"

using namespace mta::game_de;
static_assert(
    std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::FreeAnimBlendNodeArray), decltype(&CAnimBlendAssociationDE::FreeAnimBlendNodeArray)>);
static_assert(!noexcept(std::declval<CAnimBlendAssociation&>().FreeAnimBlendNodeArray()));
static_assert(std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_abstract_v<PrivateAnimationAssociationReceiver>);

namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association node-array SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationAssociationReferenceToken& first, const NativeAnimationAssociationReferenceToken& next)
    {
        return first.ownerGeneration == next.ownerGeneration && first.serial == next.serial;
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
            const std::string message = error.what();
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = message.find("FreeAnimBlendNodeArray") != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationAccess
    {
        NativeAnimationAssociationReferenceToken received{}, *mutateSource = nullptr;
        ClockResult                              status = ClockResult::Ok;
        unsigned                                 frees = 0, reads = 0, lengths = 0, creates = 0, constructs = 0, retires = 0;

        ClockResult FreePrivateAnimationAssociationNodeArray(const NativeAnimationAssociationReferenceToken& token) noexcept override
        {
            ++frees;
            if (mutateSource)
            {
                *mutateSource = {0, 0};
                mutateSource = nullptr;
            }
            received = token;
            return status;
        }
        ClockResult ReadPrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&, NativeAnimationAssociationValues&) noexcept override
        {
            ++reads;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadPrivateAnimationAssociationLength(const NativeAnimationAssociationReferenceToken&, std::uint32_t&) noexcept override
        {
            ++lengths;
            return ClockResult::NotInitialized;
        }
        ClockResult CreatePrivateAnimationAssociation(std::uint64_t, NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++creates;
            return ClockResult::NotInitialized;
        }
        ClockResult ConstructPrivateAnimationAssociationFromStatic(const NativeAnimationAssociationReferenceToken&, std::uintptr_t,
                                                                   std::uintptr_t&) noexcept override
        {
            ++constructs;
            return ClockResult::NotInitialized;
        }
        ClockResult RetirePrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++retires;
            return ClockResult::NotInitialized;
        }
        bool NoOtherCalls() const { return reads == 0 && lengths == 0 && creates == 0 && constructs == 0 && retires == 0; }
    };
    struct DefaultOwner final : NativeAnimationAssociationAccess
    {
    };
}

int main()
{
    const NativeAnimationAssociationReferenceToken expected{0xfedcba9876543210ull, 0x8877665544332211ull};
    Owner                                          owner;
    unsigned                                       beforeDestruction = 0;
    {
        auto                                caller = expected;
        PrivateAnimationAssociationReceiver receiver(owner, caller);
        CAnimBlendAssociation&              association = receiver;
        owner.mutateSource = &caller;
        association.FreeAnimBlendNodeArray();
        Check(owner.frees == 1 && Same(owner.received, expected));
        Check(caller.ownerGeneration == 0 && caller.serial == 0 && owner.NoOtherCalls());

        // An SDK spy has no native allocation or release journal. It verifies
        // only one transport per request; repeated-free safety belongs to the
        // genuine owner, not an inferred node pointer/count or facade cache.
        for (const auto failure :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.status = failure;
            const auto before = owner.frees;
            Refuses([&] { association.FreeAnimBlendNodeArray(); }, failure);
            Check(owner.frees == before + 1 && Same(owner.received, expected) && owner.NoOtherCalls());
        }
        owner.status = ClockResult::Ok;
        const NativeAnimationAssociationReferenceToken other{0x1234567800000001ull, 0xffffffff00000002ull};
        PrivateAnimationAssociationReceiver            second(owner, other);
        second.FreeAnimBlendNodeArray();
        Check(Same(owner.received, other));
        association.FreeAnimBlendNodeArray();
        Check(Same(owner.received, expected));

        // Invalid native domains are forwarded once to that same owner. The
        // facade cannot turn an unqualified token into no-resource success.
        const NativeAnimationAssociationReferenceToken invalid{};
        PrivateAnimationAssociationReceiver            invalidReceiver(owner, invalid);
        owner.status = ClockResult::InvalidArgument;
        const auto before = owner.frees;
        Refuses([&] { invalidReceiver.FreeAnimBlendNodeArray(); }, ClockResult::InvalidArgument);
        Check(owner.frees == before + 1 && Same(owner.received, invalid) && owner.NoOtherCalls());
        beforeDestruction = owner.frees;
    }
    Check(owner.frees == beforeDestruction && owner.NoOtherCalls());
    DefaultOwner                        defaults;
    PrivateAnimationAssociationReceiver missing(defaults, expected);
    Refuses([&] { missing.FreeAnimBlendNodeArray(); }, ClockResult::NotInitialized);
    Check(defaults.FreePrivateAnimationAssociationNodeArray(expected) == ClockResult::NotInitialized);
    std::puts("Actual animation association node-array SDK fixture completed");
    std::printf("Passed %u association node-array SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
