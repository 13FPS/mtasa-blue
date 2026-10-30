/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Final-entry effects for the actual dynamic clump owner protocol
 ****************************************************************************/
namespace ClockDynamicClumpInvokeTest
{
    namespace Calls = NativeAnimationAssociationCalls;
    namespace F = NativeStaticAssociationInitializeFixture;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Old = ClockPrivateAnimationAssociationTest;
    struct Context
    {
        std::uintptr_t                image = 0, receiver = 0, hierarchy = 0;
        F::State*                     setup = nullptr;
        unsigned                      mode = 0, constructors = 0, initializers = 0, defaults = 0;
        bool                          gateBusy = false, lifetimeBusy = false;
        Calls::ClumpInvocations       invocations{};
        std::array<std::size_t, 13>   offsets{};
        std::array<std::size_t, 13>   widths{};
        std::array<std::uint64_t, 13> targets{};
    };
    Context*                       current = nullptr;
    const Calls::ClumpInvocations* Supplier() noexcept
    {
        return current ? &current->invocations : nullptr;
    }

    ClockResult InitializeEffects(std::uintptr_t receiver, std::uintptr_t clump, std::uintptr_t hierarchy, std::uint32_t&) noexcept
    {
        auto& c = *current;
        Check(receiver == c.receiver && clump == c.setup->Clump() && hierarchy == c.hierarchy,
              "final clump entry receives the actual issued receiver and admitted clump/hierarchy tuple");
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        c.gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        c.lifetimeBusy = runtime.privateAssociations.IsRetained() && NativeLifetimeRetained();
        const auto count = F::Load<std::int16_t>(c.setup->container.data(), 0x10);
        Check(count == 0 || count == 2, "bounded final-entry fixture selects only its actual zero/two frame layouts");
        Old::Store(receiver + 0x18, count);
        Old::HeapRoute route;
        Check(route.Begin(), "route only the existing qualified isolated node-producer heap leaf");
        // This already-used isolated leaf executes the genuine FEBC10 node
        // producer. Neither FEB910 nor the UE/name matcher FEBA90 runs here.
        using AllocateNodes = void (*)(std::uintptr_t, std::int32_t);
        reinterpret_cast<AllocateNodes>(c.image + NativeAnimationAssociationProfile::NodeAllocateRva)(receiver, count);
        Check(route.Restore() && !Old::heapFailed, "restore actual heap virtual slots before production postqualification");
        const auto nodes = Old::Load<std::uintptr_t>(receiver + 0x20);
        for (std::int16_t n = 0; n != count; ++n)
            Old::Store(nodes + static_cast<std::uintptr_t>(n) * 40 + 0x18, receiver);
        Old::Store(receiver + 0x28, hierarchy);
        const auto sequences = Old::Load<std::uintptr_t>(hierarchy + 8);
        const auto sequenceCount = Old::Load<std::uint16_t>(hierarchy + 0x10);
        if (sequenceCount)
        {
            Check(count == 2 && sequenceCount == 2 && sequences == c.setup->Sequence(0),
                  "mixed source effects are fixed known bone/name results from the actual captured two-row fixture");
            Old::Store(nodes + 0x10, c.setup->Sequence(0));
            Old::Store(nodes + 40 + 0x10, c.setup->Sequence(1));
        }
        return ClockResult::Ok;
    }
    ClockResult Initialize(Calls::ClumpInitializeFunction function, std::uintptr_t receiver, std::uintptr_t clump, std::uintptr_t hierarchy,
                           std::uint32_t& exception) noexcept
    {
        auto& c = *current;
        ++c.initializers;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + 0xfeba90, "ordinary/custom initialization reaches the exact admitted FEBA90 entry once");
        const auto result = InitializeEffects(receiver, clump, hierarchy, exception);
        if (result == ClockResult::Ok && c.mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (result == ClockResult::Ok && c.mode == 3)
            Old::Store(c.image + NativeBorrowedGcProfile::markerRva, LONG{1});
        return result;
    }
    ClockResult Construct(Calls::ClumpConstructorFunction function, std::uintptr_t receiver, std::uintptr_t clump, std::uintptr_t hierarchy,
                          std::uintptr_t& returned, std::uint32_t& exception) noexcept
    {
        auto& c = *current;
        ++c.constructors;
        Check(reinterpret_cast<std::uintptr_t>(function) == c.image + 0xfeb910 && receiver == c.receiver,
              "clump constructor uses original FEB910 and the actual unconstructed owner-issued allocation");
        // Fixed source-defined effects at the final invocation seam, selected
        // from the saved complete89B body. Undefined callback/padding survives.
        Old::Store(receiver, c.image + NativeAnimationAssociationProfile::VtableRva);
        Old::Store(receiver + 0x1a, std::uint16_t{0xffff});
        Old::Store(receiver + 0x20, std::uintptr_t{0});
        Old::Store(receiver + 0x38, std::uint32_t{0});
        Old::Store(receiver + 0x60, static_cast<unsigned char>(0));
        Old::Store(receiver + 8, std::uintptr_t{0});
        Old::Store(receiver + 0x10, std::uintptr_t{0});
        Old::Store(receiver + 0x30, std::uint64_t{0x3f800000});
        Old::Store(receiver + 0x3c, std::uint64_t{0x3f800000});
        Old::Store(receiver + 0x44, std::uint64_t{0xffff});
        const auto result = InitializeEffects(receiver, clump, hierarchy, exception);
        returned = receiver;
        if (result == ClockResult::Ok && c.mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (result == ClockResult::Ok && c.mode == 3)
            Old::Store(c.image + NativeBorrowedGcProfile::markerRva, LONG{1});
        return result;
    }
    ClockResult Defaults(std::uintptr_t address, std::size_t width, std::uint64_t before, std::uint64_t target, std::uint64_t& observed, bool& accepted,
                         std::uint32_t& exception) noexcept
    {
        auto& c = *current;
        Check(c.defaults < 13 && address >= c.receiver && address + width <= c.receiver + 104,
              "source custom defaults are restricted to the actual issued104B receiver");
        const auto step = c.defaults++;
        c.offsets[step] = address - c.receiver;
        c.widths[step] = width;
        c.targets[step] = target;
        if (width == 2)
            observed = static_cast<std::uint16_t>(
                InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), static_cast<SHORT>(target), static_cast<SHORT>(before)));
        else if (width == 4)
            observed = static_cast<std::uint32_t>(
                InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(target), static_cast<LONG>(before)));
        else if (width == 8)
            observed = static_cast<std::uint64_t>(
                InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), static_cast<LONG64>(target), static_cast<LONG64>(before)));
        else
            return ClockResult::InvalidArgument;
        const auto mask = width == 8 ? ~std::uint64_t{0} : (std::uint64_t{1} << (width * 8)) - 1;
        accepted = observed == (before & mask);
        if (accepted && c.mode == 2 && step == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        return ClockResult::Ok;
    }
    void Select(Context& value, std::uintptr_t receiver, std::uintptr_t hierarchy, unsigned mode = 0)
    {
        value.receiver = receiver;
        value.hierarchy = hierarchy;
        value.mode = mode;
        value.defaults = 0;
        value.invocations = {&Construct, &Initialize, &Defaults};
        current = &value;
    }
}

inline constexpr unsigned char DynamicClumpConstructorProof[] = {
    0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xd9, 0x48, 0x8d, 0x05, 0xa8, 0x68, 0x2f, 0x03, 0x48, 0x89, 0x01, 0xb8, 0xff, 0xff, 0xff,
    0xff, 0x66, 0x89, 0x41, 0x1a, 0x33, 0xc9, 0x48, 0x89, 0x4b, 0x20, 0x89, 0x4b, 0x38, 0x88, 0x4b, 0x60, 0x48, 0x89, 0x4b, 0x08, 0x48, 0x89,
    0x4b, 0x10, 0x48, 0x8b, 0xcb, 0x48, 0xc7, 0x43, 0x30, 0x00, 0x00, 0x80, 0x3f, 0x48, 0xc7, 0x43, 0x3c, 0x00, 0x00, 0x80, 0x3f, 0x48, 0xc7,
    0x43, 0x44, 0xff, 0xff, 0x00, 0x00, 0xe8, 0x30, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xc3, 0x48, 0x83, 0xc4, 0x20, 0x5b, 0xc3};
inline constexpr unsigned char DynamicClumpInitializeProof[] = {
    0x48, 0x89, 0x6c, 0x24, 0x18, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x57, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xba, 0x00, 0x03, 0x00, 0x00, 0x4c,
    0x8b, 0xea, 0x4d, 0x8b, 0xf8, 0x48, 0x8b, 0xf1, 0x0f, 0xbf, 0x47, 0x10, 0x8b, 0xd0, 0x66, 0x89, 0x41, 0x18, 0xe8, 0x50, 0x01, 0x00, 0x00, 0x45, 0x33,
    0xe4, 0x45, 0x8b, 0xcc, 0x66, 0x44, 0x3b, 0x66, 0x18, 0x7d, 0x1c, 0x41, 0x8b, 0xcc, 0x48, 0x8b, 0x46, 0x20, 0x48, 0x8d, 0x49, 0x28, 0x41, 0xff, 0xc1,
    0x48, 0x89, 0x74, 0x08, 0xf0, 0x0f, 0xbf, 0x46, 0x18, 0x44, 0x3b, 0xc8, 0x7c, 0xe7, 0x4c, 0x89, 0x7e, 0x28, 0x41, 0x8b, 0xec, 0x66, 0x45, 0x3b, 0x67,
    0x10, 0x0f, 0x8d, 0xf5, 0x00, 0x00, 0x00, 0x48, 0x89, 0x5c, 0x24, 0x50, 0x4c, 0x89, 0x74, 0x24, 0x58, 0x4d, 0x8b, 0xf4, 0x0f, 0x1f, 0x84, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x8b, 0x5f, 0x08, 0x49, 0x03, 0xde, 0x0f, 0xb6, 0x43, 0x42, 0xc0, 0xe8, 0x04, 0xa8, 0x01, 0x74, 0x7a, 0x8b, 0x87, 0x40, 0x14,
    0x00, 0x00, 0x3b, 0x87, 0x6c, 0x14, 0x00, 0x00, 0x0f, 0x84, 0x9f, 0x00, 0x00, 0x00, 0x48, 0x63, 0x8f, 0x80, 0x14, 0x00, 0x00, 0x4c, 0x8d, 0x87, 0x70,
    0x14, 0x00, 0x00, 0x49, 0x8b, 0x50, 0x08, 0x48, 0xff, 0xc9, 0x4c, 0x0f, 0xbf, 0x4b, 0x40, 0x49, 0x23, 0xc9, 0x48, 0x85, 0xd2, 0x4c, 0x0f, 0x45, 0xc2,
    0x41, 0x8b, 0x04, 0x88, 0x83, 0xf8, 0xff, 0x74, 0x72, 0x48, 0x8b, 0x97, 0x38, 0x14, 0x00, 0x00, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48,
    0x98, 0x48, 0xc1, 0xe0, 0x04, 0x48, 0x03, 0xc2, 0x44, 0x39, 0x08, 0x74, 0x0a, 0x8b, 0x40, 0x08, 0x83, 0xf8, 0xff, 0x75, 0xea, 0xeb, 0x4b, 0x48, 0x85,
    0xc0, 0x48, 0x8d, 0x48, 0x04, 0x49, 0x0f, 0x44, 0xcc, 0x48, 0x85, 0xc9, 0x74, 0x3b, 0x8b, 0x01, 0xeb, 0x0b, 0x48, 0x8b, 0xd3, 0x49, 0x8b, 0xcd, 0xe8,
    0x89, 0xfa, 0x00, 0x00, 0x83, 0xf8, 0xff, 0x74, 0x27, 0x48, 0x63, 0xc8, 0x48, 0x8b, 0xc1, 0x48, 0xc1, 0xe0, 0x05, 0x48, 0x83, 0xc0, 0x28, 0x48, 0x03,
    0xc7, 0x74, 0x14, 0x66, 0x44, 0x39, 0x63, 0x44, 0x7e, 0x0d, 0x48, 0x8b, 0x46, 0x20, 0x48, 0x8d, 0x0c, 0x89, 0x48, 0x89, 0x5c, 0xc8, 0x10, 0x41, 0x0f,
    0xbf, 0x47, 0x10, 0xff, 0xc5, 0x49, 0x83, 0xc6, 0x50, 0x3b, 0xe8, 0x0f, 0x8c, 0x2a, 0xff, 0xff, 0xff, 0x4c, 0x8b, 0x74, 0x24, 0x58, 0x48, 0x8b, 0x5c,
    0x24, 0x50, 0x48, 0x8b, 0x6c, 0x24, 0x60, 0x48, 0x83, 0xc4, 0x20, 0x41, 0x5f, 0x41, 0x5d, 0x41, 0x5c, 0x5f, 0x5e, 0xc3};
#include "../../Client/game_de/NativeTemporaryDynamicAssociationInitializeCalls.h"
namespace DynamicClumpOwnerFixture
{
    unsigned scenario = 0;
    struct TemporaryContext
    {
        std::uintptr_t                                image = 0, hierarchy = 0;
        std::uint64_t                                 generation = 0;
        NativeAnimationAssociationReferenceToken      first{}, second{};
        ClockDynamicClumpInvokeTest::Context*         invoke = nullptr;
        NativeAnimationAssociationCalls::ClumpReceipt receipt{};
        std::uintptr_t                                returned = 0;
    };
    ClockResult TemporaryRow(const NativeTemporaryModelClumpBorrow& borrow, void* raw) noexcept
    {
        auto&      c = *static_cast<TemporaryContext*>(raw);
        const auto result = NativeTemporaryDynamicAssociationInitializeCalls::Apply(
            c.image, runtime.privateAssociations, c.generation, c.first, NativeAnimationAssociationClumpOperation::Construct, borrow, c.hierarchy,
            runtime.privateHierarchies, &CheckTimingOwner, c.returned, &c.receipt, &c.invoke->invocations);
        if (result == ClockResult::Ok)
        {
            const auto                                    originals = c.invoke->constructors;
            std::uintptr_t                                unused = 0;
            NativeAnimationAssociationCalls::ClumpReceipt second;
            Check(NativeTemporaryDynamicAssociationInitializeCalls::Apply(
                      c.image, runtime.privateAssociations, c.generation, c.second, NativeAnimationAssociationClumpOperation::Construct, borrow, c.hierarchy,
                      runtime.privateHierarchies, &CheckTimingOwner, unused, &second, &c.invoke->invocations) != ClockResult::Ok &&
                      !second.originalEntered && c.invoke->constructors == originals,
                  "one exact live temporary borrow cannot initialize a second actually issued dynamic receiver");
        }
        return result;
    }
    template <class Action>
    void RunTemporary(ClockImageFixture& fixture, NativeStaticAssociationInitializeFixture::State& setup, const NativeAnimationAssociationReferenceToken& first,
                      std::uintptr_t receiver, std::uintptr_t hierarchy, std::uint64_t generation, ClockDynamicClumpInvokeTest::Context& invocation,
                      const Action& action)
    {
        namespace F = NativeStaticAssociationInitializeFixture;
        namespace T = NativeTemporaryModelClumpCalls;
        namespace Hook = ClockJointTemporaryStaticOwnerTest;
        TemporaryContext row;
        row.image = fixture.image;
        row.hierarchy = hierarchy;
        row.generation = generation;
        row.first = first;
        row.invoke = &invocation;
        NativeTemporaryModelClump temporary;
        Hook::Context             effects;
        effects.image = fixture.image;
        effects.setup = &setup;
        effects.generation = generation;
        effects.temporary = &temporary;
        T::Receipt receipt;
        action(
            [&]
            {
                NativeAnimationHierarchyReferenceToken privateH;
                Check(runtime.access.CreatePrivateAnimationHierarchy(generation, privateH) == ClockResult::Ok &&
                          runtime.access.InitializePrivateAnimationHierarchy(privateH) == ClockResult::Ok,
                      "temporary private H uses its actual existing empty issued owner");
                row.hierarchy = privateH.locator;
                Check(runtime.access.CreatePrivateAnimationAssociation(generation, row.second) == ClockResult::Ok,
                      "temporary single-claim refusal uses a second real unconstructed issuer record");
                std::uintptr_t unused = 0;
                Check(runtime.access.ApplyPrivateAnimationAssociationClump(first, NativeAnimationAssociationClumpOperation::Construct, setup.Clump(),
                                                                           row.hierarchy, unused) != ClockResult::Ok &&
                          !runtime.stopping.load(),
                      "unregistered raw clump cannot impersonate the public registered-ped origin");
                Check(NativeModelBankReader({fixture.image}, &CheckTimingOwner).Read(0, effects.model) == ClockResult::Ok,
                      "temporary itinerary uses the actual current model bank value");
                effects.modelAddress = effects.model.nativeModelInfo;
                ClockJointTemporaryStaticOwnerTest::current = &effects;
                ClockDynamicClumpInvokeTest::Select(invocation, receiver, row.hierarchy);
                const T::Invocations calls{&Hook::Create, &Hook::Setup, &Hook::Prepare, &Hook::Retire};
                Check(T::WithTemporary(fixture.image, temporary, generation, effects.model, &CheckTimingOwner, &TemporaryRow, &row, &receipt, &calls) ==
                              ClockResult::Ok &&
                          receipt.verified && receipt.retireAccepted && temporary.CanDiscard() && row.receipt.verified && row.returned == receiver &&
                          invocation.constructors == 1 && invocation.gateBusy && !runtime.pedReferences.IsRetained(),
                      "closed temporary/private-H origin shares the held native gate and then acknowledges original actor retirement");
                Check(runtime.access.DestroyPrivateAnimationHierarchy(privateH) == ClockResult::OwnerConflict,
                      "temporary actor retirement does not retire the dynamic receiver's separate private H hold");
                Check(runtime.access.RetirePrivateAnimationAssociation(first) == ClockResult::Ok &&
                          runtime.access.RetirePrivateAnimationAssociation(row.second) == ClockResult::Ok &&
                          runtime.access.DestroyPrivateAnimationHierarchy(privateH) == ClockResult::Ok,
                      "temporary bridge owns neither dynamic receiver after its actual separate private retirement");
                ClockJointTemporaryStaticOwnerTest::current = nullptr;
            });
        std::puts("Production dynamic clump closed temporary origin fixture completed");
    }
}

namespace DynamicClumpOwnerFixture
{
    void RunPrepared(ClockImageFixture& fixture)
    {
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace Request = NativeStreamingRequestProfile;
        namespace B = NativeAnimationBlockProfile;
        namespace H = NativeAnimationBlockHierarchyProfile;
        namespace G = NativeAnimationGroupArrayProfile;
        using Setup = NativeStaticAssociationInitializeFixture::State;
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : Ped::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
            writes.Global(slot.rva, fixture.image + slot.targetRva, PAGE_READONLY);
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &writes);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::State heap;
        heap.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        NativeCustomAnimSequenceStorageFixture::active = &heap;
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationBlockRefProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : G::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationStaticAssociationProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // The actual owner reacquires and reads the physical row before native
        // entry. Seed those read leaves too; selector/retire pins alone only
        // admitted factory resolution and hid this fixture omission.
        for (const auto& pin : NativeAnimationStaticAssociationProfile::ReadCode)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(NativePlayerInfoProfile::BankRva, std::uintptr_t{0});
        std::vector<unsigned char> references(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(references.data() + i * R::ReferenceBytes, &next, 8);
        }
        WorldGroundOwnerFixture::Writes::Apply(&writes, R::ReferencesRva, references.data(), references.size(), PAGE_READWRITE);
        writes.Global(R::FreeHeadRva, fixture.image + R::ReferencesRva);
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto ped = reinterpret_cast<std::uintptr_t>(pool.objects);
        NativeStaticAssociationInitializeFixture::Store(pool.objects, 0, fixture.image + R::PedVtableRva);
        pool.objects[R::TypeOffset] = 3;
        NativeStaticAssociationInitializeFixture::Store(pool.objects, NativePedMetadataProfile::ModelOffset, std::uint16_t{0});
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderCountOffset + 8> provider{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ProviderRowStride>       providerRows{};
        alignas(std::uintptr_t) std::array<unsigned char, Request::ModelPrefixBytes>        model{};
        alignas(std::uintptr_t) std::array<unsigned char, 12 * 24>                          baseItems{};
        std::memcpy(baseItems.data(), graph.items.data(), graph.items.size());
        const auto providerAddress = reinterpret_cast<std::uintptr_t>(provider.data());
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0, fixture.image + Request::RootVtableRva);
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0x0c, std::int32_t{11});
        NativeStaticAssociationInitializeFixture::Store(provider.data(), 0x10, graph.Class(0));
        NativeStaticAssociationInitializeFixture::Store(provider.data(), Request::ProviderRowsOffset, reinterpret_cast<std::uintptr_t>(providerRows.data()));
        NativeStaticAssociationInitializeFixture::Store(provider.data(), Request::ProviderCountOffset, std::int32_t{1});
        NativeStaticAssociationInitializeFixture::Store(root.data(), Request::ProviderOffset, providerAddress);
        NativeStaticAssociationInitializeFixture::Store(baseItems.data(), 11 * 24, providerAddress);
        NativeStaticAssociationInitializeFixture::Store(baseItems.data(), 11 * 24 + 0x10, std::uint32_t{111});
        NativeStaticAssociationInitializeFixture::Store(model.data(), 0, fixture.image + 0x4301000);
        for (const auto& pin : NativeModelBankProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        std::array<std::uintptr_t, 0x80 / 8> modelTable{};
        modelTable[Request::TypeMethodSlot / 8] = fixture.image + NativeStaticAssociationInitializeFixture::ModelTypeRva;
        modelTable[Request::AnimationMethodSlot / 8] = fixture.image + NativeStaticAssociationInitializeFixture::EventRva;
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0x4301000, modelTable.data(), sizeof(modelTable), PAGE_READONLY);
        writes.Global(NativeModelBankProfile::BankRva, reinterpret_cast<std::uintptr_t>(model.data()));
        writes.Global(NativeModelLoadedProfile::ArrayRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(1));
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 2 * 32> parents{};
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 32>     children{};
        NativeStaticAssociationInitializeFixture::Store(parents.data(), 0, std::uint64_t{2});
        NativeStaticAssociationInitializeFixture::Store(children.data(), 0, std::uint64_t{1});
        const auto groups = reinterpret_cast<std::uintptr_t>(parents.data()) + 8;
        const auto association = reinterpret_cast<std::uintptr_t>(children.data()) + 8;
        const auto block = fixture.image + B::BankRva + B::RowBytes;
        const auto hierarchy = fixture.image + H::BankRva + H::RowBytes;
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0, block);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 8, association);
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x10, std::uint32_t{1});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x14, std::uint32_t{17});
        NativeStaticAssociationInitializeFixture::Store(reinterpret_cast<void*>(groups + 32), 0x18, std::uint32_t{1});
        writes.Global(G::GlobalRva, groups);
        writes.Global(G::CountRva, std::int32_t{2});
        writes.Global(B::CountRva, std::int32_t{2});
        writes.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{2});
        const NativeAnimationBlockName name{'b', 'a', 'n', 'k', 0};
        writes.Global(B::BankRva + B::RowBytes, name);
        writes.Global(B::BankRva + B::RowBytes + B::LoadedOffset, static_cast<unsigned char>(1));
        writes.Global(B::BankRva + B::RowBytes + B::IdOffset, std::int32_t{1});
        writes.Global(B::BankRva + B::RowBytes + B::AnimationCountOffset, std::uint32_t{1});
        Setup setup;
        setup.Reset(fixture.image, ped, association, hierarchy, &WorldGroundOwnerFixture::Writes::Apply, &writes);
        setup.EnableSkeletonInputs();
        Check(setup.AttachRegistry(baseItems.data(), 12, graph.Class(0), &WorldGroundOwnerFixture::Writes::Apply, &writes),
              "append actual clump/class/component/skeleton members to the existing world/provider fixture registry");
        if (scenario == 8)
        {
            for (const auto& pin : NativeTemporaryModelClumpProfile::Code)
                WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const unsigned char ret = 0xc3, opaque = 0x5a;
            WorldGroundOwnerFixture::Writes::Apply(&writes, NativeTemporaryModelClumpProfile::SetupRva, &ret, 1, PAGE_EXECUTE_READ);
            WorldGroundOwnerFixture::Writes::Apply(&writes, NativeTemporaryModelClumpProfile::OpaqueDefaultRva, &opaque, 1, PAGE_READONLY);
            writes.Global(0x4301000 + NativeTemporaryModelClumpProfile::CreateSlot, fixture.image + NativeTemporaryModelClumpProfile::CreateRva, PAGE_READONLY);
            writes.Global(NativeTemporaryModelClumpProfile::SetupClassSlotRva, reinterpret_cast<std::uintptr_t>(setup.actorClass.data()));
        }
        const auto previousImage = ClockReaderTestHost::image, previousReferences = ClockPedReferenceOwnerTest::image;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService service;
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "pin actual callback code while registered references and native journal may remain");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto action = [&](const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    const auto& r = runtime.animationAssociationClump;
                    std::fprintf(
                        stderr,
                        "Dynamic clump scenario=%u: %s; result=%u command=%llu entered=%u returned=%u observed=%u verified=%u nodes=%llx defaults=%u/%u/%u\n",
                        scenario, error.what(), static_cast<unsigned>(r.result), static_cast<unsigned long long>(r.command),
                        static_cast<unsigned>(r.originalEntered), static_cast<unsigned>(r.originalReturned), static_cast<unsigned>(r.observed),
                        static_cast<unsigned>(r.verified), static_cast<unsigned long long>(r.nodes), r.defaultsAttempted, r.defaultsAccepted,
                        r.defaultsVerified);
                    Check(false, "actual dynamic clump action must complete its expected result");
                }
            };
            Check(service.Renew(generation), "renew the actual dynamic clump producer before an engine action");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "actual independent producer preserves existing500ms native owner policy");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(), "join producer before subsequent owner actions");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        namespace F = NativeStaticAssociationInitializeFixture;
        namespace Dynamic = ClockDynamicClumpInvokeTest;
        namespace Old = ClockPrivateAnimationAssociationTest;
        namespace Node = ClockAnimationAssociationNodeArrayTest;
        namespace Calls = NativeAnimationAssociationCalls;
        using Token = NativeAnimationAssociationReferenceToken;
        auto&            access = static_cast<NativeAnimationAssociationAccess&>(runtime.access);
        Dynamic::Context context;
        context.image = fixture.image;
        context.setup = &setup;
        Dynamic::current = &context;
        Old::image = fixture.image;
        Old::constructorCalls = Old::destructorCalls = Old::exchangeCalls = Old::heapAllocations = Old::heapFrees = 0;
        Old::targetsValid = Old::snapshotsCopied = true;
        Old::heapFailed = false;
        Old::mode = 0;
        Node::active = Node::actualFreed = Node::freeAcknowledged = Node::clearAcknowledged = false;
        Node::mode = Node::reservationCount = 0;
        // These two saved whole bodies are profile inputs, never executed by
        // this final-entry fixture. FEBC10 and the existing copied destructor
        // are the isolated native leaves explicitly exercised here.
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0xfeb910, DynamicClumpConstructorProof, sizeof(DynamicClumpConstructorProof), PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&writes, 0xfeba90, DynamicClumpInitializeProof, sizeof(DynamicClumpInitializeProof), PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationAssociationProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        writes.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        const auto counter = block + NativeAnimationBlockRefProfile::RefOffset;
        writes.Global(counter - fixture.image, std::uint16_t{17});
        // A valid distinct bank H makes the owned-copy retarget refusal a
        // provenance test rather than an unrelated malformed-input refusal.
        auto alternateBlock = std::array<unsigned char, B::RowBytes>{};
        std::memcpy(alternateBlock.data(), reinterpret_cast<void*>(block), alternateBlock.size());
        F::Store(alternateBlock.data(), B::IdOffset, std::int32_t{0});
        F::Store(alternateBlock.data(), NativeAnimationBlockRefProfile::RefOffset, std::uint16_t{23});
        writes.Global(B::BankRva, alternateBlock);
        auto alternateHierarchy = std::array<unsigned char, H::RowBytes>{};
        std::memcpy(alternateHierarchy.data(), reinterpret_cast<void*>(hierarchy), alternateHierarchy.size());
        F::Store(alternateHierarchy.data(), 0x14, std::int32_t{0});
        writes.Global(H::BankRva, alternateHierarchy);
        const auto              otherCounter = fixture.image + B::BankRva + NativeAnimationBlockRefProfile::RefOffset;
        NativePedReferenceToken pedToken;
        if (scenario != 8)
            action(
                [&]
                {
                    Check(runtime.access.AcquirePedReference(1, pedToken) == ClockResult::Ok,
                          "actual dynamic clump input is borrowed from a genuinely registered current ped");
                });
        const auto rawClump = reinterpret_cast<RpClump*>(setup.Clump());
        const auto rawHierarchy = reinterpret_cast<CAnimBlendHierarchySAInterface*>(hierarchy);
        const auto make = [&](Token& token)
        {
            Check(access.CreatePrivateAnimationAssociation(generation, token) == ClockResult::Ok && token.ownerGeneration == generation && token.serial,
                  "actual allocator/controller issues an unconstructed104B receiver before any SDK initialization");
            context.receiver = heap.base;
            Check(heap.bytes == 104 && heap.Contains(context.receiver), "receiver address comes only from the actual bounded allocator call");
        };
        const auto select = [&](const Token& token, std::uintptr_t receiver, std::uintptr_t h, unsigned mode = 0)
        {
            Check(token.ownerGeneration == generation && token.serial, "select effects only for the actual issued receiver token");
            Node::active = false;
            Dynamic::Select(context, receiver, h, mode);
        };
        const auto freeNodes = [&](const Token& token, std::uintptr_t receiver)
        {
            Node::active = true;
            Node::actualFreed = Node::freeAcknowledged = Node::clearAcknowledged = false;
            Node::receiver = receiver;
            Node::nodes = Old::Load<std::uintptr_t>(receiver + 0x20);
            Node::raw = Old::Load<std::uintptr_t>(Node::nodes - 8);
            Node::mode = 0;
            PrivateAnimationAssociationReceiver facade(access, token);
            const auto                          beforeH = Old::Load<std::uintptr_t>(receiver + 0x28);
            facade.FreeAnimBlendNodeArray();
            Check(Node::actualFreed && Node::freeAcknowledged && Old::Load<std::uintptr_t>(receiver + 0x20) == Node::nodes &&
                      Old::Load<std::uintptr_t>(receiver + 0x28) == beforeH,
                  "actual node-only ACK leaves stale nodes/count and the readable hierarchy pointer unchanged");
        };
        const auto retire = [&](const Token& token)
        {
            Check(access.RetirePrivateAnimationAssociation(token) == ClockResult::Ok,
                  "actual private owner retires only its issued receiver with the correct reference provenance");
            Node::active = false;
        };
        const auto refuses = [&](const auto& body, const char* why)
        {
            bool refused = false;
            try
            {
                body();
            }
            catch (const std::exception&)
            {
                refused = true;
            }
            Check(refused, why);
        };
        const auto customOrder = [&]
        {
            const std::array<std::size_t, 13>   offsets{0x30, 0x3c, 0x20, 0x34, 0x38, 0x40, 0x46, 0x48, 0, 0x1a, 0x44, 0x10, 8};
            const std::array<std::size_t, 13>   widths{4, 4, 8, 4, 4, 4, 2, 4, 8, 2, 2, 8, 8};
            const std::array<std::uint64_t, 13> targets{0x3f800000, 0x3f800000, 0, 0, 0, 0, 0, 0, fixture.image + NativeAnimationAssociationProfile::VtableRva,
                                                        0xffff,     0xffff,     0, 0};
            Check(context.defaults == 13 && context.offsets == offsets && context.widths == widths && context.targets == targets,
                  "actual custom wrapper performs exactly its thirteen source stores in order before its single Init");
        };
        Token                                  token;
        std::uintptr_t                         receiver = 0;
        NativeAnimationHierarchyReferenceToken privateH{}, nextH{};
        const auto                             makeHierarchy = [&](NativeAnimationHierarchyReferenceToken& h)
        {
            Check(runtime.access.CreatePrivateAnimationHierarchy(generation, h) == ClockResult::Ok &&
                      runtime.access.InitializePrivateAnimationHierarchy(h) == ClockResult::Ok,
                  "private H is created and initialized by its actual existing runtime owner, never a stamped raw buffer");
        };
        action(
            [&]
            {
                make(token);
                receiver = context.receiver;
                NativeAnimationStaticAssociationReferenceToken staticRow;
                Check(runtime.access.ResolveAnimationStaticAssociationInterface(generation, association, staticRow) == ClockResult::Ok,
                      "actual static bank origin remains a distinct issued source domain");
                const Token    forged{generation, staticRow.locator};
                std::uintptr_t unchanged = 0x55667788;
                Check(access.ApplyPrivateAnimationAssociationClump(forged, NativeAnimationAssociationClumpOperation::Construct, setup.Clump(), hierarchy,
                                                                   unchanged) != ClockResult::Ok &&
                          unchanged == 0x55667788 && !context.constructors && !context.initializers,
                      "a genuine static source locator cannot manufacture an issued dynamic receiver serial");
            });
        Check(access.ApplyPrivateAnimationAssociationClump(token, NativeAnimationAssociationClumpOperation::Construct, setup.Clump(), hierarchy, receiver) !=
                  ClockResult::Ok,
              "issued pointers do not impersonate a natural callback outside its current owner");
        // The failed output above is deliberately not used as receiver
        // authority. Recover only the actual original allocation observation.
        receiver = context.receiver;
        const auto initialized = [&]
        {
            const auto& receipt = runtime.animationAssociationClump;
            Check(receipt.originalEntered && receipt.originalReturned && receipt.observed && receipt.verified && receipt.receiverGeneration == generation &&
                      receipt.receiverSerial == token.serial && receipt.receiver == receiver && context.gateBusy && context.lifetimeBusy &&
                      !runtime.stopping.load(),
                  "actual SDK/runtime/native ownership validates the same issued receiver under the held native gate");
            const auto nodes = Old::Load<std::uintptr_t>(receiver + 0x20);
            const auto count = Old::Load<std::int16_t>(receiver + 0x18);
            for (std::int16_t n = 0; n != count; ++n)
                Check(Old::Load<std::uintptr_t>(nodes + static_cast<std::uintptr_t>(n) * 40 + 0x18) == receiver,
                      "every actual dynamic40 node points back to its own issued receiver");
        };
        if (scenario == 0 || scenario == 1)
        {
            if (scenario == 1)
            {
                F::Store(setup.container.data(), 0x10, std::int32_t{0});
                F::Store(setup.container.data(), 0x988, std::int32_t{0});
                std::array<unsigned char, NativeStaticAssociationInitializeProfile::MapHeaderBytes> empty{};
                F::Store(empty.data(), 0x30, std::int32_t{-1});
                std::memcpy(setup.container.data() + NativeStaticAssociationInitializeProfile::MapOffset, empty.data(), empty.size());
                writes.Global(hierarchy - fixture.image + 0x10, std::uint16_t{0});
            }
            action(
                [&]
                {
                    select(token, receiver, hierarchy);
                    PrivateAnimationAssociationReceiver facade(access, token);
                    Check(facade.Constructor(rawClump, rawHierarchy) == reinterpret_cast<CAnimBlendAssociationSAInterface*>(receiver),
                          "real clump Constructor returns its actual acknowledged receiver");
                    initialized();
                    const auto nodes = Old::Load<std::uintptr_t>(receiver + 0x20);
                    Check(nodes && heap.Contains(Old::Load<std::uintptr_t>(nodes - 8)) && context.constructors == 1 && !context.initializers &&
                              !context.defaults && Old::Load<std::uint16_t>(counter) == 17 && Old::Load<unsigned char>(receiver + 0x60) == 0,
                          "clump constructor owns genuine node storage, defines extra60 and acquires no bank reference");
                    if (scenario == 1)
                        Check(runtime.animationAssociationClump.nodeCount == 0 && runtime.animationAssociationClump.nodeRequestBytes == 64,
                              "zero logical count still owns the original aligned64-byte node allocation");
                    else
                    {
                        Check(Old::Load<std::uintptr_t>(nodes + 0x10) == setup.Sequence(0) && Old::Load<std::uintptr_t>(nodes + 40 + 0x10) == setup.Sequence(1),
                              "dynamic40 mixed bone/name node results remain distinct from static QWORD8 maps");
                        freeNodes(token, receiver);
                        select(token, receiver, hierarchy);
                        facade.Init(rawClump, rawHierarchy);
                        initialized();
                        Check(context.initializers == 1 && Old::Load<std::uint16_t>(counter) == 17,
                              "ordinary Init follows real node-free ACK without constructor/default or native ref replay");
                    }
                    retire(token);
                });
            if (!scenario)
            {
                action(
                    [&]
                    {
                        make(token);
                        receiver = context.receiver;
                        select(token, receiver, hierarchy);
                        PrivateAnimationAssociationReceiver facade(access, token);
                        Check(facade.InitializeForCustomAnimation(rawClump, rawHierarchy) == reinterpret_cast<CAnimBlendAssociationSAInterface*>(receiver),
                              "actual custom source initializer returns the issued receiver after original Init");
                        initialized();
                        customOrder();
                        Check(Old::Load<std::uint64_t>(receiver + 0x50) == 0xa5a5a5a5a5a5a5a5ull &&
                                  Old::Load<std::uint64_t>(receiver + 0x58) == 0xa5a5a5a5a5a5a5a5ull && Old::Load<unsigned char>(receiver + 0x60) == 0xa5,
                              "custom source preserves undefined callbacks and native extra60 rather than calling wholesale constructor defaults");
                        retire(token);
                    });
            }
            std::puts(scenario ? "Production dynamic clump zero allocation fixture completed"
                               : "Production dynamic clump actual SDK mixed and source defaults fixture completed");
        }
        else if (scenario == 2)
        {
            action(
                [&]
                {
                    auto& staticHook = ClockStaticAssociationInitializationInvokeTest::selected;
                    staticHook = &setup;
                    ClockStaticAssociationInitializationInvokeTest::calls = ClockStaticAssociationInitializationInvokeTest::events = 0;
                    ClockStaticAssociationInitializationInvokeTest::uncertain = false;
                    NativeAnimationStaticAssociationReferenceToken row;
                    Check(runtime.access.ResolveAnimationStaticAssociationInterface(generation, association, row) == ClockResult::Ok,
                          "copy reference case uses an actually resolved static source");
                    ClockStaticAssociationInitializationInvokeTest::row = row;
                    Check(runtime.access.InitializeAnimationStaticAssociation(row, setup.Clump(), hierarchy) == ClockResult::Ok,
                          "prepare source node map through the real existing static initializer owner");
                    Old::originalSource = association;
                    Old::originalNodes = setup.Nodes();
                    PrivateAnimationAssociationReceiver facade(access, token);
                    Check(facade.Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(association)) ==
                                  reinterpret_cast<CAnimBlendAssociationSAInterface*>(receiver) &&
                              Old::Load<std::uint16_t>(counter) == 18,
                          "existing static-copy route genuinely acquires its own bank reference");
                    freeNodes(token, receiver);
                    select(token, receiver, hierarchy);
                    const auto constructs = context.constructors, inits = context.initializers;
                    refuses([&] { (void)facade.Constructor(rawClump, rawHierarchy); }, "clump constructor cannot orphan an existing Copy-owned bank ref");
                    refuses([&] { (void)facade.InitializeForCustomAnimation(rawClump, rawHierarchy); },
                            "custom clearing4000 cannot release an owned native ref");
                    refuses([&] { facade.Init(rawClump, reinterpret_cast<CAnimBlendHierarchySAInterface*>(fixture.image + H::BankRva)); },
                            "Copy-owned Init cannot retarget to a genuinely valid different bank block");
                    Check(context.constructors == constructs && context.initializers == inits && !context.defaults && !runtime.stopping.load() &&
                              Old::Load<std::uint16_t>(counter) == 18 && Old::Load<std::uint16_t>(otherCounter) == 23,
                          "unsupported reference retarget/default operations refuse before effects and preserve both counters");
                    facade.Init(rawClump, rawHierarchy);
                    initialized();
                    Check(Old::Load<std::uint16_t>(counter) == 18 && runtime.animationAssociationClump.copyOwnedReference,
                          "same-block Init preserves the actual existing Copy-owned reference provenance");
                    retire(token);
                    Check(Old::Load<std::uint16_t>(counter) == 17, "actual Copy-mode destructor balances exactly its previously acquired reference");
                    staticHook = nullptr;
                });
            std::puts("Production dynamic clump copy reference compatibility fixture completed");
        }
        else if (scenario == 3 || scenario == 7)
        {
            action(
                [&]
                {
                    makeHierarchy(privateH);
                    makeHierarchy(nextH);
                    select(token, receiver, privateH.locator);
                    PrivateAnimationAssociationReceiver facade(access, token);
                    facade.Constructor(rawClump, reinterpret_cast<CAnimBlendHierarchySAInterface*>(privateH.locator));
                    initialized();
                    Check(runtime.access.DestroyPrivateAnimationHierarchy(privateH) == ClockResult::OwnerConflict && !runtime.stopping.load(),
                          "real private hierarchy destruction defers while its actual dynamic receiver holds it");
                    freeNodes(token, receiver);
                    Check(runtime.access.DestroyPrivateAnimationHierarchy(privateH) == ClockResult::OwnerConflict && facade.GetLength() == 0.0f &&
                              reinterpret_cast<std::uintptr_t>(facade.GetAnimHierarchy()->GetInterface()) == privateH.locator && !runtime.stopping.load(),
                          "FreeNodes ACK retains the hierarchy dependency and its metadata-readable lifetime");
                    select(token, receiver, nextH.locator, scenario == 7 ? 1 : 0);
                    if (scenario == 7)
                    {
                        refuses([&] { facade.Init(rawClump, reinterpret_cast<CAnimBlendHierarchySAInterface*>(nextH.locator)); },
                                "entered uncertain hierarchy replacement propagates actual owner refusal");
                        Check(CheckCustomAnimationSequenceRecoveryOwner() == ClockResult::Ok &&
                                  runtime.privateHierarchies.Destroy(fixture.image, privateH, &CheckCustomAnimationSequenceRecoveryOwner) ==
                                      ClockResult::OwnerConflict &&
                                  runtime.privateHierarchies.Destroy(fixture.image, nextH, &CheckCustomAnimationSequenceRecoveryOwner) ==
                                      ClockResult::OwnerConflict,
                              "both old and new real hierarchy owners remain held after uncertain original replacement");
                    }
                    else
                    {
                        facade.Init(rawClump, reinterpret_cast<CAnimBlendHierarchySAInterface*>(nextH.locator));
                        initialized();
                        Check(runtime.animationAssociationClump.privateHierarchy && runtime.animationAssociationClump.dependencyHeld &&
                                  runtime.animationAssociationClump.oldDependencyReleased &&
                                  runtime.access.DestroyPrivateAnimationHierarchy(privateH) == ClockResult::Ok &&
                                  runtime.access.DestroyPrivateAnimationHierarchy(nextH) == ClockResult::OwnerConflict,
                              "verified H replacement holds new H before releasing only the old real dependency");
                        retire(token);
                        Check(runtime.access.DestroyPrivateAnimationHierarchy(nextH) == ClockResult::Ok,
                              "known receiver retirement releases its current reversible private H dependency");
                    }
                });
            if (scenario == 3)
                std::puts("Production dynamic clump reversible hierarchy lifetime fixture completed");
        }
        else if (scenario >= 4 && scenario <= 6)
        {
            action(
                [&]
                {
                    makeHierarchy(privateH);
                    select(token, receiver, privateH.locator, scenario == 4 ? 1 : scenario == 5 ? 2 : 3);
                    PrivateAnimationAssociationReceiver facade(access, token);
                    refuses([&] { (void)facade.InitializeForCustomAnimation(rawClump, reinterpret_cast<CAnimBlendHierarchySAInterface*>(privateH.locator)); },
                            "actual custom initialization reports original/default-prefix/post-owner uncertainty");
                    Old::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
                    if (scenario == 5)
                        Check(context.defaults == 3 && !context.initializers && Old::Load<std::uintptr_t>(receiver + 0x20) == 0 &&
                                  runtime.animationAssociationClump.partialAccepted,
                              "lost default-store ACK retains the exact accepted prefix without entering Init or rolling back");
                    else
                        Check(context.initializers == 1 && runtime.animationAssociationClump.originalEntered && runtime.animationAssociationClump.nodes,
                              "entered original uncertainty retains actual node observations before fallible owner bookends");
                    Check(CheckCustomAnimationSequenceRecoveryOwner() == ClockResult::Ok &&
                              runtime.privateHierarchies.Destroy(fixture.image, privateH, &CheckCustomAnimationSequenceRecoveryOwner) ==
                                  ClockResult::OwnerConflict,
                          "partial/uncertain receiver keeps its genuine private hierarchy dependency");
                });
        }
        else if (scenario == 8)
        {
            // Filled by the closed temporary bridge adapter once its exact
            // producer header lands; no public clump pointer creates a borrow.
            RunTemporary(fixture, setup, token, receiver, hierarchy, generation, context, action);
        }
        if ((scenario >= 4 && scenario <= 7))
        {
            const auto  originals = context.constructors + context.initializers, defaults = context.defaults, frees = heap.freeCalls;
            const auto& receipt = runtime.animationAssociationClump;
            Check(receipt.hadEffects && !receipt.verified && receipt.retained && runtime.stopping.load() && runtime.privateAssociations.RecoveryPending(),
                  "actual entered/default-prefix uncertainty retains the receiver, allocation/dependency journals and module owner");
            Check(!service.End(), "uncertain dynamic clump state cannot certify End cleanup");
            fixture.invokeTimer();
            Check(!service.End() && context.constructors + context.initializers == originals && context.defaults == defaults && heap.freeCalls == frees &&
                      runtime.privateHierarchies.Owns(privateH) && (!nextH.locator || runtime.privateHierarchies.Owns(nextH)),
                  "actual End never replays native/default operations or guesses frees of observed uncertain nodes and H dependencies");
            std::puts("Production dynamic clump entered/default uncertainty retained fixture completed");
        }
        else
        {
            Check(!runtime.privateAssociations.IsRetained() && !runtime.privateAssociations.RecoveryPending() && Old::Load<std::uint16_t>(counter) == 17 &&
                      Old::Load<std::uint16_t>(otherCounter) == 23,
                  "all acknowledged no-ref paths and real Copy lifetime preserve exact source reference effects");
            callback.action = [](NativeGameTimingAccess&) {};
            WorldGroundOwnerFixture::Retire(service, fixture);
            Check(!runtime.pedReferences.IsRetained(), "actual registered-ped and private dynamic resources retire before hook removal");
        }
        Dynamic::current = nullptr;
        Node::active = false;
        Node::ReleaseGuards();
        NativeCustomAnimSequenceStorageFixture::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        ClockPedReferenceOwnerTest::image = previousReferences;
        writes.Restore();
    }
}
