/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh static32 allocation and actual deleting-destructor receipts
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main FreshStaticArrayHistoricalDefinitionsMain
#include "NativeCustomAnimSequenceStorageTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeStaticAssociationArrayStorageCalls.h"
#include "../../Client/game_de/NativeStaticAssociationArrayProfile.h"
namespace FreshStaticArrayTests
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeStaticAssociationArrayStorageCalls;
    namespace Profile = NativeStaticAssociationArrayProfile;
    using Storage = NativeCustomAnimSequenceStorage;
    constexpr std::uint64_t   Generation = 31;
    std::uintptr_t            image = 0;
    unsigned                  destructorCalls = 0, mode = 0;
    Host::Calls::FreeFunction originalFree = nullptr;
    bool                      freeFailed = false;
    template <class T>
    T Load(std::uintptr_t address) noexcept
    {
        T value;
        std::memcpy(&value, reinterpret_cast<void*>(address), sizeof(value));
        return value;
    }
    template <class T>
    void Store(std::uintptr_t address, T value) noexcept
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    void Release(std::uintptr_t heap, std::uintptr_t base) noexcept
    {
        std::uint32_t exception = 0;
        if (!Host::active || Host::active->Free(originalFree, heap, base, exception) != ClockResult::Ok)
            freeFailed = true;
    }
    ClockResult Destroy(Calls::DeletingDestructorFunction function, std::uintptr_t rows, std::uint32_t flags, std::uintptr_t& returned,
                        std::uint32_t& exception) noexcept
    {
        ++destructorCalls;
        Check(reinterpret_cast<std::uintptr_t>(function) == image + Profile::DeletingDestructorRva && flags == 3 && Host::active &&
                  Host::active->Contains(rows - 8),
              "retirement invokes exactly the saved static array deleting destructor over its actual issued cookie base");
        const auto heap = Load<std::uintptr_t>(image + NativeModelReferenceTransactionProfile::HeapRva);
        const auto table = Load<std::uintptr_t>(heap), entry = Load<std::uintptr_t>(table + 0x30);
        originalFree = reinterpret_cast<Host::Calls::FreeFunction>(entry);
        DWORD protection = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(table + 0x30), 8, PAGE_READWRITE, &protection) != FALSE,
              "route only the owned fixture's exact virtual free cell for the copied native body");
        Store(table + 0x30, reinterpret_cast<std::uintptr_t>(&Release));
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::NativeStaticArrayDestructor(function, rows, flags, returned, exception);
        Store(table + 0x30, entry);
        DWORD ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(table + 0x30), 8, protection, &ignored) != FALSE && !freeFailed,
              "restore exact allocator slot and page protection before production postqualification");
        if (result != ClockResult::Ok)
            return result;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 2)
            PoolTestHost::ownerResult = ClockResult::OwnerConflict;
        return result;
    }
    void LoseMallocOwner(Host::State&) noexcept
    {
        PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
    void LoseFreeOwner(Host::State&) noexcept
    {
        PoolTestHost::ownerResult = ClockResult::OwnerConflict;
    }
}
int main(int argc, char** argv)
{
    using namespace FreshStaticArrayTests;
    if (argc != 2)
        return 2;
    SpecialFixture fixture(argv[1]);
    Host::State    host;
    Host::active = &host;
    image = fixture.transform.image.image;
    const auto reset = [&](bool proxy = false)
    {
        fixture.Reset();
        fixture.SeedHeap(proxy);
        PoolTestHost::image = image;
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        SpecialFixture::Patch(&fixture, Profile::ConstructorRecipeRva, Profile::ConstructorRecipeBytes.data(), Profile::ConstructorRecipeBytes.size(),
                              PAGE_EXECUTE_READ);
        SpecialFixture::Patch(&fixture, Profile::DeletingDestructorRva, Profile::DeletingDestructorBytes.data(), Profile::DeletingDestructorBytes.size(),
                              PAGE_EXECUTE_READ);
        const auto entry = image + Profile::DeletingDestructorRva;
        SpecialFixture::Patch(&fixture, Profile::VtableRva, &entry, 8, PAGE_READONLY);
        mode = 0;
        freeFailed = false;
    };
    Calls::Receipt receipt;
    reset();
    Storage invalid;
    Check(Calls::AllocateFresh(image, invalid, Generation, 0xffffffffu, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument && !receipt.entered &&
              !host.mallocCalls && invalid.CanDiscard(),
          "full unsigned count refuses before native signed-count overflow, never narrowing or saturating");
    for (bool proxy : {false, true})
        for (std::uint32_t count : {0u, 1u, 3u})
        {
            reset(proxy);
            Storage storage;
            Check(Calls::AllocateFresh(image, storage, Generation, count, &PoolTestHost::Owner, &receipt) == ClockResult::Ok && receipt.entered &&
                      receipt.returned && receipt.allocated && receipt.initialized && receipt.verified && receipt.capacity == count &&
                      receipt.bytes == 8 + 32 * count && receipt.readyTag == receipt.base + 8 && storage.Capacity() == count &&
                      storage.GetKind() == Storage::Kind::StaticAnimationAssociationArray && !storage.CanDiscard() && !storage.CanFreeStorage() &&
                      host.mallocCalls == 1 && host.alignment == 0 && host.bytes == 8 + 32 * count,
                  "one actual current allocator seam issues cookie8/static32 rather than relabelling sequence80 or dynamic104 storage");
            Check(Load<std::uint64_t>(receipt.base) == count, "actual native allocation receives the exact full64 cookie even when capacity is zero");
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Calls::FreshRow row;
                Check(Calls::ReadFreshRow(image, storage, Generation, i, &PoolTestHost::Owner, row) == ClockResult::Ok &&
                          row.locator == storage.IssuedTag() + i * 32 && Load<std::uint16_t>(row.locator + 8) == 0xa5a5 &&
                          Load<std::uint32_t>(row.locator + 0xa) == 0xffffffff && !Load<std::uint16_t>(row.locator + 0xe) &&
                          !Load<std::uintptr_t>(row.locator + 0x10) && !Load<std::uintptr_t>(row.locator + 0x18),
                      "exact inline construction preserves undefined WORD8 while defining IDs, flags, nodes and hierarchy");
            }
            Calls::FreshRow untouched;
            untouched.locator = 0x1234;
            Check(Calls::ReadFreshRow(image, storage, Generation, count, &PoolTestHost::Owner, untouched) == ClockResult::InvalidArgument &&
                      untouched.locator == 0x1234,
                  "physical row read bounds preserve caller output and do not invent rowzero for a zero-cookie array");
            const auto calls = destructorCalls;
            fixture.SeedHeap(!proxy);
            Check(Calls::RetireFresh(image, storage, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::Ok && receipt.released &&
                      receipt.verified && !receipt.debt && storage.CanDiscard() && host.freeCalls == 1 && destructorCalls == calls + (count ? 1u : 0u),
                  "fresh positive arrays use genuine reverse deleting destructor; zero-cookie uses original direct free branch");
            Check(Calls::RetireFresh(image, storage, Generation, &PoolTestHost::Owner, &receipt, &Destroy) != ClockResult::Ok && !receipt.entered &&
                      host.freeCalls == 1,
                  "acknowledged retirement never replays even if the released address remains in a diagnostic receipt");
        }
    reset();
    Storage cookieOwner;
    Check(Calls::AllocateFresh(image, cookieOwner, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare exact cookie and generation authority checks");
    Check(Calls::RetireFresh(image, cookieOwner, Generation + 1, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::OwnerConflict && !receipt.entered &&
              !host.freeCalls,
          "another generation cannot retire a current issued array");
    Store(cookieOwner.IssuedTag() - 8, std::uint64_t{2});
    Check(Calls::RetireFresh(image, cookieOwner, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::BindingChanged && !receipt.entered &&
              !host.freeCalls,
          "a rewritten cookie cannot select a larger reverse destructor walk");
    Store(cookieOwner.IssuedTag() - 8, std::uint64_t{1});
    Check(Calls::RetireFresh(image, cookieOwner, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::Ok,
          "a proved unentered refusal leaves the original private fresh resource available to its owner");
    for (std::size_t field : {std::size_t{0}, std::size_t{0xa}, std::size_t{0xc}, std::size_t{0xe}, std::size_t{0x10}, std::size_t{0x18}})
    {
        reset();
        Storage storage;
        Check(Calls::AllocateFresh(image, storage, Generation, 2, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
              "prepare one current private fresh array for role refusal");
        const auto address = storage.IssuedTag() + 32 + field;
        const auto before = Load<unsigned char>(address);
        Store(address, static_cast<unsigned char>(before ^ 1));
        Calls::FreshRow row;
        row.locator = 0x1234;
        Check(Calls::ReadFreshRow(image, storage, Generation, 0, &PoolTestHost::Owner, row) == ClockResult::BindingChanged && row.locator == 0x1234 &&
                  Calls::RetireFresh(image, storage, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::BindingChanged && !receipt.entered &&
                  !host.freeCalls,
              "changed constructor-defined fields or node/H pointers cannot authorize fresh-only destructor or arbitrary free");
        Store(address, before);
        Store(storage.IssuedTag() + 8, std::uint16_t{0xdead});
        Check(Calls::RetireFresh(image, storage, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::Ok,
              "opaque countWORD8 is never mistaken for initialization, native node capacity or destructor authority");
    }
    reset();
    Storage foreign;
    Check(NativeCustomAnimSequenceStorageCalls::Allocate(image, foreign, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare a genuine differently typed existing sequence allocation");
    Check(Calls::RetireFresh(image, foreign, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::OwnerConflict && !receipt.entered,
          "static destructor cannot adopt an existing sequence80 allocation through a matching byte-size or raw tag");
    Check(NativeCustomAnimSequenceStorageCalls::FreeStorage(image, foreign, Generation, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "the foreign allocation remains owned only by its original typed free path");
    reset();
    Storage nullable;
    host.nullMalloc = true;
    Check(Calls::AllocateFresh(image, nullable, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::WriteFailure && receipt.entered &&
              receipt.returned && !receipt.allocated && nullable.CanDiscard() && !receipt.debt,
          "acknowledged null allocation owns no static array and does not invent constructor success");
    reset();
    Storage postLoss;
    host.afterMalloc = &LoseMallocOwner;
    Check(Calls::AllocateFresh(image, postLoss, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::OwnerConflict && receipt.allocated &&
              receipt.initialized && postLoss.HasOutstandingResource() && !postLoss.CanFreeStorage(),
          "actual returned private allocation is recorded before a failed final owner acknowledgement");
    PoolTestHost::ownerResult = ClockResult::Ok;
    host.afterMalloc = nullptr;
    Check(Calls::RetireFresh(image, postLoss, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::Ok,
          "a known initialized unpublished allocation can later retire through its exact typed owner");
    reset();
    Storage unknown;
    host.mallocResult = ClockResult::OriginalException;
    Check(Calls::AllocateFresh(image, unknown, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::OriginalException && receipt.entered &&
              !receipt.returned && unknown.HasOutstandingResource() && !unknown.CanDiscard(),
          "missing allocator acknowledgement keeps entered debt without adopting a readable raw result");
    const auto allocations = host.mallocCalls;
    Check(Calls::AllocateFresh(image, unknown, Generation, 1, &PoolTestHost::Owner, &receipt) != ClockResult::Ok &&
              Calls::RetireFresh(image, unknown, Generation, &PoolTestHost::Owner, &receipt, &Destroy) != ClockResult::Ok && host.mallocCalls == allocations &&
              !host.freeCalls,
          "unknown allocation never retries or manually frees a guessed base");
    reset();
    Storage retiredLoss;
    Check(Calls::AllocateFresh(image, retiredLoss, Generation, 0, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare zero-cookie direct retirement acknowledgement test");
    host.afterFree = &LoseFreeOwner;
    Check(Calls::RetireFresh(image, retiredLoss, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::OwnerConflict && receipt.released &&
              retiredLoss.CanDiscard() && !receipt.debt,
          "known direct-free consumption precedes failed owner postcheck");
    reset();
    Storage lostDestructor;
    Check(Calls::AllocateFresh(image, lostDestructor, Generation, 2, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare exact copied destructor lost-return test");
    mode = 1;
    Check(Calls::RetireFresh(image, lostDestructor, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::OriginalException &&
              receipt.entered && !receipt.returned && lostDestructor.HasOutstandingResource() && !lostDestructor.CanDiscard() && host.freeCalls == 1,
          "missing destructor return acknowledgement remains retained even after injected actual native free");
    const auto frees = host.freeCalls, destructors = destructorCalls;
    Check(Calls::RetireFresh(image, lostDestructor, Generation, &PoolTestHost::Owner, &receipt, &Destroy) != ClockResult::Ok && !receipt.entered &&
              host.freeCalls == frees && destructorCalls == destructors,
          "unknown deleting destructor is never replayed or repaired by a second native free");
    reset();
    Storage knownDestructor;
    Check(Calls::AllocateFresh(image, knownDestructor, Generation, 1, &PoolTestHost::Owner, &receipt) == ClockResult::Ok,
          "prepare known deleting destructor post-owner loss");
    mode = 2;
    Check(Calls::RetireFresh(image, knownDestructor, Generation, &PoolTestHost::Owner, &receipt, &Destroy) == ClockResult::OwnerConflict && receipt.returned &&
              receipt.released && knownDestructor.CanDiscard() && !receipt.debt,
          "genuine returned deleting destructor consumption is saved before final owner failure");
    Host::active = nullptr;
    Check(destructorCalls > 0, "focused fixture executed the whole saved native deleting destructor");
    std::printf("Passed %u fresh static association array checks (64-bit)\n", checks);
    std::puts("Production fresh static association array primitive fixture completed");
    std::puts("Production original static association array deleting destructor executed");
    return 0;
}
