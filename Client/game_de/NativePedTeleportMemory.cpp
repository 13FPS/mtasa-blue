/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve one native teleport action and its already-entered effects
 ****************************************************************************/
#include "NativePedTeleportMemory.h"
#include "NativePedPoseMutation.h"
#include "NativePedTeleportProfile.h"
#include "NativePedTransformProfile.h"
#include "NativeImageAccess.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include "NativeUObjectMembershipAccess.h"
#include "NativeUObjectMembershipProfile.h"
#include "NativeStartupWorldAccess.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace mta::game_de
{
    namespace PedTeleportDetail
    {
        namespace P = NativePedTeleportProfile;
        namespace Image = NativeImageAccess;
        bool Pages(std::uintptr_t at, std::size_t bytes, bool native = false) noexcept
        {
            if (!at || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - at)
                return false;
            const auto     end = at + bytes;
            std::uintptr_t allocation = 0;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = info.Protect & 0xff;
                if (native)
                {
                    const auto current = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                    if ((info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || protection != PAGE_READWRITE || !current ||
                        (allocation && allocation != current))
                        return false;
                    allocation = current;
                }
                else if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (start > at || info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - start || start + info.RegionSize <= at)
                    return false;
                at = start + info.RegionSize < end ? start + info.RegionSize : end;
            }
            return true;
        }
        bool Read(std::uintptr_t address, void* output, std::size_t bytes) noexcept
        {
            SIZE_T copied = 0;
            return Pages(address, bytes) && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, bytes, &copied) && copied == bytes;
        }
        struct Token
        {
            std::uintptr_t               address = 0;
            std::size_t                  size = 0;
            std::array<unsigned char, 8> bytes{};
        };
        struct Ledger
        {
            std::vector<Token> tokens;
            template <class T>
            bool Observe(std::uintptr_t address, T& output)
            {
                static_assert(sizeof(T) <= 8);
                if (tokens.size() >= P::MaximumObservationTokens)
                    return false;
                Token token;
                token.address = address;
                token.size = sizeof(T);
                if (!Read(address, token.bytes.data(), token.size))
                    return false;
                std::memcpy(&output, token.bytes.data(), sizeof(T));
                tokens.push_back(token);
                return true;
            }
            bool Fence() const noexcept
            {
                for (const auto& token : tokens)
                {
                    std::array<unsigned char, 8> current{};
                    if (!Read(token.address, current.data(), token.size) || current != token.bytes)
                        return false;
                }
                return true;
            }
        };
        struct Pool
        {
            std::uintptr_t header = 0, objects = 0, flags = 0;
            std::int32_t   count = 0, cursor = 0;
            unsigned char  owns = 0;
            std::size_t    stride = 0;
            bool           Capture(std::uintptr_t image, std::uintptr_t root, std::int32_t capacity, std::size_t rowBytes, Ledger& ledger)
            {
                stride = rowBytes;
                if (!ledger.Observe(image + root, header) || !header || header % alignof(std::uintptr_t) || !Pages(header, 0x19, true) ||
                    !ledger.Observe(header, objects) || !ledger.Observe(header + 8, flags) || !ledger.Observe(header + 0x10, count) ||
                    !ledger.Observe(header + 0x14, cursor) || !ledger.Observe(header + 0x18, owns) || count != capacity || owns != 1 || cursor < -1 ||
                    cursor >= count || !objects || !flags || objects % alignof(std::uintptr_t))
                    return false;
                return Pages(objects, static_cast<std::size_t>(count) * stride, true) && Pages(flags, static_cast<std::size_t>(count), true);
            }
            bool Member(std::uintptr_t address, Ledger& ledger, std::size_t* index = nullptr) const
            {
                if (address < objects || !stride || (address - objects) % stride)
                    return false;
                const auto slot = (address - objects) / stride;
                if (slot >= static_cast<std::size_t>(count))
                    return false;
                unsigned char flag = 0;
                if (!ledger.Observe(flags + slot, flag) || (flag & 0x80))
                    return false;
                if (index)
                    *index = slot;
                return true;
            }
        };
        struct Context
        {
            Ledger                                                                         ledger;
            NativePedTransformReader::Value                                                transform;
            std::uintptr_t                                                                 intelligence = 0, actor = 0, component = 0;
            NativeStartupWorldAccess::Value                                                world;
            std::array<NativeUObjectMembershipAccess::Snapshot, P::MaximumObjectSnapshots> objects{};
            std::size_t                                                                    objectCount = 0;
        };
        struct Sections
        {
            std::array<IMAGE_SECTION_HEADER, 96> values{};
            std::size_t                          count = 0;
            bool                                 Capture(std::uintptr_t image) noexcept
            {
                IMAGE_DOS_HEADER   dos{};
                IMAGE_NT_HEADERS64 nt{};
                if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                    static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                    !Image::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                    nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                    nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                    !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > values.size())
                    return false;
                const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
                count = nt.FileHeader.NumberOfSections;
                const auto bytes = count * sizeof(IMAGE_SECTION_HEADER);
                return at <= NativePedPoolProfile::ImageSize && bytes <= NativePedPoolProfile::ImageSize - at &&
                       Image::ReadImage(image, image + at, values.data(), bytes);
            }
            bool Contains(std::uintptr_t rva, std::size_t bytes, bool code, bool writable = false) const noexcept
            {
                const DWORD required = IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : 0) | (writable ? IMAGE_SCN_MEM_WRITE : 0);
                const DWORD forbidden = (code ? 0 : IMAGE_SCN_MEM_EXECUTE) | (writable ? 0 : IMAGE_SCN_MEM_WRITE);
                for (std::size_t i = 0; i != count; ++i)
                    if (Image::SectionContains(values[i], rva, bytes, required, forbidden))
                        return true;
                return false;
            }
        };
        ClockResult Owner(NativePedTeleportMemory::Binding binding, NativePedTeleportMemory::CheckOwner check) noexcept
        {
            if (!check)
                return ClockResult::NotInitialized;
            auto result = check();
            if (result != ClockResult::Ok)
                return result;
            result = NativeStartGameCalls::CheckThread(binding.image);
            if (result != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            return check();
        }
        bool Pins(NativePedTeleportMemory::Binding binding) noexcept
        {
            if (!NativePedTransformReader::Revalidate(binding))
                return false;
            Sections sections;
            if (!sections.Capture(binding.image))
                return false;
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !sections.Contains(pin.rva, pin.size, true) ||
                    !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto& pin : P::Names)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !sections.Contains(pin.rva, pin.size, false) ||
                    !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto& slot : P::Slots)
            {
                std::uintptr_t target = 0;
                if (!sections.Contains(slot.rva, sizeof(target), false) ||
                    !Image::ReadImage(binding.image, binding.image + slot.rva, &target, sizeof(target)) || target != binding.image + slot.targetRva)
                    return false;
            }
            for (const auto& pin : NativeUObjectMembershipProfile::pins)
            {
                std::array<unsigned char, NativeUObjectMembershipProfile::maximumPinBytes> bytes{};
                if (pin.size > bytes.size() || !sections.Contains(pin.rva, pin.size, true) ||
                    !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto& pin : NativeStartupWorldProfile::pins)
            {
                std::array<unsigned char, NativeStartupWorldProfile::maximumPinBytes> bytes{};
                if (pin.size > bytes.size() || !sections.Contains(pin.rva, pin.size, pin.code) ||
                    !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto rva : {P::ActorClassCacheRva, P::ComponentClassCacheRva, NativeStartupWorldProfile::levelClassCacheRva})
                if (!sections.Contains(rva, sizeof(std::uintptr_t), false, true) ||
                    !Image::ImagePages(binding.image, binding.image + rva, sizeof(std::uintptr_t), true))
                    return false;
            return true;
        }
        bool Node(std::uintptr_t image, const Pool& pool, std::uintptr_t node, std::uintptr_t ped, std::uintptr_t headCell, Ledger& ledger)
        {
            std::uintptr_t entity = 0, next = 0, prev = 0, other = 0;
            if (!pool.Member(node, ledger) || !ledger.Observe(node, entity) || entity != ped || !ledger.Observe(node + 8, next) ||
                !ledger.Observe(node + 0x10, prev))
                return false;
            if (prev)
            {
                if (!pool.Member(prev, ledger) || !ledger.Observe(prev + 8, other) || other != node)
                    return false;
            }
            else if (!ledger.Observe(headCell, other) || other != node)
                return false;
            if (next && (!pool.Member(next, ledger) || !ledger.Observe(next + 0x10, other) || other != node))
                return false;
            static_cast<void>(image);
            return true;
        }
        bool Lists(std::uintptr_t image, std::uintptr_t ped, const Pool& entries, const Pool& pointers, Ledger& ledger)
        {
            std::uintptr_t current = 0, previous = 0, moving = 0;
            if (!ledger.Observe(ped + P::WorldEntryOffset, current) || !ledger.Observe(ped + P::MovingNodeOffset, moving))
                return false;
            std::array<bool, P::EntryCapacity>   visitedEntries{};
            std::array<bool, P::PointerCapacity> visitedNodes{};
            for (std::size_t count = 0; current; ++count)
            {
                std::size_t slot = 0, nodeSlot = 0;
                if (count >= visitedEntries.size() || !entries.Member(current, ledger, &slot) || visitedEntries[slot])
                    return false;
                visitedEntries[slot] = true;
                std::uintptr_t head = 0, node = 0, sector = 0, prev = 0, next = 0;
                if (!ledger.Observe(current, head) || !ledger.Observe(current + 8, node) || !ledger.Observe(current + 0x10, sector) ||
                    !ledger.Observe(current + 0x18, prev) || !ledger.Observe(current + 0x20, next) || prev != previous)
                    return false;
                const auto first = image + P::SectorGridRva;
                if (sector < first || sector - first >= P::SectorGridBytes || (sector - first) % P::SectorCellBytes || head != sector + P::PedHeadOffset ||
                    !pointers.Member(node, ledger, &nodeSlot) || visitedNodes[nodeSlot] || !Node(image, pointers, node, ped, head, ledger))
                    return false;
                visitedNodes[nodeSlot] = true;
                previous = current;
                current = next;
            }
            if (moving)
            {
                std::size_t slot = 0;
                if (!pointers.Member(moving, ledger, &slot) || visitedNodes[slot] || !Node(image, pointers, moving, ped, image + P::MovingHeadRva, ledger))
                    return false;
            }
            return true;
        }
        bool Object(std::uintptr_t image, std::uintptr_t address, Context& context, NativeUObjectMembershipAccess::Snapshot*& output) noexcept
        {
            for (std::size_t i = 0; i != context.objectCount; ++i)
                if (context.objects[i].objectAddress == address)
                {
                    output = &context.objects[i];
                    return true;
                }
            if (context.objectCount == context.objects.size())
                return false;
            auto& snapshot = context.objects[context.objectCount];
            if (!NativeUObjectMembershipAccess(image, &Read).Capture(address, snapshot))
                return false;
            output = &snapshot;
            ++context.objectCount;
            return true;
        }
        bool Class(std::uintptr_t image, std::uintptr_t address, Context& context)
        {
            NativeUObjectMembershipAccess::Snapshot* object = nullptr;
            std::uintptr_t                           chain = 0, selected = 0;
            std::int32_t                             index = -1;
            if (!Object(image, address, context, object) || address > (std::numeric_limits<std::uintptr_t>::max)() - 0x3c ||
                !context.ledger.Observe(address + 0x30, chain) || !context.ledger.Observe(address + 0x38, index) || !chain || index < 0 ||
                index > NativeClassChainAccess::maximumIndex)
                return false;
            const auto offset = static_cast<std::size_t>(index) * sizeof(std::uintptr_t);
            if (offset > (std::numeric_limits<std::uintptr_t>::max)() - chain || !context.ledger.Observe(chain + offset, selected) ||
                selected != address + 0x30)
                return false;
            bool matches = false;
            return NativeClassChainAccess(&Read).IsChildOf(address, address, matches) && matches;
        }
        bool Actor(NativePedTeleportMemory::Binding binding, Context& context)
        {
            auto&          ledger = context.ledger;
            std::uintptr_t actorClass = 0, componentClass = 0, levelClass = 0, mesh = 0, getter = 0;
            const auto     image = binding.image;
            if (!ledger.Observe(image + P::ActorClassCacheRva, actorClass) || !actorClass ||
                !ledger.Observe(image + P::ComponentClassCacheRva, componentClass) || !componentClass ||
                !ledger.Observe(image + NativeStartupWorldProfile::levelClassCacheRva, levelClass) || !levelClass ||
                !ledger.Observe(context.transform.ped.nativePed + P::ActorOffset, context.actor) || !context.actor || !Class(image, actorClass, context) ||
                !Class(image, componentClass, context) || !Class(image, levelClass, context))
                return false;
            NativeUObjectMembershipAccess::Snapshot* actor = nullptr;
            NativeUObjectMembershipAccess::Snapshot* component = nullptr;
            NativeUObjectMembershipAccess::Snapshot* level = nullptr;
            NativeUObjectMembershipAccess::Snapshot* world = nullptr;
            // Exact known constructor/class identities deliberately exclude
            // unproved ped actor subclasses. The proven skeletal constructor
            // owns the same component at both members, rather than an ambient
            // renderer object or an arbitrary pointer with a plausible vtable.
            if (!Object(image, context.actor, context, actor) || actor->classAddress != actorClass || actor->vtableAddress != image + P::ActorVtableRva ||
                !ledger.Observe(actor->vtableAddress + P::ActorGetWorldSlot, getter) || getter != image + P::ActorGetWorldRva ||
                !ledger.Observe(context.actor + P::ActorRootOffset, context.component) || !context.component ||
                !ledger.Observe(context.actor + P::ActorMeshOffset, mesh) || mesh != context.component ||
                !Object(image, context.component, context, component) || component->classAddress != componentClass ||
                component->outerAddress != context.actor || !NativeStartupWorldAccess(image, &Read).Read(context.actor, context.world) ||
                context.world.outerAddress != context.world.levelAddress || actor->outerAddress != context.world.levelAddress ||
                !Object(image, context.world.levelAddress, context, level) || level->classAddress != levelClass ||
                !Object(image, context.world.worldAddress, context, world))
                return false;
            NativeUObjectMembershipAccess::Snapshot* worldClass = nullptr;
            if (!Object(image, world->classAddress, context, worldClass))
                return false;
            std::uintptr_t currentWorld = 0;
            return ledger.Observe(context.world.levelAddress + NativeStartupWorldProfile::levelWorldOffset, currentWorld) &&
                   currentWorld == context.world.worldAddress;
        }
        ClockResult Fence(NativePedTeleportMemory::Binding binding, NativePedTeleportMemory::CheckOwner owner, const Context& context) noexcept
        {
            if (!context.ledger.Fence())
                return ClockResult::BindingChanged;
            NativeUObjectMembershipAccess members(binding.image, &Read);
            for (std::size_t i = 0; i != context.objectCount; ++i)
                if (!members.Recheck(context.objects[i]))
                    return ClockResult::BindingChanged;
            const auto result = NativePedTransformReader(binding, owner).Recheck(context.transform);
            if (result != ClockResult::Ok)
                return result;
            // None of the copies above grants authority to enter native code.
            // In particular a read/fault seam may revoke the callback scope.
            return Owner(binding, owner);
        }
        ClockResult Capture(NativePedTeleportMemory::Binding binding, NativePedTeleportMemory::CheckOwner owner, std::uint32_t handle, Context& output)
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Pins(binding))
                return ClockResult::BindingChanged;
            result = NativePedTransformReader(binding, owner).Read(handle, output.transform);
            if (result != ClockResult::Ok)
                return result;
            const auto ped = output.transform.ped.nativePed;
            // This bounded native action corresponds to the matrix-backed
            // classic Teleport branch. Inline-only SDK placement remains a
            // separate wrapper intent and is not routed through native effects.
            if (!output.transform.matrix)
                return ClockResult::NotInitialized;
            auto& ledger = output.ledger;
            ledger.tokens.reserve(P::MaximumObservationTokens);
            std::uint32_t  flags = 0, selector = 0;
            unsigned char  type = 0;
            std::uintptr_t references = 0, firstOwner = 0, secondOwner = 0;
            Pool           intelligence, pointers, entries;
            if (!ledger.Observe(ped + P::PhysicalFlagsOffset, flags) || (flags & 0x100) || !ledger.Observe(ped + P::EntityTypeOffset, type) ||
                (type & 7) != 3 || !ledger.Observe(ped + P::SelectorOffset, selector) || selector > 1 ||
                !ledger.Observe(ped + P::ReferenceOffset, references) || references || !ledger.Observe(ped + P::IntelligenceOffset, output.intelligence) ||
                !intelligence.Capture(binding.image, P::IntelligenceRootRva, P::IntelligenceCapacity, P::IntelligenceStride, ledger) ||
                !intelligence.Member(output.intelligence, ledger) || !ledger.Observe(output.intelligence, firstOwner) || firstOwner != ped ||
                !ledger.Observe(output.intelligence + P::IntelligenceOwnerOffset, secondOwner) || secondOwner != ped ||
                !pointers.Capture(binding.image, P::PointerRootRva, P::PointerCapacity, P::PointerStride, ledger) ||
                !entries.Capture(binding.image, P::EntryRootRva, P::EntryCapacity, P::EntryStride, ledger) ||
                !Lists(binding.image, ped, entries, pointers, ledger) || !Actor(binding, output))
                return ClockResult::BindingChanged;
            return Fence(binding, owner, output);
        }
#if !defined(MTA_DE_PED_TELEPORT_NATIVE_INVOKE)
        ClockResult NativeCall(NativePedTeleportMemory::NativeFunction function, void* ped, const CVector* value, bool reset, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped, value, reset);
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
    ClockResult NativePedTeleportMemory::Apply(std::uint32_t handle, const CVector& input, Receipt* receiptOutput, Invoke invoke) const noexcept
    {
        namespace D = PedTeleportDetail;
        Receipt receipt;
        receipt.handle = handle;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        // Adapter placement and native teleport share one callback-reentry
        // boundary. This does not pin the engine receiver or serialize its
        // internal calls; all original immediate graph checks remain required.
        NativePedPoseMutation scope;
        if (!scope)
            return finish(ClockResult::Busy);
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::Read(reinterpret_cast<std::uintptr_t>(&input), &receipt.requested, sizeof(receipt.requested)) || !std::isfinite(receipt.requested.fX) ||
            !std::isfinite(receipt.requested.fY) || !std::isfinite(receipt.requested.fZ))
            return finish(ClockResult::InvalidArgument);
        try
        {
            D::Context before;
            result = D::Capture(binding, owner, handle, before);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.nativePed = before.transform.ped.nativePed;
            receipt.intelligence = before.intelligence;
            receipt.matrixBefore = before.transform.matrix;
            receipt.entry = binding.image + NativePedTransformProfile::NativeTeleportRva;
            result = D::Owner(binding, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!D::Pins(binding))
                return finish(ClockResult::BindingChanged);
            result = D::Fence(binding, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_PED_TELEPORT_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_PED_TELEPORT_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &D::NativeCall;
#endif
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(receipt.entry), reinterpret_cast<void*>(receipt.nativePed), &receipt.requested, true,
                           receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            result = D::Owner(binding, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            D::Context after;
            result = D::Capture(binding, owner, handle, after);
            if (result != ClockResult::Ok)
                return finish(result);
            // Real native effects may replace matrices/tasks/list entries or
            // advance an allocator cursor. Qualify fresh dependencies instead
            // of demanding equality with the pre-call world graph.
            const auto& old = before.transform.ped;
            const auto& current = after.transform.ped;
            if (old.nativePed != current.nativePed || old.handle != current.handle || old.generation != current.generation || old.vtable != current.vtable ||
                old.firstMethod != current.firstMethod || old.pool != current.pool || old.objects != current.objects || old.flags != current.flags ||
                before.world.worldAddress != after.world.worldAddress)
                return finish(ClockResult::BindingChanged);
            receipt.matrixAfter = after.transform.matrix;
            receipt.afterObserved = true;
            return finish(D::Owner(binding, owner));
        }
        catch (...)
        {
            // A post-entry observation/allocation failure is still a failed
            // acknowledgement of an entered native action, never a rollback
            // lease or permission to retry its effects.
            return finish(ClockResult::ReadFailure);
        }
    }
}
