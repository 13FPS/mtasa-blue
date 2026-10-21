/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep native-visible weak cells alive until exact address retirement
 ****************************************************************************/
#include "NativePedReferenceMemory.h"
#include "NativePedReferenceProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace PedReferenceDetail
    {
        namespace P = NativePedReferenceProfile;
        namespace Image = NativeImageAccess;
        using State = NativePedReferenceCell::State;
        using CheckOwner = NativePedReferenceMemory::CheckOwner;
        struct Lock
        {
            std::atomic_flag& flag;
            bool              held;
            explicit Lock(std::atomic_flag& flag) noexcept : flag(flag), held(!flag.test_and_set(std::memory_order_acquire)) {}
            ~Lock()
            {
                if (held)
                    flag.clear(std::memory_order_release);
            }
        };
        thread_local bool mutating = false;
        struct Mutation
        {
            Mutation() noexcept { mutating = true; }
            ~Mutation() { mutating = false; }
        };
        bool ObjectPages(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (info.Protect & 0xff) != PAGE_READWRITE)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress), current = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!current || (allocation && allocation != current) || base > address ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                allocation = current;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!ObjectPages(address, sizeof(next)) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) || copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool Allocation(const State& state) noexcept
        {
            MEMORY_BASIC_INFORMATION info{};
            return state.allocationBase && state.cellAddress == state.allocationBase && state.allocationBytes >= sizeof(std::uintptr_t) &&
                   VirtualQuery(reinterpret_cast<void*>(state.allocationBase), &info, sizeof(info)) == sizeof(info) && info.State == MEM_COMMIT &&
                   info.Type == MEM_PRIVATE && reinterpret_cast<std::uintptr_t>(info.AllocationBase) == state.allocationBase &&
                   reinterpret_cast<std::uintptr_t>(info.BaseAddress) == state.allocationBase && info.RegionSize >= state.allocationBytes &&
                   (info.Protect & 0xff) == PAGE_READWRITE && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS));
        }
        ClockResult Owner(NativePedReferenceMemory::Binding binding, CheckOwner owner, bool cleanup) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok)
                return result;
            result = NativeStartGameCalls::CheckThread(binding.image);
            if (result != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (!cleanup && state != 9)
                return ClockResult::NotInitialized;
            return owner();
        }
        struct Node
        {
            std::uintptr_t next = 0, cell = 0;
        };
        static_assert(sizeof(Node) == P::ReferenceBytes && P::ReferenceBytes == 16);
        struct Bank
        {
            std::array<Node, P::ReferenceCount> nodes{};
            std::uintptr_t                      freeHead = 0;
            bool Same(const Bank& other) const noexcept { return freeHead == other.freeHead && !std::memcmp(nodes.data(), other.nodes.data(), sizeof(nodes)); }
        };
        bool Index(std::uintptr_t image, std::uintptr_t node, std::size_t& output) noexcept
        {
            const auto begin = image + P::ReferencesRva;
            if (node < begin || node - begin >= P::ReferenceCount * P::ReferenceBytes || (node - begin) % P::ReferenceBytes)
                return false;
            output = static_cast<std::size_t>((node - begin) / P::ReferenceBytes);
            return true;
        }
        bool FreeNodes(std::uintptr_t image, const Bank& bank, std::array<bool, P::ReferenceCount>& output) noexcept
        {
            auto node = bank.freeHead;
            for (std::size_t count = 0; node; ++count)
            {
                std::size_t index = 0;
                if (count >= P::ReferenceCount || !Index(image, node, index) || output[index])
                    return false;
                output[index] = true;
                node = bank.nodes[index].next;
            }
            return true;
        }
        bool CaptureBank(std::uintptr_t image, Bank& output) noexcept
        {
            Bank       first, second;
            const auto capture = [&](Bank& bank)
            {
                std::uintptr_t after = 0;
                if (!Image::ReadImage(image, image + P::FreeHeadRva, &bank.freeHead, sizeof(bank.freeHead), true) ||
                    !Image::ReadImage(image, image + P::ReferencesRva, bank.nodes.data(), sizeof(bank.nodes), true) ||
                    !Image::ReadImage(image, image + P::FreeHeadRva, &after, sizeof(after), true) || after != bank.freeHead)
                    return false;
                for (const auto& node : bank.nodes)
                {
                    std::size_t index = 0;
                    if (node.next && !Index(image, node.next, index))
                        return false;
                }
                std::array<bool, P::ReferenceCount> free{};
                return FreeNodes(image, bank, free);
            };
            if (!capture(first) || !capture(second) || !first.Same(second))
                return false;
            output = second;
            return true;
        }
        std::size_t Count(const Bank& bank, std::uintptr_t cell, std::uintptr_t image, std::uintptr_t& node) noexcept
        {
            std::size_t count = 0;
            for (std::size_t i = 0; i != bank.nodes.size(); ++i)
                if (bank.nodes[i].cell == cell)
                {
                    ++count;
                    node = image + P::ReferencesRva + i * P::ReferenceBytes;
                }
            return count;
        }
        bool Chain(std::uintptr_t image, std::uintptr_t ped, const Bank& bank, std::uintptr_t required, std::uintptr_t& head) noexcept
        {
            std::array<bool, P::ReferenceCount> free{}, seen{};
            if (!FreeNodes(image, bank, free) || !Object(ped + P::EntityHeadOffset, head))
                return false;
            auto node = head;
            bool found = !required;
            for (std::size_t count = 0; node; ++count)
            {
                std::size_t index = 0;
                if (count >= P::ReferenceCount || !Index(image, node, index) || free[index] || seen[index] || !bank.nodes[index].cell)
                    return false;
                seen[index] = true;
                if (node == required)
                    found = true;
                node = bank.nodes[index].next;
            }
            std::uintptr_t after = 0;
            return found && Object(ped + P::EntityHeadOffset, after) && head == after;
        }
        bool SameIdentity(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            // Native allocations may advance the pool cursor while a valid
            // registered cell persists. That cursor is not object identity.
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        ClockResult Ped(NativePedReferenceMemory::Binding binding, CheckOwner owner, std::uint32_t handle, NativePedPoolReader::Value& output) noexcept
        {
            auto result = NativePedPoolReader(binding, owner).Resolve(handle, output);
            if (result != ClockResult::Ok)
                return result;
            if (output.vtable != binding.image + P::PedVtableRva && output.vtable != binding.image + P::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            unsigned char type = 0;
            if (!Object(output.nativePed + P::TypeOffset, type))
                return ClockResult::ReadFailure;
            return (type & 7) == 3 ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult Relation(NativePedReferenceMemory::Binding binding, CheckOwner owner, bool cleanup, const State& state,
                             const NativePedPoolReader::Value& original, NativePedPoolReader::Value& output, Bank& bank, std::uintptr_t& node) noexcept
        {
            auto result = Owner(binding, owner, cleanup);
            if (result != ClockResult::Ok)
                return result;
            if (!Allocation(state) || !NativePedReferenceMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            std::uintptr_t word = 0;
            if (!Object(state.cellAddress, word))
                return ClockResult::ReadFailure;
            if (!word)
                return ClockResult::NotInitialized;
            if (word != state.nativePed)
                return ClockResult::OwnerConflict;
            NativePedPoolReader::Value ped;
            if ((result = Ped(binding, owner, state.handle, ped)) != ClockResult::Ok)
                return result;
            if (!SameIdentity(original, ped) || !CaptureBank(binding.image, bank) || Count(bank, state.cellAddress, binding.image, node) != 1 ||
                (state.node && node != state.node))
                return ClockResult::BindingChanged;
            std::uintptr_t head = 0, after = 0;
            if (!Chain(binding.image, ped.nativePed, bank, node, head) || !Object(state.cellAddress, after) || after != word)
                return ClockResult::BindingChanged;
            Bank final;
            if (!CaptureBank(binding.image, final) || !bank.Same(final))
                return ClockResult::BindingChanged;
            if ((result = NativePedPoolReader(binding, owner).Recheck(ped)) != ClockResult::Ok || (result = Owner(binding, owner, cleanup)) != ClockResult::Ok)
                return result;
            output = ped;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_PED_REFERENCE_REGISTER_INVOKE)
        ClockResult RegisterCall(NativePedReferenceMemory::RegisterFunction function, void* ped, void** cell, bool force, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped, cell, force);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE)
        ClockResult UnregisterCall(NativePedReferenceMemory::UnregisterFunction function, void* ped, void** cell, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped, cell);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    bool NativePedReferenceCell::ReadState(State& output) const noexcept
    {
        PedReferenceDetail::Lock lock(operation);
        if (!lock.held || !state.allocationBase)
            return false;
        output = state;
        return true;
    }
    bool NativePedReferenceMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedReferenceMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedReferenceProfile;
        namespace Image = NativeImageAccess;
        if (!NativePedPoolReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
        {
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], rva, size, required, forbidden))
                    return true;
            return false;
        };
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        for (const auto& slot : P::Slots)
        {
            std::uintptr_t target = 0;
            if (!contains(slot.rva, sizeof(target), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ReadImage(binding.image, binding.image + slot.rva, &target, sizeof(target)) || target != binding.image + slot.targetRva)
                return false;
        }
        return contains(P::ReferencesRva, P::ReferenceCount * P::ReferenceBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
               contains(P::FreeHeadRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
               Image::ImagePages(binding.image, binding.image + P::ReferencesRva, P::ReferenceCount * P::ReferenceBytes, true) &&
               Image::ImagePages(binding.image, binding.image + P::FreeHeadRva, sizeof(std::uintptr_t), true);
    }
    ClockResult NativePedReferenceMemory::Register(std::uint32_t handle, NativePedReferenceCell& cell, Receipt* receiptOutput,
                                                   InvokeRegister invoke) const noexcept
    {
        namespace D = PedReferenceDetail;
        namespace P = NativePedReferenceProfile;
        Receipt receipt;
        receipt.handle = handle;
        receipt.generation = generation;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (D::mutating)
            return finish(ClockResult::Busy);
        D::Lock lock(cell.operation);
        if (!lock.held)
            return finish(ClockResult::Busy);
        if (!generation || !normal || !recovery)
            return finish(ClockResult::NotInitialized);
        if (cell.retained.load())
            return finish(ClockResult::Busy);
        auto result = D::Owner(binding, normal, false);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Revalidate(binding))
            return finish(ClockResult::BindingChanged);
        NativePedPoolReader::Value ped;
        if ((result = D::Ped(binding, normal, handle, ped)) != ClockResult::Ok)
            return finish(result);
        D::Bank        bank;
        std::size_t    freeIndex = 0;
        std::uintptr_t head = 0;
        if (!D::CaptureBank(binding.image, bank))
            return finish(ClockResult::ReadFailure);
        // This bounded initial implementation refuses the native five-pool
        // prune route. A nonnull free head is not registration success.
        if (!bank.freeHead)
            return finish(ClockResult::NotInitialized);
        if (!D::Index(binding.image, bank.freeHead, freeIndex) || bank.nodes[freeIndex].cell || !D::Chain(binding.image, ped.nativePed, bank, 0, head))
            return finish(ClockResult::BindingChanged);
        D::Mutation referenceMutation;
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        if (system.dwPageSize < sizeof(std::uintptr_t))
            return finish(ClockResult::UnsupportedBuild);
        void* allocation = VirtualAlloc(nullptr, system.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!allocation)
            return finish(ClockResult::WriteFailure);
        auto& state = cell.state;
        state = {};
        state.image = binding.image;
        state.allocationBase = state.cellAddress = reinterpret_cast<std::uintptr_t>(allocation);
        state.allocationBytes = system.dwPageSize;
        state.generation = generation;
        state.handle = handle;
        state.nativePed = ped.nativePed;
        cell.identity = ped;
        cell.retained.store(true);
        cell.references.store(false);
        receipt.cell = state.cellAddress;
        receipt.nativePed = ped.nativePed;
        SIZE_T written = 0;
        if (!D::Allocation(state) || !WriteProcessMemory(GetCurrentProcess(), allocation, &ped.nativePed, sizeof(ped.nativePed), &written) ||
            written != sizeof(ped.nativePed))
            return finish(ClockResult::WriteFailure);
        D::Bank        final;
        std::uintptr_t absent = 0, headAfter = 0;
        if (!D::CaptureBank(binding.image, final) || !bank.Same(final) || D::Count(final, state.cellAddress, binding.image, absent) ||
            !D::Object(ped.nativePed + P::EntityHeadOffset, headAfter) || headAfter != head)
            return finish(ClockResult::BindingChanged);
        if ((result = NativePedPoolReader(binding, normal).Recheck(ped)) != ClockResult::Ok || (result = D::Owner(binding, normal, false)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_REFERENCE_REGISTER_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_PED_REFERENCE_REGISTER_INVOKE;
#else
        const auto entry = invoke ? invoke : &D::RegisterCall;
#endif
        cell.references.store(true);
        state.registrationEntered = true;
        state.uncertain = true;
        receipt.entered = true;
        result = entry(reinterpret_cast<RegisterFunction>(binding.image + P::RegisterRva), reinterpret_cast<void*>(ped.nativePed),
                       reinterpret_cast<void**>(state.cellAddress), true, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if ((result = D::Owner(binding, normal, false)) != ClockResult::Ok)
            return finish(result);
        if (!D::CaptureBank(binding.image, final))
            return finish(ClockResult::ReadFailure);
        std::uintptr_t node = 0;
        if (!D::Count(final, state.cellAddress, binding.image, node))
        {
            // A void return can silently leave no registration. Keep storage
            // until explicit address-absence release, rather than publish a
            // fabricated live reference or retry the native action.
            receipt.bankAbsent = true;
            state.uncertain = false;
            cell.references.store(false);
            return finish(ClockResult::NotInitialized);
        }
        NativePedPoolReader::Value current;
        D::Bank                    qualified;
        if ((result = D::Relation(binding, normal, false, state, cell.identity, current, qualified, node)) != ClockResult::Ok)
            return finish(result);
        state.node = node;
        state.registrationVerified = true;
        state.uncertain = false;
        receipt.node = node;
        receipt.registrationVerified = true;
        return finish(ClockResult::Ok);
    }
    ClockResult NativePedReferenceMemory::Read(const NativePedReferenceCell& cell, NativePedPoolReader::Value& output) const noexcept
    {
        namespace D = PedReferenceDetail;
        D::Lock lock(cell.operation);
        if (!lock.held)
            return ClockResult::Busy;
        const auto& state = cell.state;
        if (!state.allocationBase)
            return ClockResult::NotInitialized;
        if (!generation || state.generation != generation || state.image != binding.image)
            return ClockResult::OwnerConflict;
        if (!state.registrationVerified || state.uncertain || state.cleanupEntered)
            return ClockResult::RestoreFailure;
        NativePedPoolReader::Value current;
        D::Bank                    bank;
        std::uintptr_t             node = 0;
        const auto                 result = D::Relation(binding, normal, false, state, cell.identity, current, bank, node);
        if (result == ClockResult::Ok)
            output = current;
        return result;
    }
    ClockResult NativePedReferenceMemory::Release(NativePedReferenceCell& cell, Receipt* receiptOutput, InvokeUnregister invoke) const noexcept
    {
        namespace D = PedReferenceDetail;
        namespace P = NativePedReferenceProfile;
        Receipt receipt;
        receipt.operation = Operation::Release;
        receipt.generation = generation;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (D::mutating)
            return finish(ClockResult::Busy);
        D::Lock lock(cell.operation);
        if (!lock.held)
            return finish(ClockResult::Busy);
        auto& state = cell.state;
        if (!state.allocationBase)
            return finish(ClockResult::Ok);
        receipt.handle = state.handle;
        receipt.nativePed = state.nativePed;
        receipt.cell = state.cellAddress;
        receipt.node = state.node;
        if (!generation || state.generation != generation || state.image != binding.image)
            return finish(ClockResult::OwnerConflict);
        auto result = D::Owner(binding, recovery, true);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Revalidate(binding) || !D::Allocation(state))
            return finish(ClockResult::BindingChanged);
        D::Mutation referenceMutation;
        D::Bank     bank;
        if (!D::CaptureBank(binding.image, bank))
            return finish(ClockResult::ReadFailure);
        std::uintptr_t node = 0;
        const auto     releaseStorage = [&]()
        {
            // Native entry may have changed the qualified image. Storage
            // release needs fresh authority, even after address absence.
            if (!Revalidate(binding))
                return ClockResult::BindingChanged;
            D::Bank        final;
            std::uintptr_t found = 0;
            if (!D::CaptureBank(binding.image, final) || !bank.Same(final) || D::Count(final, state.cellAddress, binding.image, found) || !D::Allocation(state))
                return ClockResult::RestoreFailure;
            const auto owner = D::Owner(binding, recovery, true);
            if (owner != ClockResult::Ok)
                return owner;
            receipt.bankAbsent = true;
            if (!VirtualFree(reinterpret_cast<void*>(state.allocationBase), 0, MEM_RELEASE))
                return ClockResult::RestoreFailure;
            state = {};
            cell.identity = {};
            cell.references.store(false);
            cell.retained.store(false);
            receipt.allocationReleased = true;
            return ClockResult::Ok;
        };
        if (!D::Count(bank, state.cellAddress, binding.image, node))
            return finish(releaseStorage());
        // Neither zero nor an old seven-bit handle authorizes another unlink.
        // An uncertain call may have published only part of the native list;
        // quarantine it until fresh bank-wide address absence is observed.
        if (state.uncertain || !state.registrationVerified || state.cleanupEntered)
            return finish(ClockResult::RestoreFailure);
        NativePedPoolReader::Value current;
        if ((result = D::Relation(binding, recovery, true, state, cell.identity, current, bank, node)) != ClockResult::Ok)
            return finish(result);
        if ((result = D::Owner(binding, recovery, true)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE;
#else
        const auto entry = invoke ? invoke : &D::UnregisterCall;
#endif
        state.cleanupEntered = true;
        state.uncertain = true;
        receipt.entered = true;
        result = entry(reinterpret_cast<UnregisterFunction>(binding.image + P::UnregisterRva), reinterpret_cast<void*>(current.nativePed),
                       reinterpret_cast<void**>(state.cellAddress), receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if ((result = D::Owner(binding, recovery, true)) != ClockResult::Ok)
            return finish(result);
        if (!D::CaptureBank(binding.image, bank))
            return finish(ClockResult::ReadFailure);
        if (D::Count(bank, state.cellAddress, binding.image, node))
            return finish(ClockResult::RestoreFailure);
        cell.references.store(false);
        return finish(releaseStorage());
    }
}
