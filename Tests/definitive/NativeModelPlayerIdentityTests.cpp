/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe actual current model collision identity without geometry authority
 ****************************************************************************/
// Reuse only the owned image/bank scaffold. Historical entry points remain
// definitions, so this focused test does not replay unrelated model matrices.
#pragma push_macro("main")
#undef main
#define main NativeModelPlayerIdentityHistoricalObjectMain
#include "NativeModelObjectTests.cpp"
#undef main
#pragma pop_macro("main")

namespace ModelPlayerIdentityFixture
{
    using Reader = mta::game_de::NativeModelObjectReader;
    namespace P = mta::game_de::NativeModelObjectProfile;
    using Fixture = ModelObjectFixture::Fixture;
    Fixture* changing = nullptr;
    unsigned change = 0;
    DWORD    previousProtection = 0;
    bool     protectionChanged = false;

    void Collision(unsigned char* receiver, std::uintptr_t value)
    {
        std::memcpy(receiver + P::CollisionOffset, &value, sizeof(value));
    }
    void Mutate()
    {
        switch (change)
        {
            case 0:
            {
                std::uintptr_t value = 0;
                std::memcpy(&value, changing->object + P::CollisionOffset, sizeof(value));
                // The complete pointer is bookended; matching low DWORDs
                // cannot admit a collision association changed in high bits.
                Collision(changing->object, value ^ (std::uintptr_t{1} << 40));
                break;
            }
            case 1:
                changing->Slot(17, reinterpret_cast<std::uintptr_t>(changing->mapped));
                break;
            case 2:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 3:
                changing->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
                Check(VirtualProtect(changing->object, 4096, PAGE_NOACCESS, &previousProtection) != FALSE,
                      "retire only this fixture's current receiver readability");
                protectionChanged = true;
                break;
            case 5:
            {
                const auto& pin = P::CollisionIdentityCode.front();
                const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
                changing->transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
                break;
            }
            case 6:
                PoolTestHost::failAt = PoolTestHost::reads + 1;
                break;
        }
    }
}

int main(int argc, char** argv)
{
    namespace H = ModelPlayerIdentityFixture;
    using Reader = H::Reader;
    static_assert(sizeof(std::uintptr_t) == 8 && H::P::CollisionOffset == 0x30 && H::P::CollisionPrefixBytes == 0x38);
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "player identity evidence has an explicit workspace-owned directory");
    H::Fixture fixture(argv[1]);
    H::changing = &fixture;
    const Reader::Binding binding{fixture.transform.image.image};
    PoolTestHost::image = binding.image;
    Reader     reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto canonical = binding.image + H::P::CanonicalPedCollisionRva;
    bool       value = false;
    const auto reset = [&]
    {
        fixture.Reset();
        for (const auto& pin : H::P::CollisionIdentityCode)
            fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        H::Collision(fixture.object, canonical);
        H::Collision(fixture.mapped, canonical);
        value = false;
    };
    reset();
    Check(Reader::RevalidatePlayerIdentity(binding), "saved common collision consumer and canonical IDE assignment qualify independently");
    value = true;
    Check(unowned.ReadPlayerModel(17, value) == ClockResult::NotInitialized && value,
          "identity bytes cannot manufacture callback ownership or replace a refused output");

    for (const auto key : {std::uint32_t{0}, std::uint32_t{17}, B::ModelCount - 1})
    {
        reset();
        fixture.Slot(key, 0);
        value = true;
        Check(reader.ReadPlayerModel(key, value) == ClockResult::Ok && !value, "current absent model is a genuine false identity result");
    }
    for (const auto tag : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0x1231}, (std::numeric_limits<std::uintptr_t>::max)(),
                           canonical ^ (std::uintptr_t{1} << 40), canonical + 1})
    {
        reset();
        H::Collision(fixture.object, tag);
        value = true;
        Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && !value,
              "null and arbitrary noncanonical pointer bits mean false without target reads, alignment or pointee predicates");
    }

    // IsPlayerModel is literal identity, not loaded/geometry/type validation.
    // An inaccessible canonical target makes accidental pointee validation
    // observable while the receiver's current field remains fully readable.
    reset();
    DWORD canonicalPrior = 0, ignored = 0;
    auto* canonicalPage = reinterpret_cast<void*>(canonical & ~std::uintptr_t{0xfff});
    Check(VirtualProtect(canonicalPage, 4096, PAGE_NOACCESS, &canonicalPrior) != FALSE, "make only the owned canonical target inaccessible");
    Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value,
          "canonical identity remains true when collision geometry is uninitialized or inaccessible");
    Check(VirtualProtect(canonicalPage, 4096, canonicalPrior, &ignored) != FALSE, "restore only the owned canonical target protection");

    for (const auto vptr : {std::uintptr_t{0}, std::uintptr_t{1}, (std::numeric_limits<std::uintptr_t>::max)()})
    {
        reset();
        std::memcpy(fixture.object, &vptr, sizeof(vptr));
        fixture.object[0x20] = 0xff;
        Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value,
              "nonped-looking or unsupported type/vptr bytes do not gate a canonical collision identity");
        H::Collision(fixture.object, 1);
        value = true;
        Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && !value,
              "nonped-looking receiver bytes likewise do not replace a genuine noncanonical false result");
    }
    reset();
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(fixture.mapped));
    Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value, "readable mapped prefix is sufficient for current collision identity");
    reset();
    std::array<unsigned char, H::P::CollisionPrefixBytes> imageReceiver{};
    imageReceiver.fill(0xff);
    std::memcpy(imageReceiver.data() + H::P::CollisionOffset, &canonical, sizeof(canonical));
    fixture.transform.image.Patch(H::Fixture::ImageReceiverRva, imageReceiver.data(), imageReceiver.size(), PAGE_READONLY);
    fixture.Slot(17, binding.image + H::Fixture::ImageReceiverRva);
    Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value, "static image-backed receiver prefix does not acquire allocation ownership");

    // The identity field ends at byte 56, before the independent render field.
    // Exact boundary success prevents an accidental render/type layout gate.
    auto* boundary = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(boundary != nullptr, "allocate only owned receiver boundary pages");
    DWORD boundaryPrior = 0;
    Check(VirtualProtect(boundary + 4096, 4096, PAGE_NOACCESS, &boundaryPrior) != FALSE, "guard only the next owned boundary page");
    reset();
    auto* exact = boundary + 4096 - H::P::CollisionPrefixBytes;
    std::memset(exact, 0xcc, H::P::CollisionPrefixBytes);
    H::Collision(exact, canonical);
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(exact));
    Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value, "exact 56-byte readable prefix succeeds without accessing render or later fields");
    reset();
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(boundary + 4096 - 48));
    value = true;
    Check(reader.ReadPlayerModel(17, value) != ClockResult::Ok && value, "short 48-byte receiver refuses without consuming output");
    Check(VirtualFree(boundary, 0, MEM_RELEASE) != FALSE, "retire only owned receiver boundary pages");

    for (const auto protection : {DWORD{PAGE_NOACCESS}, DWORD{PAGE_EXECUTE_READ}, DWORD{PAGE_READWRITE | PAGE_GUARD}})
    {
        reset();
        DWORD prior = 0;
        Check(VirtualProtect(fixture.object, 4096, protection, &prior) != FALSE, "change only owned receiver-page protection");
        value = true;
        Check(reader.ReadPlayerModel(17, value) != ClockResult::Ok && value, "inaccessible, executable or guarded receiver prefix refuses without output");
        Check(VirtualProtect(fixture.object, 4096, prior, &ignored) != FALSE, "restore only owned receiver-page protection");
    }
    for (const auto model : {B::ModelCount, B::ModelCount + 1, std::uint32_t{65535}, std::uint32_t{65536}, std::uint32_t{0xffffffff}})
    {
        reset();
        value = true;
        Check(reader.ReadPlayerModel(model, value) == ClockResult::InvalidArgument && value && !PoolTestHost::reads && !PoolTestHost::ownerCalls,
              "full unsigned key refuses out-of-bank aliases before callback or OS reads");
    }
    for (H::change = 0; H::change != 7; ++H::change)
    {
        reset();
        PoolTestHost::watched = fixture.Address() + H::P::CollisionOffset;
        PoolTestHost::after = &H::Mutate;
        Check(reader.ReadPlayerModel(17, value) != ClockResult::Ok && !value,
              "field/bank/profile/current-receiver mutation or callback/GC/read loss cannot publish mixed true identity");
        if (H::protectionChanged)
        {
            Check(VirtualProtect(fixture.object, 4096, H::previousProtection, &ignored) != FALSE, "restore only retired owned receiver readability");
            H::protectionChanged = false;
        }
    }
    for (const auto& pin : H::P::CollisionIdentityCode)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        value = true;
        Check(reader.ReadPlayerModel(17, value) != ClockResult::Ok && value, "every saved identity producer/consumer pin remains required");
    }
    reset();
    const auto badRender = static_cast<unsigned char>(H::P::RenderConsumer[0] ^ 1);
    fixture.transform.image.Patch(H::P::RenderConsumerRva, &badRender, 1, PAGE_EXECUTE_READ);
    Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value, "independent render consumer does not gate collision identity");
    fixture.transform.image.Patch(H::P::RenderConsumerRva, H::P::RenderConsumer.data(), 1, PAGE_EXECUTE_READ);
    reset();
    const auto& identityPin = H::P::CollisionIdentityCode.front();
    const auto  badIdentity = static_cast<unsigned char>(identityPin.bytes[0] ^ 1);
    fixture.transform.image.Patch(identityPin.rva, &badIdentity, 1, PAGE_EXECUTE_READ);
    std::uintptr_t render = 0;
    Check(reader.ReadRender(17, render) == ClockResult::Ok && render == 0x123456789abcdef0,
          "new identity evidence does not constrain the existing independent render operation");
    std::uintptr_t interfaceTag = 0;
    Check(reader.ReadInterface(17, interfaceTag) == ClockResult::Ok && interfaceTag == fixture.Address(),
          "new identity evidence does not constrain the existing opaque interface observation");

    reset();
    Check(reader.ReadPlayerModel(17, value) == ClockResult::Ok && value, "measure actual identity OS-read and callback boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned failed = 1; failed <= reads; ++failed)
    {
        reset();
        value = true;
        PoolTestHost::failAt = failed;
        Check(reader.ReadPlayerModel(17, value) != ClockResult::Ok && value, "every actual failed OS read preserves the caller's true value");
    }
    for (unsigned revoked = 1; revoked <= owners; ++revoked)
    {
        reset();
        PoolTestHost::revokeAt = revoked;
        Check(reader.ReadPlayerModel(17, value) == ClockResult::WrongThread && !value, "each actual callback boundary may revoke identity observation");
    }
    for (unsigned failure = 0; failure != 4; ++failure)
    {
        reset();
        switch (failure)
        {
            case 0:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 1:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 2:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 3:
                fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
                break;
        }
        value = true;
        Check(reader.ReadPlayerModel(17, value) != ClockResult::Ok && value, "owner/GC/gameplay/thread-init refusal preserves identity output");
    }
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.ReadPlayerModel(17, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && !value, "a foreign thread cannot adopt the native game-thread identity authority");
    std::puts("Production native model player identity fixture completed");
    std::printf("Passed %u native model player identity checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
