/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual qualified bounded bank observations over owned SEC_IMAGE pages
 ****************************************************************************/
// Preserve a containing fixture's entry-point rename when this owned-image
// scaffold is itself reused by the streaming-state tests.
#pragma push_macro("main")
#undef main
#define main NativeModelBankTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelBankReader.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include <thread>
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativeModelBankReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    namespace B = NativeModelBankProfile;
    using BankReader = NativeModelBankReader;
    struct BankFixture
    {
        TransformFixture transform;
        explicit BankFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundaries{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t index = 0; index != sections.size(); ++index)
            {
                auto& section = sections[index];
                std::memcpy(section.Name, index == 0 ? ".text" : index == 1 ? ".rdata" : ".data", index == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundaries[index]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundaries[index + 1] - boundaries[index]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + index * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (index == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                           : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (index == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : B::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        void Slot(std::uint32_t index, std::uintptr_t value) { Global(B::BankRva + static_cast<std::uintptr_t>(index) * B::SlotBytes, value); }
        void Reset()
        {
            transform.Reset(false);
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            Slot(0, 0);
            Slot(B::ModelCount - 1, 0);
            Slot(17, 0x1231);
        }
    };
    BankReader::Value Sentinel()
    {
        return {0xdeadbeefu, 0xabcd, 0xfeed};
    }
    bool Untouched(const BankReader::Value& value)
    {
        const auto expected = Sentinel();
        return value.modelIndex == expected.modelIndex && value.slotAddress == expected.slotAddress && value.nativeModelInfo == expected.nativeModelInfo;
    }
    BankFixture* changingBank = nullptr;
    void         ChangeBank()
    {
        changingBank->Slot(17, 0x9871);
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "model-bank evidence has an explicit owned workspace directory");
    BankReader::Binding refused{123};
    Check(!BankReader::Qualify(refused) && refused.image == 123, "shipping bank qualification refuses the real non-game host without consuming output");
    BankFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const BankReader::Binding binding{fixture.transform.image.image};
    BankReader                reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    Check(BankReader::Revalidate(binding), "actual upper-bound/assignment/destructor bank pins and full bank data section qualify");
    auto value = Sentinel();
    Check(unowned.Read(0, value) == ClockResult::NotInitialized && Untouched(value), "native bank is not a callback owner");
    for (const auto index : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{17}, B::ModelCount - 1})
        for (const auto word : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0x1231}, (std::numeric_limits<std::uintptr_t>::max)()})
        {
            fixture.Reset();
            fixture.Slot(index, word);
            value = Sentinel();
            Check(reader.Read(index, value) == ClockResult::Ok && value.modelIndex == index && value.nativeModelInfo == word &&
                      value.slotAddress == binding.image + B::BankRva + static_cast<std::uintptr_t>(index) * B::SlotBytes,
                  "first/last bank entries preserve null, unaligned and unreadable opaque addresses without dereferencing model objects");
            Check(reader.Recheck(value) == ClockResult::Ok, "unchanged immediate raw slot snapshot rechecks inside the same owner");
            fixture.Slot(index, word ^ 8u);
            Check(reader.Recheck(value) == ClockResult::BindingChanged, "changed same-slot association refuses the old snapshot");
        }
    for (const auto invalid :
         {B::ModelCount, B::ModelCount + 1, std::uint32_t{32767}, std::uint32_t{32768}, std::uint32_t{65535}, std::uint32_t{65536}, 0xffffffffu})
    {
        fixture.Reset();
        value = Sentinel();
        Check(reader.Read(invalid, value) == ClockResult::InvalidArgument && Untouched(value) && PoolTestHost::reads == 0 && PoolTestHost::ownerCalls == 0,
              "full DWORD index refuses TXD/other stores, signed WORD negatives, ffff and truncated model-zero aliases without any bank read");
    }
    fixture.Reset();
    Check(reader.Read(17, value) == ClockResult::Ok, "measure actual checked read and owner boundaries");
    const auto readCount = PoolTestHost::reads, ownerCount = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= readCount; ++fail)
    {
        fixture.Reset();
        PoolTestHost::failAt = fail;
        value = Sentinel();
        Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "each failed real OS read preserves the caller's complete output");
    }
    for (unsigned revoke = 1; revoke <= ownerCount; ++revoke)
    {
        fixture.Reset();
        PoolTestHost::revokeAt = revoke;
        value = Sentinel();
        Check(reader.Read(17, value) == ClockResult::WrongThread && Untouched(value), "each owner bookend can revoke model association observation");
    }
    for (unsigned state = 0; state != 9; ++state)
    {
        fixture.Reset();
        fixture.Global(NativeGameStateProfile::stateRva, static_cast<std::uint32_t>(state));
        value = Sentinel();
        // The shared state reader deliberately leaves 0..5 unqualified. Only
        // proved frontend/loading states can report an initialized-state refusal.
        const auto expected = state < 6 ? ClockResult::BindingChanged : ClockResult::NotInitialized;
        Check(reader.Read(17, value) == expected && Untouched(value), "native loading/menu states do not authorize model bank observation");
    }
    fixture.Reset();
    fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "native GC scope refuses bank output");
    fixture.Reset();
    fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "an uninitialized native thread pair is not a game-thread lease");
    fixture.Reset();
    value = Sentinel();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = reader.Read(17, value); });
    other.join();
    Check(foreign == ClockResult::WrongThread && Untouched(value), "a permissive callback seam cannot replace actual native GameThread identity");
    fixture.Reset();
    changingBank = &fixture;
    PoolTestHost::watched = binding.image + B::BankRva + 17 * B::SlotBytes;
    PoolTestHost::after = &ChangeBank;
    value = Sentinel();
    Check(reader.Read(17, value) == ClockResult::BindingChanged && Untouched(value) && PoolTestHost::after == nullptr,
          "a raw slot changed between two actual reads cannot publish the earlier association");
    fixture.Reset();
    Check(reader.Read(17, value) == ClockResult::Ok, "seed raw bank tuple before forged-address recheck");
    ++value.slotAddress;
    Check(reader.Recheck(value) == ClockResult::BindingChanged, "recheck recomputes exact slot address rather than trusting a caller pointer");
    fixture.Reset();
    const auto slotAddress = binding.image + B::BankRva + 17 * B::SlotBytes;
    DWORD      previous = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(slotAddress), sizeof(std::uintptr_t), PAGE_READONLY, &previous) != FALSE,
          "restrict only the owned model bank page to readonly");
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "writable-image data contract rejects an unexpectedly readonly bank page");
    Check(VirtualProtect(reinterpret_cast<void*>(slotAddress), sizeof(std::uintptr_t), previous, &ignored) != FALSE,
          "restore only the owned model bank page protection");
    IMAGE_SECTION_HEADER bankSection{};
    const auto           sectionAt = 0x80u + sizeof(IMAGE_NT_HEADERS64) + 2 * sizeof(IMAGE_SECTION_HEADER);
    std::memcpy(&bankSection, reinterpret_cast<void*>(binding.image + sectionAt), sizeof(bankSection));
    auto shortSection = bankSection;
    shortSection.Misc.VirtualSize = static_cast<DWORD>(B::BankRva + B::BankBytes - shortSection.VirtualAddress - 1);
    fixture.transform.image.Patch(sectionAt, &shortSection, sizeof(shortSection), PAGE_READONLY);
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "last native model slot must fit wholly inside the qualified data section");
    auto executableSection = bankSection;
    executableSection.Characteristics |= IMAGE_SCN_MEM_EXECUTE;
    fixture.transform.image.Patch(sectionAt, &executableSection, sizeof(executableSection), PAGE_READONLY);
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "executable bank storage cannot impersonate native model data");
    fixture.transform.image.Patch(sectionAt, &bankSection, sizeof(bankSection), PAGE_READONLY);
    fixture.Reset();
    const auto& pin = B::Code[0];
    auto        byte = static_cast<unsigned char>(pin.bytes[0] ^ 1u);
    fixture.transform.image.Patch(pin.rva, &byte, 1, PAGE_EXECUTE_READ);
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "changed native limit/lookup code cannot reuse the profile");
    fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    fixture.Reset();
    PoolTestHost::image = 0;
    value = Sentinel();
    Check(reader.Read(17, value) != ClockResult::Ok && Untouched(value), "changed actual main-image binding cannot publish a model association");
    std::puts("Production native model bank primitive fixture completed");
    std::printf("Passed %u native model-bank checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
