/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual named archive predicate on owned image pages and borrowed owner
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelArchiveHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelArchiveReader.h"
#include "../../Client/game_de/NativeModelArchiveProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeModelLoadedReader.cpp"
#include "../../Client/game_de/NativeModelArchiveReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace AP = NativeModelArchiveProfile;
    namespace AL = NativeModelLoadedProfile;
    using ArchiveReader = NativeModelArchiveReader;
    struct ArchiveFixture : BankFixture
    {
        explicit ArchiveFixture(const std::filesystem::path& directory) : BankFixture(directory)
        {
            for (const auto& pin : AL::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : AP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        void Store(std::uint32_t model, unsigned char flags, unsigned char state)
        {
            Global(AL::ArrayRva + static_cast<std::uintptr_t>(model) * AL::RowStride + AL::FlagsOffset, std::array<unsigned char, 2>{flags, state});
        }
        void Reset()
        {
            BankFixture::Reset();
            Store(0, 0, 0);
            Store(17, 0, 0);
            Store(AP::ModelCount - 1, 0, 0);
            const auto group = transform.image.image + AP::GroupRva;
            transform.image.Patch(AP::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
            const auto branch = static_cast<std::uint32_t>(AP::BranchRva);
            transform.image.Patch(AP::BranchTableRva + AP::BranchIndex * 4, &branch, sizeof(branch), PAGE_EXECUTE_READ);
        }
    };
    ArchiveReader::Value ArchiveSentinel()
    {
        return {0xdeadbeefu, 0x123, 0x456, 0x789, 0xab, true, true};
    }
    bool ArchiveUntouched(const ArchiveReader::Value& value)
    {
        return ModelArchiveDetail::Same(value, ArchiveSentinel());
    }
    ArchiveFixture* changingArchive = nullptr;
    unsigned        archiveChange = 0;
    void            ChangeArchive()
    {
        if (archiveChange == 0)
            changingArchive->Slot(17, 0x5679);
        if (archiveChange == 1)
            changingArchive->Store(17, 0x80, 0);
        if (archiveChange == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "archive evidence lives in explicit owned workspace directory");
    ArchiveReader::Binding refused{123};
    Check(!ArchiveReader::Qualify(refused) && refused.image == 123, "shipping archive profile refuses real non-game host without output");
    ArchiveFixture fixture(argv[1]);
    changingArchive = &fixture;
    const ArchiveReader::Binding binding{fixture.transform.image.image};
    PoolTestHost::image = binding.image;
    ArchiveReader reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    auto          value = ArchiveSentinel();
    Check(ArchiveReader::Revalidate(binding),
          "named opcode dispatch, bank/flag branch, false compare, missing-source producers and current raw model domain qualify");
    Check(unowned.Read(17, value) == ClockResult::NotInitialized && ArchiveUntouched(value), "archive bytes do not create callback authority");
    for (unsigned flags = 0; flags != 256; ++flags)
    {
        fixture.Reset();
        fixture.Store(17, static_cast<unsigned char>(flags), 0);
        value = ArchiveSentinel();
        Check(reader.Read(17, value) == ClockResult::Ok && value.modelIndex == 17 && value.slotAddress == binding.image + B::BankRva + 17 * B::SlotBytes &&
                  value.nativeModelInfo == 0x1231 && value.flagsObserved && value.rawFlags == flags &&
                  value.flagsAddress == binding.image + AL::ArrayRva + 17 * AL::RowStride + AL::FlagsOffset && value.allocatedInArchive == (flags < 128),
              "all raw flag bytes use exact native signbit80 rather than requested/priority/loaded bits or any nonzero state");
    }
    for (const auto state : {static_cast<unsigned char>(0), static_cast<unsigned char>(1), static_cast<unsigned char>(2), static_cast<unsigned char>(3),
                             static_cast<unsigned char>(4), static_cast<unsigned char>(255)})
        for (const auto flags : {static_cast<unsigned char>(0), static_cast<unsigned char>(0x80)})
        {
            fixture.Reset();
            fixture.Store(17, flags, state);
            Check(reader.Read(17, value) == ClockResult::Ok && value.allocatedInArchive == (flags == 0),
                  "raw loaded state never substitutes archive membership");
        }
    for (const auto model :
         {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{999}, std::uint32_t{1000}, std::uint32_t{1193}, std::uint32_t{1194}, AP::ModelCount - 1})
        for (const auto word : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0x1231}, (std::numeric_limits<std::uintptr_t>::max)()})
        {
            fixture.Reset();
            fixture.Slot(model, word);
            fixture.Store(model, 0, 1);
            Check(reader.Read(model, value) == ClockResult::Ok && value.nativeModelInfo == word && value.allocatedInArchive == (word != 0) &&
                      value.flagsObserved == (word != 0),
                  "full ordinary keys including upgrades use genuine query without dereferencing null/unaligned/unreadable opaque metadata words");
            Check(reader.Recheck(value) == ClockResult::Ok, "same immediate native bank/flag observation rechecks inside owner");
        }
    for (const auto model : {AP::ModelCount, std::uint32_t{26312}, std::uint32_t{32768}, std::uint32_t{65535}, std::uint32_t{65536}, std::uint32_t{0xffff0000},
                             std::uint32_t{0xffffffff}})
    {
        fixture.Reset();
        value = ArchiveSentinel();
        Check(reader.Read(model, value) == ClockResult::InvalidArgument && ArchiveUntouched(value) && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
              "resource/sentinel/highDWORD/negative script aliases refuse before data or owner reads");
    }
    fixture.Reset();
    fixture.Slot(17, 0);
    const auto flagsAddress = binding.image + AL::ArrayRva + 17 * AL::RowStride + AL::FlagsOffset;
    DWORD      prior = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(flagsAddress), 2, PAGE_NOACCESS, &prior) != FALSE, "revoke only owned flags page");
    Check(reader.Read(17, value) == ClockResult::Ok && !value.allocatedInArchive && !value.flagsObserved && !value.flagsAddress,
          "null native bank short-circuits flags read exactly as named script branch does, even when owned flags page is inaccessible");
    Check(VirtualProtect(reinterpret_cast<void*>(flagsAddress), 2, prior, &ignored) != FALSE, "restore only owned flags page");
    for (archiveChange = 0; archiveChange != 3; ++archiveChange)
    {
        fixture.Reset();
        value = ArchiveSentinel();
        PoolTestHost::watched = flagsAddress;
        PoolTestHost::after = &ChangeArchive;
        Check(reader.Read(17, value) != ClockResult::Ok && ArchiveUntouched(value),
              "cross bank/flag mutation or owner loss during actual copies cannot publish a hybrid archive result");
    }
    fixture.Reset();
    Check(reader.Read(17, value) == ClockResult::Ok, "capture immediate allocation snapshot");
    fixture.Store(17, 0x80, 0);
    Check(reader.Recheck(value) == ClockResult::BindingChanged,
          "later missing-archive flag invalidates earlier positive observation without touching engine state");
    for (unsigned failure = 0; failure != 5; ++failure)
    {
        fixture.Reset();
        value = ArchiveSentinel();
        if (failure == 0)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (failure == 1)
            fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
        if (failure == 2)
            fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
        if (failure == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (failure == 4)
            PoolTestHost::failAt = 1;
        Check(reader.Read(17, value) != ClockResult::Ok && ArchiveUntouched(value),
              "actual owner/PLAYING/GC/initialized GT/read failures preserve prior output");
    }
    fixture.Reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = reader.Read(17, value); });
    thread.join();
    Check(foreign != ClockResult::Ok && ArchiveUntouched(value), "foreign thread cannot adopt native callback archive observation authority");
    for (const auto& pin : AP::Code)
    {
        fixture.Reset();
        value = ArchiveSentinel();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(reader.Read(17, value) != ClockResult::Ok && ArchiveUntouched(value),
              "each named query/provider-missing/archive predicate native pin is required");
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
    }
    fixture.Reset();
    value = ArchiveSentinel();
    const auto wrong = static_cast<std::uint32_t>(AP::BranchRva + 1);
    fixture.transform.image.Patch(AP::BranchTableRva + AP::BranchIndex * 4, &wrong, sizeof(wrong), PAGE_EXECUTE_READ);
    Check(reader.Read(17, value) != ClockResult::Ok && ArchiveUntouched(value), "actual named opcode table must still select qualified archive branch");
    fixture.Reset();
    PoolTestHost::image = 0;
    Check(reader.Read(17, value) != ClockResult::Ok && ArchiveUntouched(value), "wrong actual main image cannot reuse native archive observation");
    std::puts("Production native model archive-membership primitive fixture completed");
    std::printf("Passed %u native model-archive checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
