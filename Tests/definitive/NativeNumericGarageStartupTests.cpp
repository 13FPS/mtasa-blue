/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact numeric identity and byte intent on owned native-shaped data
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NumericGarageHistoricalWorldDefinitionsMain
#include "NativeStreamingLoadAllTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeNumericGarageMemory.h"
#include "../../Client/game_de/NativeNumericGarageProfile.h"
#include "../../Client/game_de/NativeNamedGarageGeometryProfile.h"
#include <intrin.h>
namespace NumericGarageTestHost
{
    unsigned exchanges = 0;
    bool     failBefore = false, failAfter = false, conflict = false;
    void (*effect)() = nullptr;
    NativeNumericGarageMemory* nestedNumeric = nullptr;
    NativeNamedGarageMemory*   nestedNamed = nullptr;
    ClockResult                nestedStatus = ClockResult::Ok;
    bool Exchange(std::uintptr_t address, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        if (nestedNumeric)
        {
            auto* selected = nestedNumeric;
            nestedNumeric = nullptr;
            nestedStatus = selected->SetType(8, 0x51);
        }
        if (nestedNamed)
        {
            auto* selected = nestedNamed;
            nestedNamed = nullptr;
            nestedStatus = selected->SetType(NativeNumericGarageProfile::Find(8)->name, 0x52);
        }
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (conflict)
            *reinterpret_cast<volatile std::uint8_t*>(address) = static_cast<std::uint8_t>(expected ^ 0x80);
        char before = 0, after = 0;
        std::memcpy(&before, &expected, 1);
        std::memcpy(&after, &target, 1);
        const auto old = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address), after, before);
        std::memcpy(&observed, &old, 1);
        if (effect)
            effect();
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
}
#define MTA_DE_NAMED_GARAGE_EXCHANGE NumericGarageTestHost::Exchange
#define ReadProcessMemory            PoolTestHost::Read
#include "../../Client/game_de/NativeNamedGarageMemory.cpp"
#include "../../Client/game_de/NativeNumericGarageMemory.cpp"
#undef ReadProcessMemory
#undef MTA_DE_NAMED_GARAGE_EXCHANGE
namespace NumericGarageTests
{
    namespace P = NativeNumericGarageProfile;
    namespace G = NativeNamedGarageProfile;
    namespace H = NumericGarageTestHost;
    using Memory = NativeNumericGarageMemory;
    struct Fixture : LoadFixture
    {
        explicit Fixture(const std::filesystem::path& directory) : LoadFixture(directory) { Reset(); }
        // A reverse permutation covers all fifty rows exactly once. The old
        // every-other-row layout would underflow once the complete bank is used.
        std::uint32_t  Index(std::size_t slot) const { return static_cast<std::uint32_t>(49 - slot); }
        std::uintptr_t Row(std::uint32_t index) const { return transform.image.image + G::BankRva + index * G::RowBytes; }
        std::uintptr_t Rva(std::uint32_t index) const { return G::BankRva + index * G::RowBytes; }
        void           Name(std::uint32_t index, const NativeGarageName& name)
        {
            transform.image.Patch(Rva(index) + G::NameOffset, name.data(), name.size(), PAGE_READWRITE);
        }
        void Geometry(std::uint32_t index, const NativeGarageGeometry& geometry)
        {
            transform.image.Patch(Rva(index), geometry.words.data(), sizeof(geometry.words), PAGE_READWRITE);
        }
        void         Byte(std::uint32_t index, std::uintptr_t offset, std::uint8_t value) { Global(Rva(index) + offset, value); }
        std::uint8_t Byte(std::uint32_t index, std::uintptr_t offset) const { return *reinterpret_cast<const std::uint8_t*>(Row(index) + offset); }
        void         Reset()
        {
            LoadFixture::Reset();
            for (const auto& pin : G::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeNamedGarageGeometryProfile::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            transform.image.Patch(G::FoldRva, G::Fold, sizeof(G::Fold), PAGE_READONLY);
            Global(G::CountRva, P::CertifiedPublishedCount);
            for (std::uint32_t index = 0; index != P::CertifiedPublishedCount; ++index)
            {
                std::array<unsigned char, G::RowBytes> row{};
                row[G::TypeOffset] = 0x91;
                row[G::DoorStateOffset] = 0x55;
                row[G::DoorStateOffset + 1] = 0xc1;
                row[G::DoorStateOffset + 2] = 0xd2;
                transform.image.Patch(Rva(index), row.data(), row.size(), PAGE_READWRITE);
            }
            for (std::size_t slot = 0; slot != P::Descriptors.size(); ++slot)
            {
                const auto index = Index(slot);
                Name(index, P::Descriptors[slot].name);
                Geometry(index, P::Descriptors[slot].geometry);
                Byte(index, G::TypeOffset, static_cast<std::uint8_t>(32 + slot));
                Byte(index, G::DoorStateOffset, static_cast<std::uint8_t>(slot % 6));
            }
            H::exchanges = 0;
            H::failBefore = H::failAfter = H::conflict = false;
            H::effect = nullptr;
            H::nestedNumeric = nullptr;
            H::nestedNamed = nullptr;
            H::nestedStatus = ClockResult::Ok;
            loadCalls = loadEffects = requestCalls = 0;
            PoolTestHost::reads = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = PoolTestHost::failAt = 0;
            PoolTestHost::ownerResult = ClockResult::Ok;
            PoolTestHost::watched = 0;
            PoolTestHost::after = nullptr;
        }
        std::vector<unsigned char> Bytes() const
        {
            const auto* first = reinterpret_cast<const unsigned char*>(Row(0));
            return {first, first + P::CertifiedPublishedCount * G::RowBytes};
        }
    };
    Fixture* active = nullptr;
    void     ChangeGeometry()
    {
        auto value = P::Find(8)->geometry;
        value.words[7] ^= 1;
        active->Geometry(active->Index(1), value);
    }
    void Rename()
    {
        auto name = P::Find(8)->name;
        name[0] = 'X';
        active->Name(active->Index(1), name);
    }
    void ChangeCount()
    {
        active->Global(G::CountRva, std::uint32_t{49});
    }
    void ChangeWorldSerial()
    {
        active->Put(active->graph.items.data(), 0x10, std::uint32_t{999});
    }
    void LoseWorld()
    {
        active->Put(active->graph.level.data(), 0xd0, std::uintptr_t{0});
    }
    void RevokeOwner()
    {
        PoolTestHost::ownerResult = ClockResult::WrongThread;
    }
    void ChangeDoor()
    {
        active->Byte(active->Index(1), G::DoorStateOffset, 5);
    }
    void Released()
    {
        Check(NativeStreamingCompletionContext::TryEnter(), "every numeric return releases only its temporary shared completion scope");
        NativeStreamingCompletionContext::Leave();
        Check(!NamedGarageDetail::writing, "every numeric writer releases the one shared named/numeric byte gate");
    }
}
int main(int argc, char** argv)
{
    namespace T = NumericGarageTests;
    namespace P = T::P;
    namespace G = T::G;
    namespace H = T::H;
    static_assert(P::Descriptors.size() == 50 && P::CertifiedPublishedCount == 50);
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "numeric fixture backing belongs to explicit task scratch");
    T::Fixture fixture(argv[1]);
    T::active = &fixture;
    PoolTestHost::image = fixture.transform.image.image;
    T::Memory               memory({PoolTestHost::image}, &PoolTestHost::Owner);
    NativeNamedGarageMemory named({PoolTestHost::image}, &PoolTestHost::Owner);
    T::Memory::Receipt      receipt;
    fixture.Reset();
    const auto initial = fixture.Bytes();
    Check(memory.CheckReady() == ClockResult::Ok && memory.CheckReady() == ClockResult::Ok,
          "all fifty exact descriptors qualify with physical order deliberately unrelated to SDK IDs");
    const auto ownerChecks = PoolTestHost::ownerCalls / 2;
    const auto reads = PoolTestHost::reads / 2;
    Check(fixture.Bytes() == initial && !H::exchanges && !loadCalls && !loadEffects && !requestCalls,
          "repeated readiness changes no garage byte, loader, request, journal or native engine state");
    T::Released();
    for (std::size_t slot = 0; slot != P::Descriptors.size(); ++slot)
    {
        const auto           id = P::Descriptors[slot].sdkId;
        NativeGarageGeometry geometry;
        geometry.words.fill(0xdeadbeef);
        std::uint8_t type = 0xff;
        bool         open = false;
        Check(memory.ReadGeometry(id, geometry) == ClockResult::Ok && geometry.words == P::Descriptors[slot].geometry.words,
              "each numeric read preserves every raw geometry word, including both distinct same-name garages");
        Check(memory.ReadType(id, type) == ClockResult::Ok && type == 32 + slot, "each SDK type selects the independently qualified permuted native record");
        Check(memory.ReadOpen(id, open) == ClockResult::Ok && open == (slot % 6 == 1 || slot % 6 == 3),
              "numeric open observation preserves MTA's original byte1-or3 predicate");
    }
    Check(!H::exchanges, "property reads never enter even a same-value byte exchange");
    std::puts("Production native fifty-ID numeric garage descriptor reads fixture completed");
    // Existing named lookup intentionally selects the first physical same-name
    // row, while numeric8 selects its own geometry and numeric11 stays distinct.
    std::uint8_t value = 0;
    Check(named.ReadType(P::Find(8)->name, value) == ClockResult::Ok && value == 35,
          "the unchanged named route keeps its original first-match behavior for duplicate sprLAe");
    Check(memory.ReadType(8, value) == ClockResult::Ok && value == 33 && memory.ReadType(11, value) == ClockResult::Ok && value == 35,
          "numeric duplicate names never collapse to the old named first match");
    fixture.Reset();
    const auto other = fixture.Byte(fixture.Index(3), G::TypeOffset);
    Check(memory.SetType(8, 0xa7, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && H::exchanges == 1 &&
              receipt.index == fixture.Index(1) && fixture.Byte(fixture.Index(1), G::TypeOffset) == 0xa7 &&
              fixture.Byte(fixture.Index(3), G::TypeOffset) == other && fixture.Byte(fixture.Index(1), G::DoorStateOffset) == 1 &&
              fixture.Byte(fixture.Index(1), G::DoorStateOffset + 1) == 0xc1 && fixture.Byte(fixture.Index(1), G::DoorStateOffset + 2) == 0xd2,
          "one actual BYTE CAS changes numeric8 only and preserves numeric11, door state, flags and original type");
    Check(memory.SetOpen(8, true, &receipt) == ClockResult::Ok && H::exchanges == 2 && fixture.Byte(fixture.Index(1), G::DoorStateOffset) == 3 &&
              fixture.Byte(fixture.Index(1), G::TypeOffset) == 0xa7,
          "true SDK door intent writes original MTA byte3 independently of the script command transitions");
    Check(memory.SetOpen(8, false, &receipt) == ClockResult::Ok && H::exchanges == 3 && fixture.Byte(fixture.Index(1), G::DoorStateOffset) == 2,
          "false SDK door intent writes original MTA byte2 without touching the type");
    T::Released();
    for (const auto status : {ClockResult::WrongThread, ClockResult::Expired, ClockResult::OwnerConflict, ClockResult::PatchFailure})
    {
        fixture.Reset();
        PoolTestHost::ownerResult = status;
        NativeGarageGeometry geometry;
        geometry.words.fill(0xabcdef12);
        value = 0xac;
        bool       open = true;
        const auto before = fixture.Bytes();
        Check(memory.CheckReady() == status && memory.ReadGeometry(8, geometry) == status && geometry.words[0] == 0xabcdef12 &&
                  memory.ReadType(8, value) == status && value == 0xac && memory.ReadOpen(8, open) == status && open &&
                  memory.SetType(8, 0, &receipt) == status && !receipt.entered && fixture.Bytes() == before && !H::exchanges,
              "missing native callback authority preserves exact refusal, caller outputs and all garage bytes");
        T::Released();
    }
    fixture.Reset();
    value = 0x55;
    Check(memory.ReadType(50, value) == ClockResult::InvalidArgument && memory.ReadType(0xffffffffu, value) == ClockResult::InvalidArgument && value == 0x55 &&
              memory.SetType(50, 0, &receipt) == ClockResult::InvalidArgument && !receipt.entered &&
              memory.SetType(0xffffffffu, 0, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !H::exchanges,
          "out-of-range IDs remain unavailable instead of an ordinal/name fallback or false success");
    Check(NativeStreamingCompletionContext::TryEnter(), "fixture owns a genuinely competing loading scope");
    Check(memory.CheckReady() == ClockResult::Busy && memory.SetType(8, 0, &receipt) == ClockResult::Busy && !receipt.entered && !H::exchanges,
          "numeric readiness and writes never borrow another operation's completion scope");
    NativeStreamingCompletionContext::Leave();
    T::Released();
    fixture.Reset();
    // Every physical row is now qualified. Replace a different descriptor
    // deliberately; the numeric8 write must still reject its duplicate match.
    const auto duplicateIndex = fixture.Index(P::Descriptors.size() - 1);
    fixture.Name(duplicateIndex, P::Find(8)->name);
    fixture.Geometry(duplicateIndex, P::Find(8)->geometry);
    Check(
        memory.CheckReady() == ClockResult::OwnerConflict && memory.SetType(8, 0, &receipt) == ClockResult::OwnerConflict && !receipt.entered && !H::exchanges,
        "a duplicate full descriptor refuses instead of choosing its first current ordinal");
    // The imported transform fixture already owns a global mutation seam.
    // Keep these independent garage scenarios from shadowing that state.
    for (const auto garageMutation : {&T::Rename, &T::ChangeGeometry, &T::ChangeCount, &T::LoseWorld})
    {
        fixture.Reset();
        garageMutation();
        const auto before = fixture.Bytes();
        Check(memory.CheckReady() != ClockResult::Ok && memory.SetType(8, 0, &receipt) != ClockResult::Ok && !receipt.entered && !H::exchanges &&
                  fixture.Bytes() == before,
              "missing descriptor, changed geometry/count or world identity cannot enter a garage write");
        T::Released();
    }
    for (const auto garageMutation : {&T::Rename, &T::ChangeGeometry})
    {
        fixture.Reset();
        PoolTestHost::watched = fixture.Row(fixture.Index(1)) + (garageMutation == &T::Rename ? G::NameOffset : 0);
        PoolTestHost::after = garageMutation;
        Check(memory.SetType(8, 0, &receipt) != ClockResult::Ok && !receipt.entered && !H::exchanges,
              "selection changing between independent snapshots refuses before the exact-byte effect");
        T::Released();
    }
    for (const auto at : {1u, ownerChecks / 2u, ownerChecks})
    {
        fixture.Reset();
        PoolTestHost::revokeAt = at;
        Check(memory.CheckReady() != ClockResult::Ok && !H::exchanges, "early/middle/final owner revocation cannot manufacture a readiness lease");
        T::Released();
    }
    for (const auto at : {1u, reads / 2u, reads})
    {
        fixture.Reset();
        PoolTestHost::failAt = at;
        Check(memory.CheckReady() != ClockResult::Ok && !H::exchanges, "early/middle/final current native observations fail closed without effects");
        T::Released();
    }
    fixture.Reset();
    H::nestedNamed = &named;
    Check(memory.SetType(8, 0x71, &receipt) == ClockResult::Ok && H::nestedStatus == ClockResult::Busy && H::exchanges == 1,
          "numeric exchange owns the same gate that rejects reentrant named writes");
    fixture.Reset();
    H::nestedNumeric = &memory;
    Check(named.SetType(P::Find(8)->name, 0x72) == ClockResult::Ok && H::nestedStatus == ClockResult::Busy && H::exchanges == 1,
          "existing named exchange rejects reentrant numeric writes through the shared gate");
    fixture.Reset();
    H::effect = &T::ChangeDoor;
    Check(memory.SetType(8, 0x73, &receipt) == ClockResult::Ok && receipt.verified && fixture.Byte(fixture.Index(1), G::DoorStateOffset) == 5,
          "type-only intent does not falsely own or rewind an independent legitimate door-state transition");
    for (unsigned failure = 0; failure != 7; ++failure)
    {
        fixture.Reset();
        H::failBefore = failure == 0;
        H::failAfter = failure == 1;
        H::conflict = failure == 2;
        H::effect = failure == 3   ? &T::Rename
                    : failure == 4 ? &T::ChangeGeometry
                    : failure == 5 ? &T::ChangeWorldSerial
                    : failure == 6 ? &T::RevokeOwner
                                   : nullptr;
        Check(memory.SetType(8, 0x77, &receipt) != ClockResult::Ok && receipt.entered && !receipt.verified && H::exchanges == 1,
              "entered uncertainty/conflict or lost acknowledgement returns one original receipt without retry");
        const auto expected = static_cast<std::uint8_t>(failure == 0 ? 33 : failure == 2 ? (33 ^ 0x80) : 0x77);
        Check(fixture.Byte(fixture.Index(1), G::TypeOffset) == expected,
              "accepted or competing byte outcomes persist exactly; cleanup never retries or rewinds a garage field");
        T::Released();
    }
    Check(!loadCalls && !loadEffects && !requestCalls, "no native game loading/request instruction was executed by this focused fixture");
    std::puts("Production native numeric garage startup fixture completed");
    std::printf("Passed %u native numeric garage startup checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
