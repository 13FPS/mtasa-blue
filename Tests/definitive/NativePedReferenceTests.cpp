/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verified native pointer-cell publication and conservative lifetime
 ****************************************************************************/
#define main NativePedReferenceTransformFixtureMain
#include "NativePedTransformReaderTests.cpp"
#undef main
#include "../../Client/game_de/NativePedReferenceMemory.h"
#include "../../Client/game_de/NativePedReferenceProfile.h"
#include <type_traits>

namespace ReferenceTestHost
{
    using Memory = mta::game_de::NativePedReferenceMemory;
    unsigned                  registers = 0, unregisters = 0;
    std::uintptr_t            image = 0, entity = 0, cellAddress = 0, entry = 0;
    bool                      force = false, registerEffect = true, unregisterEffect = true, leaveCellAddress = false;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok, recoveryResult = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    mta::game_de::NativePedReferenceCell* querying = nullptr;
    bool                                  busyObserved = false;
    template <class T>
    T Get(std::uintptr_t at)
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void*>(at), sizeof(value));
        return value;
    }
    template <class T>
    void Put(std::uintptr_t at, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(at), &value, sizeof(value));
    }
    mta::game_de::ClockResult Recover() noexcept
    {
        return recoveryResult;
    }
    void InspectBusy()
    {
        mta::game_de::NativePedReferenceCell::State output;
        output.cellAddress = 123;
        output.generation = 867;
        busyObserved = querying && !querying->ReadState(output) && output.cellAddress == 123 && output.generation == 867 && querying->IsRetained() &&
                       querying->HasReferences();
    }
    mta::game_de::ClockResult Register(Memory::RegisterFunction function, void* target, void** cell, bool forced, std::uint32_t& exception) noexcept
    {
        namespace P = mta::game_de::NativePedReferenceProfile;
        ++registers;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        entity = reinterpret_cast<std::uintptr_t>(target);
        cellAddress = reinterpret_cast<std::uintptr_t>(cell);
        force = forced;
        if (registerEffect)
        {
            const auto node = Get<std::uintptr_t>(image + P::FreeHeadRva);
            if (node)
            {
                Put(image + P::FreeHeadRva, Get<std::uintptr_t>(node));
                Put(node, Get<std::uintptr_t>(entity + P::EntityHeadOffset));
                Put(node + 8, cellAddress);
                Put(entity + P::EntityHeadOffset, node);
            }
        }
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
    mta::game_de::ClockResult Unregister(Memory::UnregisterFunction function, void* target, void** cell, std::uint32_t& exception) noexcept
    {
        namespace P = mta::game_de::NativePedReferenceProfile;
        ++unregisters;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        entity = reinterpret_cast<std::uintptr_t>(target);
        cellAddress = reinterpret_cast<std::uintptr_t>(cell);
        if (unregisterEffect)
        {
            auto link = entity + P::EntityHeadOffset;
            auto node = Get<std::uintptr_t>(link);
            for (std::size_t count = 0; node && count < P::ReferenceCount; ++count)
            {
                if (Get<std::uintptr_t>(node + 8) == cellAddress)
                {
                    Put(link, Get<std::uintptr_t>(node));
                    Put(node, Get<std::uintptr_t>(image + P::FreeHeadRva));
                    Put(image + P::FreeHeadRva, node);
                    if (!leaveCellAddress)
                        Put(node + 8, std::uintptr_t{0});
                    break;
                }
                link = node;
                node = Get<std::uintptr_t>(node);
            }
        }
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
    void Reset()
    {
        registers = unregisters = 0;
        entity = cellAddress = entry = 0;
        force = false;
        registerEffect = unregisterEffect = true;
        leaveCellAddress = false;
        result = recoveryResult = mta::game_de::ClockResult::Ok;
        after = nullptr;
        querying = nullptr;
        busyObserved = false;
    }
}
// Only the final typed registrar/unregistrar boundaries are substituted. All
// native pool, fixed-bank, owner and stable allocation checks remain real.
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#define MTA_DE_PED_REFERENCE_REGISTER_INVOKE   ReferenceTestHost::Register
#define MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE ReferenceTestHost::Unregister
#include "../../Client/game_de/NativePedReferenceMemory.cpp"
#undef MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE
#undef MTA_DE_PED_REFERENCE_REGISTER_INVOKE
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace
{
    struct ReferenceFixture
    {
        TransformFixture transform;
        explicit ReferenceFixture(const std::filesystem::path& directory) : transform(directory)
        {
            auto&              image = transform.image;
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, reinterpret_cast<void*>(image.image + 0x80), sizeof(nt));
            nt.FileHeader.NumberOfSections = 3;
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            const std::array<std::uintptr_t, 4> boundary{0x1000, 0x3f40000, 0x5000000, (NativePedPoolProfile::ImageSize + 0xfff) & ~std::uintptr_t{0xfff}};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto& section = sections[i];
                std::memcpy(section.Name, i == 0 ? ".text" : i == 1 ? ".rdata" : ".data", i == 1 ? 6 : 5);
                section.VirtualAddress = static_cast<DWORD>(boundary[i]);
                section.Misc.VirtualSize = static_cast<DWORD>(boundary[i + 1] - boundary[i]);
                section.SizeOfRawData = 0x200;
                section.PointerToRawData = static_cast<DWORD>(0x400 + i * 0x200);
                section.Characteristics = IMAGE_SCN_MEM_READ | (i == 0 ? IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
                                                                       : IMAGE_SCN_CNT_UNINITIALIZED_DATA | (i == 2 ? IMAGE_SCN_MEM_WRITE : 0));
            }
            image.Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            image.Patch(0x80 + sizeof(nt), sections.data(), sizeof(sections), PAGE_READONLY);
            for (const auto& pin : NativePedReferenceProfile::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& slot : NativePedReferenceProfile::Slots)
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
            Reset();
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value)
        {
            transform.image.Patch(rva, &value, sizeof(value), PAGE_READWRITE);
        }
        void Reset()
        {
            namespace P = NativePedReferenceProfile;
            transform.Reset(false);
            transform.Store(0x6a, static_cast<unsigned char>(3));
            transform.Store(P::EntityHeadOffset, std::uintptr_t{0});
            std::vector<unsigned char> nodes(P::ReferenceCount * P::ReferenceBytes);
            for (std::size_t i = 0; i + 1 < P::ReferenceCount; ++i)
            {
                const auto next = transform.image.image + P::ReferencesRva + (i + 1) * P::ReferenceBytes;
                std::memcpy(nodes.data() + i * P::ReferenceBytes, &next, sizeof(next));
            }
            transform.image.Patch(P::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
            Global(P::FreeHeadRva, transform.image.image + P::ReferencesRva);
            Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
            Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
            Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
            ReferenceTestHost::Reset();
        }
        std::uintptr_t Ped() const { return reinterpret_cast<std::uintptr_t>(transform.pool.objects); }
        // Reproduce the separately proved Resolve postcondition over only this
        // owned cell. Clearing the word without clearing its node is a distinct
        // test state and never an allocation-release acknowledgement.
        void Resolve(const NativePedReferenceCell::State& state)
        {
            namespace P = NativePedReferenceProfile;
            if (ReferenceTestHost::Get<std::uintptr_t>(state.cellAddress) == state.nativePed)
                ReferenceTestHost::Put(state.cellAddress, std::uintptr_t{0});
            auto node = state.node;
            if (!node)
                for (std::size_t i = 0; i < P::ReferenceCount; ++i)
                {
                    const auto candidate = transform.image.image + P::ReferencesRva + i * P::ReferenceBytes;
                    if (ReferenceTestHost::Get<std::uintptr_t>(candidate + 8) == state.cellAddress)
                    {
                        node = candidate;
                        break;
                    }
                }
            if (node)
            {
                ReferenceTestHost::Put(node + 8, std::uintptr_t{0});
                const auto head = ReferenceTestHost::Get<std::uintptr_t>(transform.image.image + P::FreeHeadRva);
                if (head != node)
                {
                    ReferenceTestHost::Put(node, head);
                    ReferenceTestHost::Put(transform.image.image + P::FreeHeadRva, node);
                }
            }
            ReferenceTestHost::Put(Ped() + P::EntityHeadOffset, std::uintptr_t{0});
        }
    };
    NativePedReferenceCell::State CellState(const NativePedReferenceCell& cell)
    {
        NativePedReferenceCell::State state;
        Check(cell.ReadState(state), "stable CPU ownership metadata can be sampled outside a native operation");
        return state;
    }
    void CellFreed(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION page{};
        Check(::VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) == sizeof(page) && page.State == MEM_FREE,
              "release acknowledgement actually frees only the separately owned native-visible cell allocation");
    }
}

#include "NativePedReferenceControllerTests.inl"

int main(int argc, char** argv)
{
    using Memory = NativePedReferenceMemory;
    using Cell = NativePedReferenceCell;
    namespace R = NativePedReferenceProfile;
    static_assert(!std::is_copy_constructible_v<Cell> && !std::is_move_constructible_v<Cell>);
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "native reference evidence stays in the explicit workspace output directory");
    Memory::Binding refused{123};
    Check(!Memory::Qualify(refused) && refused.image == 123, "shipping native reference qualification refuses this actual non-GTA executable");
    ReferenceFixture fixture(argv[1]);
    PoolTestHost::image = ReferenceTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &PoolTestHost::Owner, &ReferenceTestHost::Recover, 42);
    Memory                unowned(binding, nullptr, &ReferenceTestHost::Recover, 42);
    Memory                otherGeneration(binding, &PoolTestHost::Owner, &ReferenceTestHost::Recover, 43);
    Memory::Receipt       receipt;
    {
        Cell cell;
        Check(unowned.Register(1, cell, &receipt) != ClockResult::Ok && !receipt.entered && !cell.IsRetained() && !ReferenceTestHost::registers,
              "native pool/image identity cannot replace a real owner or publish an unowned stable cell");
        Check(Memory::Revalidate(binding), "exact native registration, exhaustion, unregister and resolve protocol pins qualify on owned image pages");
        ReferenceTestHost::querying = &cell;
        ReferenceTestHost::after = &ReferenceTestHost::InspectBusy;
        Check(memory.Register(1, cell, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.registrationVerified &&
                  cell.IsRetained() && cell.HasReferences() && ReferenceTestHost::registers == 1 && ReferenceTestHost::force &&
                  ReferenceTestHost::entry == R::RegisterRva && ReferenceTestHost::entity == fixture.Ped(),
              "one actual native registration boundary must publish a verified fixed-bank node for a separately retained stable cell");
        Check(ReferenceTestHost::busyObserved,
              "atomic retention is visible inside native entry while a competing CPU state query refuses without changing its output");
        const auto               state = CellState(cell);
        MEMORY_BASIC_INFORMATION page{};
        Check(state.cellAddress == receipt.cell && state.nativePed == fixture.Ped() && state.handle == 1 && state.generation == 42 &&
                  state.registrationVerified && !state.uncertain && state.node == receipt.node && state.allocationBase && state.allocationBytes >= 8 &&
                  ::VirtualQuery(reinterpret_cast<void*>(state.cellAddress), &page, sizeof(page)) == sizeof(page) && page.Type == MEM_PRIVATE &&
                  page.State == MEM_COMMIT && page.Protect == PAGE_READWRITE && reinterpret_cast<std::uintptr_t>(page.AllocationBase) == state.allocationBase &&
                  ReferenceTestHost::Get<std::uintptr_t>(state.cellAddress) == fixture.Ped(),
              "registered metadata names the real native ped, exact node and isolated writable allocation rather than caller-stack pointer storage");
        NativePedPoolReader::Value value;
        value.handle = 0xdeadbeef;
        value.nativePed = 123;
        Check(memory.Read(cell, value) == ClockResult::Ok && value.handle == 1 && value.nativePed == fixture.Ped(),
              "a verified native cell resolves its same-generation raw ped observation without constructing a CPed wrapper");
        const auto calls = ReferenceTestHost::registers;
        memory.Register(2, cell, &receipt);
        Check(ReferenceTestHost::registers == calls && CellState(cell).cellAddress == state.cellAddress,
              "an already-owned cell cannot be silently rebound to another handle or replay registration");
        Check(otherGeneration.Release(cell, &receipt) != ClockResult::Ok && cell.IsRetained() && !ReferenceTestHost::unregisters,
              "a different owner generation cannot retire registered storage using an old native handle");
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && receipt.entered && receipt.returned && receipt.bankAbsent && receipt.allocationReleased &&
                  !cell.IsRetained() && !cell.HasReferences() && ReferenceTestHost::unregisters == 1 && ReferenceTestHost::entry == R::UnregisterRva &&
                  ReferenceTestHost::cellAddress == state.cellAddress,
              "native unregistration uses the exact registered cell address and frees only after verified whole-bank absence");
        CellFreed(state.cellAddress);
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && ReferenceTestHost::registers == calls && ReferenceTestHost::unregisters == 1,
              "acknowledged release is idempotent and never replays the native unregister action");
    }
    fixture.Reset();
    {
        Cell first, second;
        Check(memory.Register(1, first, &receipt) == ClockResult::Ok && memory.Register(1, second, &receipt) == ClockResult::Ok,
              "two independent stable cells may register the same current native ped");
        const auto a = CellState(first), b = CellState(second);
        Check(a.cellAddress != b.cellAddress && a.node != b.node && memory.Release(first, &receipt) == ClockResult::Ok &&
                  ReferenceTestHost::cellAddress == a.cellAddress && second.IsRetained() && ReferenceTestHost::Get<std::uintptr_t>(b.node + 8) == b.cellAddress,
              "unregister matches exact pointer-cell identity, not merely equal native ped values");
        Check(memory.Release(second, &receipt) == ClockResult::Ok, "the other native reference retains independent lifetime and releases separately");
    }
    fixture.Reset();
    {
        Cell cell;
        fixture.Global(R::FreeHeadRva, std::uintptr_t{0});
        Check(memory.Register(1, cell, &receipt) != ClockResult::Ok && !receipt.entered && !ReferenceTestHost::registers && !cell.HasReferences(),
              "exhausted native reference storage refuses before entering the five-pool pruning registrar");
        if (cell.IsRetained())
            Check(memory.Release(cell, &receipt) == ClockResult::Ok, "unpublished exhausted-attempt storage needs an acknowledged release");
    }
    fixture.Reset();
    {
        Cell cell;
        ReferenceTestHost::registerEffect = false;
        Check(memory.Register(1, cell, &receipt) != ClockResult::Ok && !receipt.registrationVerified && ReferenceTestHost::registers == 1,
              "a void native registrar return without a node cannot be mistaken for successful registration");
        const auto state = CellState(cell);
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && !ReferenceTestHost::unregisters,
              "only verified bank absence can retire an unpublished registrar attempt without a fabricated unregister call");
        if (state.cellAddress)
            CellFreed(state.cellAddress);
    }
    fixture.Reset();
    {
        Cell cell;
        Check(memory.Register(1, cell, &receipt) == ClockResult::Ok, "seed one verified reference before an interrupted native retirement");
        const auto state = CellState(cell);
        ReferenceTestHost::Put(state.cellAddress, std::uintptr_t{0});
        Check(memory.Release(cell, &receipt) != ClockResult::Ok && cell.IsRetained() && cell.HasReferences() && !receipt.allocationReleased,
              "a zero pointer word does not release storage while a fixed native node still names its cell address");
        Check(ReferenceTestHost::Get<std::uintptr_t>(state.node + 8) == state.cellAddress,
              "the original native cell-address publication remains observable independently of its zero value");
        fixture.Resolve(state);
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && receipt.bankAbsent && receipt.allocationReleased && !cell.IsRetained(),
              "completed native resolution clears the node address before same-generation recovery can release its stable allocation");
        CellFreed(state.cellAddress);
    }
    fixture.Reset();
    {
        Cell cell;
        Check(memory.Register(1, cell, &receipt) == ClockResult::Ok, "seed a reference before a native fixed-bank reset");
        const auto state = CellState(cell);
        fixture.Reset();
        NativePedPoolReader::Value output;
        output.handle = 0xdeadbeef;
        output.nativePed = 123;
        Check(ReferenceTestHost::Get<std::uintptr_t>(state.cellAddress) == state.nativePed && memory.Read(cell, output) != ClockResult::Ok &&
                  output.handle == 0xdeadbeef && output.nativePed == 123,
              "a native bank reset loses registration even when the external word still contains the same apparently live ped address");
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && receipt.bankAbsent && !ReferenceTestHost::unregisters,
              "bank-absent reset storage retires without calling an old native handle or pretending it remains registered");
        CellFreed(state.cellAddress);
    }
    fixture.Reset();
    {
        Cell cell;
        Check(memory.Register(1, cell, &receipt) == ClockResult::Ok, "seed a verified cell before partial native unregistration");
        const auto state = CellState(cell);
        ReferenceTestHost::leaveCellAddress = true;
        ReferenceTestHost::result = ClockResult::OriginalException;
        Check(memory.Release(cell, &receipt) != ClockResult::Ok && receipt.entered && cell.IsRetained() && ReferenceTestHost::unregisters == 1,
              "an entered unregister fault preserves storage after unlinking when the native bank still contains its address");
        ReferenceTestHost::result = ClockResult::Ok;
        ReferenceTestHost::Put(state.cellAddress, std::uintptr_t{0});
        Check(memory.Release(cell, &receipt) != ClockResult::Ok && cell.IsRetained() && ReferenceTestHost::unregisters == 1,
              "an unreachable bank node and zero word still retain debt, without replaying an uncertain unregister");
        ReferenceTestHost::Put(state.node + 8, std::uintptr_t{0});
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && receipt.bankAbsent && receipt.allocationReleased && ReferenceTestHost::unregisters == 1,
              "fresh full-bank absence finally acknowledges uncertain native unregistration without a second native action");
        CellFreed(state.cellAddress);
    }
    fixture.Reset();
    {
        Cell cell;
        ReferenceTestHost::result = ClockResult::OriginalException;
        Check(memory.Register(1, cell, &receipt) != ClockResult::Ok && receipt.entered && cell.IsRetained() && !receipt.registrationVerified,
              "an entered registrar fault never fabricates successful publication or prematurely frees a possibly published cell");
        const auto state = CellState(cell);
        ReferenceTestHost::result = ClockResult::Ok;
        PoolTestHost::ownerResult = ClockResult::Expired;
        ReferenceTestHost::recoveryResult = ClockResult::WrongThread;
        Check(memory.Release(cell, &receipt) != ClockResult::Ok && cell.IsRetained(),
              "an expired normal lease cannot substitute a foreign recovery callback to release uncertain native storage");
        ReferenceTestHost::recoveryResult = ClockResult::Ok;
        fixture.Resolve(state);
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && receipt.allocationReleased && !cell.IsRetained() && !ReferenceTestHost::unregisters,
              "the same-generation real recovery scope may retire verified bank-absent storage without replaying a native action");
        CellFreed(state.cellAddress);
    }
    fixture.Reset();
    {
        Cell cell;
        Check(memory.Register(1, cell, &receipt) == ClockResult::Ok,
              "seed a verified reference before native unregistration changes qualified code provenance");
        const auto state = CellState(cell);
        // The native unlink already happened when this hook changes a pinned
        // byte. Its empty bank cannot authorize freeing storage under a lost
        // profile; later recovery must requalify and never replay that unlink.
        static ReferenceFixture* profileFixture = nullptr;
        profileFixture = &fixture;
        ReferenceTestHost::after = +[]()
        {
            const auto& pin = NativePedReferenceProfile::Code[0];
            const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            profileFixture->transform.image.Patch(pin.rva, &changed, sizeof(changed), PAGE_EXECUTE_READ);
        };
        Check(memory.Release(cell, &receipt) == ClockResult::BindingChanged && receipt.entered && receipt.returned && !receipt.bankAbsent &&
                  !receipt.allocationReleased && cell.IsRetained() && ReferenceTestHost::unregisters == 1,
              "post-unregister profile loss refuses final storage release even after the native node address was removed");
        const auto               retained = CellState(cell);
        MEMORY_BASIC_INFORMATION page{};
        Check(retained.cleanupEntered && retained.uncertain && retained.cellAddress == state.cellAddress &&
                  ReferenceTestHost::Get<std::uintptr_t>(state.node + 8) == 0 &&
                  ::VirtualQuery(reinterpret_cast<void*>(state.cellAddress), &page, sizeof(page)) == sizeof(page) && page.State == MEM_COMMIT,
              "lost release provenance retains the exact cell allocation and sticky entered-cleanup debt after a completed unlink");
        const auto& pin = NativePedReferenceProfile::Code[0];
        fixture.transform.image.Patch(pin.rva, pin.bytes, 1, PAGE_EXECUTE_READ);
        profileFixture = nullptr;
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && !receipt.entered && !receipt.returned && receipt.bankAbsent && receipt.allocationReleased &&
                  !cell.IsRetained() && ReferenceTestHost::unregisters == 1,
              "restored exact provenance releases only through fresh full-bank absence without repeating native unregistration");
        CellFreed(state.cellAddress);
    }
    fixture.Reset();
    {
        Cell cell;
        Check(memory.Register(1, cell, &receipt) == ClockResult::Ok, "seed a healthy reference before native profile loss");
        const auto state = CellState(cell);
        PoolTestHost::image = 0;
        Check(memory.Release(cell, &receipt) != ClockResult::Ok && cell.IsRetained() && !ReferenceTestHost::unregisters,
              "lost current image qualification retains native-visible storage without entering an old unregister target");
        PoolTestHost::image = fixture.transform.image.image;
        fixture.Resolve(state);
        Check(memory.Release(cell, &receipt) == ClockResult::Ok && receipt.allocationReleased,
              "fresh exact binding plus completed native resolution permits a later acknowledged release without trusting the old handle");
        CellFreed(state.cellAddress);
    }
    std::puts("Production native ped reference primitive fixture completed");
    RunReferenceControllerTests(fixture, binding);
    PoolTestHost::image = ReferenceTestHost::image = 0;
    std::printf("Passed %u native ped-reference checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
