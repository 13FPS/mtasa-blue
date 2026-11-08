/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine empty cleanup prefix, preserving typed lifetime and world readiness
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldBackupCleanupHistoricalBoundMain
#include "NativeWorldBoundRectTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeWorldBackupCleanupCalls.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldBackupCleanupMemory.cpp"
#include "../../Client/game_de/NativeWorldBackupCleanupCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace CleanupEntryTest
{
    namespace CleanupC = mta::game_de::NativeWorldBackupCleanupCalls;
    namespace CleanupM = mta::game_de::NativeWorldBackupCleanupMemory;
    namespace CleanupP = mta::game_de::NativeWorldBackupCleanupProfile;
    namespace CleanupW = NativeWorldIplFixture;
    NativeWorldPoolMatrixFixture::State* selected = nullptr;
    unsigned                             deletes = 0, resolves = 0, mode = 0, ownerChecks = 0;
    bool                                 invalid = false, refuseSecond = false;
    template <class T>
    T Read(std::uintptr_t address, std::size_t offset = 0) noexcept
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void*>(address + offset), sizeof(value));
        return value;
    }
    ClockResult Checkpoint() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (result != ClockResult::Ok)
            return result;
        // The first check after DeleteRW is its post-ACK borrow; the second is
        // the following Resolve pre-entry check. This controls known refusal
        // without replacing input reads or native writes.
        if (refuseSecond && deletes == 1 && !resolves && ++ownerChecks == 2)
            return ClockResult::ReadFailure;
        return result;
    }
    ClockResult OriginalDouble(CleanupC::Operation operation, CleanupC::Entry entry, std::uintptr_t entity, std::uint32_t& exception) noexcept
    {
        if (!selected)
        {
            invalid = true;
            return ClockResult::InvalidArgument;
        }
        const auto image = selected->image;
        const auto expected = image + (operation == CleanupC::Operation::DeleteRwObject ? CleanupP::DeleteRwRva : CleanupP::ResolveReferencesRva);
        if (reinterpret_cast<std::uintptr_t>(entry) != expected || Read<std::uintptr_t>(entity, 0x20) || Read<std::uintptr_t>(entity, 0x40))
        {
            invalid = true;
            return ClockResult::InvalidArgument;
        }
        if (operation == CleanupC::Operation::DeleteRwObject)
        {
            ++deletes;
            const auto model = Read<std::int16_t>(entity, 0x3a);
            const auto metadata = Read<std::uintptr_t>(image + mta::game_de::NativeModelBankProfile::BankRva + static_cast<std::uintptr_t>(model) * 8);
            if (Read<unsigned char>(metadata, 0x25) || (Read<std::uint32_t>(entity, 0x34) & 4))
            {
                invalid = true;
                return ClockResult::UnsupportedBuild;
            }
            for (std::size_t i = 0; i < CleanupP::ManagerCapacity; ++i)
            {
                const auto record = image + CleanupP::ManagerRva + i * CleanupP::ManagerStride;
                if (Read<unsigned char>(record, CleanupP::ManagerActiveOffset) && Read<std::uintptr_t>(record, CleanupP::ManagerOwnerOffset) == entity)
                {
                    invalid = true;
                    return ClockResult::UnsupportedBuild;
                }
            }
        }
        else
            ++resolves;
        // These are explicit saved-original doubles for source-proven empty
        // branches, not empty production facades. Native entry, typed borrows
        // and unchanged-byte ACK remain the actual production implementation.
        if (mode == 1)
        {
            exception = 0xe05d0001;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            CleanupW::Put(entity, 0x56, static_cast<unsigned char>(7));
        if (mode == 3)
            CleanupW::Put(image + CleanupP::ManagerRva + 31 * CleanupP::ManagerStride, 0, static_cast<unsigned char>(0x55));
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace CleanupEntryTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "cleanup evidence stays under an explicit workspace-owned directory");
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    CleanupW::State                     world(image, &LoadFixture::Write, &fixture);
    NativeWorldPoolMatrixFixture::State entities(image, &LoadFixture::Write, &fixture);
    selected = &entities;
    Check(world.Ready() && entities.Ready(), "owned current typed Building/Dummy/native matrix fixtures are available");
    const auto reset = [&](bool dummy, bool matrix)
    {
        fixture.Reset();
        world.Reset();
        entities.Reset();
        NativeWorldBoundRectFixture::Install(image, &LoadFixture::Write, &fixture);
        NativeWorldBoundRectFixture::PublishProvider(fixture.graph.root, fixture.graph.Game(), reinterpret_cast<std::uintptr_t>(fixture.rows), 20000);
        for (const auto& pin : CleanupP::Pins)
            LoadFixture::Write(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto slot : CleanupP::DeleteRwSlots)
        {
            const auto target = image + CleanupP::DeleteRwRva;
            LoadFixture::Write(&fixture, slot, &target, sizeof(target), PAGE_READONLY);
        }
        std::vector<unsigned char> records(CleanupP::ManagerCapacity * CleanupP::ManagerStride);
        for (std::size_t i = 0; i < records.size(); ++i)
            records[i] = static_cast<unsigned char>(0x31 + i % 83);
        for (std::size_t i = 0; i < CleanupP::ManagerCapacity; ++i)
            records[i * CleanupP::ManagerStride + CleanupP::ManagerActiveOffset] = 0;
        const auto other = entities.buildings.Row(1);
        records[10 * CleanupP::ManagerStride + CleanupP::ManagerActiveOffset] = 1;
        std::memcpy(records.data() + 10 * CleanupP::ManagerStride + CleanupP::ManagerOwnerOffset, &other, sizeof(other));
        LoadFixture::Write(&fixture, CleanupP::ManagerRva, records.data(), records.size(), PAGE_READWRITE);
        const auto entity = dummy ? entities.dummies.Row(0) : entities.buildings.Row(0);
        if (matrix)
            entities.Move(0, mta::game_de::NativeWorldPoolMatrixMemory::List::Static, entity);
        CleanupW::Put(NativeModelObjectGroupFixture::Row(image), 0x22, std::uint16_t{37});
        CleanupW::Put(entity, 0x60, entities.buildings.Row(1));
        CleanupW::Put(entities.buildings.Row(1), 0x68, static_cast<unsigned char>(13));
        // Empty ResolveReferences must not dereference a free-list head. The
        // opaque sentinel makes substituting unrelated reference work visible.
        world.Global(0x522c4b8, std::uintptr_t{0x12345678});
        deletes = resolves = mode = ownerChecks = 0;
        invalid = refuseSecond = false;
        BoundTest::entries = 0;
        return entity;
    };
    CleanupC::Receipt receipt;
    for (unsigned scenario = 0; scenario != 2; ++scenario)
    {
        const auto      entity = reset(scenario != 0, scenario != 0);
        CleanupM::Value before;
        const auto      kind = scenario ? CleanupC::Kind::Dummy : CleanupC::Kind::Building;
        Check(CleanupM::CaptureEmpty(image, 1, kind, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, before) == ClockResult::Ok,
              "genuine empty resource branches have current typed/entity/model/matrix and fixed manager authority");
        Check(CleanupC::RunEmptyPrefix(image, 1, kind, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) == ClockResult::Ok &&
                  receipt.completed == 2 && !receipt.uncertain && receipt.entity == entity && deletes == 1 && resolves == 1 && !invalid &&
                  BoundTest::entries == 0,
              "only genuine DeleteRW and Resolve native prefix entries execute; no geometry or destructor substitute is called");
        Check(receipt.stages[0].entered && receipt.stages[0].returned && receipt.stages[0].acknowledged && receipt.stages[1].entered &&
                  receipt.stages[1].returned && receipt.stages[1].acknowledged && CleanupM::SealEmpty(image, before),
              "each entered/returned stage requires a separate exact unchanged-byte ACK");
        Check(Read<std::uintptr_t>(entity) == before.inputs.entity.vtable && Read<std::uint16_t>(NativeModelObjectGroupFixture::Row(image), 0x22) == 37 &&
                  Read<unsigned char>(entities.buildings.Row(1), 0x68) == 13 && Read<std::uintptr_t>(image + 0x522c4b8) == 0x12345678,
              "vtable, metadata refcount, LOD child count and unused global reference head retain their native lifetimes");
        Check(!CleanupP::StencilOwnerCleanupQualified && !CleanupP::CompleteMtaBackupCleanup,
              "a complete native prefix cannot fake the missing per-owner stencil operation or pool-release readiness");
    }
    for (unsigned failure = 0; failure != 6; ++failure)
    {
        const auto entity = reset(true, false);
        if (failure == 0)
            CleanupW::Put(entity, 0x20, entities.buildings.Row(0));
        if (failure == 1)
            CleanupW::Put(entity, 0x40, entities.buildings.Row(0));
        if (failure == 2)
            CleanupW::Put(entity, 0x70, entities.buildings.Row(0));
        if (failure == 3)
            CleanupW::Put(entity, 0x34, std::uint32_t{4});
        if (failure == 4)
            CleanupW::Put(NativeModelObjectGroupFixture::Row(image), 0x25, static_cast<unsigned char>(1));
        if (failure == 5)
        {
            CleanupW::Put(image + CleanupP::ManagerRva, CleanupP::ManagerActiveOffset, static_cast<unsigned char>(1));
            CleanupW::Put(image + CleanupP::ManagerRva, CleanupP::ManagerOwnerOffset, entity);
        }
        Check(CleanupC::RunEmptyPrefix(image, 1, CleanupC::Kind::Dummy, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) !=
                      ClockResult::Ok &&
                  !deletes && !resolves && !receipt.completed && !receipt.uncertain,
              "actor/reference/alternate actor/provider/2DFX/matching manager ownership refuses before both originals");
    }
    for (unsigned failure = 1; failure != 4; ++failure)
    {
        reset(false, false);
        mode = failure;
        Check(CleanupC::RunEmptyPrefix(image, 1, CleanupC::Kind::Building, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) !=
                      ClockResult::Ok &&
                  receipt.stages[0].entered && !receipt.stages[0].acknowledged && receipt.uncertain && deletes == 1 && !resolves,
              "entered failure or sealed entity/manager drift is unknown and never advances to reference cleanup");
    }
    reset(false, false);
    refuseSecond = true;
    Check(CleanupC::RunEmptyPrefix(image, 1, CleanupC::Kind::Building, 0, &Checkpoint, &PoolTestHost::Owner, &receipt, &OriginalDouble) ==
                  ClockResult::ReadFailure &&
              receipt.completed == 1 && !receipt.uncertain && receipt.stages[0].acknowledged && !receipt.stages[1].entered && deletes == 1 && !resolves,
          "known second pre-entry refusal preserves the acknowledged first prefix without inventing automatic replay");
    selected = nullptr;
    std::printf("Passed %u native world backup cleanup checks (genuine empty prefix admission; final originals doubled; stencil/release unqualified)\n",
                checks);
    return 0;
}
