/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Raw weapon-instance DWORD reads/CAS over both actual occupied native ped classes
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWeaponFieldsHistoricalTransformMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedWeaponFieldsMemory.h"
#include "../../Client/game_de/NativePedWeaponProfile.h"
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>
namespace WeaponFieldsHost
{
    unsigned                                         exchanges = 0, exchangeRead = 0;
    bool                                             failBefore = false, failAfter = false;
    std::function<void()>                            before, after;
    std::function<void(std::uintptr_t, std::size_t)> afterRead;
    BOOL WINAPI                                      Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = PoolTestHost::Read(process, input, output, bytes, copied);
        if (result && afterRead)
            afterRead(reinterpret_cast<std::uintptr_t>(input), bytes);
        return result;
    }
    bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        exchangeRead = PoolTestHost::reads;
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        LONG e = 0, t = 0;
        std::memcpy(&e, &expected, 4);
        std::memcpy(&t, &target, 4);
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), t, e);
        std::memcpy(&observed, &previous, 4);
        if (after)
            after();
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
    void Reset()
    {
        exchanges = exchangeRead = 0;
        failBefore = failAfter = false;
        before = after = {};
        afterRead = {};
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory WeaponFieldsHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#include "../../Client/game_de/NativePedWeaponContext.cpp"
#define MTA_DE_PED_WEAPON_FIELDS_EXCHANGE WeaponFieldsHost::Exchange
#include "../../Client/game_de/NativePedWeaponFieldsMemory.cpp"
#undef MTA_DE_PED_WEAPON_FIELDS_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    struct WeaponFieldsFixture
    {
        TransformFixture transform;
        explicit WeaponFieldsFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundaries{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto& section = sections[i];
                std::memcpy(section.Name, i == 0 ? ".text" : i == 1 ? ".rdata" : ".data", i == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundaries[i]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundaries[i + 1] - boundaries[i]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + i * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (i == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                       : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (i == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedMetadataProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativePedWeaponProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        template <class T>
        void Store(std::size_t slot, std::size_t offset, const T& value)
        {
            std::memcpy(transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + offset, &value, sizeof(value));
        }
        void Reset()
        {
            transform.Reset(false);
            WeaponFieldsHost::Reset();
            Store(0, 0, transform.image.image + std::uintptr_t{0x4318270});
            Store(8, 0, transform.image.image + std::uintptr_t{0x4319730});
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
            {
                Store(slot, 0x3a, std::uint16_t{0xffff});
                Store(slot, 0x638, std::uint32_t{0xdeadbeef});
                Store(slot, 0x63c, std::uint32_t{0xa5a5a5a5});
                Store(slot, 0x640, std::uint32_t{0x10203040});
                Store(slot, 0x6a, static_cast<unsigned char>(0xfb));
                Store(slot, 0x5f0, std::uintptr_t{0});
                Store(slot, 0x7e0, std::int32_t{-1});
            }
            for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
                for (std::uint32_t weaponSlot = 0; weaponSlot < NativePedWeaponProfile::Count; ++weaponSlot)
                {
                    const auto at = NativePedWeaponProfile::ArrayOffset + weaponSlot * NativePedWeaponProfile::Stride;
                    Store(pedSlot, at, std::uint32_t{22} + weaponSlot);
                    Store(pedSlot, at + 4, std::uint32_t{3});
                    Store(pedSlot, at + 8, std::uint32_t{0x12345678});
                    Store(pedSlot, at + 12, std::uint32_t{0x87654321});
                    Store(pedSlot, at + 16, std::uint32_t{0xabcdef12});
                }
            // These observations come from genuine occupied pool slots. No
            // current-player pointer, selector or model lookup grants access.
            Global(0x53eb720, std::uintptr_t{0});
            Global(0x522bcf6, static_cast<unsigned char>(0xff));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        }
        NativePedPoolReader::Value Ped(std::uint32_t handle = 1)
        {
            NativePedPoolReader        reader({transform.image.image}, &PoolTestHost::Owner);
            NativePedPoolReader::Value value;
            Check(reader.Resolve(handle, value) == ClockResult::Ok, "metadata requires a real occupied native slot and issued handle observation");
            return value;
        }
    };
    using WeaponFieldsBytes = std::array<unsigned char, NativePedPoolProfile::ObjectStride>;
    WeaponFieldsBytes WeaponFieldsSnapshot(const WeaponFieldsFixture& f, std::size_t pedSlot = 0)
    {
        WeaponFieldsBytes bytes;
        std::memcpy(bytes.data(), f.transform.pool.objects + pedSlot * NativePedPoolProfile::ObjectStride, bytes.size());
        return bytes;
    }
    std::uint32_t WeaponFieldsRaw(const WeaponFieldsFixture& f, std::uint32_t weaponSlot, std::size_t offset, std::size_t pedSlot = 0)
    {
        std::uint32_t value = 0;
        std::memcpy(&value,
                    f.transform.pool.objects + pedSlot * NativePedPoolProfile::ObjectStride + NativePedWeaponProfile::ArrayOffset +
                        weaponSlot * NativePedWeaponProfile::Stride + offset,
                    4);
        return value;
    }
    void WeaponFieldsOnlyChanged(const WeaponFieldsFixture& f, WeaponFieldsBytes before, std::uint32_t weaponSlot, std::size_t offset, std::uint32_t value,
                                 std::size_t pedSlot = 0)
    {
        std::memcpy(before.data() + NativePedWeaponProfile::ArrayOffset + weaponSlot * NativePedWeaponProfile::Stride + offset, &value, 4);
        Check(WeaponFieldsSnapshot(f, pedSlot) == before, "raw weapon scalar preserves every other inline slot, ammo, timer, native pointer and ped byte");
    }
}
int main(int argc, char** argv)
{
    using Memory = NativePedWeaponFieldsMemory;
    using Context = NativePedWeaponContext;
    namespace P = NativePedWeaponProfile;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "weapon evidence stays in its explicit workspace directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping weapon binding refuses actual non-GTA test host");
    WeaponFieldsFixture f(argv[1]);
    PoolTestHost::image = f.transform.image.image;
    const auto image = f.transform.image.image;
    Memory     memory({image}, &PoolTestHost::Owner), unowned({image}, nullptr);
    Context    context({image}, &PoolTestHost::Owner);
    const auto reset = [&](std::uint32_t handle = 1)
    {
        f.Reset();
        const auto ped = f.Ped(handle);
        WeaponFieldsHost::Reset();
        return ped;
    };
    auto            ped = reset();
    Memory::Receipt receipt;
    Check(Memory::Revalidate({image}), "exact named thirteen-row query and independent type/state producers qualify");
    struct Defaults final : NativePedWeaponFieldsAccess
    {
    } defaults;
    std::uint32_t output = 0xdeadbeef;
    Check(defaults.ReadPedWeaponType({}, 0, output) == ClockResult::NotInitialized && output == 0xdeadbeef &&
              defaults.ReadPedWeaponState({}, 0, output) == ClockResult::NotInitialized && output == 0xdeadbeef &&
              defaults.SetPedWeaponType({}, 0, 0xffffffff) == ClockResult::NotInitialized &&
              defaults.SetPedWeaponState({}, 0, 0xffffffff) == ClockResult::NotInitialized,
          "default registered-token weapon access refuses and preserves outputs");
    constexpr std::array<std::uint32_t, 10> inputs{0, 1, 2, 3, 4, 46, 255, 0x80000000, 0xdeadbeef, 0xffffffff};
    for (const auto pedSlot : {std::size_t{0}, std::size_t{8}})
        for (std::uint32_t slot = 0; slot < P::Count; ++slot)
            for (const auto input : inputs)
                for (const auto offset : {P::TypeOffset, P::StateOffset})
                {
                    ped = reset(pedSlot == 0 ? 1u : 0x87fu);
                    // No current-equipped selector is needed to access any
                    // actual inline slot owned by this registered ped.
                    f.Store(pedSlot, 0xa10, static_cast<unsigned char>(0xff));
                    const auto before = WeaponFieldsSnapshot(f, pedSlot);
                    output = 0xdeadbeef;
                    const auto read = offset == P::TypeOffset ? memory.ReadType(ped, slot, output) : memory.ReadState(ped, slot, output);
                    Check(read == ClockResult::Ok && output == WeaponFieldsRaw(f, slot, offset, pedSlot),
                          "raw getter observes complete current DWORD including unknown storage values");
                    const auto result = offset == P::TypeOffset ? memory.SetType(ped, slot, input, &receipt) : memory.SetState(ped, slot, input, &receipt);
                    Check(result == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified && WeaponFieldsHost::exchanges == 1,
                          "each raw setter accepts one conditional DWORD write");
                    Check(receipt.slot == slot && receipt.nativePed == ped.nativePed &&
                              receipt.nativeWeapon == ped.nativePed + P::ArrayOffset + slot * P::Stride && receipt.address == receipt.nativeWeapon + offset,
                          "receipt belongs to exact selected ped generation and explicit inline slot");
                    WeaponFieldsOnlyChanged(f, before, slot, offset, input, pedSlot);
                    output = 0;
                    Check((offset == P::TypeOffset ? memory.ReadType(ped, slot, output) : memory.ReadState(ped, slot, output)) == ClockResult::Ok &&
                              output == input,
                          "accepted raw value survives a fresh slot/type bookend without normalization or synthetic fallback");
                }
    ped = reset();
    for (const auto slot : {std::uint32_t{13}, std::uint32_t{0xffffffff}})
    {
        output = 0xdeadbeef;
        Check(memory.ReadType(ped, slot, output) == ClockResult::InvalidArgument && output == 0xdeadbeef &&
                  memory.SetState(ped, slot, 1, &receipt) == ClockResult::InvalidArgument && !receipt.entered && WeaponFieldsHost::exchanges == 0,
              "SDK sentinel and full-DWORD invalid slot cannot alias a native weapon");
    }
    Check(unowned.SetType(ped, 0, 1, &receipt) == ClockResult::NotInitialized && !receipt.entered, "no callback owner grants no weapon write");
    Context::Value selected;
    Check(context.Capture(ped, 12, selected) == ClockResult::Ok, "context captures last genuine row");
    auto forged = selected;
    forged.nativeWeapon += P::Stride;
    Check(context.Recheck(forged) == ClockResult::OwnerConflict, "recheck rejects a forged neighboring row");
    f.Store(0, P::ArrayOffset + 12 * P::Stride, std::uint32_t{0xffffffff});
    Check(context.Recheck(selected) == ClockResult::OwnerConflict && context.Recheck(selected, 0xffffffff) == ClockResult::Ok,
          "new raw type is allowed only by an explicit expected-type postcondition");
    ped = reset();
    {
        Context::Mutation outer;
        Check(static_cast<bool>(outer) && memory.SetState(ped, 0, 2, &receipt) == ClockResult::Busy && !receipt.entered && WeaponFieldsHost::exchanges == 0,
              "shared mutation scope refuses recursive setters before native effects");
    }
    for (const auto& pin : P::Code)
    {
        ped = reset();
        auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        f.transform.image.Patch(pin.rva, &changed, 1, PAGE_EXECUTE_READ);
        output = 0xdeadbeef;
        Check(!Memory::Revalidate({image}) && memory.ReadState(ped, 0, output) == ClockResult::BindingChanged && output == 0xdeadbeef &&
                  memory.SetType(ped, 0, 1, &receipt) == ClockResult::BindingChanged && !receipt.entered,
              "every exact native layout/producer pin is mandatory before observation or mutation");
        f.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    ped = reset();
    auto stale = ped;
    stale.generation ^= 1;
    Check(memory.SetState(stale, 0, 1, &receipt) != ClockResult::Ok && !receipt.entered, "stale selected ped generation cannot lend a weapon slot");
    f.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    Check(memory.SetState(ped, 0, 1, &receipt) != ClockResult::Ok && !receipt.entered, "native GC blocks the slot writer before its CAS");
    ped = reset();
    f.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
    Check(memory.SetState(ped, 0, 1, &receipt) != ClockResult::Ok && !receipt.entered, "native GameThread identity remains mandatory");
    ped = reset();
    f.Store(0, 0, f.transform.image.image + std::uintptr_t{0x4318278});
    Check(memory.SetState(ped, 0, 1, &receipt) != ClockResult::Ok && !receipt.entered, "foreign native class cannot borrow a typed inline array");
    ped = reset();
    auto* row = f.transform.pool.objects + P::ArrayOffset;
    bool  raced = false;
    WeaponFieldsHost::afterRead = [&](std::uintptr_t at, std::size_t bytes)
    {
        if (!raced && at == reinterpret_cast<std::uintptr_t>(row) && bytes == 4)
        {
            raced = true;
            f.Store(0, P::ArrayOffset, std::uint32_t{99});
        }
    };
    output = 0xdeadbeef;
    Check(memory.ReadState(ped, 0, output) == ClockResult::OwnerConflict && output == 0xdeadbeef,
          "changing type during a read cannot lend a mixed weapon snapshot");
    ped = reset();
    WeaponFieldsHost::before = [&]() { f.Store(0, P::ArrayOffset + P::StateOffset, std::uint32_t{4}); };
    Check(memory.SetState(ped, 0, 1, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && WeaponFieldsHost::exchanges == 1 &&
              WeaponFieldsRaw(f, 0, P::StateOffset) == 4,
          "competing state store is retained after one failed CAS with no replay");
    ped = reset();
    WeaponFieldsHost::after = [&]() { PoolTestHost::ownerResult = ClockResult::NotInitialized; };
    Check(memory.SetType(ped, 0, 0xffffffff, &receipt) != ClockResult::Ok && receipt.entered && receipt.accepted && !receipt.verified &&
              WeaponFieldsHost::exchanges == 1 && WeaponFieldsRaw(f, 0, 0) == 0xffffffff,
          "accepted type followed by owner loss persists and requires runtime Stop without rollback");
    ped = reset();
    WeaponFieldsHost::failAfter = true;
    Check(memory.SetState(ped, 0, 2, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted && WeaponFieldsHost::exchanges == 1 &&
              WeaponFieldsRaw(f, 0, 4) == 2,
          "unknown post-store acknowledgement is never replayed or repaired");
    ped = reset();
    WeaponFieldsHost::after = [&]() { f.Store(0, P::ArrayOffset + P::StateOffset, std::uint32_t{4}); };
    Check(memory.SetState(ped, 0, 2, &receipt) == ClockResult::OwnerConflict && receipt.entered && receipt.accepted && !receipt.verified &&
              WeaponFieldsHost::exchanges == 1 && WeaponFieldsRaw(f, 0, 4) == 4,
          "post-acceptance conflicting state remains native reality without a restoring write");
    ped = reset();
    WeaponFieldsHost::failBefore = true;
    Check(memory.SetState(ped, 0, 2, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.accepted && WeaponFieldsHost::exchanges == 1 &&
              WeaponFieldsRaw(f, 0, 4) == 3,
          "pre-store exception exposes entered failure without pretending accepted effects");
    ped = reset();
    const auto failReadAt = PoolTestHost::reads + 1;
    PoolTestHost::failAt = failReadAt;
    output = 0xdeadbeef;
    Check(memory.ReadType(ped, 0, output) != ClockResult::Ok && output == 0xdeadbeef, "read failure preserves caller-owned scalar output");
    std::printf("Production native ped weapon fields fixture completed\n");
    std::printf("Passed %u native ped-weapon-fields checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
