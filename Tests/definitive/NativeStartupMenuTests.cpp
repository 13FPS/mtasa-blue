/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual native close admission and one-shot journal on owned pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main StartupMenuHistoricalSpecialDefinitionsMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeStartupMenuGraphFixture.h"
#include "../../Client/game_de/NativeStartupMenuCalls.h"
#include <type_traits>
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeStartupMenuCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace StartupMenuTests
{
    namespace Calls = mta::game_de::NativeStartupMenuCalls;
    namespace P = mta::game_de::NativeStartupMenuProfile;
    constexpr std::uint64_t          Generation = 141;
    SpecialFixture*                  fixture = nullptr;
    NativeStartupMenuFixture::Graph* menu = nullptr;
    Calls::Record*                   active = nullptr;
    unsigned                         mode = 0, calls = 0, mutation = 0;
    bool                             reentry = false;
    void                             Mutate()
    {
        switch (mutation)
        {
            case 0:
                menu->Root(0x348, std::uintptr_t{0});
                break;
            case 1:
                menu->Root(0x128, std::uintptr_t{0});
                break;
            case 2:
                menu->RetireWidget();
                break;
            case 3:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 4:
                fixture->Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
                break;
            case 5:
                PoolTestHost::failAt = PoolTestHost::reads + 1;
                break;
        }
    }
    ClockResult Invoke(Calls::NativeFunction function, std::uintptr_t root, bool& result, std::uint32_t& exception) noexcept
    {
        ++calls;
        Check(function == reinterpret_cast<Calls::NativeFunction>(fixture->transform.image.image + P::EntryRva) && root == menu->world.root,
              "actual controller calls the exact concrete bool(this) close entry with its current native root");
        if (reentry)
        {
            Calls::Receipt previous;
            previous.generation = 777;
            Check(Calls::Close(fixture->transform.image.image, *active, Generation, &PoolTestHost::Owner, &previous, &Invoke) == ClockResult::OwnerConflict &&
                      previous.generation == 777 && Calls::CheckReady(fixture->transform.image.image, &PoolTestHost::Owner) == ClockResult::Busy,
                  "same-record reentry preserves output while the shared native scope refuses nested readiness");
            Calls::Record nested;
            Check(Calls::Close(fixture->transform.image.image, nested, Generation, &PoolTestHost::Owner, nullptr, &Invoke) == ClockResult::Busy &&
                      nested.CanDiscard() && !nested.HasDebt(),
                  "a second record cannot borrow the active synchronous native scope");
        }
        if (mode == 1)
        {
            exception = 0xe0570141;
            return ClockResult::OriginalException;
        }
        menu->Effect(result);
        switch (mode)
        {
            case 2:
                exception = 0xe0570142;
                return ClockResult::OriginalException;
            case 3:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 4:
                fixture->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 5:
                fixture->Global(NativeStreamingLoadAllProfile::RootRva, std::uintptr_t{0});
                break;
            case 6:
                fixture->Global(P::FlagRva, static_cast<unsigned char>(1));
                break;
            case 7:
                menu->Root(0x348, menu->Widget());
                break;
            case 8:
                menu->Root(0x350, std::uint32_t{1});
                break;
            case 9:
                result = !result;
                break;
            case 10:
                fixture->Global(NativeFrontendMenuProfile::menuHeaderRva + 8, std::int32_t{1});
                break;
            case 11:
                menu->RetireWidget();
                break;
            case 12:
                fixture->Put(menu->world.world.data(), NativeStreamingLoadAllProfile::WorldModeOffset, static_cast<unsigned char>(2));
                break;
        }
        return ClockResult::Ok;
    }
}

int main(int argc, char** argv)
{
    namespace T = StartupMenuTests;
    namespace Calls = T::Calls;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "menu-close evidence stays in an explicit owned workspace directory");
    static_assert(!std::is_copy_constructible_v<Calls::Record> && !std::is_move_constructible_v<Calls::Record>);
    SpecialFixture                  fixture(argv[1]);
    NativeStartupMenuFixture::Graph menu(fixture.graph);
    T::fixture = &fixture;
    T::menu = &menu;
    const auto     image = fixture.transform.image.image;
    Calls::Receipt receipt;
    const auto     reset = [&]
    {
        fixture.Reset();
        menu.InitializePins();
        menu.Reset();
        PoolTestHost::image = image;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        T::mode = T::calls = 0;
        T::active = nullptr;
        T::reentry = false;
    };
    const auto close = [&](Calls::Record& record, std::uint64_t generation = T::Generation)
    {
        T::active = &record;
        return Calls::Close(image, record, generation, &PoolTestHost::Owner, &receipt, &T::Invoke);
    };
    reset();
    Check(Calls::CheckReady(image, &PoolTestHost::Owner) == ClockResult::Ok && !T::calls && menu.Root<std::uintptr_t>(0x348) == menu.Widget(),
          "actual readiness qualifies current world/widget without a native close or fabricated flag store");
    Calls::Record invalid;
    Check(close(invalid, 0) == ClockResult::InvalidArgument && !receipt.entered && invalid.CanDiscard() && !T::calls,
          "zero generation refuses before native effects");
    Check(close(invalid) == ClockResult::OwnerConflict && !T::calls, "a completed pre-entry refusal record cannot replay");
    reset();
    Calls::Record unowned;
    Check(Calls::Close(image, unowned, T::Generation, nullptr, &receipt, &T::Invoke) == ClockResult::NotInitialized && !receipt.entered && !T::calls,
          "a menu journal cannot manufacture native callback authority");
    for (unsigned absent = 0; absent != 2; ++absent)
    {
        reset();
        menu.Reset(absent == 0);
        Calls::Record record;
        T::reentry = true;
        Check(close(record) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.verified && !receipt.debt &&
                  receipt.nativeResult == (absent == 0) && record.GetStage() == Calls::Stage::Complete && T::calls == 1,
              "actual one-shot close acknowledges both widget removal and the genuine false/no-widget native result");
        Check(!menu.Root<std::uintptr_t>(0x348) && receipt.flagAfter == 0 && receipt.auxiliaryAfter == (absent ? 79u : 0u),
              "postconditions retain the native null-widget auxiliary semantics and only accept the native flag/widget effects");
        Check(close(record) == ClockResult::OwnerConflict && T::calls == 1, "completed native menu records cannot close twice");
    }
    for (unsigned reason = 0; reason != 7; ++reason)
    {
        reset();
        switch (reason)
        {
            case 0:
                fixture.Global(NativeFrontendMenuProfile::menuHeaderRva + 8, std::int32_t{1});
                break;
            case 1:
                fixture.Global(NativeFrontendMenuProfile::menuHeaderRva + 16, menu.Widget());
                break;
            case 2:
                menu.RetireWidget();
                break;
            case 3:
                // The graph helper owns the explicit protection for this vtable fault.
                fixture.graph.Global(NativeStartupMenuFixture::Graph::WidgetVtableRva + 0x288, image + T::P::FlagRva, PAGE_READONLY);
                break;
            case 4:
                menu.Root(0x128, std::uintptr_t{1});
                break;
            case 5:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 6:
            {
                const auto bad = static_cast<unsigned char>(T::P::Function1297ef0.back() ^ 1);
                fixture.transform.image.Patch(T::P::EntryRva + T::P::Function1297ef0.size() - 1, &bad, 1, PAGE_EXECUTE_READ);
                break;
            }
        }
        Calls::Record record;
        Check(Calls::CheckReady(image, &PoolTestHost::Owner) != ClockResult::Ok && close(record) != ClockResult::Ok && !receipt.entered && !T::calls &&
                  !record.HasDebt(),
              "nonempty legacy state, unregistered widget, invalid immediate receiver/target, GC or complete-body drift refuses before entry");
    }
    for (T::mutation = 0; T::mutation != 6; ++T::mutation)
    {
        reset();
        PoolTestHost::watched = menu.world.root + 0x128;
        PoolTestHost::after = &T::Mutate;
        Calls::Record record;
        Check(close(record) != ClockResult::Ok && !receipt.entered && !T::calls && !record.HasDebt(),
              "widget/dependency/member/owner/world/read drift cannot pass the repeated synchronous admission fence");
    }
    for (unsigned scenario = 1; scenario != 11; ++scenario)
    {
        reset();
        T::mode = scenario;
        Calls::Record record;
        Check(close(record) != ClockResult::Ok && receipt.entered && receipt.debt && record.HasDebt() && !record.CanDiscard() && T::calls == 1,
              "exception or accepted close with lost postconditions retains the precise entered native effect as debt");
        const auto survivingWidget = menu.Root<std::uintptr_t>(0x348);
        PoolTestHost::ownerResult = ClockResult::Ok;
        Check(close(record) == ClockResult::OwnerConflict && T::calls == 1 && menu.Root<std::uintptr_t>(0x348) == survivingWidget,
              "current raw widget absence or presence cannot adopt uncertainty, replay native effects or restore prior fields");
    }
    for (const auto scenario : {11u, 12u})
    {
        reset();
        T::mode = scenario;
        Calls::Record record;
        Check(close(record) == ClockResult::Ok && receipt.verified && T::calls == 1,
              "post-close authority accepts native world-mode progress and never rereads a retired detached widget");
        record.MarkUnacknowledged();
        Check(record.HasDebt() && !record.CanDiscard(), "the runtime can retain an accepted close when its outer acknowledgement is lost");
    }
    std::puts("Production native startup menu close fixture completed");
    std::printf("Passed %u native startup menu close checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
