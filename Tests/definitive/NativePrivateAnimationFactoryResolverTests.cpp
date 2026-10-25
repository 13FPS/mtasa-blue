/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Original allocation extent and strictly private current facade tokens
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalFactoryResolverDependencyMain
#include "NativePreparedArrayDependencyTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeAnimationSequenceFactoryCalls.h"
namespace PrivateFactoryResolverHost
{
    using Rows = NativeCustomAnimSequenceController;
    using Hierarchies = NativePrivateAnimationHierarchyController;
    using RowToken = NativeAnimationSequenceReferenceToken;
    using HierarchyToken = NativeAnimationHierarchyReferenceToken;
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    constexpr std::uint64_t Generation = 0xeeca02468ace1357ull;
    unsigned                resolverChecks = 0, childReads = 0;
    std::uintptr_t          cookieAddress = 0;
    std::uint64_t*          callerGeneration = nullptr;
    std::uintptr_t*         callerLocator = nullptr;
    std::uint32_t*          callerIndex = nullptr;
    void                    Verify(bool condition, const char* why)
    {
        ++resolverChecks;
        Check(condition, why);
    }
    ClockResult Owner() noexcept
    {
        const auto result = PoolTestHost::Owner();
        if (callerGeneration)
        {
            *callerGeneration = 0;
            callerGeneration = nullptr;
        }
        if (callerLocator)
        {
            *callerLocator = 0;
            callerLocator = nullptr;
        }
        if (callerIndex)
        {
            *callerIndex = ~std::uint32_t{0};
            callerIndex = nullptr;
        }
        return result;
    }
    bool Same(const RowToken& a, const RowToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.firstTag == b.firstTag && a.index == b.index;
    }
    RowToken Sentinel() noexcept
    {
        return {0x1111, 0x2222, 0x3333};
    }
    void ChildRead()
    {
        ++childReads;
    }
    void ChangeCookie()
    {
        auto* cookie = reinterpret_cast<std::uint64_t*>(cookieAddress);
        ++*cookie;
    }
}
int main(int argc, char** argv)
{
    using namespace PrivateFactoryResolverHost;
    Check(argc == 2, "factory resolver owns its evidence directory");
    SpecialFixture fixture(argv[1]);
    Heap::State    host;
    Heap::active = &host;
    fixture.Reset();
    const auto image = fixture.transform.image.image;
    PoolTestHost::image = image;
    host.Reset(image, &SpecialFixture::Patch, &fixture);
    PreparedDependencyHost::Install(fixture, image);
    PoolTestHost::ownerResult = ClockResult::Ok;
    PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
    PoolTestHost::watched = 0;
    PoolTestHost::after = nullptr;
    Rows                  rows;
    std::uintptr_t        array = 0;
    constexpr std::size_t capacity = 65537;
    Check(rows.Allocate(image, Generation, capacity, &Owner, array) == ClockResult::Ok, "issue one full-capacity native array record");
    RowToken      output = Sentinel();
    const auto    sentinel = output;
    bool          present = false;
    auto          generation = Generation;
    auto          locator = array;
    std::uint32_t index = 65536;
    callerGeneration = &generation;
    callerLocator = &locator;
    callerIndex = &index;
    Verify(rows.ResolveRowAt(image, generation, locator, index, &Owner, present, output) == ClockResult::Ok && present &&
               output.ownerGeneration == Generation && output.firstTag == array && output.index == 65536 && !generation && !locator &&
               index == ~std::uint32_t{0},
           "copy complete scalar intent before callback; full DWORD index remains independent of WORD");
    // Capacity lookup is deliberately independent from all child metadata.
    const std::uint16_t  garbage = 0xffff;
    const std::uintptr_t foreign = 1;
    std::memcpy(reinterpret_cast<void*>(array + 0x42), &garbage, sizeof(garbage));
    std::memcpy(reinterpret_cast<void*>(array + 0x44), &garbage, sizeof(garbage));
    std::memcpy(reinterpret_cast<void*>(array + 0x48), &foreign, sizeof(foreign));
    PoolTestHost::watched = array + 0x40;
    PoolTestHost::after = &ChildRead;
    output = sentinel;
    present = true;
    Verify(rows.ResolveRowAt(image, Generation, array, 0xffffffffu, &Owner, present, output) == ClockResult::Ok && !present && Same(output, sentinel) &&
               !childReads && PoolTestHost::after == &ChildRead,
           "qualified OOB absence reads no row0 flags/count/frame and does not fabricate a token");
    output = sentinel;
    present = false;
    Verify(rows.ResolveRowAt(image, Generation, array, 0, &Owner, present, output) == ClockResult::Ok && present && output.index == 0 && !childReads,
           "a current row locator does not adopt or dereference its unrelated frame pointer");
    PoolTestHost::after = nullptr;
    PoolTestHost::watched = 0;
    for (const auto badArray : {array + 80u, image + NativeAnimationBlockHierarchyProfile::BankRva, std::uintptr_t{0}})
    {
        output = sentinel;
        present = true;
        Verify(rows.ResolveRowAt(image, Generation, badArray, 0, &Owner, present, output) != ClockResult::Ok && present && Same(output, sentinel),
               "interior/foreign/null direct owner lookup is refusal, never OOB absence");
    }
    output = sentinel;
    present = true;
    Verify(rows.ResolveRowAt(image, Generation + 1, array, 0, &Owner, present, output) == ClockResult::OwnerConflict && present && Same(output, sentinel),
           "foreign generation preserves both output channels");
    // One concrete cookie transition and first/last distinct observation
    // faults cover this new primitive without sweeping old nested readers.
    cookieAddress = array - 8;
    PoolTestHost::watched = cookieAddress;
    PoolTestHost::after = &ChangeCookie;
    output = sentinel;
    present = true;
    Verify(
        rows.ResolveRowAt(image, Generation, array, 0xffffffffu, &Owner, present, output) == ClockResult::BindingChanged && present && Same(output, sentinel),
        "cookie change between original-allocation bookends is not successful absence");
    const std::uint64_t restored = capacity;
    std::memcpy(reinterpret_cast<void*>(cookieAddress), &restored, sizeof(restored));
    PoolTestHost::watched = 0;
    PoolTestHost::after = nullptr;
    PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    Check(rows.ResolveRowAt(image, Generation, array, 1, &Owner, present, output) == ClockResult::Ok, "measure only current resolver observation boundaries");
    const auto readCount = PoolTestHost::reads, ownerCount = PoolTestHost::ownerCalls;
    Verify(readCount > 1 && ownerCount > 1, "real capacity lookup uses native and owner bookends");
    for (const auto failure : {1u, readCount})
    {
        PoolTestHost::reads = 0;
        PoolTestHost::failAt = failure;
        output = sentinel;
        present = true;
        Verify(rows.ResolveRowAt(image, Generation, array, 0xffffffffu, &Owner, present, output) != ClockResult::Ok && present && Same(output, sentinel),
               "first/last native observation failure preserves token and presence");
    }
    PoolTestHost::failAt = 0;
    PoolTestHost::ownerCalls = 0;
    PoolTestHost::revokeAt = ownerCount;
    output = sentinel;
    present = true;
    Verify(rows.ResolveRowAt(image, Generation, array, 0xffffffffu, &Owner, present, output) == ClockResult::WrongThread && present && Same(output, sentinel),
           "final owner refusal cannot publish an otherwise qualified absent row");
    PoolTestHost::revokeAt = 0;
    Verify(rows.FreeStorage(image, Generation, array, &Owner) == ClockResult::Ok && !rows.IsRetained(),
           "only explicit original storage free consumes the actual record");
    output = sentinel;
    present = true;
    Verify(rows.ResolveRowAt(image, Generation, array, 0, &Owner, present, output) != ClockResult::Ok && present && Same(output, sentinel),
           "retired original tag cannot manufacture a current row token");
    Hierarchies    hierarchies;
    HierarchyToken token;
    Check(hierarchies.Create(image, Generation, &Owner, token) == ClockResult::Ok, "issue actual private CPU hierarchy record");
    HierarchyToken found{9, 10};
    generation = Generation;
    locator = token.locator;
    callerGeneration = &generation;
    callerLocator = &locator;
    Verify(hierarchies.ResolvePrivate(image, generation, locator, &Owner, found) == ClockResult::Ok && found.ownerGeneration == Generation &&
               found.locator == token.locator && !generation && !locator,
           "private H resolver copies intent and accepts undefined hash without inventing one");
    PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    Check(hierarchies.ResolvePrivate(image, Generation, token.locator, &Owner, found) == ClockResult::Ok, "measure private H resolver bookends once");
    const auto hReads = PoolTestHost::reads, hOwners = PoolTestHost::ownerCalls;
    for (const auto failure : {1u, hReads})
    {
        HierarchyToken refused{0xab, 0xcd};
        PoolTestHost::reads = 0;
        PoolTestHost::failAt = failure;
        Verify(hierarchies.ResolvePrivate(image, Generation, token.locator, &Owner, refused) != ClockResult::Ok && refused.ownerGeneration == 0xab &&
                   refused.locator == 0xcd,
               "first/last private H observation failure publishes no partial token");
    }
    PoolTestHost::failAt = 0;
    PoolTestHost::ownerCalls = 0;
    PoolTestHost::revokeAt = hOwners;
    HierarchyToken refused{0xab, 0xcd};
    Verify(hierarchies.ResolvePrivate(image, Generation, token.locator, &Owner, refused) == ClockResult::WrongThread && refused.ownerGeneration == 0xab &&
               refused.locator == 0xcd,
           "private H final owner bookend preserves both token fields");
    PoolTestHost::revokeAt = 0;
    const auto original = found;
    Verify(hierarchies.ResolvePrivate(image, Generation, image + NativeAnimationBlockHierarchyProfile::BankRva, &Owner, found) == ClockResult::BindingChanged &&
               found.ownerGeneration == original.ownerGeneration && found.locator == original.locator,
           "an image-bank locator is never a custom private hierarchy");
    PoolTestHost::ownerResult = ClockResult::WrongThread;
    Verify(hierarchies.ResolvePrivate(image, Generation, token.locator, &Owner, found) == ClockResult::WrongThread && found.locator == original.locator,
           "private H owner refusal preserves its output token");
    PoolTestHost::ownerResult = ClockResult::Ok;
    Verify(hierarchies.Destroy(image, token, &Owner) == ClockResult::Ok && !hierarchies.IsRetained(),
           "actual CPU retirement remains explicit and separate from facade lookup");
    Verify(hierarchies.ResolvePrivate(image, Generation, token.locator, &Owner, found) == ClockResult::BindingChanged && found.locator == original.locator,
           "retired private H lookup refuses without generic bank fallback");
    std::cout << "Passed " << resolverChecks << " private animation factory resolver checks (64-bit; no engine code executed)\n";
    std::cout << "Production private animation factory resolver fixture completed\n";
    return 0;
}
