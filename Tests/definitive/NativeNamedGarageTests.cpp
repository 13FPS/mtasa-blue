/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Focused named garage byte properties over the real owned SEC_IMAGE scaffold
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeNamedGarageHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeNamedGarageMemory.h"
#include "../../Client/game_de/NativeNamedGarageProfile.h"
#include <game/CGarageDoorStatePolicy.h>
#include <intrin.h>
namespace GarageHost
{
    using namespace mta::game_de;
    unsigned exchanges = 0;
    bool     failBefore = false, failAfter = false, conflict = false;
    void (*effect)() = nullptr;
    NativeNamedGarageMemory* nested = nullptr;
    NativeGarageName         nestedName{};
    ClockResult              nestedResult = ClockResult::Ok;
    bool Exchange(std::uintptr_t address, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        if (nested)
        {
            auto* current = nested;
            nested = nullptr;
            nestedResult = current->SetOpen(nestedName, true);
        }
        if (failBefore)
        {
            exception = 0xc0000005;
            return false;
        }
        if (conflict)
            *reinterpret_cast<volatile std::uint8_t*>(address) = static_cast<std::uint8_t>(expected ^ 0x80u);
        char before = 0, after = 0;
        std::memcpy(&before, &expected, 1);
        std::memcpy(&after, &target, 1);
        const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address), after, before);
        std::memcpy(&observed, &previous, 1);
        if (effect)
            effect();
        if (failAfter)
        {
            exception = 0xc0000005;
            return false;
        }
        return true;
    }
}
#define MTA_DE_NAMED_GARAGE_EXCHANGE GarageHost::Exchange
#define ReadProcessMemory            PoolTestHost::Read
#include "../../Client/game_de/NativeNamedGarageMemory.cpp"
#undef ReadProcessMemory
#undef MTA_DE_NAMED_GARAGE_EXCHANGE
namespace
{
    namespace G = NativeNamedGarageProfile;
    using GarageMemory = NativeNamedGarageMemory;
    NativeGarageName Name(const char* text)
    {
        NativeGarageName name{};
        const auto       length = std::strlen(text);
        Check(length < name.size(), "test input is an explicitly bounded native garage name");
        std::memcpy(name.data(), text, length);
        return name;
    }
    struct GarageFixture
    {
        BankFixture bank;
        explicit GarageFixture(const std::filesystem::path& directory) : bank(directory)
        {
            for (const auto& pin : G::Code)
                bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            bank.transform.image.Patch(G::FoldRva, G::Fold, sizeof(G::Fold), PAGE_READONLY);
            Reset();
        }
        std::uintptr_t Row(std::uint32_t index) const { return bank.transform.image.image + G::BankRva + static_cast<std::uintptr_t>(index) * G::RowBytes; }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            bank.Global(rva, value);
        }
        void RowName(std::uint32_t index, const NativeGarageName& name)
        {
            bank.transform.image.Patch(G::BankRva + static_cast<std::uintptr_t>(index) * G::RowBytes + G::NameOffset, name.data(), name.size(), PAGE_READWRITE);
        }
        void Byte(std::uint32_t index, std::uintptr_t offset, std::uint8_t value)
        {
            Global(G::BankRva + static_cast<std::uintptr_t>(index) * G::RowBytes + offset, value);
        }
        std::uint8_t Byte(std::uint32_t index, std::uintptr_t offset) const { return *reinterpret_cast<const std::uint8_t*>(Row(index) + offset); }
        void         Reset()
        {
            bank.Reset();
            Global(G::CountRva, std::uint32_t{3});
            for (std::uint32_t index = 0; index != 3; ++index)
            {
                std::array<unsigned char, G::DoorStateOffset + 3> row{};
                row.fill(0x6d);
                bank.transform.image.Patch(G::BankRva + static_cast<std::uintptr_t>(index) * G::RowBytes, row.data(), row.size(), PAGE_READWRITE);
                Byte(index, G::TypeOffset, static_cast<std::uint8_t>(index + 1));
                Byte(index, G::DoorStateOffset, static_cast<std::uint8_t>(index));
            }
            RowName(0, Name("alpha"));
            RowName(1, Name("bEtA"));
            RowName(2, Name("BETA"));
            GarageHost::exchanges = 0;
            GarageHost::failBefore = GarageHost::failAfter = GarageHost::conflict = false;
            GarageHost::effect = nullptr;
            GarageHost::nested = nullptr;
            GarageHost::nestedResult = ClockResult::Ok;
            PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        }
    };
    GarageFixture* currentGarage = nullptr;
    void           RenameSelected()
    {
        currentGarage->RowName(1, Name("renamed"));
    }
    void PrependDuplicate()
    {
        currentGarage->RowName(0, Name("beta"));
    }
    void ChangeCount()
    {
        currentGarage->Global(G::CountRva, std::uint32_t{2});
    }
    void RevokeAfterWrite()
    {
        PoolTestHost::ownerResult = ClockResult::WrongThread;
    }
    void ChangeTypeAfterWrite()
    {
        currentGarage->Byte(1, G::TypeOffset, 99);
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "named garage evidence uses explicit workspace output");
    GarageMemory::Binding refused{123};
    Check(!GarageMemory::Qualify(refused) && refused.image == 123, "the shipping exact-build qualifier refuses this ordinary host without output");
    GarageFixture fixture(argv[1]);
    currentGarage = &fixture;
    PoolTestHost::image = fixture.bank.transform.image.image;
    const GarageMemory::Binding binding{PoolTestHost::image};
    GarageMemory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto                  beta = Name("BeTa");
    Check(GarageMemory::Revalidate(binding), "actual lookup/append/type/door/fold pins and minimal published bank/count locations qualify");
    NativeNamedGarageAccess defaultAccess;
    std::uint8_t            output = 0xe7;
    bool                    open = false;
    Check(defaultAccess.ReadNamedGarageType(beta, output) == ClockResult::NotInitialized && output == 0xe7 &&
              defaultAccess.SetNamedGarageType(beta, 1) == ClockResult::NotInitialized &&
              defaultAccess.ReadNamedGarageOpen(beta, open) == ClockResult::NotInitialized && !open &&
              defaultAccess.SetNamedGarageOpen(beta, true) == ClockResult::NotInitialized,
          "named-property interface defaults refuse without an invented SDK garage factory");
    Check(unowned.ReadType(beta, output) == ClockResult::NotInitialized && output == 0xe7, "image storage never substitutes for callback ownership");
    GarageMemory::Receipt receipt;
    Check(memory.ReadType(beta, output) == ClockResult::Ok && output == 2, "the genuine ASCII fold selects the first duplicate name, not the final row");
    Check(memory.SetType(beta, 255, &receipt) == ClockResult::Ok && receipt.index == 1 && receipt.count == 3 && receipt.row == fixture.Row(1) &&
              receipt.address == fixture.Row(1) + G::TypeOffset && receipt.entered && receipt.accepted && receipt.verified &&
              fixture.Byte(1, G::TypeOffset) == 255 && fixture.Byte(2, G::TypeOffset) == 3,
          "one selected raw type byte preserves duplicate ordinal identity without publishing classic SDK IDs");
    for (unsigned type = 0; type != 256; ++type)
    {
        fixture.Byte(1, G::TypeOffset, static_cast<std::uint8_t>(type ^ 0xa5u));
        fixture.Byte(1, G::DoorStateOffset, 4);
        std::array<unsigned char, G::DoorStateOffset + 3> before{}, after{};
        std::memcpy(before.data(), reinterpret_cast<const void*>(fixture.Row(1)), before.size());
        const auto exchanges = GarageHost::exchanges;
        Check(memory.SetType(beta, static_cast<std::uint8_t>(type), &receipt) == ClockResult::Ok &&
                  receipt.original == static_cast<std::uint8_t>(type ^ 0xa5u) && receipt.target == type && receipt.accepted && receipt.verified &&
                  GarageHost::exchanges == exchanges + 1,
              "all raw BYTE type values use exactly one selected-byte conditional write");
        std::memcpy(after.data(), reinterpret_cast<const void*>(fixture.Row(1)), after.size());
        before[G::TypeOffset] = static_cast<std::uint8_t>(type);
        Check(before == after, "raw MTA SetType preserves every neighboring byte instead of running DE command reset effects");
        output = 0xe7;
        Check(memory.ReadType(beta, output) == ClockResult::Ok && output == type, "type getter preserves every raw BYTE representation");
    }
    for (unsigned state = 0; state != 256; ++state)
    {
        fixture.Byte(1, G::DoorStateOffset, static_cast<std::uint8_t>(state));
        open = state != 1 && state != 3;
        Check(memory.ReadOpen(beta, open) == ClockResult::Ok && open == (state == 1 || state == 3) &&
                  MtaGame::CGarageDoorStatePolicy::IsOpen(static_cast<std::uint8_t>(state)) == open,
              "actual MTA IsOpen is exactly states1/3, including refusal to reinterpret native script state4 as SDK open");
    }
    for (const auto state : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 127u, 128u, 255u})
        for (const bool desired : {false, true})
        {
            fixture.Byte(1, G::TypeOffset, 255);
            fixture.Byte(1, G::DoorStateOffset, static_cast<std::uint8_t>(state));
            std::array<unsigned char, G::DoorStateOffset + 3> before{}, after{};
            std::memcpy(before.data(), reinterpret_cast<const void*>(fixture.Row(1)), before.size());
            const auto exchanges = GarageHost::exchanges;
            Check(memory.SetOpen(beta, desired, &receipt) == ClockResult::Ok && receipt.original == state && receipt.target == (desired ? 3 : 2) &&
                      receipt.accepted && receipt.verified && GarageHost::exchanges == exchanges + 1,
                  "MTA SetOpen directly stores3/2 even where the DE script transition would refuse or do extra work");
            std::memcpy(after.data(), reinterpret_cast<const void*>(fixture.Row(1)), after.size());
            before[G::DoorStateOffset] = desired ? 3 : 2;
            Check(before == after, "door write preserves raw type, flags, original type, name and geometry bytes");
        }
    fixture.Reset();
    auto malformed = beta;
    malformed.fill('x');
    output = 0xe7;
    Check(memory.ReadType(malformed, output) == ClockResult::InvalidArgument && output == 0xe7 && PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
          "unbounded input names refuse before image or owner access");
    for (const auto count : {0x8000u, 0xffffu, 0x10000u, 0xffffffffu})
    {
        fixture.Reset();
        fixture.Global(G::CountRva, count);
        output = 0xe7;
        Check(memory.ReadType(beta, output) == ClockResult::BindingChanged && output == 0xe7 &&
                  memory.SetType(beta, 4, &receipt) == ClockResult::BindingChanged && !receipt.entered && GarageHost::exchanges == 0,
              "full DWORD count never truncates through the signed-WORD lookup domain");
    }
    fixture.Reset();
    fixture.Global(G::CountRva, std::uint32_t{0});
    output = 0xe7;
    Check(memory.ReadType(beta, output) == ClockResult::InvalidArgument && output == 0xe7, "an empty published set has no successful named association");
    fixture.Reset();
    fixture.Global(G::CountRva, G::MaximumLookupCount);
    output = 0xe7;
    Check(memory.ReadType(Name("alpha"), output) == ClockResult::Ok && output == 1,
          "lookup-domain bound is not misrepresented as a proven capacity; the already published first match alone is selected");
    fixture.Reset();
    NativeGarageName nonAscii{0xc0, 'a', 0, 0, 0, 0, 0, 0};
    fixture.RowName(1, nonAscii);
    output = 0xe7;
    Check(memory.ReadType(nonAscii, output) == ClockResult::Ok && output == 2, "equal high bytes retain genuine exact-byte comparison");
    nonAscii[0] = 0xe0;
    Check(memory.ReadType(nonAscii, output) == ClockResult::InvalidArgument && output == 2,
          "non-ASCII bytes are never casefolded by locale or guessed Unicode semantics");
    fixture.Reset();
    malformed.fill('z');
    fixture.RowName(0, malformed);
    output = 0xe7;
    Check(memory.ReadType(beta, output) == ClockResult::BindingChanged && output == 0xe7,
          "an unterminated predecessor is not scanned beyond its exact name field");
    fixture.Reset();
    Check(memory.ReadType(beta, output) == ClockResult::Ok, "measure actual checked reads and callback boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= reads; ++fail)
    {
        fixture.Reset();
        PoolTestHost::failAt = fail;
        output = 0xe7;
        Check(memory.ReadType(beta, output) != ClockResult::Ok && output == 0xe7, "each real checked observation fault preserves caller output");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        fixture.Reset();
        PoolTestHost::revokeAt = revoke;
        output = 0xe7;
        Check(memory.ReadType(beta, output) == ClockResult::WrongThread && output == 0xe7, "each callback owner bookend may revoke a named observation");
    }
    for (const auto effect : {&RenameSelected, &PrependDuplicate, &ChangeCount})
    {
        fixture.Reset();
        PoolTestHost::watched = fixture.Row(1) + G::NameOffset;
        PoolTestHost::after = effect;
        output = 0xe7;
        Check(memory.ReadType(beta, output) != ClockResult::Ok && output == 0xe7,
              "selected name, prior first-match or full published count changes refuse stale output");
    }
    for (const auto effect : {&RenameSelected, &PrependDuplicate, &ChangeCount, &RevokeAfterWrite, &ChangeTypeAfterWrite})
    {
        fixture.Reset();
        GarageHost::effect = effect;
        Check(memory.SetOpen(beta, true, &receipt) != ClockResult::Ok && receipt.entered && receipt.accepted && !receipt.verified &&
                  fixture.Byte(1, G::DoorStateOffset) == 3 && GarageHost::exchanges == 1,
              "accepted door intent persists after name/count/type/callback loss with no replay or rollback");
    }
    fixture.Reset();
    GarageHost::conflict = true;
    Check(memory.SetType(beta, 7, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.observedValid &&
              GarageHost::exchanges == 1 && fixture.Byte(1, G::TypeOffset) == static_cast<std::uint8_t>(2 ^ 0x80),
          "a selected-byte race remains visible and never retries over native progress");
    fixture.Reset();
    GarageHost::failBefore = true;
    Check(memory.SetType(beta, 7, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted && !receipt.observedValid &&
              receipt.exceptionCode == 0xc0000005 && fixture.Byte(1, G::TypeOffset) == 2 && GarageHost::exchanges == 1,
          "entry fault carries its exception without consuming readback or fabricating acceptance");
    fixture.Reset();
    GarageHost::failAfter = true;
    Check(memory.SetType(beta, 7, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted && !receipt.observedValid &&
              fixture.Byte(1, G::TypeOffset) == 7 && GarageHost::exchanges == 1,
          "an unknown partial write remains persistent and never replays or rolls back");
    fixture.Reset();
    GarageHost::nested = &memory;
    GarageHost::nestedName = beta;
    Check(memory.SetType(beta, 7, &receipt) == ClockResult::Ok && GarageHost::nestedResult == ClockResult::Busy && GarageHost::exchanges == 1,
          "cross-property reentry cannot borrow the active named garage mutation scope");
    for (const auto& pin : G::Code)
    {
        fixture.Reset();
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        fixture.bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        output = 0xe7;
        Check(memory.ReadType(beta, output) == ClockResult::BindingChanged && output == 0xe7, "each exact native garage code span is mandatory");
        fixture.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    fixture.Reset();
    auto changedFold = G::Fold['A'];
    changedFold ^= 1;
    fixture.bank.transform.image.Patch(G::FoldRva + 'A', &changedFold, 1, PAGE_READONLY);
    Check(!GarageMemory::Revalidate(binding), "casefold table identity is a qualified producer/consumer fact, not current locale");
    fixture.bank.transform.image.Patch(G::FoldRva, G::Fold, sizeof(G::Fold), PAGE_READONLY);
    for (unsigned state = 0; state != 9; ++state)
    {
        fixture.Reset();
        fixture.Global(NativeGameStateProfile::stateRva, static_cast<std::uint32_t>(state));
        output = 0xe7;
        Check(memory.ReadType(beta, output) != ClockResult::Ok && output == 0xe7,
              "frontend/loading or unqualified native states do not admit gameplay garage storage");
    }
    fixture.Reset();
    fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    output = 0xe7;
    Check(memory.ReadType(beta, output) != ClockResult::Ok && output == 0xe7, "actual native scoped GC excludes garage observation");
    fixture.Reset();
    ClockResult foreign = ClockResult::Ok;
    output = 0xe7;
    std::thread other([&] { foreign = memory.ReadType(beta, output); });
    other.join();
    Check(foreign == ClockResult::WrongThread && output == 0xe7, "callback seam cannot replace genuine native GameThread identity");
    std::printf("Production native named garage property primitive fixture completed\n");
    std::printf("Passed %u native named garage checks (64-bit; no native engine code executed)\n", checks);
    // Geometry fixtures reuse this scaffold under a renamed entry point, where
    // C++ no longer supplies main's implicit successful return.
    return 0;
}
