/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify actual owned near allocations without executing their bytes
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherRainLeaf.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

namespace
{
    using Leaf = mta::game_de::NativeWeatherRainLeaf;
    namespace Bridge = mta::game_de::NativeWeatherRainBridge;
    unsigned                 checks = 0, reads = 0, writes = 0, allocations = 0, releases = 0;
    unsigned                 failRead = 0, shortRead = 0, failWrite = 0, shortWrite = 0;
    bool                     failQuery = false, failOwnedQuery = false, wrongAllocation = false, wrongType = false;
    bool                     failProtect = false, wrongPrevious = false, failFlush = false, failFree = false, failFirstAllocation = false;
    std::uintptr_t           latestAllocation = 0, freedAddress = 0, denyAbove = 0;
    constexpr std::uintptr_t Image = 0x140000000;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, description);
            std::exit(EXIT_FAILURE);
        }
    }

    void ResetFaults()
    {
        reads = writes = allocations = releases = 0;
        failRead = shortRead = failWrite = shortWrite = 0;
        failQuery = failOwnedQuery = wrongAllocation = wrongType = false;
        failProtect = wrongPrevious = failFlush = failFree = failFirstAllocation = false;
        latestAllocation = freedAddress = denyAbove = 0;
    }

    SIZE_T WINAPI LeafQuery(LPCVOID address, PMEMORY_BASIC_INFORMATION memory, SIZE_T size)
    {
        if (failQuery)
            return 0;
        const auto result = ::VirtualQuery(address, memory, size);
        if (result && latestAllocation && reinterpret_cast<std::uintptr_t>(memory->AllocationBase) == latestAllocation)
        {
            if (failOwnedQuery)
                return 0;
            if (wrongAllocation)
                memory->AllocationBase = reinterpret_cast<void*>(latestAllocation + 0x10000);
            if (wrongType)
                memory->Type = MEM_MAPPED;
        }
        // Only deny free discovery candidates above the call site. The chosen
        // below-site allocation, its protections and all copies remain real.
        if (result && denyAbove && reinterpret_cast<std::uintptr_t>(address) >= denyAbove && memory->State == MEM_FREE)
            memory->State = MEM_RESERVE;
        return result;
    }

    LPVOID WINAPI LeafAllocate(LPVOID address, SIZE_T size, DWORD type, DWORD protection)
    {
        ++allocations;
        if (failFirstAllocation && allocations == 1)
            return nullptr;
        void* result = ::VirtualAlloc(address, size, type, protection);
        if (result)
            latestAllocation = reinterpret_cast<std::uintptr_t>(result);
        return result;
    }

    BOOL WINAPI LeafRead(HANDLE process, LPCVOID address, LPVOID output, SIZE_T size, SIZE_T* copied)
    {
        ++reads;
        if (failRead && reads == failRead)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        if (shortRead && reads == shortRead)
            return ::ReadProcessMemory(process, address, output, size ? size - 1 : 0, copied);
        return ::ReadProcessMemory(process, address, output, size, copied);
    }

    BOOL WINAPI LeafWrite(HANDLE process, LPVOID address, LPCVOID input, SIZE_T size, SIZE_T* copied)
    {
        ++writes;
        if (failWrite && writes == failWrite)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        if (shortWrite && writes == shortWrite)
            return ::WriteProcessMemory(process, address, input, size ? size - 1 : 0, copied);
        return ::WriteProcessMemory(process, address, input, size, copied);
    }

    BOOL WINAPI LeafProtect(LPVOID address, SIZE_T size, DWORD protection, PDWORD prior)
    {
        if (failProtect)
            return FALSE;
        const BOOL result = ::VirtualProtect(address, size, protection, prior);
        if (result && wrongPrevious)
            *prior = PAGE_READONLY;
        return result;
    }

    BOOL WINAPI LeafFlush(HANDLE process, LPCVOID address, SIZE_T size)
    {
        return failFlush ? FALSE : ::FlushInstructionCache(process, address, size);
    }

    BOOL WINAPI LeafFree(LPVOID address, SIZE_T size, DWORD type)
    {
        ++releases;
        freedAddress = reinterpret_cast<std::uintptr_t>(address);
        return failFree ? FALSE : ::VirtualFree(address, size, type);
    }
}

// Include the actual helper once. The seams only inject ordinary Win32 call
// failures or discovery conflicts; no generated code or native Body executes.
#define VirtualQuery          LeafQuery
#define VirtualAlloc          LeafAllocate
#define ReadProcessMemory     LeafRead
#define WriteProcessMemory    LeafWrite
#define VirtualProtect        LeafProtect
#define FlushInstructionCache LeafFlush
#define VirtualFree           LeafFree
#include "../../Client/game_de/NativeWeatherRainLeaf.cpp"
#undef VirtualQuery
#undef VirtualAlloc
#undef ReadProcessMemory
#undef WriteProcessMemory
#undef VirtualProtect
#undef FlushInstructionCache
#undef VirtualFree

namespace
{
    struct Proof
    {
        bool             qualified = true;
        unsigned         calls = 0;
        std::uintptr_t   expectedBase = 0;
        Bridge::Context* expectedContext = nullptr;
    };

    bool Quiescent(void* opaque, const Leaf::State& state) noexcept
    {
        auto& proof = *static_cast<Proof*>(opaque);
        ++proof.calls;
        return proof.qualified && state.allocationBase == proof.expectedBase && state.context == proof.expectedContext;
    }

    Leaf::State Snapshot(const Leaf& leaf)
    {
        Leaf::State state{};
        Check(leaf.Read(state), "read exact retained allocation identity");
        Check(state.allocationBase && state.leafEntry != state.allocationBase && state.context && state.allocationBytes == state.pageBytes * 2,
              "entry and context retain their reservation base separately");
        return state;
    }

    void Retire(Leaf& leaf)
    {
        auto  state = Snapshot(leaf);
        Proof proof{true, 0, state.allocationBase, state.context};
        Check(leaf.Release(Quiescent, &proof), "explicit unpublished fixture quiescence releases the reservation");
        Check(proof.calls == 1 && freedAddress == state.allocationBase && freedAddress != state.leafEntry,
              "release callback receives identity and VirtualFree receives the reservation base");
        MEMORY_BASIC_INFORMATION info{};
        Check(::VirtualQuery(reinterpret_cast<const void*>(state.allocationBase), &info, sizeof(info)) == sizeof(info) && info.State == MEM_FREE,
              "both pages are actually released by the OS");
        Leaf::State preserved{};
        preserved.leafEntry = 123;
        Check(!leaf.Read(preserved) && preserved.leafEntry == 123, "retired helper preserves no-allocation caller output");
    }

    void Inspect(const Leaf::State& state)
    {
        Check(state.prepared && state.failure == Leaf::Failure::None, "successful preparation reports ready bytes");
        MEMORY_BASIC_INFORMATION code{}, data{};
        Check(::VirtualQuery(reinterpret_cast<const void*>(state.leafEntry), &code, sizeof(code)) == sizeof(code) && code.Type == MEM_PRIVATE &&
                  code.State == MEM_COMMIT && code.Protect == PAGE_EXECUTE_READ &&
                  reinterpret_cast<std::uintptr_t>(code.AllocationBase) == state.allocationBase,
              "actual private code is RX and retains the exact allocation base");
        Check(::VirtualQuery(state.context, &data, sizeof(data)) == sizeof(data) && data.Type == MEM_PRIVATE && data.State == MEM_COMMIT &&
                  data.Protect == PAGE_READWRITE && reinterpret_cast<std::uintptr_t>(data.AllocationBase) == state.allocationBase,
              "context uses an independent non-executable RW page of that reservation");
        Check(state.context->nativeRain == Image + Bridge::NativeRainRva && state.context->saveVtable == Image + Bridge::SaveVtableRva &&
                  state.context->loadVtable == Image + Bridge::LoadVtableRva && state.context->saveSerialize == Image + Bridge::SaveSerializeRva &&
                  state.context->loadSerialize == Image + Bridge::LoadSerializeRva && !state.context->activeCalls && !state.context->unqualifiedArchive,
              "all native identities and aligned counters are initialized before publication");
        Check(std::memcmp(reinterpret_cast<const void*>(state.leafEntry), state.plan.bridge.data(), state.plan.bridge.size()) == 0,
              "actual RX bytes match the compiled bridge builder");
        std::int32_t displacement = 0;
        std::memcpy(&displacement, state.plan.entry.data() + 1, sizeof(displacement));
        const auto source = Image + Bridge::EntryRva + 5;
        const auto decoded = displacement >= 0 ? source + static_cast<std::uintptr_t>(displacement)
                                               : source - static_cast<std::uintptr_t>(-static_cast<std::int64_t>(displacement));
        Check(decoded == state.leafEntry, "entry CALL reaches the actual allocation using a signed rel32");
        const auto* bytes = reinterpret_cast<const unsigned char*>(state.allocationBase);
        for (std::size_t index = 0; index != state.pageBytes; ++index)
            if (index < 64 || index >= 64 + state.plan.bridge.size())
                Check(bytes[index] == 0, "helper writes only the exact leaf span in the code page");
    }

    void PreparationFailures()
    {
        for (unsigned operation = 0; operation != 12; ++operation)
        {
            ResetFaults();
            switch (operation)
            {
                case 0:
                    failWrite = 1;
                    break;
                case 1:
                    shortWrite = 1;
                    break;
                case 2:
                    failWrite = 2;
                    break;
                case 3:
                    shortWrite = 2;
                    break;
                case 4:
                    failRead = 1;
                    break;
                case 5:
                    shortRead = 1;
                    break;
                case 6:
                    failProtect = true;
                    break;
                case 7:
                    wrongPrevious = true;
                    break;
                case 8:
                    failFlush = true;
                    break;
                case 9:
                    failRead = 2;
                    break;
                case 10:
                    shortRead = 3;
                    break;
                case 11:
                    failOwnedQuery = true;
                    break;
            }
            Leaf leaf;
            Check(!leaf.Prepare(Image), "injected preparation failure is an honest refusal");
            const auto state = Snapshot(leaf);
            Check(!state.prepared && state.failure != Leaf::Failure::None && releases == 0,
                  "post-allocation preparation failure retains explicit cleanup debt");
            const auto base = state.allocationBase;
            ResetFaults();
            Check(!leaf.Prepare(Image), "retry never overwrites an existing allocation debt");
            Check(Snapshot(leaf).allocationBase == base, "retry preserves exact failed reservation identity");
            Retire(leaf);
        }
    }
}

int main()
{
    static_assert(sizeof(void*) == 8 && !std::is_copy_constructible_v<Leaf> && !std::is_move_constructible_v<Leaf>);
    ResetFaults();
    Leaf        empty;
    Leaf::State unchanged{};
    unchanged.allocationBase = 123;
    unchanged.leafEntry = 456;
    unchanged.plan.entry.fill(0xaa);
    Check(!empty.Read(unchanged) && unchanged.allocationBase == 123 && unchanged.leafEntry == 456 && unchanged.plan.entry[0] == 0xaa,
          "empty helper preserves every caller output field");
    Check(empty.Release(nullptr, nullptr), "empty release is idempotent without inventing a reservation");
    for (auto invalid : {std::uintptr_t{0}, std::uintptr_t{1}, std::numeric_limits<std::uintptr_t>::max() - 7})
        Check(!empty.Prepare(invalid) && !empty.Read(unchanged) && unchanged.allocationBase == 123, "invalid image never allocates or changes output");
    failQuery = true;
    Check(!empty.Prepare(Image) && !empty.Read(unchanged), "query failure before allocation has no hidden cleanup debt");

    ResetFaults();
    Leaf leaf;
    Check(leaf.Prepare(Image), "real owned near leaf prepares without executing it");
    const auto ready = Snapshot(leaf);
    Inspect(ready);
    Check(leaf.Verify(), "immutable context, code, protection and AllocationBase revalidate");
    Check(!leaf.Prepare(Image) && Snapshot(leaf).allocationBase == ready.allocationBase, "a second Prepare cannot leak or overwrite the live reservation");
    Check(leaf.Verify(), "verification preserves correctly prepared bytes after duplicate Prepare refusal");
    Proof proof{true, 0, ready.allocationBase, ready.context};
    Check(!leaf.Release(nullptr, nullptr) && releases == 0, "zero active count never replaces an explicit release authority");
    proof.qualified = false;
    Check(!leaf.Release(Quiescent, &proof) && releases == 0, "negative caller quiescence retains the complete allocation");
    ready.context->activeCalls = 1;
    Check(leaf.Verify(), "owned-memory verification does not manufacture callback or retirement authority");
    proof.qualified = true;
    const auto calls = proof.calls;
    Check(!leaf.Release(Quiescent, &proof) && proof.calls == calls && releases == 0, "live activity refuses release before the quiescence callback");
    ready.context->activeCalls = 0;
    ready.context->unqualifiedArchive = 1;
    Check(leaf.Verify(), "unsupported-archive latch remains the native owner's separate policy");
    ready.context->unqualifiedArchive = 0;

    wrongAllocation = true;
    latestAllocation = ready.allocationBase;
    Check(!leaf.Verify() && !leaf.Release(Quiescent, &proof) && releases == 0, "foreign AllocationBase refuses verification and free");
    wrongAllocation = false;
    wrongType = true;
    Check(!leaf.Verify() && !leaf.Release(Quiescent, &proof), "mapped foreign memory is not the retained private reservation");
    wrongType = false;
    failRead = reads + 1;
    Check(!leaf.Release(Quiescent, &proof) && releases == 0, "unreadable activity retains retirement debt");
    failRead = 0;
    Check(leaf.Verify(), "temporary OS query/read failure never overwrites valid owned bytes");

    DWORD previous = 0;
    Check(::VirtualProtect(reinterpret_cast<void*>(ready.allocationBase), ready.pageBytes, PAGE_READWRITE, &previous) != FALSE,
          "fixture can temporarily alter only its own unpublished code page");
    auto* byte = reinterpret_cast<unsigned char*>(ready.leafEntry);
    *byte ^= 1;
    Check(!leaf.Verify() && *byte != ready.plan.bridge[0], "wrong code protection refuses without repairing foreign bytes");
    Check(::VirtualProtect(reinterpret_cast<void*>(ready.allocationBase), ready.pageBytes, PAGE_EXECUTE_READ, &previous) != FALSE,
          "fixture restores only its owned code-page protection");
    Check(!leaf.Verify() && *byte != ready.plan.bridge[0], "RX byte conflict refuses without replacing foreign code");
    Check(::VirtualProtect(reinterpret_cast<void*>(ready.allocationBase), ready.pageBytes, PAGE_READWRITE, &previous) != FALSE,
          "fixture admits its own unpublished repair");
    *byte = ready.plan.bridge[0];
    Check(::VirtualProtect(reinterpret_cast<void*>(ready.allocationBase), ready.pageBytes, PAGE_EXECUTE_READ, &previous) != FALSE &&
              ::FlushInstructionCache(GetCurrentProcess(), byte, 1) != FALSE,
          "fixture restores its own known bytes without executing them");
    const auto nativeRain = ready.context->nativeRain;
    ready.context->nativeRain ^= 8;
    Check(!leaf.Verify() && ready.context->nativeRain != nativeRain, "foreign immutable context refuses without a hidden repair");
    ready.context->nativeRain = nativeRain;
    Check(leaf.Verify(), "original owned context and leaf remain independently verifiable");
    failFree = true;
    Check(!leaf.Release(Quiescent, &proof) && freedAddress == ready.allocationBase, "OS release failure retains exact reservation debt");
    Check(Snapshot(leaf).allocationBase == ready.allocationBase, "failed free preserves context and code identities");
    failFree = false;
    Retire(leaf);

    ResetFaults();
    Leaf first, second;
    Check(first.Prepare(Image) && second.Prepare(Image), "two independent helpers have real distinct near reservations");
    const auto a = Snapshot(first), b = Snapshot(second);
    Check(a.allocationBase != b.allocationBase && a.context != b.context, "separate owners never alias contexts or executable code");
    a.context->unqualifiedArchive = 1;
    Check(!b.context->unqualifiedArchive && first.Verify() && second.Verify(), "independent activity and latch state cannot cross allocations");
    Retire(first);
    Retire(second);

    ResetFaults();
    failFirstAllocation = true;
    Leaf collision;
    Check(collision.Prepare(Image) && allocations >= 2, "exact-reservation collision searches another near granularity slot");
    Retire(collision);
    ResetFaults();
    denyAbove = Image + Bridge::EntryRva + 5;
    Leaf below;
    Check(below.Prepare(Image), "allocation search can fall back to real free memory below the native call site");
    const auto negative = Snapshot(below);
    Check(negative.leafEntry < Image + Bridge::EntryRva + 5, "below-site leaf uses the negative signed-rel32 branch");
    Inspect(negative);
    Retire(below);

    PreparationFailures();

    ResetFaults();
    Leaf::State retained{};
    {
        Leaf scope;
        Check(scope.Prepare(Image) && scope.Read(retained), "unpublished fixture prepares a stable retained descriptor");
    }
    MEMORY_BASIC_INFORMATION memory{};
    Check(::VirtualQuery(reinterpret_cast<const void*>(retained.allocationBase), &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_COMMIT &&
              retained.context->nativeRain == Image + Bridge::NativeRainRva,
          "destructor neither frees code nor invalidates the separately allocated context");
    // No entry was published and no byte was executed in this process. The
    // fixture alone owns this deliberately abandoned reservation and can make
    // the same explicit quiescence assertion for its final direct OS cleanup.
    Check(::VirtualFree(reinterpret_cast<void*>(retained.allocationBase), 0, MEM_RELEASE) != FALSE, "fixture explicitly retires its abandoned exact base");
    std::printf("Passed %u native weather Rain leaf checks (64-bit)\n", checks);
    return EXIT_SUCCESS;
}
