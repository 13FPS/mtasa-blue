/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve genuine widget/input/audio effects under the loaded owner
 ****************************************************************************/
#include "NativeStartupMenuCalls.h"
#include "NativeStartupMenuProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeStartupMenuCalls
{
    namespace Detail
    {
        namespace P = NativeStartupMenuProfile;
        namespace M = NativeFrontendMenuProfile;
        namespace C = NativeStreamingCompletionContext;
        namespace I = NativeImageAccess;
        bool Profile(std::uintptr_t image) noexcept
        {
            const auto pins = [&](const auto& values)
            {
                for (const auto& pin : values)
                {
                    std::array<unsigned char, P::MaximumPinBytes> bytes{};
                    if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                        std::memcmp(bytes.data(), pin.bytes, pin.size))
                        return false;
                }
                return true;
            };
            const auto pointer = [&](std::uintptr_t slot, std::uintptr_t target, bool code)
            {
                std::uintptr_t actual = 0;
                return I::ReadImage(image, image + slot, &actual, sizeof(actual)) && actual == image + target && I::ImagePages(image, actual, 1, false, code);
            };
            if (!C::Profile(image) || !pins(P::Pins) || !pins(M::pins))
                return false;
            for (const auto& target : M::vtableTargets)
                if (!pointer(target.slotRva, target.targetRva, true))
                    return false;
            for (const auto& target : M::imagePointers)
                if (!pointer(target.slotRva, target.targetRva, target.code))
                    return false;
            return I::ImagePages(image, image + P::FlagRva, 1, true);
        }
        struct State
        {
            C::Value       world, dependencies;
            std::uintptr_t widget = 0, dependency = 0;
            std::uint32_t  auxiliary = 0;
            unsigned char  flag = 0;
        };
        bool LegacyEmpty(std::uintptr_t image) noexcept
        {
            std::uintptr_t screens = 0, pending = 0;
            std::int32_t   count = -1, capacity = -1;
            // The actual widget close does not prove that arbitrary legacy
            // screens were closed. Admit only the already-empty native stack.
            return I::ReadImage(image, image + M::menuHeaderRva, &screens, sizeof(screens), true) &&
                   I::ReadImage(image, image + M::menuHeaderRva + 8, &count, sizeof(count), true) &&
                   I::ReadImage(image, image + M::menuHeaderRva + 12, &capacity, sizeof(capacity), true) &&
                   I::ReadImage(image, image + M::menuHeaderRva + 16, &pending, sizeof(pending), true) && count == 0 && capacity >= 0 && !pending;
        }
        bool Dependency(std::uintptr_t image, std::uintptr_t object, std::size_t slot, C::Value& graph) noexcept
        {
            if (!object)
                return true;
            std::uintptr_t vtable = 0, target = 0;
            // These are current engine-owned receiver fields, never objects
            // allocated or adopted by this capability. Membership is only a
            // synchronous fence under the existing GameThread/GC protocol.
            return C::Writable(object, 0x28) && C::Add(image, object, graph) && C::At(object, 0, vtable) &&
                   vtable <= (std::numeric_limits<std::uintptr_t>::max)() - slot && I::ReadImage(image, vtable + slot, &target, sizeof(target)) &&
                   I::ImagePages(image, target, 1, false, true);
        }
        ClockResult Capture(std::uintptr_t image, CheckOwner owner, bool dependencies, State& output) noexcept
        {
            auto result = C::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            State next;
            if (!Profile(image) || !LegacyEmpty(image))
                return ClockResult::BindingChanged;
            if ((result = C::Capture(image, next.world)) != ClockResult::Ok)
                return result;
            if (!C::Writable(next.world.root, P::OwnerBytes) || !C::At(next.world.root, M::widgetOffset, next.widget) ||
                !C::At(next.world.root, P::AuxiliaryOffset, next.auxiliary) || !C::At(next.world.root, P::DependencyOffset, next.dependency) ||
                !I::ReadImage(image, image + P::FlagRva, &next.flag, sizeof(next.flag), true))
                return ClockResult::BindingChanged;
            next.dependencies = next.world;
            if (dependencies)
            {
                if (!Dependency(image, next.widget, 0x288, next.dependencies) || !Dependency(image, next.dependency, 0x690, next.dependencies))
                    return ClockResult::BindingChanged;
                for (std::size_t index = 0; index != next.dependencies.count; ++index)
                    if (!C::Add(image, next.dependencies.members[index].classAddress, next.dependencies))
                        return ClockResult::BindingChanged;
            }
            if (!C::Fence(image, next.dependencies) || !Profile(image) || !LegacyEmpty(image))
                return ClockResult::BindingChanged;
            if ((result = C::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const State& first, const State& next) noexcept
        {
            return first.widget == next.widget && first.dependency == next.dependency && first.auxiliary == next.auxiliary && first.flag == next.flag &&
                   C::Same(first.world, next.world, true) && C::Same(first.dependencies, next.dependencies, true);
        }
#if !defined(MTA_DE_STARTUP_MENU_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t root, bool& result, std::uint32_t& exception) noexcept
        {
            __try
            {
                result = function(root);
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
    void Record::MarkUnacknowledged() noexcept
    {
        if (journal.entered)
        {
            journal.debt = true;
            stage.store(Stage::FaultDebt, std::memory_order_release);
        }
    }
    ClockResult CheckReady(std::uintptr_t image, CheckOwner owner) noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        if (!Detail::C::TryEnter())
            return ClockResult::Busy;
        Detail::C::Scope scope;
        Detail::State    first, next;
        auto             result = Detail::Capture(image, owner, true, first);
        if (result == ClockResult::Ok)
            result = Detail::Capture(image, owner, true, next);
        return result == ClockResult::Ok && !Detail::Same(first, next) ? ClockResult::BindingChanged : result;
    }
    struct Access
    {
        static ClockResult Finish(Record& record, ClockResult result, Receipt* output) noexcept
        {
            record.journal.debt = record.journal.entered && !record.journal.verified;
            record.stage.store(record.journal.debt ? Stage::FaultDebt : Stage::Complete, std::memory_order_release);
            if (output)
                *output = record.journal;
            return result;
        }
        static ClockResult Run(std::uintptr_t image, Record& record, std::uint64_t generation, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
        {
            Stage empty = Stage::Empty;
            if (!record.stage.compare_exchange_strong(empty, Stage::Preparing, std::memory_order_acq_rel))
            {
                // Reentry must not inspect a journal that the active call is
                // still filling, nor provide another native-entry attempt.
                return ClockResult::OwnerConflict;
            }
            auto& receipt = record.journal;
            receipt.generation = generation;
            if (!generation)
                return Finish(record, ClockResult::InvalidArgument, output);
            if (!owner)
                return Finish(record, ClockResult::NotInitialized, output);
            if (!Detail::C::TryEnter())
                return Finish(record, ClockResult::Busy, output);
            Detail::C::Scope scope;
            Detail::State    first, next;
            auto             result = Detail::Capture(image, owner, true, first);
            if (result != ClockResult::Ok || (result = Detail::Capture(image, owner, true, next)) != ClockResult::Ok)
                return Finish(record, result, output);
            if (!Detail::Same(first, next))
                return Finish(record, ClockResult::BindingChanged, output);
            receipt.root = first.world.root;
            receipt.gameMode = first.world.gameMode;
            receipt.level = first.world.level;
            receipt.world = first.world.world;
            receipt.widgetBefore = first.widget;
            receipt.auxiliaryBefore = first.auxiliary;
            receipt.flagBefore = first.flag;
#if defined(MTA_DE_STARTUP_MENU_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_STARTUP_MENU_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &Detail::NativeCall;
#endif
            record.stage.store(Stage::Entering, std::memory_order_release);
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(image + Detail::P::EntryRva), receipt.root, receipt.nativeResult, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return Finish(record, result, output);
            receipt.returned = true;
            // Native owns removal, its root-field clears, and all subsequent
            // input/audio effects. The detached widget may already be dead;
            // only the enclosing world/root authority is reacquired here.
            if ((result = Detail::Capture(image, owner, false, next)) != ClockResult::Ok)
                return Finish(record, result, output);
            receipt.widgetAfter = next.widget;
            receipt.auxiliaryAfter = next.auxiliary;
            receipt.flagAfter = next.flag;
            if (!Detail::C::Same(first.world, next.world, false) || next.widget || next.flag || (first.widget && next.auxiliary) ||
                receipt.nativeResult != (first.widget != 0))
                return Finish(record, ClockResult::BindingChanged, output);
            receipt.verified = true;
            return Finish(record, ClockResult::Ok, output);
        }
    };
    ClockResult Close(std::uintptr_t image, Record& record, std::uint64_t generation, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        return Access::Run(image, record, generation, owner, output, invoke);
    }
}
