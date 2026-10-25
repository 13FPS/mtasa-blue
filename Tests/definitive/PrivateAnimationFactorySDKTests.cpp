/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Four actual private manager products and borrowed CPU lifetimes
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
namespace FactoryAllocationFault
{
    bool        armed = false;
    std::size_t expected = 0;
    void        Arm(std::size_t bytes) noexcept
    {
        expected = bytes;
        armed = true;
    }
}
void* operator new(std::size_t bytes)
{
    if (FactoryAllocationFault::armed && bytes == FactoryAllocationFault::expected)
    {
        FactoryAllocationFault::armed = false;
        throw std::bad_alloc();
    }
    if (void* value = std::malloc(bytes ? bytes : 1))
        return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept
{
    std::free(value);
}
void operator delete(void* value, std::size_t) noexcept
{
    std::free(value);
}
namespace PrivateFactoryTests
{
    constexpr std::uint64_t Generation = 0xfedcba9876543210ull;
    unsigned                checks = 0;
    void                    Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationHierarchyAccess, NativeAnimationSequenceAccess, NativeAnimationSequenceStorageAccess
    {
        enum Channel : std::size_t
        {
            Create,
            ResolveH,
            Destroy,
            ResolveRow,
            ReadH,
            ReadRow,
            Count
        };
        std::array<unsigned, Count> calls{};
        ClockResult    createResult = ClockResult::Ok, hierarchyResult = ClockResult::Ok, destroyResult = ClockResult::Ok, rowResult = ClockResult::Ok;
        std::uint64_t  seenGeneration = 0;
        std::uintptr_t hierarchy = (std::numeric_limits<std::uintptr_t>::max)() - 4095u;
        std::uintptr_t firstTag = (std::numeric_limits<std::uintptr_t>::max)() - 8191u;
        std::uintptr_t rowInterface = (std::numeric_limits<std::uintptr_t>::max)() - 12287u;
        std::uintptr_t seenLocator = 0;
        std::uint32_t  seenIndex = 0;
        NativeAnimationHierarchyReferenceToken seenHierarchy{};
        NativeAnimationSequenceReferenceToken  seenSequence{};
        unsigned                               storageCalls = 0, shape = 0;
        bool                                   present = true, armCreate = false, armClone = false, armRow = false;
        CAnimBlendHierarchySAInterface**       callerHierarchy = nullptr;
        CAnimBlendSequenceSAInterface**        callerArray = nullptr;
        DWORD*                                 callerIndex = nullptr;
        unsigned                               Total() const noexcept
        {
            unsigned result = storageCalls;
            for (auto value : calls)
                result += value;
            return result;
        }
        ClockResult CreatePrivateAnimationHierarchy(std::uint64_t generation, NativeAnimationHierarchyReferenceToken& output) noexcept override
        {
            ++calls[Create];
            seenGeneration = generation;
            output = {generation + (shape == 1 ? 1u : 0u), shape == 2 ? 0 : hierarchy};
            if (armCreate && createResult == ClockResult::Ok && !shape)
                FactoryAllocationFault::Arm(sizeof(CAnimBlendHierarchyDE));
            return createResult;
        }
        ClockResult ResolvePrivateAnimationHierarchyReference(std::uint64_t generation, std::uintptr_t locator,
                                                              NativeAnimationHierarchyReferenceToken& output) noexcept override
        {
            ++calls[ResolveH];
            seenGeneration = generation;
            seenLocator = locator;
            output = {generation + (shape == 1 ? 1u : 0u), locator + (shape == 2 ? 1u : 0u)};
            if (callerHierarchy)
            {
                *callerHierarchy = nullptr;
                callerHierarchy = nullptr;
            }
            if (armClone && hierarchyResult == ClockResult::Ok && !shape)
                FactoryAllocationFault::Arm(sizeof(CAnimBlendHierarchyDE));
            return hierarchyResult;
        }
        ClockResult DestroyPrivateAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++calls[Destroy];
            seenHierarchy = token;
            return destroyResult;
        }
        ClockResult ResolvePrivateAnimationSequenceAt(std::uint64_t generation, std::uintptr_t array, std::uint32_t index, bool& found,
                                                      NativeAnimationSequenceReferenceToken& output) noexcept override
        {
            ++calls[ResolveRow];
            seenGeneration = generation;
            seenLocator = array;
            seenIndex = index;
            found = present;
            if (present || rowResult != ClockResult::Ok)
                output = {generation + (shape == 1 ? 1u : 0u), array + (shape == 2 ? 1u : 0u), static_cast<std::size_t>(index) + (shape == 3 ? 1u : 0u)};
            if (callerArray)
            {
                *callerArray = nullptr;
                callerArray = nullptr;
            }
            if (callerIndex)
            {
                *callerIndex = 0;
                callerIndex = nullptr;
            }
            if (armRow && rowResult == ClockResult::Ok && present && !shape)
                FactoryAllocationFault::Arm(sizeof(CAnimBlendSequenceDE));
            return rowResult;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token, NativeAnimationHierarchyValues& output) noexcept override
        {
            ++calls[ReadH];
            seenHierarchy = token;
            output.interfaceAddress = token.locator;
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationSequence(const NativeAnimationSequenceReferenceToken& token, NativeAnimationSequenceValues& output) noexcept override
        {
            ++calls[ReadRow];
            seenSequence = token;
            output.interfaceAddress = rowInterface;
            return ClockResult::Ok;
        }
        ClockResult AllocateAnimationSequences(std::uint64_t, std::size_t, std::uintptr_t&) noexcept override
        {
            ++storageCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult FreeAnimationSequences(std::uint64_t, std::uintptr_t) noexcept override
        {
            ++storageCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult AllocateAnimationKeyFrames(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++storageCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult FreeAnimationKeyFrames(std::uint64_t, std::uintptr_t) noexcept override
        {
            ++storageCalls;
            return ClockResult::NotInitialized;
        }
    };
    template <class Call>
    void Refuses(const char* operation, ClockResult status, Call call)
    {
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE animation manager owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "factory masked a refusal as absence or changed exact operation/status");
    }
}
int main()
{
    using namespace PrivateFactoryTests;
    try
    {
        static_assert(std::is_abstract_v<CAnimManagerDE> && !std::is_abstract_v<CAnimBlendHierarchyDE> && !std::is_abstract_v<CAnimBlendSequenceDE>);
        Owner                                  owner, other;
        std::uint64_t                          callerGeneration = Generation;
        PrivateAnimationFactoryManagerReceiver manager(owner, owner, owner, owner, callerGeneration);
        CAnimManager&                          sdk = manager;
        callerGeneration = 0;
        Check(!owner.Total(), "manager construction queried providers or allocated native resources");
        auto created = sdk.GetCustomAnimBlendHierarchy();
        Check(created && owner.calls[Owner::Create] == 1 && owner.seenGeneration == Generation && !owner.calls[Owner::ReadH],
              "new H factory changed copied generation or added read/initialization");
        Check(reinterpret_cast<std::uintptr_t>(created->GetInterface()) == owner.hierarchy && owner.seenHierarchy.ownerGeneration == Generation,
              "new actual facade lost its owner token or narrowed the locator");
        auto raw = reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy);
        owner.callerHierarchy = &raw;
        auto clone = sdk.GetCustomAnimBlendHierarchy(raw);
        Check(clone && !raw && owner.seenGeneration == Generation && owner.seenLocator == owner.hierarchy && owner.calls[Owner::ResolveH] == 1 &&
                  owner.calls[Owner::Create] == 1,
              "clone factory borrowed mutable caller input, re-created storage or skipped current resolution");
        auto  array = reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag);
        DWORD index = 0xffffffffu;
        owner.callerArray = &array;
        owner.callerIndex = &index;
        auto row = sdk.GetCustomAnimBlendSequenceAt(array, index);
        Check(row && !array && !index && owner.seenLocator == owner.firstTag && owner.seenIndex == 0xffffffffu && owner.seenGeneration == Generation,
              "row factory narrowed full DWORD or consulted caller variables after provider entry");
        Check(reinterpret_cast<std::uintptr_t>(row->GetInterface()) == owner.rowInterface && owner.seenSequence.ownerGeneration == Generation &&
                  owner.seenSequence.firstTag == owner.firstTag && owner.seenSequence.index == static_cast<std::size_t>(0xffffffffu),
              "row facade changed copied token or used classic pointer arithmetic");
        auto before = owner.Total();
        Check(!sdk.GetCustomAnimBlendHierarchy(nullptr) && !sdk.GetCustomAnimBlendSequenceAt(nullptr, 0xffffffffu), "null source created a resource");
        sdk.DeleteCustomAnimHierarchyInterface(nullptr);
        Check(owner.Total() == before, "classic no-resource null paths queried owner services");
        owner.present = false;
        Check(!sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag), 0xffffffffu),
              "qualified successful OOB was not null");
        owner.rowResult = ClockResult::ReadFailure;
        Refuses("GetCustomAnimBlendSequenceAt", ClockResult::ReadFailure,
                [&] { sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag), 0xffffffffu); });
        owner.rowResult = ClockResult::Ok;
        owner.present = true;
        for (unsigned shape : {1u, 2u, 3u})
        {
            owner.shape = shape;
            Refuses("GetCustomAnimBlendSequenceAt", ClockResult::BindingChanged,
                    [&] { sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag), 17); });
        }
        for (unsigned shape : {1u, 2u})
        {
            owner.shape = shape;
            Refuses("GetCustomAnimBlendHierarchy", ClockResult::BindingChanged, [&] { sdk.GetCustomAnimBlendHierarchy(); });
            Refuses("GetCustomAnimBlendHierarchy", ClockResult::BindingChanged,
                    [&] { sdk.GetCustomAnimBlendHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy)); });
        }
        owner.shape = 0;
        for (auto status :
             {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::OriginalException})
        {
            owner.createResult = owner.hierarchyResult = owner.destroyResult = owner.rowResult = status;
            Refuses("GetCustomAnimBlendHierarchy", status, [&] { sdk.GetCustomAnimBlendHierarchy(); });
            Refuses("GetCustomAnimBlendHierarchy", status,
                    [&] { sdk.GetCustomAnimBlendHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy)); });
            Refuses("DeleteCustomAnimHierarchyInterface", status,
                    [&] { sdk.DeleteCustomAnimHierarchyInterface(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy)); });
            Refuses("GetCustomAnimBlendSequenceAt", status,
                    [&] { sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag), 17); });
        }
        owner.createResult = owner.hierarchyResult = owner.destroyResult = owner.rowResult = ClockResult::Ok;
        sdk.DeleteCustomAnimHierarchyInterface(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy));
        Check(owner.seenHierarchy.ownerGeneration == Generation && owner.seenHierarchy.locator == owner.hierarchy,
              "explicit H delete did not forward exact owner token");
        before = owner.Total();
        created.reset();
        clone.reset();
        row.reset();
        Check(owner.Total() == before && !owner.storageCalls, "CPU facade destruction retired native resources or used unrelated storage providers");
        for (unsigned mismatch : {0u, 1u, 2u})
        {
            bool refused = false;
            try
            {
                PrivateAnimationFactoryManagerReceiver invalid(owner, mismatch == 0 ? other : owner, mismatch == 1 ? other : owner,
                                                               mismatch == 2 ? other : owner, Generation);
            }
            catch (const std::invalid_argument&)
            {
                refused = true;
            }
            Check(refused && owner.Total() == before && !other.Total(), "different complete owners were accepted or constructor queried a service");
        }
        PrivateAnimationFactoryManagerReceiver observer(owner, Generation);
        CAnimManager&                          without = observer;
        Refuses("GetCustomAnimBlendHierarchy", ClockResult::NotInitialized, [&] { without.GetCustomAnimBlendHierarchy(); });
        Refuses("GetCustomAnimBlendHierarchy", ClockResult::NotInitialized,
                [&] { without.GetCustomAnimBlendHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy)); });
        Refuses("DeleteCustomAnimHierarchyInterface", ClockResult::NotInitialized,
                [&] { without.DeleteCustomAnimHierarchyInterface(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy)); });
        Refuses("GetCustomAnimBlendSequenceAt", ClockResult::NotInitialized,
                [&] { without.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag), 1); });
        Check(owner.Total() == before, "observer-only receiver fabricated a provider or selected a global Game");
        for (auto cleanup : {ClockResult::Ok, ClockResult::WriteFailure})
        {
            owner.destroyResult = cleanup;
            owner.armCreate = true;
            const auto destroys = owner.calls[Owner::Destroy];
            bool       failed = false, nested = false;
            try
            {
                sdk.GetCustomAnimBlendHierarchy();
            }
            catch (const std::bad_alloc&)
            {
                failed = cleanup == ClockResult::Ok;
            }
            catch (const std::runtime_error& error)
            {
                failed = cleanup != ClockResult::Ok &&
                         std::string(error.what()) == "DE hierarchy facade construction failed; owner cleanup acknowledgement refused: " +
                                                          std::to_string(static_cast<unsigned>(cleanup));
                try
                {
                    std::rethrow_if_nested(error);
                }
                catch (const std::bad_alloc&)
                {
                    nested = true;
                }
            }
            Check(failed && (cleanup == ClockResult::Ok || nested) && !FactoryAllocationFault::armed && owner.calls[Owner::Destroy] == destroys + 1 &&
                      owner.seenHierarchy.ownerGeneration == Generation && owner.seenHierarchy.locator == owner.hierarchy,
                  "actual new-H bad_alloc missed matching owner cleanup or hid acknowledgement refusal/nested allocation failure");
            owner.armCreate = false;
        }
        owner.destroyResult = ClockResult::Ok;
        for (bool sequence : {false, true})
        {
            const auto destroys = owner.calls[Owner::Destroy];
            bool       failed = false;
            owner.armClone = !sequence;
            owner.armRow = sequence;
            try
            {
                if (sequence)
                    sdk.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.firstTag), 1);
                else
                    sdk.GetCustomAnimBlendHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.hierarchy));
            }
            catch (const std::bad_alloc&)
            {
                failed = true;
            }
            owner.armClone = owner.armRow = false;
            Check(failed && !FactoryAllocationFault::armed && owner.calls[Owner::Destroy] == destroys && !owner.storageCalls,
                  "CPU clone/row allocation failure deleted a pre-existing resource");
        }
        std::puts("Actual private animation manager factory SDK fixture completed");
        std::printf("Passed %u private animation factory SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private factory SDK failed: %s\n", error.what());
        return 1;
    }
}
