/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real native teleport prerequisites with a typed final-call seam
 ****************************************************************************/
// Reuse the existing checked image, real pool and selected-matrix fixture.
// Renaming main does not execute that suite or bypass its shipping readers.
#define main NativePedTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedTeleportMemory.h"
#include "../../Client/game_de/NativePedTeleportProfile.h"

namespace TeleportTestHost
{
    using Memory = mta::game_de::NativePedTeleportMemory;
    unsigned                  calls = 0;
    std::uintptr_t            entry = 0, receiver = 0;
    CVector                   requested;
    bool                      argument = false, applyEffect = true, reenter = false;
    Memory*                   active = nullptr;
    mta::game_de::ClockResult nativeResult = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult nestedResult = mta::game_de::ClockResult::Ok;
    Memory::Receipt           nestedReceipt;
    void (*after)() = nullptr;
    std::uintptr_t watched = 0;
    unsigned       watchMatches = 0, revokeAtMatch = 0, matchesAtEntry = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = PoolTestHost::Read(process, input, output, bytes, copied);
        if (result && reinterpret_cast<std::uintptr_t>(input) == watched && ++watchMatches == revokeAtMatch)
            PoolTestHost::ownerResult = mta::game_de::ClockResult::WrongThread;
        return result;
    }

    mta::game_de::ClockResult Invoke(Memory::NativeFunction function, void* ped, const CVector* position, bool value, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function);
        receiver = reinterpret_cast<std::uintptr_t>(ped);
        requested = *position;
        argument = value;
        matchesAtEntry = watchMatches;
        if (reenter)
        {
            reenter = false;
            nestedResult = active->Apply(1, *position, &nestedReceipt, &Invoke);
        }
        if (applyEffect)
        {
            // This seam supplies one visible effect at the native boundary;
            // it does not emulate native task/list operations or their success.
            std::uintptr_t matrix = 0;
            std::memcpy(&matrix, static_cast<unsigned char*>(ped) + NativePedTransformProfile::MatrixPointerOffset, sizeof(matrix));
            auto* output =
                matrix ? reinterpret_cast<unsigned char*>(matrix) + 0x30 : static_cast<unsigned char*>(ped) + NativePedTransformProfile::InlinePositionOffset;
            std::memcpy(output, position, sizeof(*position));
            const CVector zero(0, 0, 0);
            std::memcpy(static_cast<unsigned char*>(ped) + NativePedTransformProfile::VelocityOffset, &zero, sizeof(zero));
        }
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (nativeResult != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return nativeResult;
    }
}

// Real PE/section/page and every immediate native dependency check remain in
// the production primitive. Only the final engine invocation is substituted.
#define ReadProcessMemory TeleportTestHost::Read
#define GetModuleHandleW  PoolTestHost::Module
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#define MTA_DE_PED_TELEPORT_NATIVE_INVOKE TeleportTestHost::Invoke
#include "../../Client/game_de/NativePedTeleportMemory.cpp"
#undef MTA_DE_PED_TELEPORT_NATIVE_INVOKE
#undef GetModuleHandleW
#undef ReadProcessMemory

namespace
{
    void TeleportImageSections(OwnedImage& image)
    {
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
        // Existing readers inspect actual section policies as well as OS page
        // identity. Expand this owned synthetic PE's proof regions so the GT,
        // GC and GetWorld text can qualify without weakening shipping guards.
        image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
        image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
    }

    struct TeleportPool
    {
        unsigned char* header = nullptr;
        unsigned char* objects = nullptr;
        unsigned char* flags = nullptr;
        OwnedImage&    image;
        std::uintptr_t root;
        std::size_t    capacity, stride;
        TeleportPool(OwnedImage& image, std::uintptr_t root, std::size_t capacity, std::size_t stride)
            : image(image), root(root), capacity(capacity), stride(stride)
        {
            header = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, capacity * stride, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, capacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(header && objects && flags, "own exact separate native-style dependency pool allocations");
            Reset();
        }
        template <typename T>
        void Put(std::size_t offset, T value)
        {
            std::memcpy(header + offset, &value, sizeof(value));
        }
        void Reset()
        {
            Put(0, reinterpret_cast<std::uintptr_t>(objects));
            Put(8, reinterpret_cast<std::uintptr_t>(flags));
            Put(0x10, static_cast<std::int32_t>(capacity));
            Put(0x14, std::int32_t{-1});
            Put(0x18, static_cast<unsigned char>(1));
            std::memset(flags, 0x80, capacity);
            const auto pointer = reinterpret_cast<std::uintptr_t>(header);
            image.Patch(root, &pointer, sizeof(pointer), PAGE_READWRITE);
        }
        ~TeleportPool()
        {
            VirtualFree(flags, 0, MEM_RELEASE);
            VirtualFree(objects, 0, MEM_RELEASE);
            VirtualFree(header, 0, MEM_RELEASE);
        }
    };

    struct TeleportFixture
    {
        using Object = std::array<unsigned char, 0x40>;
        TransformFixture transform;
        TeleportPool     intelligence, pointers, entries;
        alignas(std::uintptr_t) Object meta{}, actorClass{}, componentClass{}, levelClass{}, worldClass{}, derivedActorClass{};
        alignas(std::uintptr_t) Object component{}, world{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x300> actor{};
        alignas(std::uintptr_t) std::array<unsigned char, 0xd8> level{};
        std::array<std::array<std::uintptr_t, 2>, 5>                               chains{};
        std::array<unsigned char, NativeStartupWorldProfile::objectItemBytes * 16> items{};
        std::array<std::uintptr_t, 1>                                              chunks{};
        std::size_t                                                                count = 0, actorIndex = 0;
        static constexpr std::uintptr_t ActorClassCache = 0x5726c28, ComponentClassCache = 0x5727c38, ActorVtable = 0x4200a30;
        explicit TeleportFixture(const std::filesystem::path& directory)
            : transform(directory),
              intelligence(transform.image, 0x572b530, 1000, 0x440),
              pointers(transform.image, 0x572b548, 7500, 0x18),
              entries(transform.image, 0x572b5a8, 600, 0x28)
        {
            auto& image = transform.image;
            TeleportImageSections(image);
            for (const auto& pin : NativePedTeleportProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativePedTeleportProfile::Names)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_READONLY);
            for (const auto& slot : NativePedTeleportProfile::Slots)
            {
                const auto target = image.image + slot.targetRva;
                image.Patch(slot.rva, &target, sizeof(target), PAGE_READONLY);
            }
            for (const auto& pin : NativeGameStateProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeBorrowedGcProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartGameProfile::Code)
                if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                    image.Patch(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeUObjectMembershipProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeStartupWorldProfile::pins)
                image.Patch(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            const auto getter = image.image + NativeStartupWorldProfile::getterRva;
            const auto method = image.image + NativePedPoolProfile::Code[1].rva;
            image.Patch(ActorVtable, &method, sizeof(method), PAGE_READONLY);
            image.Patch(ActorVtable + 0x158, &getter, sizeof(getter), PAGE_READONLY);
            image.Patch(NativeStartupWorldProfile::ownerGetWorldSlotRva, &getter, sizeof(getter), PAGE_READONLY);
            Reset();
        }
        template <class T>
        static std::uintptr_t Address(const T& value)
        {
            return reinterpret_cast<std::uintptr_t>(value.data());
        }
        template <class T, class V>
        static void Store(T& object, std::size_t offset, const V& value)
        {
            std::memcpy(reinterpret_cast<unsigned char*>(object.data()) + offset, &value, sizeof(value));
        }
        template <class V>
        void Global(std::uintptr_t rva, const V& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        template <class T>
        std::size_t Register(T& object, std::uintptr_t type)
        {
            const auto index = count++;
            Check(index < 16, "bounded callback-local actor graph stays within its real owned registry chunk");
            Store(object, 0, transform.image.image + ActorVtable);
            Store(object, 0xc, static_cast<std::int32_t>(index));
            Store(object, 0x10, type);
            Store(items, index * NativeStartupWorldProfile::objectItemBytes, Address(object));
            Store(items, index * NativeStartupWorldProfile::objectItemBytes + 0x10, static_cast<std::uint32_t>(100 + index));
            return index;
        }
        void Type(Object& type, std::size_t index)
        {
            chains[index][0] = Address(type) + 0x30;
            Store(type, 0x30, Address(chains[index]));
            Store(type, 0x38, std::int32_t{0});
        }
        void Reset(bool linked = true)
        {
            transform.Reset(linked);
            intelligence.Reset();
            pointers.Reset();
            entries.Reset();
            std::memset(intelligence.objects, 0, intelligence.capacity * intelligence.stride);
            std::memset(pointers.objects, 0, pointers.capacity * pointers.stride);
            std::memset(entries.objects, 0, entries.capacity * entries.stride);
            for (auto* object : {&meta, &actorClass, &componentClass, &levelClass, &worldClass, &derivedActorClass, &component, &world})
                object->fill(0);
            actor.fill(0);
            level.fill(0);
            items.fill(0);
            chains = {};
            count = 0;
            Register(meta, Address(meta));
            for (auto* type : {&actorClass, &componentClass, &levelClass, &worldClass, &derivedActorClass})
                Register(*type, Address(meta));
            actorIndex = Register(actor, Address(actorClass));
            Register(component, Address(componentClass));
            Register(level, Address(levelClass));
            Register(world, Address(worldClass));
            Type(actorClass, 0);
            Type(componentClass, 1);
            Type(levelClass, 2);
            Type(worldClass, 3);
            chains[4][0] = Address(actorClass) + 0x30;
            chains[4][1] = Address(derivedActorClass) + 0x30;
            Store(derivedActorClass, 0x30, Address(chains[4]));
            Store(derivedActorClass, 0x38, std::int32_t{1});
            Store(actor, 0x20, Address(level));
            Store(actor, 0x130, Address(component));
            Store(actor, 0x298, Address(component));
            Store(component, 0x20, Address(actor));
            Store(level, NativeStartupWorldProfile::levelWorldOffset, Address(world));
            intelligence.flags[0] = 3;
            const auto ped = reinterpret_cast<std::uintptr_t>(transform.pool.objects);
            std::memcpy(intelligence.objects, &ped, sizeof(ped));
            std::memcpy(intelligence.objects + 0x60, &ped, sizeof(ped));
            transform.Store(0x5e8, reinterpret_cast<std::uintptr_t>(intelligence.objects));
            transform.Store(0x20, Address(actor));
            transform.Store(0x30, std::uint32_t{0});
            transform.Store(0x6a, static_cast<unsigned char>(3));
            transform.Store(0x7e0, std::uint32_t{0});
            transform.Store(0x138, std::uintptr_t{0});
            transform.Store(0xe8, std::uintptr_t{0});
            transform.Store(0xf0, std::uintptr_t{0});
            chunks[0] = Address(items);
            Global(NativeStartupWorldProfile::objectChunksRva, Address(chunks));
            Global(NativeStartupWorldProfile::objectCountRva, static_cast<std::int32_t>(count));
            Global(NativeStartupWorldProfile::levelClassCacheRva, Address(levelClass));
            Global(ActorClassCache, Address(actorClass));
            Global(ComponentClassCache, Address(componentClass));
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            Global(0x53d3b18, std::uintptr_t{0});
            TeleportTestHost::calls = 0;
            TeleportTestHost::nativeResult = ClockResult::Ok;
            TeleportTestHost::applyEffect = true;
            TeleportTestHost::reenter = false;
            TeleportTestHost::after = nullptr;
            TeleportTestHost::watched = 0;
            TeleportTestHost::watchMatches = TeleportTestHost::revokeAtMatch = TeleportTestHost::matchesAtEntry = 0;
        }
        void Lists()
        {
            const auto ped = reinterpret_cast<std::uintptr_t>(transform.pool.objects);
            const auto node = reinterpret_cast<std::uintptr_t>(pointers.objects);
            const auto moving = node + pointers.stride;
            const auto entry = reinterpret_cast<std::uintptr_t>(entries.objects);
            const auto sector = transform.image.image + 0x5423ea0;
            const auto head = sector + 8;
            pointers.flags[0] = 4;
            pointers.flags[1] = 5;
            entries.flags[0] = 6;
            std::memcpy(pointers.objects, &ped, sizeof(ped));
            std::memcpy(pointers.objects + pointers.stride, &ped, sizeof(ped));
            std::memcpy(entries.objects, &head, sizeof(head));
            std::memcpy(entries.objects + 8, &node, sizeof(node));
            std::memcpy(entries.objects + 0x10, &sector, sizeof(sector));
            transform.Store(0xe8, entry);
            transform.Store(0xf0, moving);
            Global(0x5423ea0 + 8, node);
            Global(0x53d3b18, moving);
        }
    };

    TeleportFixture* changingTeleport = nullptr;
    unsigned         teleportMutation = 0;
    void             ChangeTeleport()
    {
        auto& fixture = *changingTeleport;
        if (teleportMutation == 0)
            fixture.transform.pool.flags[0] = 2;
        if (teleportMutation == 1)
            fixture.transform.Matrix(NativePedTransformProfile::MatrixOwnerOffset, std::uintptr_t{0});
        if (teleportMutation == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(fixture.intelligence.objects + 0x60, &zero, sizeof(zero));
        }
        if (teleportMutation == 3)
            PoolTestHost::ownerResult = ClockResult::WrongThread;
        if (teleportMutation == 4)
            TeleportFixture::Store(fixture.items, fixture.actorIndex * NativeStartupWorldProfile::objectItemBytes + 8, std::uint32_t{0x30000000});
        if (teleportMutation == 5)
            fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
        if (teleportMutation == 6)
            fixture.intelligence.Put(0x14, std::int32_t{7});
    }
}

int main(int argc, char** argv)
{
    using Memory = NativePedTeleportMemory;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native teleport evidence uses an explicit workspace directory");
    TeleportFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const NativePedPoolReader::Binding binding{fixture.transform.image.image};
    Memory                             memory(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    TeleportTestHost::active = &memory;
    const CVector   target(123.25f, -456.5f, 789.75f);
    Memory::Receipt receipt;
    Check(unowned.Apply(1, target, &receipt) == ClockResult::NotInitialized && !receipt.entered && !TeleportTestHost::calls,
          "an image/ped handle cannot replace real borrowed callback authority");
    Check(memory.Apply(1, target, &receipt) == ClockResult::Ok && receipt.handle == 1 && receipt.entered && receipt.returned && receipt.afterObserved &&
              receipt.nativePed == reinterpret_cast<std::uintptr_t>(fixture.transform.pool.objects) &&
              receipt.intelligence == reinterpret_cast<std::uintptr_t>(fixture.intelligence.objects) &&
              receipt.matrixBefore == reinterpret_cast<std::uintptr_t>(fixture.transform.matrix) && receipt.matrixAfter == receipt.matrixBefore &&
              TeleportTestHost::calls == 1 && TeleportTestHost::receiver == receipt.nativePed && TeleportTestHost::entry == receipt.entry &&
              receipt.entry == binding.image + NativePedTransformProfile::NativeTeleportRva && TeleportTestHost::argument &&
              TeleportTestHost::requested.fX == target.fX && TeleportTestHost::requested.fY == target.fY && TeleportTestHost::requested.fZ == target.fZ,
          "actual matrix-backed ped/native world dependencies admit one typed true-bool teleport and an honest post-call receipt");
    CVector observed;
    std::memcpy(&observed, fixture.transform.matrix + 0x30, sizeof(observed));
    Check(observed.fX == target.fX && observed.fY == target.fY && observed.fZ == target.fZ,
          "the native boundary effect remains visible without a frame writer or SDK shadow vector");
    fixture.Reset();
    fixture.Lists();
    Check(memory.Apply(1, target, &receipt) == ClockResult::Ok && TeleportTestHost::calls == 1,
          "genuine occupied sector EntryInfo and moving PtrNode identities qualify their native reciprocal-list contract");
    fixture.Reset();
    TeleportFixture::Store(fixture.actor, 0x10, TeleportFixture::Address(fixture.derivedActorClass));
    Check(memory.Apply(1, target, &receipt) != ClockResult::Ok && !receipt.entered && !TeleportTestHost::calls,
          "an unproved actor subclass cannot inherit the exact constructor/component action contract merely from IsChildOf");
    fixture.Reset();
    fixture.transform.Store(0x7e0, std::uint32_t{1});
    Check(memory.Apply(1, target, &receipt) == ClockResult::Ok && TeleportTestHost::calls == 1,
          "the native player selector1 is supported through the actual known actor without demanding an ambient CJ");
    fixture.Reset(false);
    Check(memory.Apply(1, target, &receipt) == ClockResult::NotInitialized && !receipt.entered && !TeleportTestHost::calls,
          "inline-only classic placement does not masquerade as the matrix-backed native Teleport branch");
    for (unsigned component = 0; component < 3; ++component)
    {
        fixture.Reset();
        CVector invalid = target;
        if (component == 0)
            invalid.fX = (std::numeric_limits<float>::quiet_NaN)();
        if (component == 1)
            invalid.fY = (std::numeric_limits<float>::infinity)();
        if (component == 2)
            invalid.fZ = -(std::numeric_limits<float>::infinity)();
        Check(memory.Apply(1, invalid, &receipt) == ClockResult::InvalidArgument && !receipt.entered && !TeleportTestHost::calls,
              "a copied nonfinite native coordinate is refused before any native action");
    }
    fixture.Reset();
    Check(memory.Apply(0xdeadbeef, target, &receipt) != ClockResult::Ok && !receipt.entered && !TeleportTestHost::calls,
          "arbitrary or stale script handles never substitute an ambient player receiver");
    for (unsigned refusal = 0; refusal < 19; ++refusal)
    {
        fixture.Reset();
        switch (refusal)
        {
            case 0:
                fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
                break;
            case 1:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
                break;
            case 2:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{7});
                break;
            case 3:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
                break;
            case 4:
                fixture.transform.Store(0x30, std::uint32_t{0x100});
                break;
            case 5:
                fixture.transform.Store(0x6a, static_cast<unsigned char>(4));
                break;
            case 6:
                fixture.transform.Store(0x7e0, std::uint32_t{2});
                break;
            case 7:
                fixture.transform.Store(0x138, std::uintptr_t{1});
                break;
            case 8:
                fixture.intelligence.flags[0] = 0x80;
                break;
            case 9:
                fixture.intelligence.Put(0x10, std::int32_t{999});
                break;
            case 10:
            {
                const std::uintptr_t zero = 0;
                std::memcpy(fixture.intelligence.objects + 0x60, &zero, sizeof(zero));
                break;
            }
            case 11:
                fixture.Global(0x572b548, std::uintptr_t{0});
                break;
            case 12:
                fixture.entries.Put(0x18, static_cast<unsigned char>(0));
                break;
            case 13:
                TeleportFixture::Store(fixture.actor, 0x130, std::uintptr_t{0});
                break;
            case 14:
                TeleportFixture::Store(fixture.component, 0x20, std::uintptr_t{0});
                break;
            case 15:
                TeleportFixture::Store(fixture.actor, 0x10, TeleportFixture::Address(fixture.worldClass));
                break;
            case 16:
                TeleportFixture::Store(fixture.items, fixture.actorIndex * NativeStartupWorldProfile::objectItemBytes + 8, std::uint32_t{0x30000000});
                break;
            case 17:
                TeleportFixture::Store(fixture.level, NativeStartupWorldProfile::levelWorldOffset, std::uintptr_t{0});
                break;
            case 18:
                fixture.transform.Matrix(NativePedTransformProfile::MatrixOwnerOffset, std::uintptr_t{0});
                break;
        }
        Check(memory.Apply(1, target, &receipt) != ClockResult::Ok && !receipt.entered && !TeleportTestHost::calls,
              "native thread/GC/playing scope, physical/intelligence pools, consumed objects and selected-matrix ownership each refuse before entry");
        if (refusal == 2)
        {
            LONG marker = 0;
            std::memcpy(&marker, reinterpret_cast<void*>(binding.image + NativeBorrowedGcProfile::markerRva), sizeof(marker));
            Check(marker == 7, "the native atomic GC observation preserves an existing foreign marker");
        }
    }
    fixture.Reset();
    fixture.Lists();
    const auto entry = reinterpret_cast<std::uintptr_t>(fixture.entries.objects);
    std::memcpy(fixture.entries.objects + 0x20, &entry, sizeof(entry));
    Check(memory.Apply(1, target, &receipt) != ClockResult::Ok && !receipt.entered && !TeleportTestHost::calls,
          "an occupied but cyclic native entry chain cannot become a borrowed world-list lease");
    fixture.Reset();
    TeleportTestHost::watched = reinterpret_cast<std::uintptr_t>(fixture.intelligence.header) + 0x14;
    Check(memory.Apply(1, target, &receipt) == ClockResult::Ok && TeleportTestHost::matchesAtEntry > 0,
          "measure the final actual dependency ledger read before native entry");
    const auto lastMatch = TeleportTestHost::matchesAtEntry;
    fixture.Reset();
    TeleportTestHost::watched = reinterpret_cast<std::uintptr_t>(fixture.intelligence.header) + 0x14;
    TeleportTestHost::revokeAtMatch = lastMatch;
    Check(memory.Apply(1, target, &receipt) == ClockResult::WrongThread && !receipt.entered && !TeleportTestHost::calls,
          "owner revocation during the final dependency fence is rechecked immediately before native invocation");
    fixture.Reset();
    TeleportTestHost::reenter = true;
    Check(memory.Apply(1, target, &receipt) == ClockResult::Ok && TeleportTestHost::calls == 1 && TeleportTestHost::nestedResult == ClockResult::Busy &&
              !TeleportTestHost::nestedReceipt.entered,
          "same-thread recursive teleport intent is refused instead of entering the action twice");
    fixture.Reset();
    TeleportTestHost::nativeResult = ClockResult::OriginalException;
    Check(memory.Apply(1, target, &receipt) == ClockResult::OriginalException && receipt.entered && !receipt.returned &&
              receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && TeleportTestHost::calls == 1,
          "an exception receipt reports the already-entered native boundary without an automatic retry");
    std::memcpy(&observed, fixture.transform.matrix + 0x30, sizeof(observed));
    Check(observed.fX == target.fX, "entered native placement effects survive an exception acknowledgement failure");
    changingTeleport = &fixture;
    for (teleportMutation = 0; teleportMutation < 7; ++teleportMutation)
    {
        fixture.Reset();
        TeleportTestHost::after = &ChangeTeleport;
        const auto result = memory.Apply(1, target, &receipt);
        Check((teleportMutation == 6 ? result == ClockResult::Ok && receipt.afterObserved : result != ClockResult::Ok && !receipt.afterObserved) &&
                  receipt.entered && receipt.returned && TeleportTestHost::calls == 1,
              "fresh post-call dependencies admit a legitimate allocator cursor advance and honestly refuse recycled/changed native identity or owner");
        std::memcpy(&observed, fixture.transform.matrix + 0x30, sizeof(observed));
        Check(observed.fX == target.fX && observed.fY == target.fY && observed.fZ == target.fZ,
              "post-call refusal preserves the entered placement effect instead of rewinding an old object snapshot");
    }
    PoolTestHost::image = 0;
    std::printf("Passed %u native ped-teleport checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
