/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual initializer admission, membership and entered-call receipts
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalInitializerClothesFixtureDefinitionsMain
#include "NativePedClothesCallsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeStaticAssociationInitializeFixture.h"
#include "NativeStaticAssociationInitializeTestHost.h"
#include "NativeAnimationBlockFactoryFixture.h"
#include "../../Client/game_de/NativeAnimationBlockProfile.h"
#include "../../Client/game_de/NativeAnimationBlockHierarchyProfile.h"
#include "../../Client/game_de/NativeStreamingCompletionContext.h"
namespace
{
    namespace Init = mta::game_de::NativeStaticAssociationInitializeCalls;
    namespace F = NativeStaticAssociationInitializeFixture;
    namespace Block = mta::game_de::NativeAnimationBlockProfile;
    namespace Hierarchy = mta::game_de::NativeAnimationBlockHierarchyProfile;
    struct InitializerFixture : ClothesFixture
    {
        F::State native;
        alignas(std::uintptr_t) std::array<unsigned char, 32> row{};
        Init::Arguments arguments;
        unsigned        calls = 0, beforeChecks = 0, afterChecks = 0, scenario = 0, eventEffects = 0;
        ClockResult     nested = ClockResult::Ok;
        bool            bankAdmitted = true;
        // Keep the explicitly aligned fixture free of implicit tail padding
        // under MSVC /W4; these bytes are never native state or admission data.
        std::array<unsigned char, 7> tail{};
        explicit InitializerFixture(const std::filesystem::path& path) : ClothesFixture(path) { Reset(); }
        std::uintptr_t HierarchyTag() const noexcept { return transform.image.image + Hierarchy::BankRva + 6 * Hierarchy::RowBytes; }
        void           Reset()
        {
            ClothesFixture::Reset();
            calls = beforeChecks = afterChecks = scenario = eventEffects = 0;
            nested = ClockResult::Ok;
            bankAdmitted = true;
            const auto image = transform.image.image;
            for (const auto& pin : Block::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto group = image + Block::GroupRva;
            Global(Block::CountRva, std::int32_t{3});
            transform.image.Patch(Block::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
            const auto selector = static_cast<unsigned char>(Block::LoadedSelector);
            const auto branch = static_cast<std::uint32_t>(Block::LoadedBranchRva);
            transform.image.Patch(Block::RemapRva + Block::LoadedOpcodeIndex, &selector, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(Block::DispatchRva + Block::LoadedSelector * 4, &branch, 4, PAGE_EXECUTE_READ);
            NativeAnimationBlockFactoryFixture::Install(image, &SpecialFixture::Patch, this);
            Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{12});
            std::array<unsigned char, Block::RowBytes> block{};
            const char                                 name[] = "PED";
            std::memcpy(block.data(), name, sizeof(name));
            F::Store(block.data(), Block::LoadedOffset, std::uint8_t{1});
            F::Store(block.data(), Block::IdOffset, std::int32_t{5});
            F::Store(block.data(), Block::AnimationCountOffset, std::uint32_t{3});
            transform.image.Patch(Block::BankRva + Block::RowBytes, block.data(), block.size(), PAGE_READWRITE);
            native.Reset(image, Address(transform.pool.objects), Address(row.data()), HierarchyTag(), &SpecialFixture::Patch, this);
            native.EnableSkeletonInputs();
            Check(native.AttachRegistry(items, 17, graph.Class(0), &SpecialFixture::Patch, this),
                  "extend the one existing world/model/provider registry with real-shaped actor/class/component/skeleton members");
            const auto type = image + F::ModelTypeRva;
            transform.image.Patch(0x4301000 + NativeStreamingRequestProfile::TypeMethodSlot, &type, sizeof(type), PAGE_READONLY);
            const auto model = Address(metadata);
            Global(NativeModelBankProfile::BankRva, model);
            const std::array<unsigned char, 2> loaded{0x40, 1};
            transform.image.Patch(NativeModelLoadedProfile::ArrayRva + NativeModelLoadedProfile::FlagsOffset, loaded.data(), loaded.size(), PAGE_READWRITE);
            arguments.ped = Ped();
            arguments.clump = native.Clump();
            arguments.association = Address(row.data());
            arguments.hierarchy = HierarchyTag();
            PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        }
        static ClockResult Recheck(const Init::Arguments& input, bool after, void* context) noexcept
        {
            auto& self = *static_cast<InitializerFixture*>(context);
            if (after)
                ++self.afterChecks;
            else
                ++self.beforeChecks;
            if (!self.bankAdmitted)
                return ClockResult::OwnerConflict;
            if (input.association != self.arguments.association || input.hierarchy != self.arguments.hierarchy ||
                F::Load<std::uintptr_t>(self.row.data(), 0x18) != input.hierarchy)
                return ClockResult::BindingChanged;
            return PoolTestHost::Owner();
        }
        ClockResult Run(Init::Receipt& receipt) { return Init::Initialize(transform.image.image, arguments, &PoolTestHost::Owner, &Recheck, this, &receipt); }
    };
    InitializerFixture* current = nullptr;
}
namespace NativeStaticAssociationInitializeTestHost
{
    HMODULE WINAPI Module(LPCWSTR name)
    {
        return PoolTestHost::Module(name);
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        return PoolTestHost::Read(process, input, output, bytes, copied);
    }
    mta::game_de::ClockResult Invoke(Init::NativeFunction function, std::uintptr_t row, std::uintptr_t clump, std::uintptr_t hierarchy,
                                     std::uint32_t& exception) noexcept
    {
        auto& f = *current;
        ++f.calls;
        Check(reinterpret_cast<std::uintptr_t>(function) == f.transform.image.image + F::P::EntryRva && row == f.arguments.association &&
                  clump == f.native.Clump() && hierarchy == f.arguments.hierarchy,
              "one whole original entry receives exact current row/clump/hierarchy without a name-provider precall");
        // This marker represents opaque original event-selected work. Neither
        // fixture nor production reimplements the stock name/bone matcher.
        ++f.eventEffects;
        f.native.AcceptInjectedResult();
        if (f.scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (f.scenario == 2)
        {
            Init::Receipt nested;
            f.nested = f.Run(nested);
            Check(!nested.entered, "reentry shares the existing native gate and cannot enter another original initializer");
        }
        if (f.scenario == 3)
            F::Store(f.native.RenderItem(), 8, std::uint32_t{0x30000000});
        if (f.scenario == 4)
            F::Store(f.native.RenderItem(), 0x10, std::uint32_t{9999});
        if (f.scenario == 5)
            F::Store(f.native.RenderItem(), 0, f.native.Container());
        if (f.scenario == 6)
            f.native.nodes[1] = f.native.Sequence(1) + 8;
        if (f.scenario == 7)
            f.bankAdmitted = false;
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "initializer evidence uses explicit workspace output");
    InitializerFixture fixture{std::filesystem::path(argv[1])};
    current = &fixture;
    const auto    reset = [&] { fixture.Reset(); };
    Init::Receipt receipt;
    reset();
    Check(fixture.Run(receipt) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.verified && receipt.rowObserved &&
              receipt.observedCount == 2 && receipt.observedNodes == fixture.native.Nodes() && fixture.calls == 1 && fixture.eventEffects == 1 &&
              fixture.beforeChecks == 2 && fixture.afterChecks == 1,
          "mixed bone/name layouts and nonzero undefined prior count pass one original-call boundary with exact output targets");
    reset();
    fixture.scenario = 2;
    Check(fixture.Run(receipt) == ClockResult::Ok && fixture.nested == ClockResult::Busy && fixture.calls == 1,
          "native reentry is refused without replaying the accepted outer entry");
    for (unsigned scenario = 1; scenario <= 7; ++scenario)
    {
        if (scenario == 2)
            continue;
        reset();
        fixture.scenario = scenario;
        Check(fixture.Run(receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.rowObserved &&
                  receipt.observedNodes == fixture.native.Nodes() && fixture.calls == 1 && fixture.eventEffects == 1,
              "entered exception, pending-kill/serial/slot reuse, invalid target or lost bank leaves exact observed prefix and no retry");
    }
    for (unsigned fault = 0; fault != 12; ++fault)
    {
        reset();
        if (fault == 0)
            fixture.arguments.clump += 8;
        if (fault == 1)
        {
            const auto wrong = fixture.transform.image.image + F::P::NameProviderRva + 1;
            fixture.transform.image.Patch(F::RenderVtableRva + 0x698, &wrong, sizeof(wrong), PAGE_READONLY);
        }
        if (fault == 2)
            fixture.bankAdmitted = false;
        if (fault == 3)
            F::Store(fixture.native.RenderItem(), 8, std::uint32_t{0x30000000});
        if (fault == 4)
            F::Store(fixture.native.RenderItem(), 0, fixture.native.Container());
        if (fault == 5)
            F::Store(fixture.native.RenderClassItem(), 8, std::uint32_t{0x30000000});
        if (fault == 6)
            F::Store(fixture.native.ComponentItem(), 8, std::uint32_t{0x30000000});
        if (fault == 7)
            F::Store(fixture.native.SkeletonItem(), 8, std::uint32_t{0x30000000});
        if (fault == 8)
            F::Store(fixture.row.data(), 0x10, fixture.native.Nodes());
        if (fault == 9)
            F::Store(reinterpret_cast<void*>(fixture.HierarchyTag()), 0x10, std::uint16_t{3});
        if (fault == 10)
            fixture.native.mapRow[2] = 0;
        if (fault == 11)
            F::Store(fixture.row.data(), 0x18, std::uintptr_t{0});
        Check(fixture.Run(receipt) != ClockResult::Ok && !receipt.entered && !fixture.calls && !fixture.eventEffects,
              "stale clump/provider/bank and pending-kill/reused UObject dependencies refuse before original entry");
    }
    std::puts("Production native static association initializer fixture completed");
    std::printf("Passed %u native static-association initializer checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
    return 0;
}
