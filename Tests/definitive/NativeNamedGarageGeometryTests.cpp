/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Coherent14-word native geometry over the actual owned named garage scaffold
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeNamedGarageGeometryHistoricalPropertiesMain
#include "NativeNamedGarageTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeNamedGarageGeometryProfile.h"
namespace
{
    namespace GG = NativeNamedGarageGeometryProfile;
    NativeGarageGeometry GeometrySentinel()
    {
        NativeGarageGeometry result;
        result.words.fill(0xdeadbeefu);
        return result;
    }
    NativeGarageGeometry GeometryWords(std::uint32_t salt)
    {
        NativeGarageGeometry                result;
        const std::array<std::uint32_t, 14> bits{0u,          0x80000000u, 0x3f800000u, 0xbf800000u, 0x7f800000u, 0xff800000u, 0x7fc12345u,
                                                 0x7fa54321u, 1u,          0x80000001u, 0x7f7fffffu, 0xff7fffffu, 0x00800000u, 0x80800000u};
        for (std::size_t i = 0; i != result.words.size(); ++i)
            result.words[i] = bits[i] ^ salt;
        return result;
    }
    GarageFixture* changingGeometry = nullptr;
    void           ChangeGeometryWord()
    {
        changingGeometry->Global(G::BankRva + G::RowBytes + 7 * sizeof(std::uint32_t), std::uint32_t{0x12345678});
    }
    void ChangeGeometryFirstMatch()
    {
        changingGeometry->RowName(0, Name("beta"));
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "geometry evidence uses explicit owned workspace output");
    GarageMemory::Binding refused{123};
    Check(!GarageMemory::QualifyGeometry(refused) && refused.image == 123,
          "geometry exact-build admission refuses ordinary host without fabricating a binding");
    GarageFixture fixture(argv[1]);
    changingGeometry = &fixture;
    PoolTestHost::image = fixture.bank.transform.image.image;
    for (const auto& pin : GG::Code)
        fixture.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const GarageMemory::Binding binding{PoolTestHost::image};
    GarageMemory                memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    const auto                  beta = Name("BeTa");
    const auto                  seed = [&](const NativeGarageGeometry& value)
    { fixture.bank.transform.image.Patch(G::BankRva + G::RowBytes, value.words.data(), GG::GeometryBytes, PAGE_READWRITE); };
    Check(GarageMemory::RevalidateGeometry(binding), "shared append producer and both independent containment geometry consumers qualify");
    NativeNamedGarageAccess defaultAccess;
    auto                    value = GeometrySentinel();
    Check(defaultAccess.ReadNamedGarageGeometry(beta, value) == ClockResult::NotInitialized && value.words == GeometrySentinel().words,
          "timing-only default preserves all14 raw words without inventing geometry");
    Check(unowned.ReadGeometry(beta, value) == ClockResult::NotInitialized && value.words == GeometrySentinel().words,
          "native geometry storage is not a retained callback owner");
    for (const auto salt : {0u, 0xffffffffu, 0x5a5a5a5au})
    {
        fixture.Reset();
        const auto expected = GeometryWords(salt);
        seed(expected);
        value = GeometrySentinel();
        Check(
            memory.ReadGeometry(beta, value) == ClockResult::Ok && value.words == expected.words && GarageHost::exchanges == 0,
            "all56bytes preserve raw position/direction/upperZ/size/bounds includingNaN,signzero,denormal andinfinitybits without float arithmetic or writes");
    }
    fixture.Reset();
    const auto expected = GeometryWords(0);
    seed(expected);
    for (std::size_t word = 0; word != expected.words.size(); ++word)
    {
        const auto                                 before = fixture.bank.transform.image.image + G::BankRva + G::RowBytes;
        std::array<unsigned char, G::RowBytes * 3> allRows{};
        std::memcpy(allRows.data(), reinterpret_cast<void*>(before - G::RowBytes), allRows.size());
        value = GeometrySentinel();
        Check(memory.ReadGeometry(beta, value) == ClockResult::Ok && value.words[word] == expected.words[word] &&
                  std::memcmp(allRows.data(), reinterpret_cast<void*>(before - G::RowBytes), allRows.size()) == 0,
              "each selected component comes from the first matched native row and never normalizes or mutates neighboring fields");
    }
    fixture.Reset();
    seed(expected);
    value = GeometrySentinel();
    Check(memory.ReadGeometry(beta, value) == ClockResult::Ok, "measure only this new production geometry reader's real read/owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned failure = 1; failure <= reads; ++failure)
    {
        fixture.Reset();
        seed(expected);
        PoolTestHost::reads = 0;
        PoolTestHost::failAt = failure;
        value = GeometrySentinel();
        Check(memory.ReadGeometry(beta, value) != ClockResult::Ok && value.words == GeometrySentinel().words && GarageHost::exchanges == 0,
              "every geometry/header/name/count/pin OS read fault preserves the complete caller value");
    }
    for (unsigned revoke = 1; revoke <= owners; ++revoke)
    {
        fixture.Reset();
        seed(expected);
        PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = revoke;
        value = GeometrySentinel();
        Check(memory.ReadGeometry(beta, value) == ClockResult::WrongThread && value.words == GeometrySentinel().words,
              "every current callback bookend can revoke geometry without publishing partial fields");
    }
    for (const auto effect : {&ChangeGeometryWord, &ChangeGeometryFirstMatch})
    {
        fixture.Reset();
        seed(expected);
        PoolTestHost::watched = fixture.Row(1);
        PoolTestHost::after = effect;
        value = GeometrySentinel();
        Check(memory.ReadGeometry(beta, value) != ClockResult::Ok && value.words == GeometrySentinel().words && GarageHost::exchanges == 0,
              "concurrent geometry or first-name-match changes refuse rather than mixing snapshots or reinterpreting a cached row");
    }
    for (const auto& pin : GG::Code)
    {
        fixture.Reset();
        seed(expected);
        const auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
        fixture.bank.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        value = GeometrySentinel();
        Check(memory.ReadGeometry(beta, value) == ClockResult::BindingChanged && value.words == GeometrySentinel().words,
              "both independent new geometry consumer profiles are mandatory for geometry output");
        fixture.bank.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    fixture.Reset();
    seed(expected);
    fixture.Global(G::CountRva, std::uint32_t{0x10000});
    value = GeometrySentinel();
    Check(memory.ReadGeometry(beta, value) == ClockResult::BindingChanged && value.words == GeometrySentinel().words,
          "geometry does not weaken fullDWORD count or signedlookup-domain admission");
    fixture.Reset();
    seed(expected);
    fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    value = GeometrySentinel();
    Check(memory.ReadGeometry(beta, value) != ClockResult::Ok && value.words == GeometrySentinel().words, "native scoped GC refuses geometry snapshot");
    fixture.Reset();
    seed(expected);
    ClockResult foreign = ClockResult::Ok;
    value = GeometrySentinel();
    std::thread other([&] { foreign = memory.ReadGeometry(beta, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && value.words == GeometrySentinel().words, "foreign threads cannot borrow geometry by holding a name");
    std::puts("Production native named garage geometry primitive fixture completed");
    std::printf("Passed %u native named garage geometry checks (64-bit; no native engine code executed)\n", checks);
}
