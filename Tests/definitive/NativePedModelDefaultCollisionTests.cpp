/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real default-collision reader over owned image and private pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main DefaultCollisionHistoricalSpecialDefinitionsMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativePedModelDefaultCollisionFixture.inl"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedModelDefaultCollision.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace DefaultCollisionTests
{
    namespace Calls = NativePedModelDefaultCollision;
    namespace Profile = NativePedModelMetadataProfile;
    using Fixture = NativePedModelDefaultCollisionFixture::Fixture;
    Calls::Value Sentinel()
    {
        Calls::Value value;
        value.canonical = 0x1231;
        value.data = 0x2341;
        value.spheres = 0x3451;
        value.segments = 0x4561;
        value.dataAllocation = 0x5671;
        value.sphereAllocation = 0x6781;
        value.segmentAllocation = 0x7891;
        value.canonicalBytes.fill(0xa5);
        value.dataBytes.fill(0xb6);
        value.sphereBytes.fill(0xc7);
        value.segmentBytes.fill(0xd8);
        value.boundsLiteral.fill(0xe9);
        value.maxYLiteral.fill(0xfa);
        return value;
    }
    Fixture* changing = nullptr;
    unsigned change = 0;
    void     ChangeSnapshot()
    {
        auto& f = *changing;
        if (change == 0)
        {
            const auto at = f.Canonical() + 0x29;
            const auto byte = static_cast<unsigned char>(*reinterpret_cast<const unsigned char*>(at) ^ 1);
            f.transform.image.Patch(Profile::DefaultCollisionRva + 0x29, &byte, 1, PAGE_READWRITE);
        }
        if (change == 1)
            f.data[0] ^= 1;
        if (change == 2)
            f.spheres[0x13] ^= 1;
        if (change == 3)
            f.segments[0xc] ^= 1;
        if (change == 4)
            f.Global(Profile::DefaultCollisionRva + 0x30, f.Address(f.data + 0x100));
        if (change == 5)
            f.Put(f.data, 8, f.Address(f.spheres + 0x100));
        if (change == 6)
            f.Put(f.data, 0x18, f.Address(f.segments + 0x100));
    }
}
int main(int argc, char** argv)
{
    namespace T = DefaultCollisionTests;
    namespace Calls = T::Calls;
    namespace Profile = T::Profile;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "default reader evidence has an owned workspace directory");
    T::Fixture fixture(argv[1]);
    const auto image = fixture.Image();
    const auto read = [&](Calls::Value& value) { return Calls::Read(image, &PoolTestHost::Owner, value); };
    const auto reset = [&] { fixture.ResetDefaultCollision(); };
    const auto refuses = [&](const char* reason)
    {
        auto value = T::Sentinel();
        Check(read(value) != ClockResult::Ok && Calls::Same(value, T::Sentinel()), reason);
    };
    reset();
    Calls::Value value;
    Check(NativeStreamingCompletionContext::TryEnter(), "hold the existing shared gate before the narrow default borrow");
    {
        NativeStreamingCompletionContext::Scope scope;
        Check(read(value) == ClockResult::Ok && value.canonical == fixture.Canonical() && value.data == fixture.Address(fixture.data) &&
                  value.spheres == fixture.Address(fixture.spheres) && value.segments == fixture.Address(fixture.segments),
              "actual reader accepts only the canonical completed default while its caller already holds the shared gate");
    }
    Check(Calls::Recheck(image, &PoolTestHost::Owner, value) == ClockResult::Ok, "stable immediate default borrow rechecks with the actual reader");
    Check(value.canonicalBytes[0x29] == 0x9b && value.dataBytes[0] == 0xcd && value.sphereBytes[0x13] == 0xcd && value.segmentBytes[0xc] == 0xcd,
          "stable nonzero opaque header and padding bytes are observed without invented zero defaults");
    value.canonical ^= 8;
    Check(Calls::Recheck(image, &PoolTestHost::Owner, value) == ClockResult::BindingChanged,
          "recheck recomputes the canonical address rather than adopting a supplied pointer");
    reset();
    fixture.SeedDefaultCollision(1);
    Check(read(value) == ClockResult::Ok && value.data % 8 == 1 && value.spheres % 8 == 1 && value.segments % 8 == 1,
          "raw RPM and memcpy safely accept an exact valid unaligned seed; no unproved alignment rule is invented");

    // Exercise each independently meaningful written field, including all
    // three record metadata words, instead of assuming nonnull means ready.
    for (const auto offset : {0u, 15u, 16u, 19u, 20u, 24u, 32u, 36u, 40u})
    {
        reset();
        const auto byte = static_cast<unsigned char>(*reinterpret_cast<const unsigned char*>(fixture.Canonical() + offset) ^ 1);
        fixture.transform.image.Patch(Profile::DefaultCollisionRva + offset, &byte, 1, PAGE_READWRITE);
        refuses("each bounding literal, final bound, center, radius and written outer byte is mandatory");
    }
    for (std::size_t row = 0; row != 3; ++row)
        for (const auto offset : {0u, 4u, 8u, 12u, 16u, 17u, 18u})
        {
            reset();
            fixture.spheres[row * 20 + offset] ^= 1;
            refuses("each sphere-shaped center/radius and exact metadata byte is mandatory");
        }
    for (std::size_t row = 0; row != 2; ++row)
        for (const auto offset : {0u, 4u, 8u, 16u, 20u, 24u})
        {
            reset();
            fixture.segments[row * 32 + offset] ^= 1;
            refuses("both segment-shaped start and end vectors retain their exact raw producer effects");
        }
    reset();
    fixture.data[6] = 1;
    refuses("the one proved cleared data-header byte is required without guessing other header fields");
    for (unsigned source = 0; source != 2; ++source)
    {
        reset();
        const auto at = source ? Profile::MaxYLiteralRva : Profile::BoundsLiteralRva;
        const auto byte = static_cast<unsigned char>(*reinterpret_cast<const unsigned char*>(image + at) ^ 1);
        fixture.transform.image.Patch(at, &byte, 1, PAGE_READONLY);
        refuses("outer bounds must match their actual immutable literal sources, not a guessed numeric box");
        reset();
        DWORD previous = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(image + at), 4, PAGE_READWRITE, &previous) != FALSE, "change only an owned literal page protection");
        refuses("writable literal pages cannot supply the immutable-image default comparison");
    }
    for (unsigned pointer = 0; pointer != 3; ++pointer)
        for (const auto bad : {std::uintptr_t{0}, std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)()})
        {
            reset();
            if (pointer == 0)
                fixture.Global(Profile::DefaultCollisionRva + 0x30, bad);
            else
                fixture.Put(fixture.data, pointer == 1 ? 8 : 0x18, bad);
            refuses("missing, malformed and overflowing required pointers refuse without output clobber or unsafe dereference");
        }
    for (unsigned overlap = 0; overlap != 3; ++overlap)
    {
        reset();
        if (overlap == 0)
            fixture.Put(fixture.data, 8, fixture.Address(fixture.data + 1));
        if (overlap == 1)
            fixture.Put(fixture.data, 0x18, fixture.Address(fixture.data + 1));
        if (overlap == 2)
            fixture.Put(fixture.data, 0x18, fixture.Address(fixture.spheres + 1));
        refuses("overlapping data and record spans cannot impersonate distinct initialized payloads");
    }
    reset();
    fixture.Put(fixture.data, 8, fixture.Canonical());
    refuses("an image-owned address cannot replace required private sphere storage");
    for (unsigned page = 0; page != 3; ++page)
        for (const auto protection : {DWORD{PAGE_NOACCESS}, DWORD{PAGE_EXECUTE_READ}})
        {
            reset();
            DWORD previous = 0;
            Check(VirtualProtect(fixture.block + page * 0x1000, 0x1000, protection, &previous) != FALSE, "restrict only an owned private payload page");
            refuses("unreadable or executable private payload pages cannot authorize the default borrow");
        }
    reset();
    DWORD previous = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(fixture.Canonical()), 0x38, PAGE_READONLY, &previous) != FALSE,
          "restrict the canonical owned image data page");
    refuses("canonical storage must retain its proved writable nonexecuting image-data contract");

    for (T::change = 0; T::change != 7; ++T::change)
    {
        reset();
        T::changing = &fixture;
        std::memcpy(fixture.data + 0x100, fixture.data, 0x20);
        std::memcpy(fixture.spheres + 0x100, fixture.spheres, 0x3b);
        std::memcpy(fixture.segments + 0x100, fixture.segments, 0x3c);
        PoolTestHost::watched = T::change == 0 || T::change == 4   ? fixture.Canonical()
                                : T::change == 1 || T::change >= 5 ? fixture.Address(fixture.data)
                                : T::change == 2                   ? fixture.Address(fixture.spheres)
                                                                   : fixture.Address(fixture.segments);
        PoolTestHost::after = &T::ChangeSnapshot;
        refuses("raw opaque bytes or valid pointer relocation between actual read bookends refuses the earlier snapshot");
        Check(PoolTestHost::after == nullptr, "the selected mutation occurred after the real first snapshot read");
    }
    for (const auto& pin : Profile::Code)
    {
        reset();
        const auto byte = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &byte, 1, PAGE_EXECUTE_READ);
        refuses("changed exact causal recipe, getter, consumer or default-effect instruction refuses the borrow");
    }
    reset();
    value = T::Sentinel();
    Check(Calls::Read(image, nullptr, value) == ClockResult::NotInitialized && Calls::Same(value, T::Sentinel()),
          "the default pointer grants no callback owner by itself");
    PoolTestHost::ownerResult = ClockResult::Expired;
    refuses("the existing owner can refuse before any default snapshot is published");
    reset();
    fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
    refuses("the native initialized game state9 remains mandatory");
    reset();
    value = T::Sentinel();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = read(value); });
    worker.join();
    Check(foreign == ClockResult::WrongThread && Calls::Same(value, T::Sentinel()), "actual native thread identity survives a permissive owner seam");
    reset();
    PoolTestHost::image = 0;
    refuses("the current main image must still be the qualified owner image");
    reset();
    Check(read(value) == ClockResult::Ok, "measure only this new reader's checked read and owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (const auto fail : {1u, reads / 2, reads})
    {
        reset();
        PoolTestHost::failAt = fail;
        refuses("bounded first, middle and final read failures preserve the complete caller output");
    }
    for (const auto revoke : {1u, owners / 2, owners})
    {
        reset();
        PoolTestHost::revokeAt = revoke;
        refuses("bounded owner revocation at this reader's bookends cannot publish an earlier borrow");
    }
    std::puts("Production native ped model default collision fixture completed");
    std::printf("Passed %u native ped model default collision checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
