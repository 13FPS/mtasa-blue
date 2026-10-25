/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager raw-row borrow and metadata-only delete dispatch
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Sequence interface SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected, const char* operation)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = message.find(operation) != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationManagerAccess,
                         NativeAnimationHierarchyAccess,
                         NativeAnimationSequenceAccess,
                         NativeAnimationSequenceStorageAccess,
                         NativeCustomAnimationSequenceOperationAccess
    {
        const std::uint64_t                     generation = (std::numeric_limits<std::uint64_t>::max)() - 7;
        const std::uintptr_t                    raw = (std::numeric_limits<std::uintptr_t>::max)() - 0x1000;
        NativeAnimationSequenceResolvedIdentity identity{{generation, raw - 0x4000, (std::numeric_limits<std::size_t>::max)()}, raw};
        NativeAnimationSequenceReferenceToken   deleted{}, read{};
        std::uint64_t                           requestedGeneration = 0;
        std::uintptr_t                          requestedRaw = 0;
        ClockResult                             resolveStatus = ClockResult::Ok, deleteStatus = ClockResult::Ok;
        unsigned                                resolves = 0, deletes = 0, oldStandaloneResolves = 0, forbiddenCleanupOrFree = 0;
        CAnimBlendSequenceSAInterface**         mutateCaller = nullptr;
        ClockResult                             ResolvePrivateAnimationSequenceReference(std::uint64_t gen, std::uintptr_t locator,
                                                                                         NativeAnimationSequenceResolvedIdentity& output) noexcept override
        {
            ++resolves;
            requestedGeneration = gen;
            requestedRaw = locator;
            if (mutateCaller)
                *mutateCaller = nullptr;
            if (resolveStatus == ClockResult::Ok)
                output = identity;
            return resolveStatus;
        }
        ClockResult DeletePrivateStandaloneAnimationSequenceInterface(const NativeAnimationSequenceReferenceToken& reference) noexcept override
        {
            ++deletes;
            deleted = reference;
            return deleteStatus;
        }
        ClockResult ResolvePrivateStandaloneAnimationSequence(std::uint64_t, std::uintptr_t, NativeAnimationSequenceReferenceToken&) noexcept override
        {
            ++oldStandaloneResolves;
            return ClockResult::BindingChanged;
        }
        ClockResult ReadAnimationSequence(const NativeAnimationSequenceReferenceToken& reference, NativeAnimationSequenceValues& output) noexcept override
        {
            read = reference;
            output.interfaceAddress = raw;
            return ClockResult::Ok;
        }
        ClockResult BeginPrivateAnimationSequenceCleanup(const NativeAnimationSequenceReferenceToken&,
                                                         NativeCustomAnimationSequenceOperationBeginReceipt&) noexcept override
        {
            ++forbiddenCleanupOrFree;
            return ClockResult::NotInitialized;
        }
        ClockResult AllocateAnimationSequences(std::uint64_t, std::size_t, std::uintptr_t&) noexcept override { return ClockResult::NotInitialized; }
        ClockResult AllocateAnimationKeyFrames(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept override { return ClockResult::NotInitialized; }
        ClockResult FreeAnimationSequences(std::uint64_t, std::uintptr_t) noexcept override
        {
            ++forbiddenCleanupOrFree;
            return ClockResult::NotInitialized;
        }
        ClockResult FreeAnimationKeyFrames(std::uint64_t, std::uintptr_t) noexcept override
        {
            ++forbiddenCleanupOrFree;
            return ClockResult::NotInitialized;
        }
    };
    bool Equal(const NativeAnimationSequenceReferenceToken& left, const NativeAnimationSequenceReferenceToken& right)
    {
        return left.ownerGeneration == right.ownerGeneration && left.firstTag == right.firstTag && left.index == right.index;
    }
}
int main()
{
    static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::DeleteCustomAnimSequenceInterface),
                                 decltype(&CAnimManagerDE::DeleteCustomAnimSequenceInterface)>);
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner, owner, owner, owner.generation);
    auto*                                  raw = reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.raw);
    Check(!manager.GetCustomAnimBlendSequence(nullptr));
    manager.DeleteCustomAnimSequenceInterface(nullptr);
    Check(owner.resolves == 0 && owner.deletes == 0);
    owner.mutateCaller = &raw;
    auto borrowed = manager.GetCustomAnimBlendSequence(raw);
    Check(borrowed && !raw && owner.requestedRaw == owner.raw && owner.requestedGeneration == owner.generation);
    owner.mutateCaller = nullptr;
    Check(borrowed->GetInterface() == reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.raw));
    Check(Equal(owner.read, owner.identity.reference));
    borrowed.reset();
    Check(owner.deletes == 0 && owner.forbiddenCleanupOrFree == 0 && owner.oldStandaloneResolves == 0);
    raw = reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.raw);
    manager.DeleteCustomAnimSequenceInterface(raw);
    Check(owner.deletes == 1 && Equal(owner.deleted, owner.identity.reference));
    Check(owner.forbiddenCleanupOrFree == 0 && owner.oldStandaloneResolves == 0);
    for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                              ClockResult::OwnerConflict, ClockResult::OriginalException})
    {
        owner.resolveStatus = status;
        const auto before = owner.deletes;
        Refuses([&] { manager.GetCustomAnimBlendSequence(raw); }, status, "GetCustomAnimBlendSequence");
        Refuses([&] { manager.DeleteCustomAnimSequenceInterface(raw); }, status, "DeleteCustomAnimSequenceInterface");
        Check(owner.deletes == before);
    }
    owner.resolveStatus = ClockResult::Ok;
    const auto valid = owner.identity;
    for (unsigned shape = 0; shape != 3; ++shape)
    {
        owner.identity = valid;
        if (shape == 0)
            --owner.identity.reference.ownerGeneration;
        if (shape == 1)
            owner.identity.reference.firstTag = 0;
        if (shape == 2)
            --owner.identity.interfaceAddress;
        const auto before = owner.deletes;
        Refuses([&] { manager.GetCustomAnimBlendSequence(raw); }, ClockResult::BindingChanged, "GetCustomAnimBlendSequence");
        Refuses([&] { manager.DeleteCustomAnimSequenceInterface(raw); }, ClockResult::BindingChanged, "DeleteCustomAnimSequenceInterface");
        Check(owner.deletes == before);
    }
    owner.identity = valid;
    for (const auto status : {ClockResult::InvalidArgument, ClockResult::Busy, ClockResult::WriteFailure, ClockResult::OriginalException})
    {
        owner.deleteStatus = status;
        const auto before = owner.deletes;
        Refuses([&] { manager.DeleteCustomAnimSequenceInterface(raw); }, status, "DeleteCustomAnimSequenceInterface");
        Check(owner.deletes == before + 1 && owner.forbiddenCleanupOrFree == 0);
    }
    PrivateAnimationFactoryManagerReceiver observer(owner, owner.generation);
    const auto                             before = owner.resolves;
    Refuses([&] { observer.GetCustomAnimBlendSequence(raw); }, ClockResult::NotInitialized, "GetCustomAnimBlendSequence");
    Refuses([&] { observer.DeleteCustomAnimSequenceInterface(raw); }, ClockResult::NotInitialized, "DeleteCustomAnimSequenceInterface");
    Check(owner.resolves == before && owner.forbiddenCleanupOrFree == 0);
    std::puts("Actual custom sequence interface SDK fixture completed");
    std::printf("Passed %u sequence interface SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
