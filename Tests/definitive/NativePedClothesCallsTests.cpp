/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine clothes admission over owned native pages and real readers
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalClothesNameDefinitionsMain
#include "NativeStreamingSpecialModelTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativePedClothesFixture.h"
#include "../../Client/game_de/NativePedClothesCalls.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <string>
namespace mta::game_de::NativeImageAccess
{
    bool ClothesFixtureExecutable() noexcept
    {
        return PoolTestHost::image != 0;
    }
}
namespace
{
    namespace PC = mta::game_de::NativePedClothesCalls;
    ClockResult ClothesEntry(PC::NativeFunction, std::uintptr_t, const char*, const char*, std::uint8_t, std::uint32_t&) noexcept;
}
#define GetModuleHandleW     PoolTestHost::Module
#define ReadProcessMemory    PoolTestHost::Read
#define CheckExecutableBuild ClothesFixtureExecutable
// Match the established isolated aggregate-TU include. The production
// reader body, current player checks and real helper implementations stay
// unchanged; only private helper identifiers cannot collide with pools.
#define P              ClothesPlayerInfoProfile
#define Binding        ClothesPlayerInfoBinding
#define Context        ClothesPlayerInfoContext
#define Object         ClothesPlayerInfoObject
#define ObjectPages    ClothesPlayerInfoObjectPages
#define Global         ClothesPlayerInfoGlobal
#define Owner          ClothesPlayerInfoOwner
#define Exchange       ClothesPlayerInfoExchange
#define PlayerIdentity ClothesPlayerInfoIdentity
#include "../../Client/game_de/NativePlayerInfoMemory.cpp"
#undef PlayerIdentity
#undef Exchange
#undef Owner
#undef Global
#undef ObjectPages
#undef Object
#undef Context
#undef Binding
#undef P
#undef CheckExecutableBuild
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_CLOTHES_NATIVE_INVOKE ::ClothesEntry
#include "../../Client/game_de/NativePedClothesCalls.cpp"
#undef MTA_DE_PED_CLOTHES_NATIVE_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace CP = NativePedClothesProfile;
    namespace IP = NativePlayerInfoProfile;
    struct ClothesFixture : SpecialFixture
    {
        NativePlayerInfoMemory             playerInfo;
        NativePedClothesFixture::NameState names;
        unsigned char*                     clothes = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        explicit ClothesFixture(const std::filesystem::path& directory) : SpecialFixture(directory)
        {
            Check(clothes != nullptr, "own one bounded writable clothes descriptor page");
            Reset();
        }
        ~ClothesFixture()
        {
            if (clothes)
                VirtualFree(clothes, 0, MEM_RELEASE);
        }
        template <class T>
        void PedAt(std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + offset, &value, sizeof(value));
        }
        void Reset()
        {
            playerInfo.Reset();
            SpecialFixture::Reset();
            PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
            PoolTestHost::ownerResult = ClockResult::Ok;
            PoolTestHost::watched = 0;
            PoolTestHost::after = nullptr;
            DWORD ignored = 0;
            Check(VirtualProtect(clothes, 4096, PAGE_READWRITE, &ignored) != FALSE, "restore owned descriptor page permissions");
            std::memset(clothes, 0, 4096);
            NativePedClothesFixture::Install(transform.image.image, &SpecialFixture::Patch, this);
            for (const auto& pin : NativePedMetadataProfile::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            names.Reset(transform.image.image, &SpecialFixture::Patch, this);
            const auto ped = Address(transform.pool.objects);
            PedAt(0, transform.image.image + IP::PlayerVtableRva);
            PedAt(0x5f0, transform.image.image + IP::BankRva + 8);
            PedAt(0x7e0, std::int32_t{0});
            PedAt(NativePedMetadataProfile::ModelOffset, std::uint16_t{0});
            PedAt(NativePedMetadataProfile::TypeOffset, static_cast<unsigned char>(3));
            Global(IP::BankRva, ped);
            Global(IP::PlayerIndexRva, static_cast<unsigned char>(0));
            Global(CP::ClothesSlotRva, Address(clothes));
            PoolTestHost::image = transform.image.image;
            Check(playerInfo.Bind() == ClockResult::Ok, "reuse actual strict bound player-info reader and native local data identity");
        }
        PC::Ped Ped()
        {
            PC::Ped value;
            Check(NativePedPoolReader({transform.image.image}, &PoolTestHost::Owner).Resolve(1, value) == ClockResult::Ok,
                  "resolve actual current registered-style occupied local player receiver");
            return value;
        }
    };
    ClothesFixture* activeClothes = nullptr;
    unsigned        clothesCalls = 0, clothesEffects = 0, clothesScenario = 0;
    std::uint8_t    wantedPart = 0;
    const char*     wantedTexture = "Shirt_12";
    const char*     wantedModel = "torso";
    const char*     originalTexture = nullptr;
    const char*     originalModel = nullptr;
    ClockResult     nestedClothes = ClockResult::Ok, nestedSpecial = ClockResult::Ok;
    PC::Ped*        mutablePedCaller = nullptr;
    ClockResult     MutatingClothesOwner() noexcept
    {
        if (mutablePedCaller)
            *mutablePedCaller = {};
        return PoolTestHost::Owner();
    }
    ClockResult ClothesEntry(PC::NativeFunction function, std::uintptr_t clothes, const char* texture, const char* model, std::uint8_t part,
                             std::uint32_t& exception) noexcept
    {
        auto& f = *activeClothes;
        Check(reinterpret_cast<std::uintptr_t>(function) == f.transform.image.image + CP::EntryRva && clothes == reinterpret_cast<std::uintptr_t>(f.clothes) &&
                  part == wantedPart,
              "one exact genuine setter receives current clothes and unchanged narrowed part");
        Check((!wantedTexture && !texture) || (wantedTexture && texture && std::strcmp(texture, wantedTexture) == 0), "nullable texture intent is unchanged");
        Check((!wantedModel && !model) || (wantedModel && model && std::strcmp(model, wantedModel) == 0),
              "nullable model intent is unchanged even for sentinel model parts");
        Check((!texture || texture != originalTexture) && (!model || model != originalModel),
              "non-null caller names are copied for the complete synchronous native entry");
        ++clothesCalls;
        ++clothesEffects;
        std::memcpy(f.clothes, &clothesEffects, sizeof(clothesEffects));
        if (texture || model)
            f.names.Cache(f.transform.image.image, &SpecialFixture::Patch, &f, true);
        if (clothesScenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (clothesScenario == 2)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (clothesScenario == 3)
            f.transform.pool.flags[0] ^= 1;
        if (clothesScenario == 4)
            f.Global(CP::ClothesSlotRva, std::uintptr_t{0});
        if (clothesScenario == 5)
            f.PedAt(0x5f0, std::uintptr_t{0});
        if (clothesScenario == 6)
            f.Global(XP::NameInitializedRva, static_cast<unsigned char>(0));
        if (clothesScenario == 7)
            f.Global(AP::HeapAuthorityRva, std::uintptr_t{0});
        if (clothesScenario == 8)
        {
            const auto& pin = CP::Pins[0];
            const auto  bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            f.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        }
        if (clothesScenario == 9)
        {
            const auto  current = f.Ped();
            PC::Receipt nested;
            nestedClothes = PC::Set(f.transform.image.image, current, f.playerInfo, nullptr, nullptr, 0, &PoolTestHost::Owner, &nested);
            Check(!nested.entered, "recursive clothes intent cannot enter a second native call");
            SC::Receipt special;
            nestedSpecial = SC::Request(f.transform.image.image, 17, "other", 0, &PoolTestHost::Owner, &special);
            Check(!special.entered, "the existing special-model service cannot enter during clothes name mutation");
        }
        if (clothesScenario == 10)
            PoolTestHost::failAt = PoolTestHost::reads + 1;
        if (clothesScenario == 11)
            // SpecialFixture publishes its merged items allocation, not the
            // graph.items source it copied during Reset. Invalidate the
            // current registry serial actually consumed by the guard.
            f.Put(f.items, 0x10, std::uint32_t{999});
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    ClothesFixture fixture{std::filesystem::path(argv[1])};
    activeClothes = &fixture;
    const auto reset = [&]
    {
        fixture.Reset();
        clothesCalls = clothesEffects = clothesScenario = 0;
        wantedPart = 0;
        wantedTexture = "Shirt_12";
        wantedModel = "torso";
        originalTexture = originalModel = nullptr;
        nestedClothes = nestedSpecial = ClockResult::Ok;
        mutablePedCaller = nullptr;
    };
    const auto run = [&](const char* texture, const char* model, std::uint8_t part, PC::Receipt& receipt)
    {
        const auto ped = fixture.Ped();
        originalTexture = texture;
        originalModel = model;
        return PC::Set(fixture.transform.image.image, ped, fixture.playerInfo, texture, model, part, &PoolTestHost::Owner, &receipt);
    };
    reset();
    PC::Receipt receipt;
    Check(run(wantedTexture, wantedModel, 0, receipt) == ClockResult::Ok && receipt.entered && receipt.returned && clothesCalls == 1 && clothesEffects == 1,
          "existing local clothes uses one genuine entry and accepts descriptor plus native cache effects");
    for (unsigned part = 0; part < CP::TextureParts; ++part)
    {
        reset();
        wantedPart = static_cast<std::uint8_t>(part);
        PC::Receipt next;
        Check(run(wantedTexture, wantedModel, wantedPart, next) == ClockResult::Ok && clothesCalls == 1 && next.part == part,
              "all real MTA texture parts retain their exact byte value");
    }
    for (const auto texture : {static_cast<const char*>(nullptr), ""})
        for (const auto model : {static_cast<const char*>(nullptr), ""})
        {
            reset();
            wantedTexture = texture;
            wantedModel = model;
            PC::Receipt next;
            Check(run(texture, model, 0, next) == ClockResult::Ok && clothesCalls == 1 && next.textureBytes == (texture ? 1 : 0) &&
                      next.modelBytes == (model ? 1 : 0),
                  "null and empty names remain distinct arguments, including intentional null/null slot clearing");
        }
    reset();
    fixture.Global(CP::ClothesSlotRva, std::uintptr_t{0});
    const auto unreadable = reinterpret_cast<const char*>(std::uintptr_t{1});
    Check(run(unreadable, unreadable, 255, receipt) == ClockResult::Ok && receipt.nullClothes && !receipt.entered && !clothesCalls,
          "classic null-clothes branch never reads invalid names or imposes a texture-part precondition");
    reset();
    wantedTexture = wantedModel = nullptr;
    fixture.Global(XP::NameInitializedRva, static_cast<unsigned char>(0));
    Check(run(nullptr, nullptr, 0, receipt) == ClockResult::Ok && clothesCalls == 1,
          "null/null clearing requires no unused native name construction authority");
    reset();
    wantedTexture = "";
    wantedModel = nullptr;
    fixture.Global(XP::NameInitializedRva, static_cast<unsigned char>(0));
    Check(run("", nullptr, 0, receipt) != ClockResult::Ok && !receipt.entered && !clothesCalls,
          "non-null empty input still needs the genuine CRC/name-cache path");
    reset();
    wantedPart = 4;
    Check(run(nullptr, unreadable, 4, receipt) != ClockResult::Ok && !receipt.entered && !clothesCalls,
          "sentinel model destination does not erase the native earlier model-string/cache evaluation");
    reset();
    const char embedded[] = "Mip\0ignored";
    wantedTexture = "Mip";
    Check(run(embedded, wantedModel, 0, receipt) == ClockResult::Ok && receipt.textureBytes == 4,
          "native NUL termination is retained without copying a later caller tail");
    for (const auto part : {std::uint8_t{18}, std::uint8_t{255}})
    {
        reset();
        Check(run(unreadable, unreadable, part, receipt) == ClockResult::InvalidArgument && !receipt.entered && !clothesCalls,
              "nonnull descriptor refuses out-of-row part before string IO; no clamp");
    }
    for (const auto name : {unreadable, "\x80"})
    {
        reset();
        Check(run(name, "torso", 0, receipt) != ClockResult::Ok && !receipt.entered && !clothesCalls,
              "unreadable or unsupported signed-byte text cannot enter");
    }
    std::string boundary(CP::MaximumNameBytes - 1, 'a');
    reset();
    wantedTexture = boundary.c_str();
    Check(run(boundary.c_str(), wantedModel, 0, receipt) == ClockResult::Ok && receipt.textureBytes == CP::MaximumNameBytes,
          "the exact native below-1024 name branch accepts its last NUL-terminated length");
    boundary.push_back('a');
    reset();
    Check(run(boundary.c_str(), wantedModel, 0, receipt) != ClockResult::Ok && !receipt.entered && !clothesCalls,
          "overlong name intent does not invoke an unqualified native error branch");
    reset();
    fixture.playerInfo.Reset();
    Check(run(wantedTexture, wantedModel, 0, receipt) != ClockResult::Ok && !clothesCalls,
          "missing existing PlayerInfo binding cannot become a new data owner");
    reset();
    fixture.Global(IP::PlayerIndexRva, static_cast<unsigned char>(1));
    Check(run(wantedTexture, wantedModel, 0, receipt) != ClockResult::Ok && !clothesCalls,
          "remote/current-index substitution is not silently admitted as local owned data");
    reset();
    fixture.PedAt(0x5f0, reinterpret_cast<std::uintptr_t>(fixture.clothes));
    Check(run(wantedTexture, wantedModel, 0, receipt) != ClockResult::Ok && !clothesCalls,
          "arbitrary writable player data cannot replace the proved inline-data producer");
    for (unsigned scenario = 1; scenario <= 11; ++scenario)
    {
        reset();
        clothesScenario = scenario;
        PC::Receipt next;
        const auto  result = run(wantedTexture, wantedModel, 0, next);
        // Retain every original assertion while making a compound failure
        // identify the actual native boundary instead of guessing its cause.
        const bool diagnosticAccepted = scenario == 9 ? result == ClockResult::Ok && nestedClothes == ClockResult::Busy && nestedSpecial == ClockResult::Busy
                                                      : result != ClockResult::Ok && next.entered && clothesCalls == 1 && clothesEffects == 1;
        if (!diagnosticAccepted)
            std::fprintf(stderr, "clothes-scenario=%u result=%u entered=%u returned=%u calls=%u effects=%u exception=%08x\n", scenario,
                         static_cast<unsigned>(result), static_cast<unsigned>(next.entered), static_cast<unsigned>(next.returned), clothesCalls, clothesEffects,
                         static_cast<unsigned>(next.exceptionCode));
        if (scenario == 9)
            Check(result == ClockResult::Ok && nestedClothes == ClockResult::Busy && nestedSpecial == ClockResult::Busy,
                  "shared native name gate rejects both recursive consumers");
        else
            Check(result != ClockResult::Ok && next.entered && clothesCalls == 1 && clothesEffects == 1,
                  "native exception or lost postflight authority retains one entered prefix and never replays/rolls back");
    }
    for (const auto& pin : CP::Pins)
    {
        reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(run(wantedTexture, wantedModel, 0, receipt) != ClockResult::Ok && !clothesCalls,
              "changed actual setter/caller/mapper/descriptor-construction proof refuses before effects");
    }
    reset();
    auto mutablePed = fixture.Ped();
    mutablePedCaller = &mutablePed;
    originalTexture = wantedTexture;
    originalModel = wantedModel;
    Check(PC::Set(fixture.transform.image.image, mutablePed, fixture.playerInfo, wantedTexture, wantedModel, 0, &MutatingClothesOwner, &receipt) ==
                  ClockResult::Ok &&
              !mutablePed.nativePed && clothesCalls == 1,
          "primitive captures caller ped value before callbacks instead of retaining mutable caller storage");
    reset();
    ClockResult foreign = ClockResult::Ok;
    const auto  foreignPed = fixture.Ped();
    std::thread worker(
        [&]
        { foreign = PC::Set(fixture.transform.image.image, foreignPed, fixture.playerInfo, wantedTexture, wantedModel, 0, &PoolTestHost::Owner, &receipt); });
    worker.join();
    Check(foreign != ClockResult::Ok && !clothesCalls, "foreign thread cannot borrow the strict current player/native world authority");
    std::puts("Production native ped clothes primitive fixture completed");
    std::printf("Passed %u native ped-clothes checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
    return 0;
}
