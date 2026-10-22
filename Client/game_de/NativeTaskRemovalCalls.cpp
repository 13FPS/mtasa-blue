/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native deletion order on the existing admitted ped callback
 ****************************************************************************/
#include "NativeTaskRemovalCalls.h"
#include "NativeTaskRemovalProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <tuple>

namespace mta::game_de::NativeTaskRemovalCalls
{
    namespace
    {
        namespace P = NativeTaskRemovalProfile;
        bool Pins(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool SameIdentity(const NativePedIntelligenceReader::Value& a, const NativePedIntelligenceReader::Value& b) noexcept
        {
            // A native mutation deliberately changes one task word. Recheck
            // owner/pool identity independently instead of comparing an old
            // full manager snapshot or trying to borrow the deleted task.
            return a.ped.handle == b.ped.handle && a.ped.nativePed == b.ped.nativePed &&
                   std::tie(a.pool, a.objects, a.flags, a.nativeIntelligence, a.taskManager, a.slot, a.capacity, a.cursor, a.generation, a.ownsAllocations,
                            a.rawType) == std::tie(b.pool, b.objects, b.flags, b.nativeIntelligence, b.taskManager, b.slot, b.capacity, b.cursor, b.generation,
                                                   b.ownsAllocations, b.rawType);
        }
        bool ReadChild(std::uintptr_t address, std::uintptr_t& child) noexcept
        {
            // The task reader has checked the complete native-sized writable
            // row. Still use the checked OS-copy boundary for this final word;
            // a generation observation never authorizes an unchecked load.
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address + P::ComplexChildOffset), &child, sizeof(child), &copied) &&
                   copied == sizeof(child);
        }
        bool Aliased(const NativePedIntelligenceReader::Value& value, std::size_t index, std::uintptr_t task) noexcept
        {
            for (const auto primary : value.primaryTasks)
                if (primary == task)
                    return true;
            for (std::size_t i = 0; i != value.secondaryTasks.size(); ++i)
                if (i != index && value.secondaryTasks[i] == task)
                    return true;
            return false;
        }
#if !defined(MTA_DE_TASK_REMOVAL_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t manager, std::int32_t index, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(manager, 0, index);
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
    ClockResult RemoveSecondary(std::uintptr_t image, const NativePedPoolReader::Value& ped, int index, CheckOwner owner, Receipt* output,
                                Invoke invoke) noexcept
    {
        Receipt receipt;
        receipt.entryRva = P::SecondaryRva;
        receipt.index = index;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        // The native entry sign-extends an unchecked array index. Refuse
        // unsupported memory intent explicitly, never clamp it to a real slot.
        if (index < 0 || index >= static_cast<int>(P::SecondaryCount))
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        NativePedIntelligenceReader        intelligence({image}, owner);
        NativePedIntelligenceReader::Value before, after;
        auto                               result = intelligence.Read(ped, before);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Pins(image))
            return finish(ClockResult::UnsupportedBuild);
        const auto slot = static_cast<std::size_t>(index);
        receipt.nativeIntelligence = before.nativeIntelligence;
        receipt.manager = before.taskManager;
        receipt.oldTask = before.secondaryTasks[slot];
        if (!receipt.oldTask)
        {
            // Native SetSecondary(nullptr) returns immediately when the old
            // cell is already null. This exact equality path needs no delete
            // capability and does not pretend to have entered a native call.
            receipt.alreadyEmpty = true;
            return finish(intelligence.Recheck(before));
        }
        if (Aliased(before, slot, receipt.oldTask))
            return finish(ClockResult::OwnerConflict);
        NativeTaskPoolReader        taskReader({image}, owner);
        NativeTaskPoolReader::Value task;
        if ((result = taskReader.Read(receipt.oldTask, task)) != ClockResult::Ok)
            return finish(result);
        receipt.oldTaskSlot = task.slot;
        receipt.oldTaskType = task.nativeType;
        receipt.oldTaskGeneration = task.generation;
        if (!task.simple)
        {
            std::uintptr_t child = 0;
            if (!ReadChild(task.nativeTask, child))
                return finish(ClockResult::ReadFailure);
            // The proved complex destructor deletes its child recursively.
            // Unknown child allocation/destruction is not admitted by merely
            // recognizing its parent type. Empty-child305 is fully bounded.
            if (child)
                return finish(ClockResult::UnsupportedBuild);
        }
        if ((result = intelligence.Recheck(before)) != ClockResult::Ok || (result = taskReader.Recheck(task)) != ClockResult::Ok)
            return finish(result);
        if (!Pins(image))
            return finish(ClockResult::UnsupportedBuild);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        if (!task.simple)
        {
            std::uintptr_t child = 0;
            // Pool/vtable bookends do not include the complex child word.
            // Confirm its empty state after those observations and the final
            // owner check, directly before entering recursive deletion code.
            if (!ReadChild(task.nativeTask, child))
                return finish(ClockResult::ReadFailure);
            if (child)
                return finish(ClockResult::UnsupportedBuild);
        }
        const auto function = reinterpret_cast<NativeFunction>(image + receipt.entryRva);
#if defined(MTA_DE_TASK_REMOVAL_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_TASK_REMOVAL_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(function, receipt.manager, static_cast<std::int32_t>(index), receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        // The deleted task no longer has a readable live-task identity. Check
        // the ped/Intelligence manager afresh and acknowledge only the targeted
        // null cell. Other unexpected native effects stop the owner as well;
        // none justify a retry or restoration of a stale pointer snapshot.
        if ((result = intelligence.Read(ped, after)) != ClockResult::Ok)
            return finish(result);
        if (!SameIdentity(before, after) || after.secondaryTasks[slot] || before.primaryTasks != after.primaryTasks)
            return finish(ClockResult::OwnerConflict);
        before.secondaryTasks[slot] = 0;
        if (before.secondaryTasks != after.secondaryTasks || !Pins(image))
            return finish(ClockResult::BindingChanged);
        return finish(owner());
    }
}
