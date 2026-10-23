/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only current manager observations over the real owned image scaffold
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeAnimationManagerHistoricalBlockMain
#include "NativeAnimationBlockTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationManagerMemory.h"
#include "../../Client/game_de/NativeAnimationManagerProfile.h"
#include "../../Client/game_de/NativeAnimationManagerMemory.cpp"
namespace
{
    namespace M = NativeAnimationManagerProfile;
    using ManagerMemory = NativeAnimationManagerMemory;
    NativeAnimationBlockName Name(const char* text)
    {
        NativeAnimationBlockName name{};
        for (std::size_t i = 0; i + 1 < name.size() && text[i]; ++i)
            name[i] = static_cast<unsigned char>(text[i]);
        return name;
    }
    struct ManagerFixture
    {
        BlockFixture block;
        explicit ManagerFixture(const std::filesystem::path& path) : block(path)
        {
            for (const auto& pin : M::Code)
                block.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            block.bank.transform.image.Patch(M::NameFoldRva, M::NameFold.data(), M::NameFold.size(), PAGE_READONLY);
            Reset();
        }
        void Seed(std::uint32_t index, const NativeAnimationBlockName& name)
        {
            block.bank.transform.image.Patch(A::BankRva + static_cast<std::uintptr_t>(index) * A::RowBytes, name.data(), name.size(), PAGE_READWRITE);
        }
        void Reset()
        {
            block.Reset();
            block.Cell(M::AnimationCountRva, std::int32_t{73});
            Seed(0, Name("ped"));
            Seed(1, Name("PED"));
            Seed(2, Name("run"));
            PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        }
    };
    ManagerFixture*           changingManager = nullptr;
    NativeAnimationBlockName* changingQuery = nullptr;
    void                      ChangeManagerCount()
    {
        changingManager->block.Cell(A::CountRva, std::int32_t{2});
    }
    void ChangeAnimationCount()
    {
        changingManager->block.Cell(M::AnimationCountRva, std::int32_t{74});
    }
    void ChangeFirstName()
    {
        changingManager->Seed(0, Name("new"));
    }
    void ChangeSelectedName()
    {
        changingManager->Seed(2, Name("new"));
    }
    void ChangeQueryName()
    {
        (*changingQuery)[0] = 'X';
    }
    bool CountsUnchanged(const NativeAnimationManagerCounts& counts)
    {
        return counts.blocks == -123 && counts.animations == -456;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "manager evidence uses explicit workspace output");
    ManagerMemory::Binding unavailableBinding{123};
    Check(!ManagerMemory::Qualify(unavailableBinding) && unavailableBinding.image == 123, "wrong executable cannot qualify a manager binding");
    ManagerFixture managerFixture(argv[1]);
    changingManager = &managerFixture;
    PoolTestHost::image = managerFixture.block.bank.transform.image.image;
    const ManagerMemory::Binding managerBinding{PoolTestHost::image};
    ManagerMemory                manager(managerBinding, &PoolTestHost::Owner), unownedManager(managerBinding, nullptr);
    NativeAnimationManagerCounts counts{-123, -456};
    std::int32_t                 selected = -123;
    auto                         query = Name("RUN");
    Check(ManagerMemory::Revalidate(managerBinding), "actual prior block metadata plus new index/extent/fold pins qualify native manager observations");
    Check(unownedManager.ReadCounts(counts) == ClockResult::NotInitialized && CountsUnchanged(counts) &&
              unownedManager.FindBlockIndex(query, selected) == ClockResult::NotInitialized && selected == -123,
          "native globals do not create callback authority");
    for (const auto blockCount : {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, std::int32_t{181}, (std::numeric_limits<std::int32_t>::min)(),
                                  (std::numeric_limits<std::int32_t>::max)()})
        for (const auto animationCount :
             {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
        {
            managerFixture.Reset();
            managerFixture.block.Cell(A::CountRva, blockCount);
            managerFixture.block.Cell(M::AnimationCountRva, animationCount);
            Check(manager.ReadCounts(counts) == ClockResult::Ok && counts.blocks == blockCount && counts.animations == animationCount,
                  "both actual signed32 counters preserve storage; lookup worklimit is not counter normalization");
        }
    managerFixture.Reset();
    for (const auto lookup : {Name("ped"), Name("PeD"), Name("PED")})
    {
        selected = -123;
        Check(manager.FindBlockIndex(lookup, selected) == ClockResult::Ok && selected == 0,
              "exact native ASCII folding returns the FIRST case-insensitive match, not a later duplicate");
    }
    Check(manager.FindBlockIndex(query, selected) == ClockResult::Ok && selected == 2, "last matching row is included");
    Check(manager.FindBlockIndex(Name("missing"), selected) == ClockResult::Ok && selected == -1, "genuine missing name is Ok/-1");
    managerFixture.Seed(1, Name(""));
    Check(manager.FindBlockIndex(Name(""), selected) == ClockResult::Ok && selected == 1,
          "empty query follows exact native NUL match, not an invented invalid-name rule");
    auto tailed = Name("PeD");
    tailed[4] = 0xff;
    Check(manager.FindBlockIndex(tailed, selected) == ClockResult::Ok && selected == 0, "native comparison ignores copied query bytes after first NUL");
    for (unsigned byte = 1; byte != 256; ++byte)
    {
        managerFixture.Reset();
        managerFixture.block.Cell(A::CountRva, std::int32_t{1});
        NativeAnimationBlockName row{}, lookup{};
        row[0] = static_cast<unsigned char>(byte);
        lookup[0] = row[0];
        managerFixture.Seed(0, row);
        Check(manager.FindBlockIndex(lookup, selected) == ClockResult::Ok && selected == 0,
              "every nonzero native byte, including high bytes, matches itself exactly");
        lookup[0] = static_cast<unsigned char>(byte <= 127 ? M::NameFold[byte] : byte ^ 0x80u);
        const auto expected = byte <= 127 ? 0 : -1;
        Check(manager.FindBlockIndex(lookup, selected) == ClockResult::Ok && selected == expected,
              "only the exact pinned ASCII table folds; high bytes never use a locale or table extension");
    }
    managerFixture.Reset();
    managerFixture.block.Cell(A::CountRva, static_cast<std::int32_t>(A::ObservationLimit));
    for (std::uint32_t i = 0; i < A::ObservationLimit; ++i)
        managerFixture.Seed(i, Name("other"));
    managerFixture.Seed(A::ObservationLimit - 1, Name("last"));
    Check(manager.FindBlockIndex(Name("LAST"), selected) == ClockResult::Ok && selected == static_cast<std::int32_t>(A::ObservationLimit - 1),
          "last admitted observation row is usable without inferring allocation capacity");
    for (const auto count : {std::int32_t{0}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)()})
    {
        managerFixture.Reset();
        managerFixture.block.Cell(A::CountRva, count);
        selected = -123;
        Check(manager.FindBlockIndex(query, selected) == ClockResult::Ok && selected == -1,
              "genuine signed nonpositive native count selects null/-1, independent from counter getter facts");
    }
    for (const auto count : {std::int32_t{181}, (std::numeric_limits<std::int32_t>::max)()})
    {
        managerFixture.Reset();
        managerFixture.block.Cell(A::CountRva, count);
        selected = -123;
        Check(manager.FindBlockIndex(query, selected) == ClockResult::BindingChanged && selected == -123,
              "exceeding conservative positive lookup work refuses rather than fabricating absence");
    }
    managerFixture.Reset();
    auto badQuery = query;
    badQuery.fill('x');
    selected = -123;
    Check(manager.FindBlockIndex(badQuery, selected) == ClockResult::InvalidArgument && selected == -123 && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
          "unterminated copied name refuses before native effects or observations");
    for (const bool lookup : {false, true})
    {
        managerFixture.Reset();
        counts = {-123, -456};
        selected = -123;
        Check((lookup ? manager.FindBlockIndex(query, selected) : manager.ReadCounts(counts)) == ClockResult::Ok,
              "measure only current manager observation boundaries");
        const auto managerReads = PoolTestHost::reads, managerOwners = PoolTestHost::ownerCalls;
        for (unsigned fail = 1; fail <= managerReads; ++fail)
        {
            managerFixture.Reset();
            counts = {-123, -456};
            selected = -123;
            PoolTestHost::failAt = fail;
            Check((lookup ? manager.FindBlockIndex(query, selected) : manager.ReadCounts(counts)) != ClockResult::Ok &&
                      (lookup ? selected == -123 : CountsUnchanged(counts)),
                  "every actual failed read preserves output and never becomes valid zero/absence");
        }
        for (unsigned revoke = 1; revoke <= managerOwners; ++revoke)
        {
            managerFixture.Reset();
            counts = {-123, -456};
            selected = -123;
            PoolTestHost::revokeAt = revoke;
            Check((lookup ? manager.FindBlockIndex(query, selected) : manager.ReadCounts(counts)) == ClockResult::WrongThread &&
                      (lookup ? selected == -123 : CountsUnchanged(counts)),
                  "each real owner boundary can revoke without publishing a partial observation");
        }
    }
    for (const auto action : {&ChangeManagerCount, &ChangeAnimationCount})
    {
        managerFixture.Reset();
        counts = {-123, -456};
        PoolTestHost::watched = PoolTestHost::image + M::AnimationCountRva;
        PoolTestHost::after = action;
        Check(manager.ReadCounts(counts) == ClockResult::BindingChanged && CountsUnchanged(counts),
              "changed raw count between snapshots refuses the complete pair");
    }
    for (const auto action : {&ChangeManagerCount, &ChangeFirstName, &ChangeSelectedName, &ChangeQueryName})
    {
        managerFixture.Reset();
        query = Name("run");
        changingQuery = &query;
        selected = -123;
        PoolTestHost::watched = managerFixture.block.Row(2);
        PoolTestHost::after = action;
        Check(manager.FindBlockIndex(query, selected) == ClockResult::BindingChanged && selected == -123,
              "count, earlier candidate, selected name or caller query mutation cannot publish mixed first-match identity");
    }
    query = Name("run");
    for (const auto state : {std::uint32_t{6}, std::uint32_t{7}, std::uint32_t{8}})
    {
        managerFixture.Reset();
        managerFixture.block.Cell(NativeGameStateProfile::stateRva, state);
        selected = -123;
        counts = {-123, -456};
        Check(manager.ReadCounts(counts) == ClockResult::NotInitialized && CountsUnchanged(counts) &&
                  manager.FindBlockIndex(query, selected) == ClockResult::NotInitialized && selected == -123,
              "frontend/loading state cannot borrow native manager observations");
    }
    managerFixture.Reset();
    managerFixture.block.Cell(NativeBorrowedGcProfile::markerRva, LONG{1});
    selected = -123;
    Check(manager.FindBlockIndex(query, selected) != ClockResult::Ok && selected == -123, "actual GC refuses native row lookup");
    managerFixture.Reset();
    managerFixture.block.Cell(NativeStartGameProfile::ThreadInitializedRva, std::uint8_t{0});
    counts = {-123, -456};
    Check(manager.ReadCounts(counts) == ClockResult::NotInitialized && CountsUnchanged(counts), "uninitialized actual game thread refuses native counts");
    managerFixture.Reset();
    selected = -123;
    ClockResult managerForeign = ClockResult::Ok;
    std::thread managerThread([&] { managerForeign = manager.FindBlockIndex(query, selected); });
    managerThread.join();
    Check(managerForeign == ClockResult::WrongThread && selected == -123, "callback alone cannot replace actual native game-thread identity");
    for (const auto& pin : M::Code)
    {
        managerFixture.Reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        managerFixture.block.bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        counts = {-123, -456};
        Check(manager.ReadCounts(counts) != ClockResult::Ok && CountsUnchanged(counts), "each new genuine consumer pin is required");
        managerFixture.block.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    managerFixture.Reset();
    auto changedFold = M::NameFold;
    changedFold['A'] = 'A';
    managerFixture.block.bank.transform.image.Patch(M::NameFoldRva, changedFold.data(), changedFold.size(), PAGE_READONLY);
    selected = -123;
    Check(manager.FindBlockIndex(query, selected) != ClockResult::Ok && selected == -123,
          "changed native ASCII table refuses instead of drifting to host locale behavior");
    managerFixture.block.bank.transform.image.Patch(M::NameFoldRva, M::NameFold.data(), M::NameFold.size(), PAGE_READONLY);
    managerFixture.Reset();
    DWORD managerOld = 0, managerIgnored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(managerFixture.block.Row(2)), 16, PAGE_READONLY, &managerOld) != FALSE, "restrict owned native name row");
    selected = -123;
    Check(manager.FindBlockIndex(query, selected) != ClockResult::Ok && selected == -123, "read-only row cannot replace current writable native bank");
    Check(VirtualProtect(reinterpret_cast<void*>(managerFixture.block.Row(2)), 16, managerOld, &managerIgnored) != FALSE, "restore owned row page");
    managerFixture.Reset();
    PoolTestHost::image = 0;
    counts = {-123, -456};
    selected = -123;
    Check(manager.ReadCounts(counts) != ClockResult::Ok && CountsUnchanged(counts) && manager.FindBlockIndex(query, selected) != ClockResult::Ok &&
              selected == -123,
          "current main-image loss never fabricates manager facts");
    std::puts("Production native animation manager observation primitive fixture completed");
    std::printf("Passed %u native animation manager observation checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
