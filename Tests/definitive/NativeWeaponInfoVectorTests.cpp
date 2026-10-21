/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Concrete SDK weapon vectors/slots over an owned native-shaped image
 *****************************************************************************/
// Reuse the actual sparse Windows image builder without rerunning earlier
// scalar/getter checks or replacing any production SDK implementation.
#define wmain FrozenReadOnlyWeaponFixtureMain
#include "NativeWeaponInfoReaderTests.cpp"
#undef wmain
#include "../../Client/game_de/NativeWeaponInfoVectorMemory.h"
#include "../../Client/game_de/NativeWeaponInfoVectorProfile.h"
#include "../../Client/game_de/CWeaponInfoDE.h"
#include <functional>
#include <intrin.h>

namespace VectorTestHost
{
    unsigned                  exchanges = 0, admissionCalls = 0, failAt = 0;
    mta::game_de::ClockResult admitted = mta::game_de::ClockResult::Ok;
    bool                      sourceFailure = false, unstableSource = false;
    const CVector*            watchedSource = nullptr;
    unsigned                  sourceReads = 0;
    std::function<void()>     before, after;
    mta::game_de::ClockResult Admit() noexcept
    {
        ++admissionCalls;
        return admitted;
    }
    LONG Exchange(volatile LONG* address, LONG value, LONG expected)
    {
        ++exchanges;
        if (exchanges == failAt)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        if (before)
            before();
        const LONG result = _InterlockedCompareExchange(address, value, expected);
        if (after)
            after();
        return result;
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (source == watchedSource && bytes == sizeof(CVector))
        {
            ++sourceReads;
            if (sourceFailure)
            {
                if (copied)
                    *copied = bytes / 2;
                return FALSE;
            }
        }
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && unstableSource && source == watchedSource && sourceReads == 2)
            static_cast<unsigned char*>(output)[0] ^= 1;
        return result;
    }
    void Reset()
    {
        exchanges = admissionCalls = failAt = sourceReads = 0;
        sourceFailure = unstableSource = false;
        watchedSource = nullptr;
        admitted = mta::game_de::ClockResult::Ok;
        before = after = {};
        WeaponTestHost::failAt.store(0);
    }
}
#undef InterlockedCompareExchange
#define InterlockedCompareExchange VectorTestHost::Exchange
#define GetModuleHandleW           WeaponTestHost::Module
#define ReadProcessMemory          VectorTestHost::Read
#include "../../Client/game_de/NativeWeaponInfoVectorMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
#undef InterlockedCompareExchange
#include "../../Client/game_de/CWeaponInfoWriteDE.cpp"
#include "../../Client/game_de/CWeaponInfoDE.cpp"

namespace
{
    namespace V = NativeWeaponInfoVectorMemory;
    namespace VP = NativeWeaponInfoVectorProfile;
    using Bank = std::array<unsigned char, P::RowCount * P::RowBytes>;
    Bank WholeBank(std::uintptr_t image)
    {
        Bank result;
        std::memcpy(result.data(), reinterpret_cast<const void*>(image + P::BankRva), result.size());
        return result;
    }
    void VectorAt(std::uintptr_t image, std::size_t slot, const CVector& value)
    {
        std::memcpy(reinterpret_cast<void*>(image + P::BankRva + slot * P::RowBytes + 0x24), &value, sizeof(value));
    }
    void ComponentAt(std::uintptr_t image, std::size_t slot, unsigned component, float value)
    {
        std::memcpy(reinterpret_cast<void*>(image + P::BankRva + slot * P::RowBytes + 0x24 + component * 4), &value, sizeof(value));
    }
    void ExactVector(const Bank& before, std::uintptr_t image, std::size_t slot, const CVector& value, const char* why)
    {
        Bank expected = before;
        std::memcpy(expected.data() + slot * P::RowBytes + 0x24, &value, sizeof(value));
        Check(WholeBank(image) == expected, why);
    }
    class Owner final : public NativeWeaponInfoWriteAccess, public NativeWeaponInfoVectorAccess
    {
    public:
        const NativeWeaponInfoReader& reader;
        std::uintptr_t                image;
        V::FireOffsetJournal          journal;
        std::size_t                   lastSlot = P::RowCount;
        unsigned                      calls = 0;
        Owner(const NativeWeaponInfoReader& input, std::uintptr_t base) : reader(input), image(base) {}
        ClockResult BorrowWeaponFireOffset(std::size_t slot, CVector*& output) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return V::BorrowFireOffset(reader, image, slot, output, VectorTestHost::Admit);
        }
        ClockResult SetWeaponFireOffset(std::size_t slot, const CVector* source) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return V::SetFireOffset(reader, image, slot, source, VectorTestHost::Admit, &journal);
        }
        ClockResult SetWeaponSlot(std::size_t slot, eWeaponSlot value) noexcept override
        {
            ++calls;
            lastSlot = slot;
            return V::SetSlot(reader, image, slot, value, VectorTestHost::Admit);
        }
    };
    class Refusing final : public NativeWeaponInfoWriteAccess, public NativeWeaponInfoVectorAccess
    {
    };
    static_assert(!std::is_abstract_v<CWeaponInfoDE>);

    void ConcreteSdk(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        Owner    owner(reader, image);
        Refusing refusing;
        for (std::size_t slot = 0; slot != P::RowCount; ++slot)
        {
            CWeaponInfoDE object(reader, owner, owner, slot);
            CWeaponInfo&  sdk = object;
            auto*         alias = sdk.GetFireOffset();
            Check(reinterpret_cast<std::uintptr_t>(alias) == image + P::BankRva + slot * P::RowBytes + 0x24,
                  "actual SDK returns the physical native row alias for every one of eighty slots");
            Check(alias == sdk.GetFireOffset(), "repeated borrowed access keeps native pointer identity");
            const CVector input(static_cast<float>(slot) + 10.125f, -20.5f, 30.875f);
            auto          before = WholeBank(image);
            CVector       mutableInput = input;
            sdk.SetFireOffset(&mutableInput);
            ExactVector(before, image, slot, input, "actual SDK vector setter changes exactly twelve bytes in whole bank");
            Check(owner.journal.committed && owner.journal.appliedMask == 7 && !owner.journal.restoredMask && !owner.journal.remainingMask,
                  "successful XYZ intent persists and has no pending partial cleanup");
            Check(V::RecoverFireOffset(reader, image, slot, owner.journal, VectorTestHost::Admit) == ClockResult::InvalidArgument,
                  "recovery can never undo a successfully committed vector intent");
            Check(alias->fX == input.fX && alias->fY == input.fY && alias->fZ == input.fZ, "previous borrowed alias observes later SDK setter");
            // Classic MTA returns a mutable native pointer. Prove its ordinary
            // alias semantics rather than supplying a writable cached vector.
            before = WholeBank(image);
            alias->fY = 41.25f;
            ExactVector(before, image, slot, CVector(input.fX, 41.25f, input.fZ), "borrowed mutation affects native bank and nothing adjacent");
            before = WholeBank(image);
            sdk.SetFireOffset(alias);
            Check(WholeBank(image) == before && owner.journal.committed && !owner.journal.appliedMask,
                  "self-alias vector setter snapshots source and needs no destructive write");
            for (std::uint32_t value = 0; value != 13; ++value)
            {
                before = WholeBank(image);
                sdk.SetSlot(static_cast<eWeaponSlot>(value));
                Bank expected = before;
                std::memcpy(expected.data() + slot * P::RowBytes + 0x14, &value, sizeof(value));
                Check(WholeBank(image) == expected && sdk.GetSlot() == static_cast<eWeaponSlot>(value),
                      "all thirteen native slots preserve exact DWORD and adjacent fields");
            }
            Check(owner.lastSlot == slot, "concrete SDK preserves its selected native row");
        }
        CWeaponInfoDE disabled(reader, refusing, refusing, 0);
        CVector       source(1, 2, 3);
        Refuses([&] { disabled.GetFireOffset(); }, "default owner refuses SDK borrowed-pointer access");
        Refuses([&] { disabled.SetFireOffset(&source); }, "default owner refuses SDK vector writes");
        Refuses([&] { disabled.SetSlot(WEAPONSLOT_TYPE_MELEE); }, "default owner refuses SDK native slot writes");
        CWeaponInfoDE object(reader, owner, owner, 0);
        auto          calls = owner.calls;
        Refuses([&] { object.SetFireOffset(nullptr); }, "null SDK source rejected");
        Refuses([&] { object.SetSlot(static_cast<eWeaponSlot>(-1)); }, "negative/sentinel slot does not index native ped slots");
        Refuses([&] { object.SetSlot(static_cast<eWeaponSlot>(13)); }, "SDK maximum sentinel is outside actual native bound");
        Check(owner.calls == calls, "malformed SDK arguments are rejected before owner dispatch");
        const auto    before = WholeBank(image);
        const CVector overlappingSource = *reinterpret_cast<const CVector*>(image + P::BankRva + 0x28);
        object.SetFireOffset(reinterpret_cast<CVector*>(image + P::BankRva + 0x28));
        ExactVector(before, image, 0, overlappingSource, "overlapping native source is completely captured before XYZ destination writes");
    }

    void PartialWrites(NativeWeaponInfoReader& reader, std::uintptr_t image)
    {
        constexpr std::size_t slot = 7;
        const CVector         initial(1, 2, 3), target(11, 22, 33);
        V::FireOffsetJournal  journal;
        for (unsigned failure = 1; failure != 4; ++failure)
        {
            VectorTestHost::Reset();
            VectorAt(image, slot, initial);
            const auto before = WholeBank(image);
            VectorTestHost::failAt = failure;
            Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::WriteFailure,
                  "failure at each XYZ CAS is reported");
            Check(WholeBank(image) == before, "partial journal restores only the successfully applied XYZ components");
            Check(!journal.committed && !journal.remainingMask && !journal.foreignMask && journal.appliedMask == journal.restoredMask,
                  "conditional rollback has exact component ownership proof");
        }
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        auto before = WholeBank(image);
        VectorTestHost::before = [&]
        {
            if (VectorTestHost::exchanges == 2)
                ComponentAt(image, slot, 1, 77);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::Busy,
              "foreign second component stops vector intent");
        ExactVector(before, image, slot, CVector(1, 77, 3), "rollback preserves foreign Y while retiring only applied X");
        Check(journal.appliedMask == 1 && journal.restoredMask == 1 && !journal.remainingMask, "unwritten foreign component is never journal-owned");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::before = [&]
        {
            if (VectorTestHost::exchanges == 3)
            {
                ComponentAt(image, slot, 0, 88);
                ComponentAt(image, slot, 2, 99);
            }
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::Busy,
              "foreign Z collision preserves later foreign X");
        ExactVector(before, image, slot, CVector(88, 2, 99), "partial cleanup restores Y but preserves independently replaced X and Z");
        Check(journal.appliedMask == 3 && journal.restoredMask == 2 && journal.foreignMask == 1 && !journal.remainingMask,
              "journal distinguishes foreign X from conditionally restored Y");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::after = []
        {
            if (VectorTestHost::exchanges == 1)
                VectorTestHost::admitted = ClockResult::Stopped;
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::RestoreFailure,
              "retired owner cannot write a second component or its partial cleanup");
        ExactVector(before, image, slot, CVector(11, 2, 3), "owner loss leaves explicit partial intent without forbidden cleanup writes");
        Check(journal.failure == ClockResult::Stopped && journal.appliedMask == 1 && journal.remainingMask == 1 && VectorTestHost::exchanges == 1,
              "pending journal records original owner failure and remaining native component");
        Check(V::RecoverFireOffset(reader, image, slot + 1, journal, VectorTestHost::Admit) == ClockResult::InvalidArgument,
              "pending vector journal cannot be replayed against another weapon row");
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::Stopped && journal.remainingMask == 1,
              "deferred recovery preserves debt while the callback owner remains retired");
        VectorTestHost::Reset();
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::Ok,
              "next admitted callback conditionally retires the exact pending component");
        Check(WholeBank(image) == before && journal.restoredMask == 1 && !journal.remainingMask && journal.failure == ClockResult::Stopped,
              "deferred recovery restores only the incomplete call and keeps its original failure evidence");
        const auto recoveredCalls = VectorTestHost::exchanges;
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::Ok && VectorTestHost::exchanges == recoveredCalls,
              "completed recovery is idempotent without replaying old stores");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::after = [&]
        {
            if (VectorTestHost::exchanges == 3)
                ComponentAt(image, slot, 0, 55);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::WriteFailure,
              "final readback catches foreign replacement after all three writes");
        ExactVector(before, image, slot, CVector(55, 2, 3), "final verification rollback preserves foreign X and retires YZ");
        Check(journal.appliedMask == 7 && journal.foreignMask == 1 && journal.restoredMask == 6 && !journal.remainingMask,
              "final verification has component-level foreign preservation");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::after = []
        {
            if (VectorTestHost::exchanges == 1)
                WeaponTestHost::failAt.store(WeaponTestHost::reads.load() + 1);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::BindingChanged,
              "partial OS qualification copy between components interrupts vector intent");
        Check(WholeBank(image) == before && journal.appliedMask == 1 && journal.restoredMask == 1,
              "transient OS-copy failure retires only the first applied component");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::after = []
        {
            if (VectorTestHost::exchanges == 3)
                WeaponTestHost::failAt.store(WeaponTestHost::reads.load() + 1);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::WriteFailure,
              "final partial OS read cannot accept an unverified vector write");
        Check(WholeBank(image) == before && journal.appliedMask == 7 && journal.restoredMask == 7,
              "transient final readback failure retires all owned XYZ components exactly");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::before = [&]
        {
            if (VectorTestHost::exchanges == 2)
                ComponentAt(image, slot, 1, 77);
            if (VectorTestHost::exchanges == 3)
                ComponentAt(image, slot, 0, 88);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::Busy,
              "rollback CAS detects a new foreign replacement after its own qualified copy");
        ExactVector(before, image, slot, CVector(88, 77, 3), "conditional rollback never erases a racing foreign X");
        Check(journal.appliedMask == 1 && journal.foreignMask == 1 && !journal.restoredMask && !journal.remainingMask,
              "rollback collision records retired ownership without claiming restoration");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::before = []
        {
            if (VectorTestHost::exchanges == 2 || VectorTestHost::exchanges == 3)
                RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::RestoreFailure,
              "failed component and failed conditional cleanup retain explicit pending intent");
        ExactVector(before, image, slot, CVector(11, 2, 3), "failed cleanup is never replaced by an unrestricted vector or row restore");
        Check(journal.failure == ClockResult::WriteFailure && journal.appliedMask == 1 && journal.remainingMask == 1,
              "journal exposes the exact unretired native component after cleanup fault");
        VectorTestHost::Reset();
        DWORD      priorProtection = 0, ignoredProtection = 0;
        const auto rowAddress = image + P::BankRva + slot * P::RowBytes;
        Check(VirtualProtect(reinterpret_cast<void*>(rowAddress), P::RowBytes, PAGE_NOACCESS, &priorProtection) != FALSE,
              "make only owned pending row unreadable");
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::BindingChanged && journal.remainingMask == 1,
              "unreadable pending row retains debt without fabricating restoration");
        Check(VirtualProtect(reinterpret_cast<void*>(rowAddress), P::RowBytes, priorProtection, &ignoredProtection) != FALSE,
              "restore only owned pending row protection");
        ComponentAt(image, slot, 0, 66);
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::OwnerConflict,
              "later foreign supersession retires only that component debt with honest ownership result");
        Check(journal.foreignMask == 1 && !journal.remainingMask && !journal.restoredMask && !VectorTestHost::exchanges,
              "foreign retirement performs no cleanup CAS or false restoration claim");
        ExactVector(before, image, slot, CVector(66, 2, 3), "deferred recovery preserves terminal foreign vector component");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::failAt = 2;
        VectorTestHost::after = []
        {
            if (VectorTestHost::exchanges == 3)
                WeaponTestHost::failAt.store(WeaponTestHost::reads.load() + 1);
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::RestoreFailure && journal.remainingMask == 1,
              "rollback CAS followed by failed readback preserves uncertain debt");
        Check(WholeBank(image) == before, "uncertain rollback may already have restored original bytes");
        VectorTestHost::Reset();
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::Ok,
              "later original-valued observation resolves uncertain rollback without false foreign supersession");
        Check(journal.restoredMask == 1 && !journal.foreignMask && !journal.remainingMask && !VectorTestHost::exchanges,
              "already-original recovery needs no store and keeps truthful restoration result");
        VectorTestHost::Reset();
        VectorAt(image, slot, initial);
        before = WholeBank(image);
        VectorTestHost::after = []
        {
            if (VectorTestHost::exchanges == 2)
                VectorTestHost::admitted = ClockResult::Stopped;
        };
        Check(V::SetFireOffset(reader, image, slot, &target, VectorTestHost::Admit, &journal) == ClockResult::RestoreFailure && journal.remainingMask == 3,
              "two applied components remain explicit when owner retires before Z");
        VectorTestHost::Reset();
        ComponentAt(image, slot, 0, 88);
        VectorTestHost::failAt = 1;
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::RestoreFailure,
              "mixed foreign supersession and faulted pending component remain incomplete");
        Check(journal.foreignMask == 1 && journal.remainingMask == 2 && !journal.restoredMask,
              "mixed recovery exposes both foreign history and still-owned Y debt");
        VectorTestHost::Reset();
        Check(V::RecoverFireOffset(reader, image, slot, journal, VectorTestHost::Admit) == ClockResult::OwnerConflict,
              "final pending recovery reports earlier foreign supersession after debt is retired");
        ExactVector(before, image, slot, CVector(88, 2, 3), "mixed recovery preserves foreign X and retires only owned Y");
        Check(journal.foreignMask == 1 && journal.restoredMask == 2 && !journal.remainingMask,
              "terminal mixed recovery never claims all components were restored");
        VectorTestHost::Reset();
    }

    void RefusalAndLifetime(NativeWeaponInfoReader& reader, OwnedImage& owned)
    {
        const auto    image = owned.image;
        const CVector source(1, 2, 3);
        VectorTestHost::Reset();
        auto     before = WholeBank(image);
        CVector  marker(9, 8, 7);
        CVector* output = &marker;
        Check(V::BorrowFireOffset(reader, image, 80, output) == ClockResult::InvalidArgument && output == &marker,
              "failed borrow preserves caller output and rejects row eighty");
        Check(V::SetFireOffset(reader, image, SIZE_MAX, &source) == ClockResult::InvalidArgument, "huge vector row rejected before address arithmetic");
        Check(V::SetSlot(reader, image, 0, static_cast<eWeaponSlot>(13)) == ClockResult::InvalidArgument,
              "memory helper independently enforces actual thirteen-slot bound");
        Check(V::SetFireOffset(reader, image, 0, nullptr) == ClockResult::InvalidArgument, "memory helper refuses null source");
        Check(V::SetFireOffset(reader, image, 0, reinterpret_cast<const CVector*>(1)) == ClockResult::ReadFailure,
              "OS copy safely refuses unreadable source pointer");
        VectorTestHost::watchedSource = &source;
        VectorTestHost::sourceFailure = true;
        Check(V::SetFireOffset(reader, image, 0, &source) == ClockResult::ReadFailure, "partial caller source copy prevents all writes");
        VectorTestHost::sourceFailure = false;
        VectorTestHost::sourceReads = 0;
        VectorTestHost::unstableSource = true;
        Check(V::SetFireOffset(reader, image, 0, &source) == ClockResult::ReadFailure, "unstable caller vector cannot produce a torn target intent");
        VectorTestHost::Reset();
        Check(V::BorrowFireOffset(reader, image + 4096, 0, output) == ClockResult::BindingChanged && output == &marker,
              "different explicit image cannot return a pointer");
        ClockResult result = ClockResult::Ok;
        std::thread worker([&] { result = V::BorrowFireOffset(reader, image, 0, output); });
        worker.join();
        Check(result == ClockResult::WrongThread && output == &marker, "wrong thread cannot obtain native borrowed alias");
        VectorTestHost::admitted = ClockResult::Expired;
        Check(V::BorrowFireOffset(reader, image, 0, output, VectorTestHost::Admit) == ClockResult::Expired && output == &marker,
              "retired callback cannot publish a pointer");
        Check(V::SetSlot(reader, image, 0, WEAPONSLOT_TYPE_MELEE, VectorTestHost::Admit) == ClockResult::Expired, "retired callback cannot mutate native slot");
        Check(V::SetFireOffset(reader, image, 0, &source, VectorTestHost::Admit) == ClockResult::Expired, "retired callback cannot start XYZ intent");
        Check(VectorTestHost::exchanges == 0 && WholeBank(image) == before, "all argument, caller and admission refusals preserve whole bank");
        VectorTestHost::Reset();
        const auto&         pin = VP::Code.front();
        const unsigned char changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        owned.Patch(pin.rva, &changed, 1);
        Check(V::BorrowFireOffset(reader, image, 0, output) == ClockResult::BindingChanged,
              "changed native slot-consumer provenance prevents concrete object access");
        owned.Patch(pin.rva, pin.bytes.data(), 1);
        DWORD old = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(image + P::BankRva), 4096, PAGE_READONLY, &old) != FALSE, "protect only owned fixture bank page");
        Check(V::BorrowFireOffset(reader, image, 0, output) == ClockResult::BindingChanged, "read-only image cannot grant mutable native alias");
        Check(VirtualProtect(reinterpret_cast<void*>(image + P::BankRva), 4096, old, &ignored) != FALSE, "restore owned fixture page");
        CVector* borrowed = nullptr;
        Check(V::BorrowFireOffset(reader, image, 79, borrowed) == ClockResult::Ok, "qualified physical alias is available before owner reset");
        const auto physical = reinterpret_cast<std::uintptr_t>(borrowed);
        reader.Reset();
        Check(V::BorrowFireOffset(reader, image, 79, output) == ClockResult::NotInitialized && output == &marker, "retired reader refuses new borrowing");
        Check(physical == image + P::BankRva + 79 * P::RowBytes + 0x24 && NativeImageAccess::ImagePages(image, physical, sizeof(CVector), true),
              "reader/wrapper retirement never owns or frees executable image vector storage");
        Check(reader.Bind() == Status::Ok, "same physical main-image bank can be qualified by subsequent generation");
        Check(WholeBank(image) == before, "provenance/page/lifetime refusals never mutate native bank");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    OwnedImage owned;
    owned.Create(argv[1]);
    for (const auto& pin : VP::Code)
    {
        owned.Patch(pin.rva, pin.bytes.data(), pin.size);
        DWORD old = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(owned.image + pin.rva), pin.size, PAGE_EXECUTE_READ, &old) != FALSE,
              "extra native consumer pins live on owned executable image pages");
    }
    WeaponTestHost::image = owned.image;
    NativeWeaponInfoReader reader;
    Check(reader.Bind() == Status::Ok, "actual native reader binds the complete owned weapon bank");
    ConcreteSdk(reader, owned.image);
    PartialWrites(reader, owned.image);
    RefusalAndLifetime(reader, owned);
    std::printf("Passed %u native weapon-vector and concrete SDK checks (64-bit)\n", checks);
    return 0;
}
